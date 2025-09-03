// GameState.cpp - Core game world state management implementation
#include "GameState.h"

#include <iostream>

#include "../Threading/JobSystem.h"
#include "../World/VoxelChunk.h"
#include "../Rendering/GlobalLightingManager.h"

// External job system (we'll refactor this later)
extern JobSystem g_jobSystem;

GameState::GameState()
{
    // Constructor - minimal initialization
}

GameState::~GameState()
{
    shutdown();
}

bool GameState::initialize(bool shouldCreateDefaultWorld)
{
    if (m_initialized)
    {
        std::cerr << "GameState already initialized!" << std::endl;
        return false;
    }

    std::cout << "🌍 Initializing GameState..." << std::endl;

    // Initialize physics system - Re-enabled with fixed BodyID handling
    m_physicsSystem = std::make_unique<PhysicsSystem>();
    // Physics system initialization is automatic - no explicit init needed
    /*
    if (!m_physicsSystem->initialize()) {
        std::cerr << "Failed to initialize physics system!" << std::endl;
        return false;
    }
    */

    // Create primary player
    m_primaryPlayer = std::make_unique<Player>();

    // Configure lighting system for maximum performance
    g_globalLighting.setUpdateFrequency(60.0f);   // Back to 60 FPS for responsiveness
    g_globalLighting.setOcclusionEnabled(false);  // Disable occlusion for maximum performance
    
    std::cout << "💡 Configured lighting: Simple face-orientation lighting for performance" << std::endl;

    // Create default world if requested
    if (shouldCreateDefaultWorld)
    {
        createDefaultWorld();
    }

    m_initialized = true;
    // Removed verbose debug output
    return true;
}

void GameState::shutdown()
{
    if (!m_initialized)
    {
        return;
    }

    std::cout << "🔄 Shutting down GameState..." << std::endl;

    // Clear island data
    m_islandIDs.clear();

    // Shutdown systems
    m_primaryPlayer.reset();

    // Physics system will be shut down automatically when destroyed

    m_initialized = false;
    std::cout << "✅ GameState shutdown complete" << std::endl;
}

void GameState::updateSimulation(float deltaTime)
{
    if (!m_initialized)
    {
        return;
    }

    // Update physics first
    updatePhysics(deltaTime);

    // Update player
    updatePlayer(deltaTime);

    // Update island physics
    m_islandSystem.updateIslandPhysics(deltaTime);

    // Sync physics to visual representation
    m_islandSystem.syncPhysicsToChunks();
}

void GameState::setPrimaryPlayerPosition(const Vec3& position)
{
    if (m_primaryPlayer)
    {
        m_primaryPlayer->setPosition(position);
    }
}

void GameState::applyPlayerMovement(const Vec3& movement, float deltaTime)
{
    if (m_primaryPlayer)
    {
        // For now, just update position directly
        // Later this will go through proper physics movement
        Vec3 currentPos = m_primaryPlayer->getPosition();
        m_primaryPlayer->setPosition(currentPos + movement * deltaTime);
    }
}

bool GameState::setVoxel(uint32_t islandID, const Vec3& localPos, uint8_t voxelType)
{
    // Delegate to island system
    m_islandSystem.setVoxelInIsland(islandID, localPos, voxelType);
    return true;  // Error handling will be added when async operations are implemented
}

uint8_t GameState::getVoxel(uint32_t islandID, const Vec3& localPos) const
{
    // Delegate to island system
    return m_islandSystem.getVoxelFromIsland(islandID, localPos);
}

Vec3 GameState::getIslandCenter(uint32_t islandID) const
{
    return m_islandSystem.getIslandCenter(islandID);
}

void GameState::createDefaultWorld()
{
    std::cout << "🏝️ Creating default world (3 floating islands)..." << std::endl;

    // **OPTIMIZED ISLAND SPACING** - Calculate safe distances to prevent overlap
    // Main island: Half size for testing lightmaps - 160-unit radius = 320 units diameter
    // Start with smaller island for lightmap testing
    float mainRadius = 160.0f;     // Halved from 320.0f for lightmap testing
    float spacing = 200.0f;        // Not needed for single island but kept for consistency
    
    // Create 1 island for testing and optimization
    uint32_t island1ID = m_islandSystem.createIsland(Vec3(0.0f, 0.0f, 0.0f));  // Center island

    // Track the island
    m_islandIDs.push_back(island1ID);

    // **SINGLE ISLAND GENERATION** - Focus on getting lighting working first
    std::cout << "[WORLD] Generating single test island with lighting optimization..." << std::endl;
    std::cout << "[ISLAND] Main island radius: " << mainRadius << " units" << std::endl;
    
    m_islandSystem.generateFloatingIslandOrganic(island1ID, 12345, mainRadius);      // Single test island

    // Log collision mesh generation for each island
    for (uint32_t islandID : m_islandIDs)
    {
        const FloatingIsland* island = m_islandSystem.getIsland(islandID);
        if (island)
        {
            // Count solid voxels across all chunks in this island
            int solidVoxels = 0;
            int totalChunks = 0;
            
            for (const auto& [chunkCoord, chunk] : island->chunks)
            {
                if (chunk)
                {
                    totalChunks++;
                    for (int x = 0; x < 32; x++)
                    {
                        for (int y = 0; y < 32; y++)
                        {
                            for (int z = 0; z < 32; z++)
                            {
                                if (chunk->getVoxel(x, y, z) > 0)
                                    solidVoxels++;
                            }
                        }
                    }
                    
                    // Generate mesh and build collision mesh for each chunk
                    const_cast<VoxelChunk*>(chunk.get())->generateMesh();
                    const_cast<VoxelChunk*>(chunk.get())->buildCollisionMesh();
                }
            }
            
            std::cout << "[SERVER] Island " << islandID << " has " << totalChunks << " chunks with " << solidVoxels << " solid voxels total" << std::endl;
            
            // Log collision mesh info from first chunk (for backward compatibility)
            if (!island->chunks.empty())
            {
                const auto& firstChunk = island->chunks.begin()->second;
                std::cout << "[SERVER] Generated island " << islandID << " with collision mesh ("
                          << firstChunk->getCollisionMesh().faces.size() << " faces in first chunk)"
                          << std::endl;
            }
        }
    }
    Vec3 islandCenter = m_islandSystem.getIslandCenter(island1ID);
    Vec3 playerSpawnPos = Vec3(16.0f, 16.0f, 16.0f);  // Relative to island center
    m_primaryPlayer->setPosition(playerSpawnPos);

    // Removed verbose debug output
}

void GameState::updatePhysics(float deltaTime)
{
    // Physics integration with Jolt Physics will be implemented when needed for gameplay
}

void GameState::updatePlayer(float deltaTime)
{
    if (m_primaryPlayer)
    {
        m_primaryPlayer->update(deltaTime);
    }
}
