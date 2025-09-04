// World/Chunk.cpp - Chunk implementation
#include "pch.h"
#include "Chunk.h"
#include <algorithm>

namespace Engine {
namespace World {

Chunk::Chunk(int chunkX, int chunkY, int chunkZ)
    : m_chunkX(chunkX)
    , m_chunkY(chunkY)
    , m_chunkZ(chunkZ)
    , m_isDirty(false)
    , m_isEmpty(true)
    , m_isGenerated(false)
    , m_blocks(BLOCKS_PER_CHUNK, static_cast<uint8_t>(BlockType::AIR))
{
}

Chunk::~Chunk() = default;

uint8_t Chunk::getBlock(int x, int y, int z) const {
    if (!isValidLocalCoord(x, y, z)) return static_cast<uint8_t>(BlockType::AIR);
    return m_blocks[getBlockIndex(x, y, z)];
}

void Chunk::setBlock(int x, int y, int z, uint8_t blockType) {
    if (!isValidLocalCoord(x, y, z)) return;
    
    int index = getBlockIndex(x, y, z);
    if (m_blocks[index] != blockType) {
        m_blocks[index] = blockType;
        markDirty();
        
        // Check if chunk is still empty
        if (blockType != static_cast<uint8_t>(BlockType::AIR)) {
            m_isEmpty = false;
        }
    }
}

void Chunk::checkIfEmpty() {
    m_isEmpty = std::all_of(m_blocks.begin(), m_blocks.end(), 
        [](uint8_t block) { 
            return block == static_cast<uint8_t>(BlockType::AIR); 
        });
}

std::vector<uint8_t> Chunk::serialize() const {
    std::vector<uint8_t> data;
    data.reserve(BLOCKS_PER_CHUNK + 12); // Extra space for metadata
    
    // Header: chunk coordinates
    data.push_back(m_chunkX & 0xFF);
    data.push_back((m_chunkX >> 8) & 0xFF);
    data.push_back(m_chunkY & 0xFF);
    data.push_back((m_chunkY >> 8) & 0xFF);
    data.push_back(m_chunkZ & 0xFF);
    data.push_back((m_chunkZ >> 8) & 0xFF);
    
    // Flags
    uint8_t flags = 0;
    if (m_isEmpty) flags |= 0x01;
    if (m_isGenerated) flags |= 0x02;
    data.push_back(flags);
    
    // Block data (only if not empty)
    if (!m_isEmpty) {
        data.insert(data.end(), m_blocks.begin(), m_blocks.end());
    }
    
    return data;
}

void Chunk::deserialize(const std::vector<uint8_t>& data) {
    if (data.size() < 7) return; // Minimum header size
    
    // Read header
    m_chunkX = data[0] | (data[1] << 8);
    m_chunkY = data[2] | (data[3] << 8);
    m_chunkZ = data[4] | (data[5] << 8);
    
    uint8_t flags = data[6];
    m_isEmpty = (flags & 0x01) != 0;
    m_isGenerated = (flags & 0x02) != 0;
    
    // Read block data
    if (m_isEmpty) {
        // Fill with air
        std::fill(m_blocks.begin(), m_blocks.end(), static_cast<uint8_t>(BlockType::AIR));
    } else if (data.size() >= 7 + BLOCKS_PER_CHUNK) {
        // Copy block data
        std::copy(data.begin() + 7, data.begin() + 7 + BLOCKS_PER_CHUNK, m_blocks.begin());
    }
    
    markClean(); // Data just received from network
}

int Chunk::getBlockIndex(int x, int y, int z) const {
    return y * CHUNK_SIZE * CHUNK_SIZE + z * CHUNK_SIZE + x;
}

bool Chunk::isValidLocalCoord(int x, int y, int z) const {
    return x >= 0 && x < CHUNK_SIZE && 
           y >= 0 && y < CHUNK_SIZE && 
           z >= 0 && z < CHUNK_SIZE;
}

} // namespace World
} // namespace Engine
