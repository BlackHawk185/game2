// World/ChunkManager.cpp - ChunkManager implementation
#include "pch.h"
#include "ChunkManager.h"
#include "Chunk.h"
#include <iostream>

namespace Engine {
namespace World {

ChunkManager::ChunkManager(uint32_t worldSeed)
    : m_worldSeed(worldSeed)
{
    std::cout << "ChunkManager: Initialized with seed " << worldSeed << std::endl;
}

ChunkManager::~ChunkManager() = default;

void ChunkManager::update(float deltaTime) {
    // Update physics for all islands
    for (auto& [key, island] : m_islands) {
        if (island) {
            island->updatePhysics(deltaTime);
        }
    }
    
    // TODO: Handle chunk loading/unloading based on player positions
    // TODO: Update world generation tasks
}

Island* ChunkManager::getOrCreateIsland(int islandX, int islandZ) {
    uint64_t key = getIslandKey(islandX, islandZ);
    
    auto it = m_islands.find(key);
    if (it != m_islands.end()) {
        return it->second.get();
    }
    
    // Create new island
    std::cout << "ChunkManager: Creating new island at (" << islandX << ", " << islandZ << ")" << std::endl;
    auto island = std::make_unique<Island>(islandX, islandZ, m_worldSeed);
    Island* islandPtr = island.get();
    m_islands[key] = std::move(island);
    
    return islandPtr;
}

Island* ChunkManager::getIsland(int islandX, int islandZ) {
    uint64_t key = getIslandKey(islandX, islandZ);
    
    auto it = m_islands.find(key);
    if (it != m_islands.end()) {
        return it->second.get();
    }
    
    return nullptr;
}

void ChunkManager::ensureIslandGenerated(int islandX, int islandZ) {
    Island* island = getOrCreateIsland(islandX, islandZ);
    if (island && !island->isGenerated()) {
        generateIslandIfNeeded(island);
    }
}

uint8_t ChunkManager::getBlock(int worldX, int worldY, int worldZ) {
    int islandX, islandZ, localX, localZ;
    worldToIslandCoords(worldX, worldZ, islandX, islandZ, localX, localZ);
    
    Island* island = getIsland(islandX, islandZ);
    if (!island) return static_cast<uint8_t>(BlockType::AIR);
    
    return island->getBlock(localX, worldY, localZ);
}

void ChunkManager::setBlock(int worldX, int worldY, int worldZ, uint8_t blockType) {
    int islandX, islandZ, localX, localZ;
    worldToIslandCoords(worldX, worldZ, islandX, islandZ, localX, localZ);
    
    Island* island = getOrCreateIsland(islandX, islandZ);
    if (island) {
        island->setBlock(localX, worldY, localZ, blockType);
    }
}

void ChunkManager::worldToIslandCoords(int worldX, int worldZ, int& islandX, int& islandZ, int& localX, int& localZ) {
    islandX = worldX / Island::ISLAND_WIDTH;
    islandZ = worldZ / Island::ISLAND_DEPTH;
    
    localX = worldX % Island::ISLAND_WIDTH;
    localZ = worldZ % Island::ISLAND_DEPTH;
    
    // Handle negative coordinates properly
    if (worldX < 0 && localX != 0) {
        islandX--;
        localX = Island::ISLAND_WIDTH + localX;
    }
    if (worldZ < 0 && localZ != 0) {
        islandZ--;
        localZ = Island::ISLAND_DEPTH + localZ;
    }
}

void ChunkManager::worldToChunkCoords(int worldX, int worldY, int worldZ, int& chunkX, int& chunkY, int& chunkZ, int& localX, int& localY, int& localZ) {
    chunkX = worldX / Chunk::CHUNK_SIZE;
    chunkY = worldY / Chunk::CHUNK_SIZE;
    chunkZ = worldZ / Chunk::CHUNK_SIZE;
    
    localX = worldX % Chunk::CHUNK_SIZE;
    localY = worldY % Chunk::CHUNK_SIZE;
    localZ = worldZ % Chunk::CHUNK_SIZE;
    
    // Handle negative coordinates properly
    if (worldX < 0 && localX != 0) {
        chunkX--;
        localX = Chunk::CHUNK_SIZE + localX;
    }
    if (worldY < 0 && localY != 0) {
        chunkY--;
        localY = Chunk::CHUNK_SIZE + localY;
    }
    if (worldZ < 0 && localZ != 0) {
        chunkZ--;
        localZ = Chunk::CHUNK_SIZE + localZ;
    }
}

std::vector<Chunk*> ChunkManager::getAllDirtyChunks() {
    std::vector<Chunk*> allDirtyChunks;
    
    for (auto& [key, island] : m_islands) {
        if (island) {
            auto dirtyChunks = island->getDirtyChunks();
            allDirtyChunks.insert(allDirtyChunks.end(), dirtyChunks.begin(), dirtyChunks.end());
        }
    }
    
    return allDirtyChunks;
}

std::vector<Island*> ChunkManager::getAllIslands() {
    std::vector<Island*> allIslands;
    
    for (auto& [key, island] : m_islands) {
        if (island) {
            allIslands.push_back(island.get());
        }
    }
    
    return allIslands;
}

void ChunkManager::markAllChunksClean() {
    for (auto& [key, island] : m_islands) {
        if (island) {
            island->markAllChunksClean();
        }
    }
}

Vec3 ChunkManager::getSpawnLocation() const {
    // Spawn player at a reasonable location in the first island
    // Place them at a height that should be above the terrain
    return Vec3(Island::ISLAND_WIDTH * 0.5f, Island::ISLAND_HEIGHT * 0.8f, Island::ISLAND_DEPTH * 0.5f);
}

void ChunkManager::generateInitialWorld() {
    std::cout << "ChunkManager: Generating initial world..." << std::endl;
    
    // Generate the main island (0, 0) to start
    ensureIslandGenerated(0, 0);
    
    std::cout << "ChunkManager: Initial world generation complete" << std::endl;
}

uint64_t ChunkManager::getIslandKey(int islandX, int islandZ) const {
    // Pack two 32-bit integers into a 64-bit key
    return (uint64_t(uint32_t(islandX)) << 32) | uint64_t(uint32_t(islandZ));
}

void ChunkManager::generateIslandIfNeeded(Island* island) {
    if (island && !island->isGenerated()) {
        island->generateTerrain();
    }
}

} // namespace World
} // namespace Engine
