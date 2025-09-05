// World/Chunk.h - Individual chunk in the voxel world
#pragma once

#include <cstdint>
#include <vector>

namespace Engine {
namespace World {

/**
 * Chunk - 16x16x16 block of voxels
 * Uses Structure of Arrays (SoA) for better cache performance
 */
class Chunk {
public:
    static constexpr int CHUNK_SIZE = 16;
    static constexpr int BLOCKS_PER_CHUNK = CHUNK_SIZE * CHUNK_SIZE * CHUNK_SIZE;

public:
    Chunk(int chunkX, int chunkY, int chunkZ);
    ~Chunk();

    // Chunk coordinates (in chunk space within the island)
    int getChunkX() const { return m_chunkX; }
    int getChunkY() const { return m_chunkY; }
    int getChunkZ() const { return m_chunkZ; }
    
    // Block access (local coordinates 0-15)
    uint8_t getBlock(int x, int y, int z) const;
    void setBlock(int x, int y, int z, uint8_t blockType);
    
    // Chunk state management
    bool isDirty() const { return m_isDirty; }
    void markDirty() { m_isDirty = true; }
    void markClean() { m_isDirty = false; }
    
    bool isEmpty() const { return m_isEmpty; }
    void checkIfEmpty();
    
    // Generation
    bool isGenerated() const { return m_isGenerated; }
    void markGenerated() { m_isGenerated = true; }
    
    // Network serialization
    std::vector<uint8_t> serialize() const;
    void deserialize(const std::vector<uint8_t>& data);
    
    // Direct access to block data for fast generation
    uint8_t* getBlockData() { return m_blocks.data(); }
    const uint8_t* getBlockData() const { return m_blocks.data(); }

private:
    int m_chunkX, m_chunkY, m_chunkZ;
    bool m_isDirty;
    bool m_isEmpty;
    bool m_isGenerated;
    
    // SoA: Block data stored as flat array for cache efficiency
    std::vector<uint8_t> m_blocks;  // BLOCKS_PER_CHUNK elements
    
    // Helper functions
    int getBlockIndex(int x, int y, int z) const;
    bool isValidLocalCoord(int x, int y, int z) const;
};

// Block types for our simple dirt world
enum class BlockType : uint8_t {
    AIR = 0,
    DIRT = 1,
    GRASS = 2,   // For future expansion
    STONE = 3    // For future expansion
};

} // namespace World
} // namespace Engine
