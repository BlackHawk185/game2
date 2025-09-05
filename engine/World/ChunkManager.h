// World/ChunkManager.h - Manages world chunks and islands
#pragma once

#include "Island.h"
#include "../Math/Vec3.h"
#include <unordered_map>
#include <vector>
#include <memory>
#include <cstdint>

namespace Engine {
namespace World {

/**
 * ChunkManager - Manages all islands and chunks in the world
 * Handles world generation, chunk loading/unloading, and networking
 */
class ChunkManager {
public:
    ChunkManager(uint32_t worldSeed = 12345);
    ~ChunkManager();

    // World management
    void update(float deltaTime);
    
    // Island management
    Island* getOrCreateIsland(int islandX, int islandZ);
    Island* getIsland(int islandX, int islandZ);
    void ensureIslandGenerated(int islandX, int islandZ);
    
    // Block access (world coordinates)
    uint8_t getBlock(int worldX, int worldY, int worldZ);
    void setBlock(int worldX, int worldY, int worldZ, uint8_t blockType);
    
    // Coordinate conversion utilities
    static void worldToIslandCoords(int worldX, int worldZ, int& islandX, int& islandZ, int& localX, int& localZ);
    static void worldToChunkCoords(int worldX, int worldY, int worldZ, int& chunkX, int& chunkY, int& chunkZ, int& localX, int& localY, int& localZ);
    
    // Networking support
    std::vector<Chunk*> getAllDirtyChunks();
    void markAllChunksClean();
    
    // Get all islands for entity updates
    std::vector<Island*> getAllIslands();
    
    // Player spawn location
    Vec3 getSpawnLocation() const;
    
    // World generation
    void generateInitialWorld();

private:
    uint32_t m_worldSeed;
    
    // Island storage - using island coordinates as key
    std::unordered_map<uint64_t, std::unique_ptr<Island>> m_islands;
    
    // Helper functions
    uint64_t getIslandKey(int islandX, int islandZ) const;
    void generateIslandIfNeeded(Island* island);
};

} // namespace World
} // namespace Engine
