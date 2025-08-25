// IslandChunkSystem.cpp - Implementation of physics-driven chunking
#include "IslandChunkSystem.h"
#include "VoxelChunk.h"
#include <iostream>
#include <cmath>
#include <memory>
#include <cstdlib>
#include <ctime>

IslandChunkSystem g_islandSystem;

IslandChunkSystem::IslandChunkSystem() {
    // Initialize system
    // Seed random number generator for island velocity
    std::srand(static_cast<unsigned int>(std::time(nullptr)));
}

IslandChunkSystem::~IslandChunkSystem() {
    // Clean up all islands
    for (auto& [id, island] : m_islands) {
        destroyIsland(id);
    }
}

uint32_t IslandChunkSystem::createIsland(const Vec3& physicsCenter) {
    uint32_t islandID = m_nextIslandID++;
    FloatingIsland& island = m_islands[islandID];
    island.islandID = islandID;
    island.physicsCenter = physicsCenter;
    std::srand(static_cast<unsigned int>(std::time(nullptr)) + islandID * 137);
    float randomX = (std::rand() % 201 - 100) / 100.0f;
    float randomY = (std::rand() % 201 - 100) / 100.0f;
    float randomZ = (std::rand() % 201 - 100) / 100.0f;
    // Set velocity to ±0.4f for all axes (4x faster drift)
    island.velocity = Vec3(
        randomX * 0.4f,
        randomY * 0.4f,
        randomZ * 0.4f
    );
    island.acceleration = Vec3(0, 0, 0); // No gravity for islands
    return islandID;
}

void IslandChunkSystem::destroyIsland(uint32_t islandID) {
    auto it = m_islands.find(islandID);
    if (it == m_islands.end()) return;
    
    m_islands.erase(it);
}

FloatingIsland* IslandChunkSystem::getIsland(uint32_t islandID) {
    auto it = m_islands.find(islandID);
    return (it != m_islands.end()) ? &it->second : nullptr;
}

const FloatingIsland* IslandChunkSystem::getIsland(uint32_t islandID) const {
    auto it = m_islands.find(islandID);
    return (it != m_islands.end()) ? &it->second : nullptr;
}

Vec3 IslandChunkSystem::getIslandCenter(uint32_t islandID) const {
    auto it = m_islands.find(islandID);
    if (it != m_islands.end()) {
        return it->second.physicsCenter;
    }
    return Vec3(0, 0, 0); // Return zero vector if island not found
}

Vec3 IslandChunkSystem::getIslandVelocity(uint32_t islandID) const {
    auto it = m_islands.find(islandID);
    if (it != m_islands.end()) {
        return it->second.velocity;
    }
    return Vec3(0, 0, 0); // Return zero velocity if island not found
}

void IslandChunkSystem::addChunkToIsland(uint32_t islandID, const Vec3& localPosition) {
    FloatingIsland* island = getIsland(islandID);
    if (!island) return;
    
}

void IslandChunkSystem::removeChunkFromIsland(uint32_t islandID, const Vec3& localPosition) {
    FloatingIsland* island = getIsland(islandID);
    if (!island) return;
    
}

void IslandChunkSystem::generateFloatingIsland(uint32_t islandID, uint32_t seed, float radius) {
    FloatingIsland* island = getIsland(islandID);
    if (!island) return;
    
    // Create the main chunk for this island
    island->mainChunk = std::make_unique<VoxelChunk>();
    island->mainChunk->generateFloatingIsland(seed);
}

uint8_t IslandChunkSystem::getVoxelFromIsland(uint32_t islandID, const Vec3& localPosition) const {
    const FloatingIsland* island = getIsland(islandID);
    if (!island || !island->mainChunk) return 0;
    
    int x = (int)localPosition.x;
    int y = (int)localPosition.y;
    int z = (int)localPosition.z;
    
    // Check bounds
    if (x < 0 || x >= 32 || y < 0 || y >= 32 || z < 0 || z >= 32) {
        return 0;
    }
    
    return island->mainChunk->getVoxel(x, y, z);
}

void IslandChunkSystem::setVoxelInIsland(uint32_t islandID, const Vec3& localPosition, uint8_t voxelType) {
    FloatingIsland* island = getIsland(islandID);
    if (!island || !island->mainChunk) return;
    
    int x = (int)localPosition.x;
    int y = (int)localPosition.y;
    int z = (int)localPosition.z;
    
    // Check bounds
    if (x < 0 || x >= 32 || y < 0 || y >= 32 || z < 0 || z >= 32) {
        return;
    }
    
    island->mainChunk->setVoxel(x, y, z, voxelType);
    island->mainChunk->generateMesh(); // Update visual
    
}

void IslandChunkSystem::getAllChunks(std::vector<VoxelChunk*>& outChunks) {
    outChunks.clear();
    
    for (auto& [id, island] : m_islands) {
        if (island.mainChunk) {
            outChunks.push_back(island.mainChunk.get());
        }
    }
}

void IslandChunkSystem::getVisibleChunks(const Vec3& viewPosition, std::vector<VoxelChunk*>& outChunks) {
    // Frustum culling will be added when we implement proper camera frustum
    getAllChunks(outChunks);
}

void IslandChunkSystem::renderAllIslands() {
    // Render each island at its physics center position
    for (auto& [id, island] : m_islands) {
        if (island.mainChunk) {
            island.mainChunk->render(island.physicsCenter);
        }
    }
}

void IslandChunkSystem::updateIslandPhysics(float deltaTime) {
    for (auto& [id, island] : m_islands) {
        // Only apply random drift, no gravity, bobbing, or wind
        island.physicsCenter.x += island.velocity.x * deltaTime;
        island.physicsCenter.y += island.velocity.y * deltaTime;
        island.physicsCenter.z += island.velocity.z * deltaTime;
        island.needsPhysicsUpdate = true;
    }
}

void IslandChunkSystem::syncPhysicsToChunks() {
    // Sync physics positions to chunk rendering positions
    for (auto& [id, island] : m_islands) {
        if (island.mainChunk && island.needsPhysicsUpdate) {
            // The chunk renders at the island's physics center
            // (No need for separate world position storage - use physics center directly)
            island.needsPhysicsUpdate = false;
        }
    }
}

void IslandChunkSystem::updatePlayerChunks(const Vec3& playerPosition) {
    // Infinite world generation will be implemented in a future version
    // For now, we manually create islands in GameState
}

void IslandChunkSystem::generateChunksAroundPoint(const Vec3& center) {
    // Chunk generation around points will be used for infinite world expansion
    // Currently handled manually through createIsland()
}
