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
    std::lock_guard<std::mutex> lock(m_islandsMutex);
    uint32_t islandID = m_nextIslandID++;
    FloatingIsland& island = m_islands[islandID];
    island.islandID = islandID;
    island.physicsCenter = physicsCenter;
    island.needsPhysicsUpdate = true;  // Ensure initial MDI transform sync
    std::srand(static_cast<unsigned int>(std::time(nullptr)) + islandID * 137);
    float randomX = (std::rand() % 201 - 100) / 100.0f;
    float randomY = (std::rand() % 201 - 100) / 100.0f;
    float randomZ = (std::rand() % 201 - 100) / 100.0f;
    // Set velocity to ±0.4f for all axes (original drift speed)
    island.velocity = Vec3(randomX * 0.4f, randomY * 0.4f, randomZ * 0.4f);
    island.acceleration = Vec3(0.0f, 0.0f, 0.0f);  // No gravity for islands
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
                    
                    // **BLOCK TYPE SELECTION** - 50% dirt, 50% stone using IDs (clean and efficient!)
                    uint8_t blockID;
                    
                    // More random per-voxel distribution using multiple noise sources
                    float blockNoise1 = (std::sin(dx * 0.31f + seed * 0.7f) + 1.0f) * 0.5f;
                    float blockNoise2 = (std::cos(dz * 0.37f + seed * 1.3f) + 1.0f) * 0.5f;
                    float blockNoise3 = (std::sin(dy * 0.23f + seed * 2.1f) + 1.0f) * 0.5f;
                    
                    // Combine noise sources for more randomness
                    float combinedNoise = (blockNoise1 + blockNoise2 + blockNoise3) / 3.0f;
                    
                    if (combinedNoise > 0.5f) {
                        blockID = BlockID::DIRT;
                    } else {
                        blockID = BlockID::STONE;
                    }
                    
                    setBlockIDWithAutoChunk(islandID, islandRelativePos, blockID);
                    // Occasionally place decorative grass above dirt
                    if (blockID == BlockID::DIRT) {
                        // Only place if above within island bounds and with low probability
                        if ((std::rand() % 100) < 3) { // ~3%
                            Vec3 above = islandRelativePos + Vec3(0, 1, 0);
                            setBlockIDWithAutoChunk(islandID, above, BlockID::DECOR_GRASS);
                        }
                    }
                    
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
                Vec3 worldOffset = island->physicsCenter + Vec3(
                    chunkCoord.x * VoxelChunk::SIZE,
                    chunkCoord.y * VoxelChunk::SIZE,
                    chunkCoord.z * VoxelChunk::SIZE
                );
                int mdiIndex = g_mdiRenderer->registerChunk(chunk.get(), worldOffset);
                chunk->setMDIIndex(mdiIndex);  // Store for future transform updates
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
              << (voxelGenDuration * 100 / std::max(1LL, totalDuration)) << "%), Meshes=" 
              << meshGenDuration << "ms (" << (meshGenDuration * 100 / std::max(1LL, totalDuration)) << "%)" << std::endl;
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
    
    // Register or update MDI renderer for real-time voxel changes
    if (g_mdiRenderer)
    {
        Vec3 worldOffset = islandCenter + Vec3(
            chunkCoord.x * VoxelChunk::SIZE,
            chunkCoord.y * VoxelChunk::SIZE,
            chunkCoord.z * VoxelChunk::SIZE
        );
        
        if (isNewChunk)
        {
            // New chunk: register with MDI renderer
            int mdiIndex = g_mdiRenderer->registerChunk(chunk, worldOffset);
            chunk->setMDIIndex(mdiIndex);
        }
        else
        {
            // Existing chunk: update mesh data
            int mdiIndex = chunk->getMDIIndex();
            if (mdiIndex >= 0)
            {
                g_mdiRenderer->updateChunkMesh(mdiIndex, chunk);
            }
        }
    }
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

        localX = static_cast<int>(islandRelativePos.x) % VoxelChunk::SIZE;
        localY = static_cast<int>(islandRelativePos.y) % VoxelChunk::SIZE;
        localZ = static_cast<int>(islandRelativePos.z) % VoxelChunk::SIZE;
        if (localX < 0) localX += VoxelChunk::SIZE;
        if (localY < 0) localY += VoxelChunk::SIZE;
        if (localZ < 0) localZ += VoxelChunk::SIZE;

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

void IslandChunkSystem::getAllChunksWithWorldPos(std::vector<std::pair<VoxelChunk*, Vec3>>& out) const
{
    out.clear();
    std::lock_guard<std::mutex> lock(m_islandsMutex);
    for (const auto& [id, island] : m_islands)
    {
        for (const auto& [chunkCoord, chunk] : island.chunks)
        {
            if (chunk)
            {
                Vec3 worldPos = island.physicsCenter + FloatingIsland::chunkCoordToWorldPos(chunkCoord);
                out.emplace_back(chunk.get(), worldPos);
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
        // Only apply random drift, no gravity, bobbing, or wind
        island.physicsCenter.x += island.velocity.x * deltaTime;
        island.physicsCenter.y += island.velocity.y * deltaTime;
        island.physicsCenter.z += island.velocity.z * deltaTime;
        island.needsPhysicsUpdate = true;
    }
}

void IslandChunkSystem::syncPhysicsToChunks()
{
    // Sync physics positions to chunk rendering positions
    std::lock_guard<std::mutex> lock(m_islandsMutex);
    
    if (!g_mdiRenderer)
    {
        std::cerr << "⚠️  syncPhysicsToChunks: MDI renderer not available!" << std::endl;
        return;  // No renderer to update
    }
    
    static int updateCount = 0;
    int chunksUpdated = 0;
    
    for (auto& [id, island] : m_islands)
    {
        if (island.needsPhysicsUpdate)
        {
            // Update MDI transforms for all chunks in this island
            for (auto& [chunkCoord, chunk] : island.chunks)
            {
                if (chunk && chunk->getMDIIndex() >= 0)
                {
                    // Calculate new world position for this chunk
                    Vec3 worldOffset = island.physicsCenter + Vec3(
                        chunkCoord.x * VoxelChunk::SIZE,
                        chunkCoord.y * VoxelChunk::SIZE,
                        chunkCoord.z * VoxelChunk::SIZE
                    );
                    
                    // Update the transform in the MDI renderer
                    g_mdiRenderer->updateChunkTransform(chunk->getMDIIndex(), worldOffset);
                    chunksUpdated++;
                }
            }
            
            island.needsPhysicsUpdate = false;
        }
    }
    
    if (updateCount % 300 == 0 && chunksUpdated > 0)
    {
        std::cout << "Synced " << chunksUpdated << " chunks to physics (update #" << updateCount << ")" << std::endl;
    }
    updateCount++;
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


