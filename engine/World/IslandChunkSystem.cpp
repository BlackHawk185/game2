// IslandChunkSystem.cpp - Implementation of physics-driven chunking
#include "IslandChunkSystem.h"

#include <cmath>
#include <cstdlib>
#include <ctime>
#include <iostream>
#include <memory>
#include <string>
#include <chrono>
#include <unordered_set>

#include "VoxelChunk.h"
#include "BlockType.h"
#include "ConnectivityAnalyzer.h"
#include "../Profiling/Profiler.h"
#include "../Rendering/MDIRenderer.h"
#include "../Rendering/ModelInstanceRenderer.h"
#include <FastNoise/FastNoise.h>
#include <FastNoise/Generators/Perlin.h>
#include <FastNoise/Generators/Simplex.h>
#include <FastNoise/Generators/BasicGenerators.h>

IslandChunkSystem g_islandSystem;

IslandChunkSystem::IslandChunkSystem()
{
    // Initialize system
    // Seed random number generator for island velocity
    std::srand(static_cast<unsigned int>(std::time(nullptr)));
}

IslandChunkSystem::~IslandChunkSystem()
{
    // Clean up all islands
    // Collect IDs first to avoid iterator invalidation
    std::vector<uint32_t> islandIDs;
    for (auto& [id, island] : m_islands)
    {
        islandIDs.push_back(id);
    }
    
    // Now safely destroy all islands
    for (uint32_t id : islandIDs)
    {
        destroyIsland(id);
    }
}

uint32_t IslandChunkSystem::createIsland(const Vec3& physicsCenter)
{
    return createIsland(physicsCenter, 0);  // 0 = auto-assign ID
}

uint32_t IslandChunkSystem::createIsland(const Vec3& physicsCenter, uint32_t forceIslandID)
{
    std::lock_guard<std::mutex> lock(m_islandsMutex);
    
    // Determine island ID
    uint32_t islandID;
    if (forceIslandID == 0)
    {
        // Auto-assign: use next available ID
        islandID = m_nextIslandID++;
    }
    else
    {
        // Force specific ID (for network sync)
        islandID = forceIslandID;
        // Update next ID to avoid collisions
        if (forceIslandID >= m_nextIslandID)
        {
            m_nextIslandID = forceIslandID + 1;
        }
    }
    
    // Create the island
    FloatingIsland& island = m_islands[islandID];
    island.islandID = islandID;
    island.physicsCenter = physicsCenter;
    island.needsPhysicsUpdate = true;
    island.acceleration = Vec3(0.0f, 0.0f, 0.0f);
    
    // Set initial velocity - no random drift, islands start stationary
    // Velocity will be controlled by piloting or network updates
    island.velocity = Vec3(0.0f, 0.0f, 0.0f);
    
    return islandID;
}

void IslandChunkSystem::destroyIsland(uint32_t islandID)
{
    std::lock_guard<std::mutex> lock(m_islandsMutex);
    auto it = m_islands.find(islandID);
    if (it == m_islands.end())
        return;

    m_islands.erase(it);
}

FloatingIsland* IslandChunkSystem::getIsland(uint32_t islandID)
{
    std::lock_guard<std::mutex> lock(m_islandsMutex);
    auto it = m_islands.find(islandID);
    return (it != m_islands.end()) ? &it->second : nullptr;
}

const FloatingIsland* IslandChunkSystem::getIsland(uint32_t islandID) const
{
    std::lock_guard<std::mutex> lock(m_islandsMutex);
    auto it = m_islands.find(islandID);
    return (it != m_islands.end()) ? &it->second : nullptr;
}

Vec3 IslandChunkSystem::getIslandCenter(uint32_t islandID) const
{
    std::lock_guard<std::mutex> lock(m_islandsMutex);
    auto it = m_islands.find(islandID);
    if (it != m_islands.end())
    {
        return it->second.physicsCenter;
    }
    return Vec3(0.0f, 0.0f, 0.0f);  // Return zero vector if island not found
}

Vec3 IslandChunkSystem::getIslandVelocity(uint32_t islandID) const
{
    std::lock_guard<std::mutex> lock(m_islandsMutex);
    auto it = m_islands.find(islandID);
    if (it != m_islands.end())
    {
        return it->second.velocity;
    }
    return Vec3(0.0f, 0.0f, 0.0f);  // Return zero velocity if island not found
}

void IslandChunkSystem::addChunkToIsland(uint32_t islandID, const Vec3& chunkCoord)
{
    std::lock_guard<std::mutex> lock(m_islandsMutex);
    auto itIsl = m_islands.find(islandID);
    FloatingIsland* island = (itIsl != m_islands.end()) ? &itIsl->second : nullptr;
    if (!island)
        return;

    // Check if chunk already exists
    if (island->chunks.find(chunkCoord) != island->chunks.end())
        return;

    // Create new chunk
    island->chunks[chunkCoord] = std::make_unique<VoxelChunk>();
}

void IslandChunkSystem::removeChunkFromIsland(uint32_t islandID, const Vec3& chunkCoord)
{
    std::lock_guard<std::mutex> lock(m_islandsMutex);
    auto itIsl = m_islands.find(islandID);
    FloatingIsland* island = (itIsl != m_islands.end()) ? &itIsl->second : nullptr;
    if (!island)
        return;

    auto it = island->chunks.find(chunkCoord);
    if (it != island->chunks.end())
    {
        island->chunks.erase(it);
    }
}

VoxelChunk* IslandChunkSystem::getChunkFromIsland(uint32_t islandID, const Vec3& chunkCoord)
{
    std::lock_guard<std::mutex> lock(m_islandsMutex);
    auto itIsl = m_islands.find(islandID);
    FloatingIsland* island = (itIsl != m_islands.end()) ? &itIsl->second : nullptr;
    if (!island)
        return nullptr;

    auto it = island->chunks.find(chunkCoord);
    if (it != island->chunks.end())
    {
        return it->second.get();
    }

    return nullptr;
}

void IslandChunkSystem::generateFloatingIslandOrganic(uint32_t islandID, uint32_t seed, float radius)
{
    PROFILE_SCOPE("IslandChunkSystem::generateFloatingIslandOrganic");
    
    auto startTime = std::chrono::high_resolution_clock::now();
    
    FloatingIsland* island = getIsland(islandID);
    if (!island)
        return;

    // Start with a center chunk at origin to ensure we have at least one chunk
    addChunkToIsland(islandID, Vec3(0, 0, 0));
    
    // **NOISE PARAMETERS** - Tuned for natural island terrain
    float noiseScale = 0.05f;  // Frequency for detail features
    float terrainScale = 0.02f;  // Lower frequency for large hills/valleys
    float densityThreshold = 0.3f;  // Threshold for solid voxels
    
    // **ENVIRONMENT OVERRIDES** for noise parameters
    const char* scaleEnv = std::getenv("NOISE_SCALE");
    if (scaleEnv) noiseScale = std::stof(scaleEnv);
    
    const char* thresholdEnv = std::getenv("NOISE_THRESHOLD");
    if (thresholdEnv) densityThreshold = std::stof(thresholdEnv);
    
    int voxelsGenerated = 0;
    int chunksCreated = 0;
    
    // **SURFACE CACHE** - Track surface positions for decoration pass
    std::vector<Vec3> surfacePositions;
    surfacePositions.reserve(static_cast<size_t>(radius * radius * 2)); // Rough estimate
    
    // **ISLAND HEIGHT CONFIGURATION** - More natural vertical proportions
    // Increased height ratio for less pancake-like islands
    float baseHeightRatio = 0.15f;  // 15% of radius for height (increased from 8%)
    float heightScaling = 1.0f;     // Normal scaling for more vertical variation
    int islandHeight = static_cast<int>(radius * baseHeightRatio * heightScaling);
    
    int searchRadius = static_cast<int>(radius * 1.4f);
    
    auto voxelGenStart = std::chrono::high_resolution_clock::now();
    
    // **OPTIMIZATION PRECALCULATIONS**
    float radiusSquared = (radius * 1.4f) * (radius * 1.4f);
    float radiusDivisor = 1.0f / (radius * 1.2f);
    float seedOffset1 = seed * 0.1f;
    float seedOffset2 = seed * 0.2f;
    float seedOffset3 = seed * 0.3f;
    float seedOffset4 = seed * 0.4f;
    float seedOffset5 = seed * 0.5f;
    
    // Iterate through potential island volume
    for (int y = -islandHeight; y <= islandHeight; y++)
    {
        float dy = static_cast<float>(y);
        
        // Pre-calculate vertical density with smooth gradual falloff (no hard cutoffs)
        float islandHeightRange = islandHeight * 2.0f;
        float normalizedY = (dy + islandHeight) / islandHeightRange;  // 0 at bottom, 1 at top
        
        // Smooth parabolic falloff from center - creates natural rounded shape
        // Center (0.5) = full density, edges taper gradually
        float centerOffset = normalizedY - 0.5f;  // -0.5 to +0.5
        float verticalDensity = 1.0f - (centerOffset * centerOffset * 4.0f);  // Parabola
        verticalDensity = std::max(0.0f, verticalDensity);  // Clamp to 0-1
        
        // Pre-calculate Y-based noise component (shared across X/Z)
        float noiseY_freq1 = dy * noiseScale * 0.5f + seedOffset2;
        float noiseY_octave1 = std::cos(noiseY_freq1);
        
        for (int x = -searchRadius; x <= searchRadius; x++)
        {
            float dx = static_cast<float>(x);
            float dx_squared = dx * dx;
            
            // Pre-calculate X-based noise components
            float noiseX_freq1 = dx * noiseScale + seedOffset1;
            float noiseX_sin1 = std::sin(noiseX_freq1);
            float terrainX_freq1 = dx * terrainScale + seedOffset4;
            float terrainX_sin1 = std::sin(terrainX_freq1);
            
            for (int z = -searchRadius; z <= searchRadius; z++)
            {
                float dz = static_cast<float>(z);
                
                // **OPTIMIZED DISTANCE CHECK** - Use squared distance to avoid sqrt
                float distanceSquared = dx_squared + dz * dz;
                if (distanceSquared > radiusSquared) continue; // Early exit
                
                // Only calculate sqrt when we know voxel is within radius
                float distanceFromCenter = std::sqrt(distanceSquared);
                
                // **BASE ISLAND SHAPE** - Radial falloff for island edges
                float islandBase = 1.0f - (distanceFromCenter * radiusDivisor);
                islandBase = std::max(0.0f, islandBase);
                islandBase = islandBase * islandBase; // Smooth falloff
                
                // **OPTIMIZED 3D NOISE** - Reuse pre-calculated components
                float surfaceNoise = 0.0f;
                float amplitude = 1.0f;
                float frequency = noiseScale;
                
                // Octave 1 (reuse pre-calculated components)
                float noiseZ_freq1 = dz * frequency + seedOffset3;
                float octaveNoise1 = (noiseX_sin1 * noiseY_octave1 * std::sin(noiseZ_freq1) + 1.0f) * 0.5f;
                surfaceNoise += octaveNoise1 * amplitude;
                
                // Octave 2
                amplitude *= 0.5f;
                frequency *= 2.0f;
                float noiseX_freq2 = dx * frequency + seedOffset1;
                float noiseY_freq2 = dy * frequency * 0.5f + seedOffset2;
                float noiseZ_freq2 = dz * frequency + seedOffset3;
                float octaveNoise2 = (std::sin(noiseX_freq2) * std::cos(noiseY_freq2) * std::sin(noiseZ_freq2) + 1.0f) * 0.5f;
                surfaceNoise += octaveNoise2 * amplitude;
                
                // Octave 3
                amplitude *= 0.5f;
                frequency *= 2.0f;
                float noiseX_freq3 = dx * frequency + seedOffset1;
                float noiseY_freq3 = dy * frequency * 0.5f + seedOffset2;
                float noiseZ_freq3 = dz * frequency + seedOffset3;
                float octaveNoise3 = (std::sin(noiseX_freq3) * std::cos(noiseY_freq3) * std::sin(noiseZ_freq3) + 1.0f) * 0.5f;
                surfaceNoise += octaveNoise3 * amplitude;
                
                // **OPTIMIZED TERRAIN NOISE** - Reuse pre-calculated X component
                float terrainNoise = 0.0f;
                amplitude = 0.8f;
                frequency = terrainScale;
                
                // Octave 1 (reuse pre-calculated X component)
                float terrainZ_freq1 = dz * frequency + seedOffset5;
                float terrainOctave1 = (terrainX_sin1 * std::cos(terrainZ_freq1) + 1.0f) * 0.5f;
                terrainNoise += terrainOctave1 * amplitude;
                
                // Octave 2
                amplitude *= 0.5f;
                frequency *= 2.0f;
                float terrainX_freq2 = dx * frequency + seedOffset4;
                float terrainZ_freq2 = dz * frequency + seedOffset5;
                float terrainOctave2 = (std::sin(terrainX_freq2) * std::cos(terrainZ_freq2) + 1.0f) * 0.5f;
                terrainNoise += terrainOctave2 * amplitude;
                
                // **FINAL ISLAND DENSITY** - Combine all factors (smooth, no hard cutoffs)
                // Parabolic vertical density already creates natural rounded shape
                float finalDensity = islandBase * verticalDensity *
                                    (surfaceNoise * 0.6f + terrainNoise * 0.4f);
                
                bool shouldPlaceVoxel = (finalDensity > densityThreshold);
                
                if (shouldPlaceVoxel)
                {
                    // Track chunks before placement
                    size_t chunksBefore = island->chunks.size();
                    
                    // Use island-relative coordinates for placement
                    Vec3 islandRelativePos(dx, dy, dz);
                    
                    // All blocks are dirt
                    uint8_t blockID = BlockID::DIRT;
                    
                    setBlockIDWithAutoChunk(islandID, islandRelativePos, blockID);
                    
                    // Cache this as a potential surface position for decoration
                    surfacePositions.push_back(islandRelativePos);
                    
                    voxelsGenerated++;
                    
                    // Check if a new chunk was created
                    if (island->chunks.size() > chunksBefore)
                    {
                        chunksCreated++;
                    }
                }  // End shouldPlaceVoxel
            }  // End z loop
        }  // End x loop
    }  // End y loop
    
    auto voxelGenEnd = std::chrono::high_resolution_clock::now();
    auto voxelGenDuration = std::chrono::duration_cast<std::chrono::milliseconds>(voxelGenEnd - voxelGenStart).count();
    
    std::cout << "🔨 Voxel Generation: " << voxelGenDuration << "ms (" << voxelsGenerated << " voxels, " 
              << island->chunks.size() << " chunks)" << std::endl;
    
    // **CONNECTIVITY CLEANUP (FAST PATH)** - Remove disconnected satellite chunks
    auto connectivityStart = std::chrono::high_resolution_clock::now();
    
    // Use fast path: assumes (0,0,0) is part of main island, deletes everything else
    int voxelsRemoved = ConnectivityAnalyzer::cleanupSatellites(island, Vec3(0, 0, 0));
    
    if (voxelsRemoved > 0) {
        std::cout << "✅ Cleaned up " << voxelsRemoved << " satellite voxels" << std::endl;
    } else {
        std::cout << "✅ Island is fully connected (no satellites)" << std::endl;
    }
    
    auto connectivityEnd = std::chrono::high_resolution_clock::now();
    auto connectivityDuration = std::chrono::duration_cast<std::chrono::milliseconds>(connectivityEnd - connectivityStart).count();
    std::cout << "🔍 Connectivity Cleanup: " << connectivityDuration << "ms" << std::endl;
    
    // **DECORATION PASS** - Place grass and trees on surface positions
    auto decorationStart = std::chrono::high_resolution_clock::now();
    
    int grassPlaced = 0;
    int treesPlaced = 0; // For future tree implementation
    
    // Filter surface positions to only include actual surface blocks
    // (blocks with air above them that survived connectivity cleanup)
    for (const Vec3& pos : surfacePositions) {
        // Check if this block still exists (wasn't removed by connectivity cleanup)
        uint8_t blockID = getVoxelFromIsland(islandID, pos);
        if (blockID == BlockID::AIR) continue; // Block was removed
        
        // Check if the block above is air (this is a surface block)
        Vec3 above = pos + Vec3(0, 1, 0);
        uint8_t blockAbove = getVoxelFromIsland(islandID, above);
        if (blockAbove != BlockID::AIR) continue; // Not a surface block
        
        // **GRASS DECORATION** - 25% coverage (reduced from 50% for debugging)
        if ((std::rand() % 100) < 25) {
            setBlockIDWithAutoChunk(islandID, above, BlockID::DECOR_GRASS);
            grassPlaced++;
        }
        
        // **TREE PLACEMENT** - Future implementation
        // TODO: Add tree generation logic here
        // if (shouldPlaceTree(pos, seed)) {
        //     placeTree(islandID, above, seed);
        //     treesPlaced++;
        // }
    }
    
    auto decorationEnd = std::chrono::high_resolution_clock::now();
    auto decorationDuration = std::chrono::duration_cast<std::chrono::milliseconds>(decorationEnd - decorationStart).count();
    std::cout << "🌿 Decoration Pass: " << decorationDuration << "ms (" << grassPlaced << " grass)" << std::endl;
    
    auto meshGenStart = std::chrono::high_resolution_clock::now();
    
    // **SINGLE MESH GENERATION PASS** - much faster than per-voxel generation
    for (auto& [chunkCoord, chunk] : island->chunks)
    {
        if (chunk)
        {
            chunk->generateMesh();
            chunk->buildCollisionMesh();
            
            // Register with MDI renderer for batched rendering (1 draw call for all chunks!)
            if (g_mdiRenderer)
            {
                // Compute chunk local position (relative to island center)
                Vec3 chunkLocalPos(
                    chunkCoord.x * VoxelChunk::SIZE,
                    chunkCoord.y * VoxelChunk::SIZE,
                    chunkCoord.z * VoxelChunk::SIZE
                );
                
                // Compute full transform: island transform * chunk local offset
                glm::mat4 chunkTransform = island->getTransformMatrix() * 
                    glm::translate(glm::mat4(1.0f), glm::vec3(chunkLocalPos.x, chunkLocalPos.y, chunkLocalPos.z));
                
                g_mdiRenderer->queueChunkRegistration(chunk.get(), chunkTransform);
            }
        }
    }
    
    auto meshGenEnd = std::chrono::high_resolution_clock::now();
    auto meshGenDuration = std::chrono::duration_cast<std::chrono::milliseconds>(meshGenEnd - meshGenStart).count();
    
    auto totalEnd = std::chrono::high_resolution_clock::now();
    auto totalDuration = std::chrono::duration_cast<std::chrono::milliseconds>(totalEnd - startTime).count();
    
    std::cout << "📐 Mesh Generation: " << meshGenDuration << "ms (" << island->chunks.size() << " chunks)" << std::endl;
    std::cout << "✅ Island Generation Complete: " << totalDuration << "ms total" << std::endl;
    std::cout << "   └─ Breakdown: Voxels=" << voxelGenDuration << "ms (" 
              << (voxelGenDuration * 100 / std::max(1LL, totalDuration)) << "%), Decoration=" 
              << decorationDuration << "ms (" << (decorationDuration * 100 / std::max(1LL, totalDuration)) 
              << "%), Meshes=" << meshGenDuration << "ms (" 
              << (meshGenDuration * 100 / std::max(1LL, totalDuration)) << "%)" << std::endl;
}

uint8_t IslandChunkSystem::getVoxelFromIsland(uint32_t islandID, const Vec3& islandRelativePosition) const
{
    // Hold lock across the entire access to prevent races
    std::lock_guard<std::mutex> lock(m_islandsMutex);
    auto itIsl = m_islands.find(islandID);
    if (itIsl == m_islands.end())
        return 0;

    const FloatingIsland& island = itIsl->second;

    // Convert island-relative position to chunk coordinate and local voxel position
    Vec3 chunkCoord = FloatingIsland::islandPosToChunkCoord(islandRelativePosition);
    Vec3 localPos = FloatingIsland::islandPosToLocalPos(islandRelativePosition);

    // Find the chunk
    auto it = island.chunks.find(chunkCoord);
    if (it == island.chunks.end())
        return 0; // Chunk doesn't exist

    const VoxelChunk* chunk = it->second.get();
    if (!chunk)
        return 0;

    // Get voxel from chunk using local coordinates
    int x = static_cast<int>(localPos.x);
    int y = static_cast<int>(localPos.y);
    int z = static_cast<int>(localPos.z);

    // Check bounds (should be 0-15 for 16x16x16 chunks)
    if (x < 0 || x >= VoxelChunk::SIZE || y < 0 || y >= VoxelChunk::SIZE || z < 0 || z >= VoxelChunk::SIZE)
        return 0;

    return chunk->getVoxel(x, y, z);
}

void IslandChunkSystem::setVoxelInIsland(uint32_t islandID, const Vec3& islandRelativePosition, uint8_t voxelType)
{
    // Acquire chunk pointer under lock, then perform heavy work without holding the map mutex
    VoxelChunk* chunk = nullptr;
    Vec3 localPos;
    Vec3 chunkCoord;
    Vec3 islandCenter;
    bool isNewChunk = false;
    {
        std::lock_guard<std::mutex> lock(m_islandsMutex);
        auto itIsl = m_islands.find(islandID);
        if (itIsl == m_islands.end())
            return;
        FloatingIsland& island = itIsl->second;
        chunkCoord = FloatingIsland::islandPosToChunkCoord(islandRelativePosition);
        localPos = FloatingIsland::islandPosToLocalPos(islandRelativePosition);
        islandCenter = island.physicsCenter;
        std::unique_ptr<VoxelChunk>& chunkPtr = island.chunks[chunkCoord];
        if (!chunkPtr)
        {
            chunkPtr = std::make_unique<VoxelChunk>();
            isNewChunk = true;
        }
        chunk = chunkPtr.get();
    }

    // Set voxel and rebuild meshes outside of islands mutex to avoid deadlocks
    int x = static_cast<int>(localPos.x);
    int y = static_cast<int>(localPos.y);
    int z = static_cast<int>(localPos.z);
    if (x < 0 || x >= VoxelChunk::SIZE || y < 0 || y >= VoxelChunk::SIZE || z < 0 || z >= VoxelChunk::SIZE)
        return;
    
    chunk->setVoxel(x, y, z, voxelType);
    chunk->generateMesh();
    chunk->buildCollisionMesh();  // Rebuild collision mesh for accurate physics
}

void IslandChunkSystem::setVoxelWithAutoChunk(uint32_t islandID, const Vec3& islandRelativePos, uint8_t voxelType)
{
    // Acquire chunk pointer under lock, then write outside the lock
    VoxelChunk* chunk = nullptr;
    int localX = 0, localY = 0, localZ = 0;
    {
        std::lock_guard<std::mutex> lock(m_islandsMutex);
        auto itIsl = m_islands.find(islandID);
        if (itIsl == m_islands.end())
            return;
        FloatingIsland& island = itIsl->second;

        int chunkX = static_cast<int>(std::floor(islandRelativePos.x / VoxelChunk::SIZE));
        int chunkY = static_cast<int>(std::floor(islandRelativePos.y / VoxelChunk::SIZE));
        int chunkZ = static_cast<int>(std::floor(islandRelativePos.z / VoxelChunk::SIZE));
        Vec3 chunkCoord(chunkX, chunkY, chunkZ);

        // Calculate local coordinates correctly for negative positions
        // Use the same floor-based calculation to keep coordinates consistent
        localX = static_cast<int>(std::floor(islandRelativePos.x)) - (chunkX * VoxelChunk::SIZE);
        localY = static_cast<int>(std::floor(islandRelativePos.y)) - (chunkY * VoxelChunk::SIZE);
        localZ = static_cast<int>(std::floor(islandRelativePos.z)) - (chunkZ * VoxelChunk::SIZE);

        std::unique_ptr<VoxelChunk>& chunkPtr = island.chunks[chunkCoord];
        if (!chunkPtr)
            chunkPtr = std::make_unique<VoxelChunk>();
        chunk = chunkPtr.get();
    }

    if (!chunk) return;
    chunk->setVoxel(localX, localY, localZ, voxelType);
    // Mesh generation can be deferred/batched; leave as-is to avoid stutters
}

// ID-based block methods (clean and efficient)
void IslandChunkSystem::setBlockIDWithAutoChunk(uint32_t islandID, const Vec3& islandRelativePos, uint8_t blockID)
{
    setVoxelWithAutoChunk(islandID, islandRelativePos, blockID);
}

uint8_t IslandChunkSystem::getBlockIDInIsland(uint32_t islandID, const Vec3& islandRelativePosition) const
{
    return getVoxelFromIsland(islandID, islandRelativePosition);
}

void IslandChunkSystem::getAllChunks(std::vector<VoxelChunk*>& outChunks)
{
    outChunks.clear();
    std::lock_guard<std::mutex> lock(m_islandsMutex);
    for (auto& [id, island] : m_islands)
    {
        // Add all chunks from this island
        for (auto& [chunkCoord, chunk] : island.chunks)
        {
            if (chunk)
            {
                outChunks.push_back(chunk.get());
            }
        }
    }
}

void IslandChunkSystem::getVisibleChunks(const Vec3& viewPosition,
                                         std::vector<VoxelChunk*>& outChunks)
{
    // Frustum culling will be added when we implement proper camera frustum
    getAllChunks(outChunks);
}

void IslandChunkSystem::renderAllIslands()
{
    // DEPRECATED: This function is no longer used.
    // All rendering is now handled by MDIRenderer via GameClient::renderWorld()
    // Kept for backwards compatibility only.
    std::cerr << "⚠️  IslandChunkSystem::renderAllIslands() called but deprecated. Use MDIRenderer instead." << std::endl;
}

void IslandChunkSystem::updateIslandPhysics(float deltaTime)
{
    std::lock_guard<std::mutex> lock(m_islandsMutex);
    for (auto& [id, island] : m_islands)
    {
        // Apply velocity to position
        island.physicsCenter.x += island.velocity.x * deltaTime;
        island.physicsCenter.y += island.velocity.y * deltaTime;
        island.physicsCenter.z += island.velocity.z * deltaTime;
        
        // Apply angular velocity to rotation
        island.rotation.x += island.angularVelocity.x * deltaTime;
        island.rotation.y += island.angularVelocity.y * deltaTime;
        island.rotation.z += island.angularVelocity.z * deltaTime;
        
        island.needsPhysicsUpdate = true;
    }
}

void IslandChunkSystem::syncPhysicsToChunks()
{
    // UNIFIED TRANSFORM UPDATE: Single source of truth for ALL rendering (MDI + GLB)
    // This function calculates transforms once and updates both MDI chunks and GLB model instances
    std::lock_guard<std::mutex> lock(m_islandsMutex);
    
    if (!g_mdiRenderer)
    {
        std::cerr << "⚠️  syncPhysicsToChunks: MDI renderer not available!" << std::endl;
        return;
    }
    
    // Get block type registry for GLB model iteration
    auto& registry = BlockTypeRegistry::getInstance();
    
    for (auto& [id, island] : m_islands)
    {
        // Calculate island transform once (includes rotation + translation)
        glm::mat4 islandTransform = island.getTransformMatrix();
        
        // Update transforms for all chunks in this island
        for (auto& [chunkCoord, chunk] : island.chunks)
        {
            if (!chunk) continue;
            
            // Compute chunk local position
            Vec3 chunkLocalPos(
                chunkCoord.x * VoxelChunk::SIZE,
                chunkCoord.y * VoxelChunk::SIZE,
                chunkCoord.z * VoxelChunk::SIZE
            );
            
            // Compute full transform: island transform * chunk local offset
            // This is the ONLY place this calculation happens - single source of truth
            glm::mat4 chunkTransform = islandTransform * 
                glm::translate(glm::mat4(1.0f), glm::vec3(chunkLocalPos.x, chunkLocalPos.y, chunkLocalPos.z));
            
            // === UPDATE MDI RENDERER (voxel chunks) ===
            if (chunk->getMDIIndex() >= 0)
            {
                // Existing chunk - update transform
                g_mdiRenderer->updateChunkTransform(chunk->getMDIIndex(), chunkTransform);
            }
            else
            {
                // New chunk - register with correct transform
                g_mdiRenderer->queueChunkRegistration(chunk.get(), chunkTransform);
            }
            
            // === UPDATE GLB MODEL RENDERER (for all block types that use models) ===
            if (g_modelRenderer)
            {
                for (const auto& blockType : registry.getAllBlockTypes())
                {
                    if (blockType.renderType == BlockRenderType::OBJ)
                    {
                        // Update model matrix for this block type in this chunk
                        // Uses the same chunkTransform we calculated above
                        g_modelRenderer->updateModelMatrix(blockType.id, chunk.get(), chunkTransform);
                    }
                }
            }
        }
    }
}

void IslandChunkSystem::updatePlayerChunks(const Vec3& playerPosition)
{
    // Infinite world generation will be implemented in a future version
    // For now, we manually create islands in GameState
}

void IslandChunkSystem::generateChunksAroundPoint(const Vec3& center)
{
    // Chunk generation around points will be used for infinite world expansion
    // Currently handled manually through createIsland()
}


