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
#include "../../libs/FastNoiseLite/FastNoiseLite.h"

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

    // Create new chunk and set island context
    auto newChunk = std::make_unique<VoxelChunk>();
    newChunk->setIslandContext(islandID, chunkCoord);
    island->chunks[chunkCoord] = std::move(newChunk);
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
    
    // **NOISE CONFIGURATION**
    float densityThreshold = 0.35f;
    float baseHeightRatio = 0.15f;
    float noise3DFrequency = 0.02f;
    float noise2DFrequency = 0.015f;
    int fractalOctaves = 2;
    float fractalGain = 0.4f;
    
    // **ENVIRONMENT OVERRIDES** for noise parameters
    const char* freq3DEnv = std::getenv("NOISE_FREQ_3D");
    if (freq3DEnv) noise3DFrequency = std::stof(freq3DEnv);
    
    const char* freq2DEnv = std::getenv("NOISE_FREQ_2D");
    if (freq2DEnv) noise2DFrequency = std::stof(freq2DEnv);
    
    const char* thresholdEnv = std::getenv("NOISE_THRESHOLD");
    if (thresholdEnv) densityThreshold = std::stof(thresholdEnv);
    
    // Setup noise generators
    FastNoiseLite noise3D;
    noise3D.SetNoiseType(FastNoiseLite::NoiseType_Perlin);
    noise3D.SetSeed(seed);
    noise3D.SetFrequency(noise3DFrequency);
    noise3D.SetFractalType(FastNoiseLite::FractalType_FBm);
    noise3D.SetFractalOctaves(fractalOctaves);
    noise3D.SetFractalLacunarity(2.0f);
    noise3D.SetFractalGain(fractalGain);
    
    FastNoiseLite noise2D;
    noise2D.SetNoiseType(FastNoiseLite::NoiseType_Perlin);
    noise2D.SetSeed(seed + 1000);
    noise2D.SetFrequency(noise2DFrequency);
    noise2D.SetFractalType(FastNoiseLite::FractalType_FBm);
    noise2D.SetFractalOctaves(fractalOctaves);
    noise2D.SetFractalLacunarity(2.0f);
    noise2D.SetFractalGain(fractalGain);
    
    // **SURFACE CACHE** - Track surface positions for decoration pass
    std::vector<Vec3> surfacePositions;
    surfacePositions.reserve(static_cast<size_t>(radius * radius * 2));
    
    auto voxelGenStart = std::chrono::high_resolution_clock::now();
    
    // **SPHERE-BOUNDED DENSE SAMPLING**
    // Calculate island bounds (sphere + vertical extent)
    int islandHeight = static_cast<int>(radius * baseHeightRatio);
    int searchRadius = static_cast<int>(radius * 1.4f);
    float radiusSquared = (radius * 1.4f) * (radius * 1.4f);
    float radiusDivisor = 1.0f / (radius * 1.2f);
    
    long long voxelsGenerated = 0;
    long long voxelsSampled = 0;
    long long voxelsSkipped = 0;
    long long earlyRejects = 0;
    
    auto noiseStart = std::chrono::high_resolution_clock::now();
    long long noiseTimeUs = 0;
    
    // Iterate with optimized loop order (Y outermost for better cache locality)
    for (int y = -islandHeight; y <= islandHeight; y++)
    {
        float dy = static_cast<float>(y);
        
        // **VERTICAL FALLOFF** - Pre-calculate for this Y layer
        float islandHeightRange = islandHeight * 2.0f;
        float normalizedY = (dy + islandHeight) / islandHeightRange;
        float centerOffset = normalizedY - 0.5f;
        float verticalDensity = 1.0f - (centerOffset * centerOffset * 4.0f);
        verticalDensity = std::max(0.0f, verticalDensity);
        
        // **EARLY OUT** - Skip entire Y layer if vertical density too low
        if (verticalDensity < 0.01f) {
            continue;
        }
        
        for (int x = -searchRadius; x <= searchRadius; x++)
        {
            float dx = static_cast<float>(x);
            float xSquared = dx * dx;
            
            for (int z = -searchRadius; z <= searchRadius; z++)
            {
                voxelsSampled++;
                
                float dz = static_cast<float>(z);
                
                // **SPHERE CULLING** - Skip positions outside island radius
                float distanceSquared = xSquared + dz * dz;
                if (distanceSquared > radiusSquared) {
                    voxelsSkipped++;
                    continue;
                }
                
                // **RADIAL FALLOFF** (cached sqrt and pre-calculated divisor)
                float distanceFromCenter = std::sqrt(distanceSquared);
                float islandBase = 1.0f - (distanceFromCenter * radiusDivisor);
                islandBase = std::max(0.0f, islandBase);
                islandBase = islandBase * islandBase;
                
                // **EARLY OUT** - Skip if radial falloff too low
                if (islandBase < 0.01f) {
                    earlyRejects++;
                    continue;
                }
                
                // Combined density from radial and vertical falloff
                float baseDensity = islandBase * verticalDensity;
                
                // **EARLY OUT** - Skip noise if base density too low (saves 2 noise calls!)
                if (baseDensity < 0.05f) {
                    earlyRejects++;
                    continue;
                }
                
                auto densityStart = std::chrono::high_resolution_clock::now();
                
                // **3D PERLIN NOISE**
                float volumetricNoise = noise3D.GetNoise(dx, dy, dz);
                volumetricNoise = (volumetricNoise + 1.0f) * 0.5f;
                
                // **2D PERLIN NOISE**
                float terrainNoise = noise2D.GetNoise(dx, dz);
                terrainNoise = (terrainNoise + 1.0f) * 0.5f;
                
                // **FINAL DENSITY**
                float finalDensity = islandBase * verticalDensity * (volumetricNoise * 0.6f + terrainNoise * 0.4f);
                
                auto densityEnd = std::chrono::high_resolution_clock::now();
                noiseTimeUs += std::chrono::duration_cast<std::chrono::microseconds>(densityEnd - densityStart).count();
                
                // Place voxel if density exceeds threshold
                if (finalDensity > densityThreshold)
                {
                    Vec3 pos(x, y, z);
                    setBlockIDWithAutoChunk(islandID, pos, BlockID::DIRT);
                    surfacePositions.push_back(pos);
                    voxelsGenerated++;
                }
            }
        }
    }
    
    auto voxelGenEnd = std::chrono::high_resolution_clock::now();
    auto voxelGenDuration = std::chrono::duration_cast<std::chrono::milliseconds>(voxelGenEnd - voxelGenStart).count();
    
    long long noiseDuration = noiseTimeUs / 1000;
    long long loopOverhead = voxelGenDuration - noiseDuration;
    
    std::cout << "🔨 Voxel Generation: " << voxelGenDuration << "ms (" << voxelsGenerated << " voxels, " 
              << island->chunks.size() << " chunks)" << std::endl;
    std::cout << "   ├─ Positions Sampled: " << voxelsSampled << " (" << voxelsSkipped << " sphere culled, " 
              << earlyRejects << " density rejected)" << std::endl;
    std::cout << "   ├─ Noise Generation: " << noiseDuration << "ms (" 
              << (noiseDuration * 100 / std::max(1LL, voxelGenDuration)) << "%)" << std::endl;
    std::cout << "   └─ Loop Overhead: " << loopOverhead << "ms (" 
              << (loopOverhead * 100 / std::max(1LL, voxelGenDuration)) << "%)" << std::endl;
    
    // Connectivity cleanup - remove disconnected satellite chunks
    auto connectivityStart = std::chrono::high_resolution_clock::now();
    
    int voxelsRemoved = ConnectivityAnalyzer::cleanupSatellites(island, Vec3(0, 0, 0));
    
    if (voxelsRemoved > 0) {
        std::cout << "✅ Cleaned up " << voxelsRemoved << " satellite voxels" << std::endl;
    } else {
        std::cout << "✅ Island is fully connected (no satellites)" << std::endl;
    }
    
    auto connectivityEnd = std::chrono::high_resolution_clock::now();
    auto connectivityDuration = std::chrono::duration_cast<std::chrono::milliseconds>(connectivityEnd - connectivityStart).count();
    std::cout << "🔍 Connectivity Cleanup: " << connectivityDuration << "ms" << std::endl;
    
    // Decoration pass - place grass on surface positions
    auto decorationStart = std::chrono::high_resolution_clock::now();
    
    int grassPlaced = 0;
    
    for (const Vec3& pos : surfacePositions) {
        uint8_t blockID = getVoxelFromIsland(islandID, pos);
        if (blockID == BlockID::AIR) continue;
        
        Vec3 above = pos + Vec3(0, 1, 0);
        uint8_t blockAbove = getVoxelFromIsland(islandID, above);
        if (blockAbove != BlockID::AIR) continue;
        
        if ((std::rand() % 100) < 25) {
            setBlockIDWithAutoChunk(islandID, above, BlockID::DECOR_GRASS);
            grassPlaced++;
        }
    }
    
    auto decorationEnd = std::chrono::high_resolution_clock::now();
    auto decorationDuration = std::chrono::duration_cast<std::chrono::milliseconds>(decorationEnd - decorationStart).count();
    std::cout << "🌿 Decoration: " << decorationDuration << "ms (" << grassPlaced << " grass)" << std::endl;
    
    auto meshGenStart = std::chrono::high_resolution_clock::now();
    
    long long renderMeshTime = 0;
    long long collisionMeshTime = 0;
    long long mdiRegistrationTime = 0;
    int chunksProcessed = 0;
    
    for (auto& [chunkCoord, chunk] : island->chunks)
    {
        if (chunk)
        {
            auto renderMeshStart = std::chrono::high_resolution_clock::now();
            chunk->generateMesh();
            auto renderMeshEnd = std::chrono::high_resolution_clock::now();
            renderMeshTime += std::chrono::duration_cast<std::chrono::microseconds>(renderMeshEnd - renderMeshStart).count();
            
            auto collisionMeshStart = std::chrono::high_resolution_clock::now();
            chunk->buildCollisionMesh();
            auto collisionMeshEnd = std::chrono::high_resolution_clock::now();
            collisionMeshTime += std::chrono::duration_cast<std::chrono::microseconds>(collisionMeshEnd - collisionMeshStart).count();
            
            if (g_mdiRenderer)
            {
                auto mdiStart = std::chrono::high_resolution_clock::now();
                
                Vec3 chunkLocalPos(
                    chunkCoord.x * VoxelChunk::SIZE,
                    chunkCoord.y * VoxelChunk::SIZE,
                    chunkCoord.z * VoxelChunk::SIZE
                );
                
                glm::mat4 chunkTransform = island->getTransformMatrix() * 
                    glm::translate(glm::mat4(1.0f), glm::vec3(chunkLocalPos.x, chunkLocalPos.y, chunkLocalPos.z));
                
                g_mdiRenderer->queueChunkRegistration(chunk.get(), chunkTransform);
                
                auto mdiEnd = std::chrono::high_resolution_clock::now();
                mdiRegistrationTime += std::chrono::duration_cast<std::chrono::microseconds>(mdiEnd - mdiStart).count();
            }
            
            chunksProcessed++;
        }
    }
    
    auto meshGenEnd = std::chrono::high_resolution_clock::now();
    auto meshGenDuration = std::chrono::duration_cast<std::chrono::milliseconds>(meshGenEnd - meshGenStart).count();
    
    long long renderMeshDuration = renderMeshTime / 1000;
    long long collisionMeshDuration = collisionMeshTime / 1000;
    long long mdiRegistrationDuration = mdiRegistrationTime / 1000;
    
    std::cout << "📐 Mesh Generation: " << meshGenDuration << "ms (" << chunksProcessed << " chunks)" << std::endl;
    std::cout << "   ├─ Render: " << renderMeshDuration << "ms (" 
              << (renderMeshDuration * 100 / std::max(1LL, meshGenDuration)) << "%)" << std::endl;
    std::cout << "   ├─ Collision: " << collisionMeshDuration << "ms (" 
              << (collisionMeshDuration * 100 / std::max(1LL, meshGenDuration)) << "%)" << std::endl;
    std::cout << "   └─ MDI: " << mdiRegistrationDuration << "ms (" 
              << (mdiRegistrationDuration * 100 / std::max(1LL, meshGenDuration)) << "%)" << std::endl;
    
    auto totalEnd = std::chrono::high_resolution_clock::now();
    auto totalDuration = std::chrono::duration_cast<std::chrono::milliseconds>(totalEnd - startTime).count();
    
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
            chunkPtr->setIslandContext(islandID, chunkCoord);
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
    chunk->buildCollisionMesh();
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
        if (!chunkPtr) {
            chunkPtr = std::make_unique<VoxelChunk>();
            chunkPtr->setIslandContext(islandID, chunkCoord);
        }
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
    // Event-driven: Only update chunks whose islands have actually moved
    std::lock_guard<std::mutex> lock(m_islandsMutex);
    
    if (!g_mdiRenderer)
    {
        std::cerr << "⚠️  syncPhysicsToChunks: MDI renderer not available!" << std::endl;
        return;
    }
    
    // Cache OBJ block types once instead of querying every iteration
    static std::vector<uint8_t> objBlockTypes;
    static bool objBlockTypesCached = false;
    if (!objBlockTypesCached)
    {
        auto& registry = BlockTypeRegistry::getInstance();
        for (const auto& blockType : registry.getAllBlockTypes())
        {
            if (blockType.renderType == BlockRenderType::OBJ)
            {
                objBlockTypes.push_back(blockType.id);
            }
        }
        objBlockTypesCached = true;
    }
    
    for (auto& [id, island] : m_islands)
    {
        // Skip islands that haven't moved
        if (!island.needsPhysicsUpdate) continue;
        
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
            glm::mat4 chunkTransform = islandTransform * 
                glm::translate(glm::mat4(1.0f), glm::vec3(chunkLocalPos.x, chunkLocalPos.y, chunkLocalPos.z));
            
            // === UPDATE MDI RENDERER (voxel chunks) ===
            if (chunk->getMDIIndex() >= 0)
            {
                g_mdiRenderer->updateChunkTransform(chunk->getMDIIndex(), chunkTransform);
            }
            else
            {
                g_mdiRenderer->queueChunkRegistration(chunk.get(), chunkTransform);
            }
            
            // === UPDATE GLB MODEL RENDERER (only for OBJ block types) ===
            if (g_modelRenderer && !objBlockTypes.empty())
            {
                for (uint8_t blockID : objBlockTypes)
                {
                    g_modelRenderer->updateModelMatrix(blockID, chunk.get(), chunkTransform);
                }
            }
        }
        
        // Clear update flag after processing
        island.needsPhysicsUpdate = false;
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


