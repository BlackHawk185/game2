// IslandChunkSystem.cpp - Implementation of physics-driven chunking
#include "IslandChunkSystem.h"

#include <cmath>
#include <cstdlib>
#include <ctime>
#include <iostream>
#include <memory>

#include "VoxelChunk.h"

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
    for (auto& [id, island] : m_islands)
    {
        destroyIsland(id);
    }
}

uint32_t IslandChunkSystem::createIsland(const Vec3& physicsCenter)
{
    uint32_t islandID = m_nextIslandID++;
    FloatingIsland& island = m_islands[islandID];
    island.islandID = islandID;
    island.physicsCenter = physicsCenter;
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
    auto it = m_islands.find(islandID);
    if (it == m_islands.end())
        return;

    m_islands.erase(it);
}

FloatingIsland* IslandChunkSystem::getIsland(uint32_t islandID)
{
    auto it = m_islands.find(islandID);
    return (it != m_islands.end()) ? &it->second : nullptr;
}

const FloatingIsland* IslandChunkSystem::getIsland(uint32_t islandID) const
{
    auto it = m_islands.find(islandID);
    return (it != m_islands.end()) ? &it->second : nullptr;
}

Vec3 IslandChunkSystem::getIslandCenter(uint32_t islandID) const
{
    auto it = m_islands.find(islandID);
    if (it != m_islands.end())
    {
        return it->second.physicsCenter;
    }
    return Vec3(0.0f, 0.0f, 0.0f);  // Return zero vector if island not found
}

Vec3 IslandChunkSystem::getIslandVelocity(uint32_t islandID) const
{
    auto it = m_islands.find(islandID);
    if (it != m_islands.end())
    {
        return it->second.velocity;
    }
    return Vec3(0.0f, 0.0f, 0.0f);  // Return zero velocity if island not found
}

void IslandChunkSystem::addChunkToIsland(uint32_t islandID, const Vec3& chunkCoord)
{
    FloatingIsland* island = getIsland(islandID);
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
    FloatingIsland* island = getIsland(islandID);
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
    FloatingIsland* island = getIsland(islandID);
    if (!island)
        return nullptr;

    auto it = island->chunks.find(chunkCoord);
    if (it != island->chunks.end())
    {
        return it->second.get();
    }

    return nullptr;
}

void IslandChunkSystem::generateFloatingIsland(uint32_t islandID, uint32_t seed, float radius)
{
    FloatingIsland* island = getIsland(islandID);
    if (!island)
        return;

    // Create the main chunk at origin (0,0,0) for backward compatibility
    Vec3 originChunk(0, 0, 0);
    addChunkToIsland(islandID, originChunk);
    
    VoxelChunk* chunk = getChunkFromIsland(islandID, originChunk);
    if (chunk)
    {
        const char* noiseEnv = std::getenv("ISLAND_NOISE");
        bool useNoise = (noiseEnv && (noiseEnv[0]=='1' || noiseEnv[0]=='T' || noiseEnv[0]=='t' || noiseEnv[0]=='Y' || noiseEnv[0]=='y'));
        chunk->generateFloatingIsland(seed, useNoise);
    }
}

void IslandChunkSystem::generateFloatingIslandMultiChunk(uint32_t islandID, uint32_t seed, float radius, int chunkRadius)
{
    FloatingIsland* island = getIsland(islandID);
    if (!island)
        return;

    // Generate chunks in a cubic pattern around the origin
    for (int x = -chunkRadius; x <= chunkRadius; x++)
    {
        for (int y = -chunkRadius; y <= chunkRadius; y++)
        {
            for (int z = -chunkRadius; z <= chunkRadius; z++)
            {
                Vec3 chunkCoord(x, y, z);
                addChunkToIsland(islandID, chunkCoord);
                
                VoxelChunk* chunk = getChunkFromIsland(islandID, chunkCoord);
                if (chunk)
                {
                    // Generate island content for this chunk
                    // Use coordinate-based seed variation for different chunks
                    uint32_t chunkSeed = seed + static_cast<uint32_t>(x * 1000 + y * 100 + z * 10);
                    
                    const char* noiseEnv = std::getenv("ISLAND_NOISE");
                    bool useNoise = (noiseEnv && (noiseEnv[0]=='1' || noiseEnv[0]=='T' || noiseEnv[0]=='t' || noiseEnv[0]=='Y' || noiseEnv[0]=='y'));
                    chunk->generateFloatingIsland(chunkSeed, useNoise);
                }
            }
        }
    }
    
    std::cout << "Generated multi-chunk island " << islandID << " with " << (2*chunkRadius+1) << "x" << (2*chunkRadius+1) << "x" << (2*chunkRadius+1) << " chunks" << std::endl;
}

uint8_t IslandChunkSystem::getVoxelFromIsland(uint32_t islandID, const Vec3& worldPosition) const
{
    const FloatingIsland* island = getIsland(islandID);
    if (!island)
        return 0;

    // Convert world position to chunk coordinate and local position
    Vec3 chunkCoord = FloatingIsland::worldPosToChunkCoord(worldPosition);
    Vec3 localPos = FloatingIsland::worldPosToLocalPos(worldPosition);

    // Find the chunk
    auto it = island->chunks.find(chunkCoord);
    if (it == island->chunks.end())
        return 0; // Chunk doesn't exist

    VoxelChunk* chunk = it->second.get();
    if (!chunk)
        return 0;

    // Get voxel from chunk using local coordinates
    int x = static_cast<int>(localPos.x);
    int y = static_cast<int>(localPos.y);
    int z = static_cast<int>(localPos.z);

    // Check bounds (should be 0-31 for 32x32x32 chunks)
    if (x < 0 || x >= VoxelChunk::SIZE || y < 0 || y >= VoxelChunk::SIZE || z < 0 || z >= VoxelChunk::SIZE)
        return 0;

    return chunk->getVoxel(x, y, z);
}

void IslandChunkSystem::setVoxelInIsland(uint32_t islandID, const Vec3& worldPosition, uint8_t voxelType)
{
    FloatingIsland* island = getIsland(islandID);
    if (!island)
        return;

    // Convert world position to chunk coordinate and local position
    Vec3 chunkCoord = FloatingIsland::worldPosToChunkCoord(worldPosition);
    Vec3 localPos = FloatingIsland::worldPosToLocalPos(worldPosition);

    // Find or create the chunk
    auto it = island->chunks.find(chunkCoord);
    if (it == island->chunks.end())
    {
        // Create new chunk if it doesn't exist
        addChunkToIsland(islandID, chunkCoord);
        it = island->chunks.find(chunkCoord);
    }

    VoxelChunk* chunk = it->second.get();
    if (!chunk)
        return;

    // Set voxel in chunk using local coordinates
    int x = static_cast<int>(localPos.x);
    int y = static_cast<int>(localPos.y);
    int z = static_cast<int>(localPos.z);

    // Check bounds (should be 0-31 for 32x32x32 chunks)
    if (x < 0 || x >= VoxelChunk::SIZE || y < 0 || y >= VoxelChunk::SIZE || z < 0 || z >= VoxelChunk::SIZE)
        return;

    chunk->setVoxel(x, y, z, voxelType);
    chunk->generateMesh();  // Update visual mesh
}

void IslandChunkSystem::getAllChunks(std::vector<VoxelChunk*>& outChunks)
{
    outChunks.clear();

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
    // Render each island at its physics center position
    for (auto& [id, island] : m_islands)
    {
        // Render all chunks in this island
        for (const auto& [chunkCoord, chunk] : island.chunks)
        {
            if (chunk)
            {
                // Calculate world position for this chunk
                Vec3 chunkWorldPos = island.physicsCenter + FloatingIsland::chunkCoordToWorldPos(chunkCoord);
                chunk->render(chunkWorldPos);
            }
        }
    }
}

void IslandChunkSystem::updateIslandPhysics(float deltaTime)
{
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
    for (auto& [id, island] : m_islands)
    {
        if (island.needsPhysicsUpdate)
        {
            // All chunks in the island move together with the physics center
            // Individual chunk positions are calculated during rendering
            island.needsPhysicsUpdate = false;
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


