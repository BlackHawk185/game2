// World/Island.h - Island management for voxel world
#pragma once

#include "../Math/Vec3.h"
#include <vector>
#include <memory>
#include <cstdint>

namespace Engine {
namespace World {

// Forward declarations
class Chunk;
class TerrainGenerator;

/**
 * Island - A 1000x100x1000 block island containing multiple chunks
 * Uses Perlin noise for terrain generation
 * Coordinates: Island contains chunks in a grid pattern
 */
class Island {
public:
    static constexpr int ISLAND_WIDTH = 2000;
    static constexpr int ISLAND_HEIGHT = 200;
    static constexpr int ISLAND_DEPTH = 2000;
    static constexpr int CHUNK_SIZE = 16;
    
    // Calculate chunks per island (rounded up)
    static constexpr int CHUNKS_X = (ISLAND_WIDTH + CHUNK_SIZE - 1) / CHUNK_SIZE;   // ~125
    static constexpr int CHUNKS_Y = (ISLAND_HEIGHT + CHUNK_SIZE - 1) / CHUNK_SIZE;  // ~13
    static constexpr int CHUNKS_Z = (ISLAND_DEPTH + CHUNK_SIZE - 1) / CHUNK_SIZE;   // ~125

public:
    Island(int islandX, int islandZ, uint32_t seed);
    ~Island();

    // Island coordinates (in island space)
    int getIslandX() const { return m_islandX; }
    int getIslandZ() const { return m_islandZ; }
    
    // Chunk management
    Chunk* getChunk(int chunkX, int chunkY, int chunkZ);
    bool isValidChunkCoord(int chunkX, int chunkY, int chunkZ) const;
    
    // World generation
    void generateTerrain();
    void generateTerrainSIMD(const TerrainGenerator& generator);
    bool isGenerated() const { return m_isGenerated; }
    
    // Block access (world coordinates relative to island)
    uint8_t getBlock(int x, int y, int z) const;
    void setBlock(int x, int y, int z, uint8_t blockType);
    
    // Networking - get chunks that need to be sent to clients
    std::vector<Chunk*> getDirtyChunks();
    void markAllChunksClean();
    
    // Physics - island movement in 3D space
    void updatePhysics(float deltaTime);
    Vec3 getWorldPosition() const { return m_worldPosition; }
    Vec3 getVelocity() const { return m_velocity; }
    void setVelocity(const Vec3& velocity) { m_velocity = velocity; }
    void setWorldPosition(const Vec3& position) { m_worldPosition = position; }
    bool hasPhysics() const { return m_hasPhysics; }
    void enablePhysics(bool enable = true) { m_hasPhysics = enable; }

private:
    int m_islandX, m_islandZ;
    uint32_t m_seed;
    bool m_isGenerated;
    
    // Physics state
    Vec3 m_worldPosition;    // World position in 3D space
    Vec3 m_velocity;         // Movement velocity
    bool m_hasPhysics;       // Whether this island moves
    
    // Chunk storage - flat array for better cache locality
    std::vector<std::unique_ptr<Chunk>> m_chunks;
    
    // Helper functions
    int getChunkIndex(int chunkX, int chunkY, int chunkZ) const;
};

} // namespace World
} // namespace Engine
