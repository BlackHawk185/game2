// GlobalLightingManager.cpp - Implementation of frustum-culled global lighting
#include "GlobalLightingManager.h"

#include <chrono>
#include <iostream>
#include <algorithm>
#include <cmath>

#include "../Input/Camera.h"
#include "../World/VoxelChunk.h"
#include "../World/IslandChunkSystem.h"
#include "../Culling/FrustumCuller.h"
#include "../Profiling/Profiler.h"

// Global instance
GlobalLightingManager g_globalLighting;

GlobalLightingManager::GlobalLightingManager() {
    m_sunDirection = m_sunDirection.normalized();
}

GlobalLightingManager::~GlobalLightingManager() = default;

void GlobalLightingManager::updateGlobalLighting(const Camera& camera, IslandChunkSystem* islandSystem, float aspect) {
    if (!m_enabled || !islandSystem) return;
    
    PROFILE_SCOPE("GlobalLightingManager::updateGlobalLighting");
    
    // NEW: Smart throttling - only update when chunks need lighting OR sun direction changed
    auto currentTime = std::chrono::high_resolution_clock::now();
    float currentTimeMs = std::chrono::duration<float, std::milli>(currentTime.time_since_epoch()).count();
    
    // Check if we need to force an update due to sun direction change
    bool forceUpdate = m_sunDirectionChanged;
    
    // Use different throttle intervals based on whether we have pending updates
    const float UPDATE_INTERVAL = forceUpdate ? 16.7f : 500.0f; // 60 FPS when sun changes, 2 FPS for maintenance
    if (!forceUpdate && currentTimeMs - m_lastUpdateTime < UPDATE_INTERVAL) {
        return; // Skip this update - no urgent work to do
    }
    m_lastUpdateTime = currentTimeMs;
    
    // Store island system reference for use by other methods
    m_currentIslandSystem = islandSystem;
    
    auto startTime = std::chrono::high_resolution_clock::now();
    m_stats = Stats{}; // Reset stats
    
    // Step 1: Gather visible chunks using frustum culling (but limit scope)
    gatherVisibleChunksEfficient(camera, islandSystem, aspect);
    
    // Step 2: Only process chunks that actually need lighting updates (much more efficient!)
    if (!m_visibleChunks.empty()) {
        generateOptimizedLighting();  // Now only processes chunks with lightingDirty flag
    }
    
    auto endTime = std::chrono::high_resolution_clock::now();
    m_stats.updateTimeMs = std::chrono::duration<float, std::milli>(endTime - startTime).count();
    
    // Debug output occasionally
    static int debugCounter = 0;
    if (++debugCounter % 120 == 0) { // Every ~60 seconds at 2 FPS (was every 5 seconds)
        std::cout << "[GLOBAL_LIGHTING] Processed " << m_stats.chunksLit << "/" 
                  << m_stats.chunksConsidered << " chunks in " 
                  << m_stats.updateTimeMs << "ms (Event-driven mode)" << std::endl;
    }
}

// Recalculate occlusion-only lightmaps for all chunks within a neighborhood radius around a center
void GlobalLightingManager::recalcOcclusionNeighborhood(IslandChunkSystem* islandSystem,
                                                        uint32_t islandID,
                                                        const Vec3& centerChunkCoord,
                                                        int radiusChunks)
{
    if (!islandSystem) return;
    PROFILE_SCOPE("GlobalLightingManager::recalcOcclusionNeighborhood");

    FloatingIsland* island = islandSystem->getIsland(islandID);
    if (!island) return;

    // Iterate in a cube radius around the center chunk
    for (int dz = -radiusChunks; dz <= radiusChunks; ++dz) {
        for (int dy = -radiusChunks; dy <= radiusChunks; ++dy) {
            for (int dx = -radiusChunks; dx <= radiusChunks; ++dx) {
                Vec3 cc(centerChunkCoord.x + dx, centerChunkCoord.y + dy, centerChunkCoord.z + dz);
                VoxelChunk* chunk = islandSystem->getChunkFromIsland(islandID, cc);
                if (!chunk) continue;

                // Compute world pos of chunk origin
                Vec3 chunkWorldPos = island->physicsCenter + FloatingIsland::chunkCoordToWorldPos(cc);

                // Generate occlusion-only face lightmaps factoring neighbor chunks
                ChunkLightMaps& lightMaps = chunk->getLightMaps();

                // Six faces normals
                const Vec3 faceNormals[6] = {
                    Vec3(1, 0, 0),  Vec3(-1, 0, 0),
                    Vec3(0, 1, 0),  Vec3(0, -1, 0),
                    Vec3(0, 0, 1),  Vec3(0, 0, -1)
                };

                const int LIGHTMAP_SIZE = FaceLightMap::LIGHTMAP_SIZE;
                const int SAMPLE_STEP = 4; // 4x4 texel blocks

                auto sampleSolidAtIslandPos = [&](const Vec3& islandPos) -> bool {
                    // Query voxel value via island system; returns >0 if solid
                    return islandSystem->getVoxelFromIsland(islandID, islandPos) != 0;
                };

                auto worldToIsland = [&](const Vec3& world) -> Vec3 {
                    return world - island->physicsCenter;
                };

                // For each face, fill the lightmap using a shared, group-anchored sampling grid
                for (int faceIndex = 0; faceIndex < 6; ++faceIndex) {
                    FaceLightMap& faceMap = lightMaps.getFaceMap(faceIndex);
                    faceMap.data.resize(LIGHTMAP_SIZE * LIGHTMAP_SIZE * 3);

                    const Vec3 n = faceNormals[faceIndex];

                    // Determine constant axis and varying axes per face to match mesh UV mapping
                    // 0=+X(Y,Z), 1=-X(Z,Y), 2=+Y(Z,X), 3=-Y(X,Z), 4=+Z(X,Y), 5=-Z(Y,X)
                    int axisConst = (faceIndex == 0 || faceIndex == 1) ? 0 : (faceIndex == 2 || faceIndex == 3) ? 1 : 2; // 0=X,1=Y,2=Z
                    int axisU, axisV; // which axes correspond to U and V
                    switch (faceIndex) {
                        case 0: axisU = 1; axisV = 2; break; // +X: U=Y, V=Z
                        case 1: axisU = 2; axisV = 1; break; // -X: U=Z, V=Y
                        case 2: axisU = 2; axisV = 0; break; // +Y: U=Z, V=X
                        case 3: axisU = 0; axisV = 2; break; // -Y: U=X, V=Z
                        case 4: axisU = 0; axisV = 1; break; // +Z: U=X, V=Y
                        case 5: axisU = 1; axisV = 0; break; // -Z: U=Y, V=X
                        default: axisU = 0; axisV = 1; break;
                    }

                    // Constant coordinate on the face plane (exact chunk boundary to align with neighbors)
                    float planeCoord;
                    if (faceIndex == 0)      planeCoord = chunkWorldPos.x + VoxelChunk::SIZE; // +X
                    else if (faceIndex == 1) planeCoord = chunkWorldPos.x;                    // -X
                    else if (faceIndex == 2) planeCoord = chunkWorldPos.y + VoxelChunk::SIZE; // +Y
                    else if (faceIndex == 3) planeCoord = chunkWorldPos.y;                    // -Y
                    else if (faceIndex == 4) planeCoord = chunkWorldPos.z + VoxelChunk::SIZE; // +Z
                    else                     planeCoord = chunkWorldPos.z;                    // -Z

                    for (int v = 0; v < LIGHTMAP_SIZE; v += SAMPLE_STEP) {
                        for (int u = 0; u < LIGHTMAP_SIZE; u += SAMPLE_STEP) {
                            // Map (u,v) in [0..LIGHTMAP_SIZE) to world coordinates along the two varying axes
                            float fu = (static_cast<float>(u) + 0.5f) / static_cast<float>(LIGHTMAP_SIZE);
                            float fv = (static_cast<float>(v) + 0.5f) / static_cast<float>(LIGHTMAP_SIZE);
                            float coordU = fu * VoxelChunk::SIZE;
                            float coordV = fv * VoxelChunk::SIZE;

                            // Build world position vector [X,Y,Z]
                            float worldCoord[3] = { chunkWorldPos.x, chunkWorldPos.y, chunkWorldPos.z };
                            worldCoord[axisConst] = planeCoord;
                            worldCoord[axisU] = (axisU == 0 ? chunkWorldPos.x : axisU == 1 ? chunkWorldPos.y : chunkWorldPos.z) + coordU;
                            worldCoord[axisV] = (axisV == 0 ? chunkWorldPos.x : axisV == 1 ? chunkWorldPos.y : chunkWorldPos.z) + coordV;
                            Vec3 world(worldCoord[0], worldCoord[1], worldCoord[2]);

                            // Hemisphere AO sampling: a few rays in the face normal hemisphere
                            const Vec3 dirs[6] = {
                                n,
                                (n + Vec3(0.5f, 0.0f, 0.0f)).normalized(),
                                (n + Vec3(-0.5f, 0.0f, 0.0f)).normalized(),
                                (n + Vec3(0.0f, 0.5f, 0.0f)).normalized(),
                                (n + Vec3(0.0f, -0.5f, 0.0f)).normalized(),
                                (n + Vec3(0.0f, 0.0f, 0.5f)).normalized()
                            };

                            int blocked = 0;
                            const int rayCount = 6;
                            const float maxDist = static_cast<float>(VoxelChunk::SIZE * radiusChunks);
                            const float step = 2.0f;
                            Vec3 start = world + n * 0.001f; // nudge off the surface
                            for (int r = 0; r < rayCount; ++r) {
                                bool hit = false;
                                Vec3 p = start;
                                float traveled = 0.0f;
                                while (traveled < maxDist) {
                                    p = p + dirs[r] * step;
                                    traveled += step;
                                    Vec3 islandPos = worldToIsland(p);
                                    if (sampleSolidAtIslandPos(islandPos)) { hit = true; break; }
                                }
                                if (hit) blocked++;
                            }

                            float occlusion = 1.0f - (static_cast<float>(blocked) / static_cast<float>(rayCount));
                            occlusion = std::max(0.0f, std::min(1.0f, occlusion));
                            uint8_t vbyte = static_cast<uint8_t>(occlusion * 255);

                            for (int dv = v; dv < std::min(v + SAMPLE_STEP, LIGHTMAP_SIZE); ++dv) {
                                for (int du = u; du < std::min(u + SAMPLE_STEP, LIGHTMAP_SIZE); ++du) {
                                    int idx = (dv * LIGHTMAP_SIZE + du) * 3;
                                    faceMap.data[idx + 0] = vbyte;
                                    faceMap.data[idx + 1] = vbyte;
                                    faceMap.data[idx + 2] = vbyte;
                                }
                            }
                        }
                    }

                    faceMap.needsUpdate = true;
                }

                // Upload or refresh textures
                chunk->updateLightMapTextures();
                chunk->markLightingClean();
            }
        }
    }
}

void GlobalLightingManager::gatherVisibleChunks(const Camera& camera, IslandChunkSystem* islandSystem, float aspect) {
    PROFILE_SCOPE("GlobalLightingManager::gatherVisibleChunks");
    
    m_visibleChunks.clear();
    m_islandChunkMap.clear();
    
    if (!islandSystem) {
        std::cout << "[GLOBAL_LIGHTING_DEBUG] No island system provided!" << std::endl;
        return;
    }
    
    // Update frustum culler
    extern FrustumCuller g_frustumCuller;
    g_frustumCuller.updateFromCamera(camera, aspect, 75.0f);
    
    // Get all islands from the provided island system
    const auto& islands = islandSystem->getIslands();
    
    std::cout << "[GLOBAL_LIGHTING_DEBUG] Found " << islands.size() << " islands in system" << std::endl;
    
    for (const auto& [islandID, island] : islands) {
        std::cout << "[GLOBAL_LIGHTING_DEBUG] Island " << islandID << " has " << island.chunks.size() << " chunks" << std::endl;
        for (const auto& [chunkCoord, chunk] : island.chunks) {
            if (!chunk) continue;
            
            m_stats.chunksConsidered++;
            
            // Calculate world position for this chunk
            Vec3 chunkWorldPos = island.physicsCenter + 
                FloatingIsland::chunkCoordToWorldPos(chunkCoord);
            
            // Frustum culling check - TEMPORARILY DISABLED FOR DEBUG
            Vec3 chunkCenter = chunkWorldPos + Vec3(16.0f, 16.0f, 16.0f); // 32x32x32 chunk center
            // if (g_frustumCuller.shouldCullChunk(chunkCenter, 22.6f)) { // sqrt(16^2 * 3) = ~27.7, use 22.6 for tighter culling
            //     m_stats.chunksCulled++;
            //     continue;
            // }
            
            // Distance culling check - TEMPORARILY DISABLED FOR DEBUG
            // if (g_frustumCuller.shouldCullByDistance(chunkCenter, camera.position)) {
            //     m_stats.chunksCulled++;
            //     continue;
            // }
            
            // This chunk is visible - add to processing list
            VisibleChunk visibleChunk;
            visibleChunk.chunk = chunk.get();
            visibleChunk.worldPosition = chunkWorldPos;
            visibleChunk.islandID = islandID;
            m_visibleChunks.push_back(visibleChunk);
            
            std::cout << "[GLOBAL_LIGHTING_DEBUG] Added chunk at " << chunkWorldPos.x << "," << chunkWorldPos.y << "," << chunkWorldPos.z << std::endl;
            
            // Build chunk lookup map for fast cross-chunk queries
            m_islandChunkMap[islandID] = &island;
        }
    }
}

bool GlobalLightingManager::performGlobalSunRaycast(const Vec3& rayStart, const Vec3& sunDirection, float maxDistance) const {
    PROFILE_SCOPE("GlobalLightingManager::performGlobalSunRaycast");
    
    const float stepSize = 1.0f;
    const int maxSteps = static_cast<int>(maxDistance / stepSize);
    
    Vec3 rayPos = rayStart;
    Vec3 rayStep = sunDirection * stepSize;
    
    for (int step = 0; step < maxSteps; ++step) {
        rayPos = rayPos + rayStep;
        
        // Sample voxel at current world position using global lookup
        uint8_t voxel = sampleVoxelAtWorldPos(rayPos);
        if (voxel != 0) {
            // Hit a solid voxel - ray is occluded
            return true;
        }
    }
    
    // Ray traveled maximum distance without hitting anything
    return false;
}

bool GlobalLightingManager::performFastSunRaycast(const Vec3& rayStart, const Vec3& sunDirection, float maxDistance) const {
    PROFILE_SCOPE("GlobalLightingManager::performFastSunRaycast");
    
    // Much faster raycast - larger steps, shorter distance
    const float stepSize = 2.0f; // Double the step size for speed
    const int maxSteps = static_cast<int>(maxDistance / stepSize); // Much fewer steps
    
    Vec3 rayPos = rayStart;
    Vec3 rayStep = sunDirection * stepSize;
    
    for (int step = 0; step < maxSteps; ++step) {
        rayPos = rayPos + rayStep;
        
        // Sample voxel at current world position using global lookup
        uint8_t voxel = sampleVoxelAtWorldPos(rayPos);
        if (voxel != 0) {
            // Hit a solid voxel - ray is occluded
            return true;
        }
    }
    
    // Ray traveled maximum distance without hitting anything
    return false;
}

uint8_t GlobalLightingManager::sampleVoxelAtWorldPos(const Vec3& worldPos) const {
    PROFILE_SCOPE("GlobalLightingManager::sampleVoxelAtWorldPos");
    
    // Check all visible chunks for intersection
    for (const auto& visibleChunk : m_visibleChunks) {
        Vec3 chunkMin = visibleChunk.worldPosition;
        Vec3 chunkMax = chunkMin + Vec3(VoxelChunk::SIZE, VoxelChunk::SIZE, VoxelChunk::SIZE);
        
        // Check if world position is within this chunk's bounds
        if (worldPos.x >= chunkMin.x && worldPos.x < chunkMax.x &&
            worldPos.y >= chunkMin.y && worldPos.y < chunkMax.y &&
            worldPos.z >= chunkMin.z && worldPos.z < chunkMax.z) {
            
            // Convert world position to local chunk coordinates
            Vec3 localPos = worldPos - chunkMin;
            int x = static_cast<int>(localPos.x);
            int y = static_cast<int>(localPos.y);
            int z = static_cast<int>(localPos.z);
            
            // Clamp to valid bounds
            x = std::max(0, std::min(VoxelChunk::SIZE - 1, x));
            y = std::max(0, std::min(VoxelChunk::SIZE - 1, y));
            z = std::max(0, std::min(VoxelChunk::SIZE - 1, z));
            
            return visibleChunk.chunk->getVoxel(x, y, z);
        }
    }
    
    // Position not in any visible chunk - assume air
    return 0;
}

void GlobalLightingManager::generateOptimizedLighting() {
    PROFILE_SCOPE("GlobalLightingManager::generateOptimizedLighting");
    
    // NEW: Check if sun direction changed (time-of-day lighting updates)
    bool sunDirectionChanged = m_sunDirectionChanged;
    if (sunDirectionChanged) {
        // Force lighting update on all visible chunks when sun direction changes
        for (const auto& visibleChunk : m_visibleChunks) {
            visibleChunk.chunk->markLightingDirty();
        }
        m_sunDirectionChanged = false;
    }
    
    // Only process chunks that actually need lighting updates
    int chunksProcessed = 0;
    int chunksSkipped = 0;
    
    for (const auto& visibleChunk : m_visibleChunks) {
        VoxelChunk* chunk = visibleChunk.chunk;
        
        // SMART UPDATE: Only recompute lighting if:
        // 1. Chunk geometry changed (lighting marked dirty)
        // 2. Lighting has never been computed (no valid lightmaps)
        // 3. Sun direction changed (time-of-day update)
        if (chunk->needsLightingUpdate() || !chunk->hasValidLightMaps()) {
            processChunkLightingOptimized(chunk, visibleChunk.worldPosition);
            chunk->markLightingClean();  // Mark as clean after processing
            chunksProcessed++;
            m_stats.chunksLit++;
        } else {
            chunksSkipped++;
        }
    }
    
    // Debug output for optimization tracking (only occasionally)
    static int debugCounter = 0;
    if (debugCounter++ % 300 == 0) {  // Every 5 seconds at 60 FPS
        std::cout << "[LIGHTING_OPTIMIZATION] Processed: " << chunksProcessed 
                  << ", Skipped: " << chunksSkipped 
                  << " (Efficiency: " << (chunksSkipped * 100 / std::max(1, chunksProcessed + chunksSkipped)) << "%)"
                  << (sunDirectionChanged ? " [SUN_CHANGED]" : "") << std::endl;
    }
}
void GlobalLightingManager::gatherVisibleChunksEfficient(const Camera& camera, IslandChunkSystem* islandSystem, float aspect) {
    PROFILE_SCOPE("GlobalLightingManager::gatherVisibleChunks");
    
    m_visibleChunks.clear();
    m_islandChunkMap.clear();
    
    if (!islandSystem) {
        return;
    }
    
    // Update frustum culler
    extern FrustumCuller g_frustumCuller;
    g_frustumCuller.updateFromCamera(camera, aspect, 75.0f);
    
    // Get all islands from the provided island system
    const auto& islands = islandSystem->getIslands();
    
    for (const auto& [islandID, island] : islands) {
        // Limit chunks per island to prevent performance issues
        int chunksAdded = 0;
        const int MAX_CHUNKS_PER_ISLAND = 50; // Reasonable limit
        
        for (const auto& [chunkCoord, chunk] : island.chunks) {
            if (!chunk) continue;
            if (chunksAdded >= MAX_CHUNKS_PER_ISLAND) break; // Stop processing this island
            
            m_stats.chunksConsidered++;
            
            // Calculate world position for this chunk
            Vec3 chunkWorldPos = island.physicsCenter + 
                FloatingIsland::chunkCoordToWorldPos(chunkCoord);
            
            // Frustum culling check
            Vec3 chunkCenter = chunkWorldPos + Vec3(16.0f, 16.0f, 16.0f);
            if (g_frustumCuller.shouldCullChunk(chunkCenter, 22.6f)) {
                m_stats.chunksCulled++;
                continue;
            }
            
            // Distance culling check  
            if (g_frustumCuller.shouldCullByDistance(chunkCenter, camera.position)) {
                m_stats.chunksCulled++;
                continue;
            }
            
            // This chunk is visible - add to processing list
            VisibleChunk visibleChunk;
            visibleChunk.chunk = chunk.get();
            visibleChunk.worldPosition = chunkWorldPos;
            visibleChunk.islandID = islandID;
            m_visibleChunks.push_back(visibleChunk);
            
            // Build chunk lookup map for fast cross-chunk queries
            m_islandChunkMap[islandID] = &island;
            
            chunksAdded++;
        }
    }
    
}

void GlobalLightingManager::processChunkLightingOptimized(VoxelChunk* chunk, const Vec3& chunkWorldPos) {
    PROFILE_SCOPE("GlobalLightingManager::processChunkLighting");
    
    // Get chunk's light maps
    ChunkLightMaps& lightMaps = chunk->getLightMaps();
    
    // Face normals for lighting calculations
    const Vec3 faceNormals[6] = {
        Vec3(-1, 0, 0), Vec3(1, 0, 0),   // Left, Right (X faces)
        Vec3(0, -1, 0), Vec3(0, 1, 0),   // Bottom, Top (Y faces)  
        Vec3(0, 0, -1), Vec3(0, 0, 1)    // Back, Front (Z faces)
    };
    
    const int LIGHTMAP_SIZE = 32;
    
    // Generate lighting for each face
    for (int faceIndex = 0; faceIndex < 6; faceIndex++) {
        FaceLightMap& faceMap = lightMaps.getFaceMap(faceIndex);
        faceMap.data.resize(LIGHTMAP_SIZE * LIGHTMAP_SIZE * 3);
        
        Vec3 faceNormal = faceNormals[faceIndex];
        
        // FAST: Use larger sample steps for maximum performance
        const int SAMPLE_STEP = 8; // Sample every 8th pixel for 64x fewer calculations than full resolution
        
        for (int v = 0; v < LIGHTMAP_SIZE; v += SAMPLE_STEP) {
            for (int u = 0; u < LIGHTMAP_SIZE; u += SAMPLE_STEP) {
                float normalizedU = static_cast<float>(u) / (LIGHTMAP_SIZE - 1);
                float normalizedV = static_cast<float>(v) / (LIGHTMAP_SIZE - 1);
                
                // Calculate world position for this light map texel
                Vec3 localPos = chunk->calculateWorldPositionFromLightMapUV(faceIndex, normalizedU, normalizedV);
                Vec3 worldPos = chunkWorldPos + localPos;
                
                // Simple face orientation lighting - no raycasting for performance
                bool isOccluded = false;
                // Skip occlusion checks entirely for maximum performance
                
                // Calculate lighting based on face orientation and occlusion
                float dotProduct = faceNormal.dot(m_sunDirection);
                
                // Debug the sun direction and face normal
                static int debugOutputCount = 0;
                if (debugOutputCount < 3 && u == 0 && v == 0 && faceIndex == 0) {
                    std::cout << "[LIGHTING_DEBUG] Sun direction: " << m_sunDirection.x << "," << m_sunDirection.y << "," << m_sunDirection.z << std::endl;
                    std::cout << "[LIGHTING_DEBUG] Face normal: " << faceNormal.x << "," << faceNormal.y << "," << faceNormal.z << std::endl;
                    std::cout << "[LIGHTING_DEBUG] Dot product: " << dotProduct << std::endl;
                    debugOutputCount++;
                }
                
                float finalLight;
                if (dotProduct > 0.0f) {
                    // Surface faces toward sun
                    if (isOccluded) {
                        // Occluded - darker but not completely black
                        finalLight = 0.2f; // 20% ambient light in shadows
                    } else {
                        // Clear path to sun - BRIGHT WHITE for dramatic contrast
                        finalLight = 1.0f; // Pure white for sun-facing surfaces
                    }
                } else {
                    // Surface faces away from sun - PURE BLACK for dramatic contrast
                    finalLight = 0.0f; // Pure black for shadow areas
                }
                
                // Clamp and convert to RGB
                finalLight = std::max(0.0f, std::min(1.0f, finalLight));
                uint8_t lightValue = static_cast<uint8_t>(finalLight * 255);
                
                // Fill a block of texels with the same value (since we're sampling every 4th)
                for (int fv = v; fv < std::min(v + SAMPLE_STEP, LIGHTMAP_SIZE); fv++) {
                    for (int fu = u; fu < std::min(u + SAMPLE_STEP, LIGHTMAP_SIZE); fu++) {
                        int texelIndex = (fv * LIGHTMAP_SIZE + fu) * 3;
                        faceMap.data[texelIndex + 0] = lightValue; // R
                        faceMap.data[texelIndex + 1] = lightValue; // G
                        faceMap.data[texelIndex + 2] = lightValue; // B
                    }
                }
            }
        }
    }
    
    // Only update textures if this chunk doesn't have valid lightmaps yet
    // Don't force recreation every frame!
    if (!chunk->hasValidLightMaps()) {
        chunk->updateLightMapTextures();
    }
}

bool GlobalLightingManager::performFastOcclusionCheck(const Vec3& worldPos, const Vec3& faceNormal) const {
    PROFILE_SCOPE("GlobalLightingManager::performFastOcclusionCheck");
    
    // OPTIMIZED: Simple occlusion check - only test immediate neighbors
    // Sample just a few points instead of full raycast
    const float CHECK_DISTANCE = 10.0f; // Much shorter than original 128.0f
    const int SAMPLE_COUNT = 3; // Much fewer samples than original
    
    Vec3 rayStep = m_sunDirection * (CHECK_DISTANCE / SAMPLE_COUNT);
    Vec3 rayPos = worldPos + faceNormal * 0.5f; // Start offset from surface
    
    for (int i = 0; i < SAMPLE_COUNT; i++) {
        rayPos = rayPos + rayStep;
        
        // Simple check - sample voxel at position  
        uint8_t voxel = sampleVoxelAtWorldPosOptimized(rayPos);
        if (voxel != 0) {
            return true; // Occluded
        }
    }
    
    return false; // Not occluded
}

uint8_t GlobalLightingManager::sampleVoxelAtWorldPosOptimized(const Vec3& worldPos) const {
    PROFILE_SCOPE("GlobalLightingManager::sampleVoxelAtWorldPos");
    
    // OPTIMIZED: Only check the first few visible chunks instead of all
    int chunksChecked = 0;
    const int MAX_CHUNKS_TO_CHECK = 5; // Limit search scope
    
    for (const auto& visibleChunk : m_visibleChunks) {
        if (chunksChecked >= MAX_CHUNKS_TO_CHECK) break;
        
        Vec3 chunkMin = visibleChunk.worldPosition;
        Vec3 chunkMax = chunkMin + Vec3(VoxelChunk::SIZE, VoxelChunk::SIZE, VoxelChunk::SIZE);
        
        // Check if world position is within this chunk's bounds
        if (worldPos.x >= chunkMin.x && worldPos.x < chunkMax.x &&
            worldPos.y >= chunkMin.y && worldPos.y < chunkMax.y &&
            worldPos.z >= chunkMin.z && worldPos.z < chunkMax.z) {
            
            // Convert world position to local chunk coordinates
            Vec3 localPos = worldPos - chunkMin;
            int x = static_cast<int>(localPos.x);
            int y = static_cast<int>(localPos.y);
            int z = static_cast<int>(localPos.z);
            
            // Clamp to valid bounds
            x = std::max(0, std::min(VoxelChunk::SIZE - 1, x));
            y = std::max(0, std::min(VoxelChunk::SIZE - 1, y));
            z = std::max(0, std::min(VoxelChunk::SIZE - 1, z));
            
            return visibleChunk.chunk->getVoxel(x, y, z);
        }
        
        chunksChecked++;
    }
    
    // Position not in any nearby chunk - assume air
    return 0;
}
