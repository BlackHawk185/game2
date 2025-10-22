// GameState.cpp - Core game world state management implementation
#include "GameState.h"

#include <iostream>
#include <cstdlib>
#include <ctime>

#include "../Threading/JobSystem.h"
#include "../World/VoxelChunk.h"
#include "../Rendering/GlobalLightingManager.h"
#include "../Physics/FluidSystem.h"

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
    g_globalLighting.setUpdateFrequency(20.0f);   // Increased from 10 FPS to 20 FPS for smoother lighting
    g_globalLighting.setOcclusionEnabled(false);  // Disable occlusion for maximum performance
    
    std::cout << "💡 Configured lighting: Simple face-orientation lighting at 10 FPS for performance" << std::endl;

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

    // Update fluid system
    g_fluidSystem.update(deltaTime);

    // Update player
    updatePlayer(deltaTime);

    // Update island physics
    m_islandSystem.updateIslandPhysics(deltaTime);
    
    // NOTE: syncPhysicsToChunks() is called by GameClient, not here
    // Server doesn't have a renderer, so it shouldn't sync physics to rendering
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
    std::cout << "🏝️ Creating default world (multiple floating islands)..." << std::endl;

    // **ISLAND CONFIGURATION** - Reduced for physics debugging
    float mainRadius = 120.0f;     // Main island size (reduced from 500.0f for faster iteration)
    float smallRadius = 50.0f;     // Smaller island size (reduced from 120.0f)
    float spacing = 150.0f;        // Distance between island centers (reduced from 300.0f)
    
    // Create multiple islands with varied positions
    uint32_t island1ID = m_islandSystem.createIsland(Vec3(0.0f, 0.0f, 0.0f));        // Center
    uint32_t island2ID = m_islandSystem.createIsland(Vec3(spacing, 50.0f, 0.0f));   // East
    uint32_t island3ID = m_islandSystem.createIsland(Vec3(-spacing, -30.0f, 0.0f)); // West
    uint32_t island4ID = m_islandSystem.createIsland(Vec3(0.0f, 40.0f, spacing));   // North
    uint32_t island5ID = m_islandSystem.createIsland(Vec3(0.0f, -50.0f, -spacing)); // South

    // Track all islands
    m_islandIDs.push_back(island1ID);
    m_islandIDs.push_back(island2ID);
    m_islandIDs.push_back(island3ID);
    m_islandIDs.push_back(island4ID);
    m_islandIDs.push_back(island5ID);

    // **MULTIPLE ISLAND GENERATION** - Create a small archipelago with random seeds
    std::cout << "[WORLD] Generating " << m_islandIDs.size() << " islands..." << std::endl;
    
    // Generate random seeds for each island (unique world every time)
    std::srand(static_cast<unsigned int>(std::time(nullptr)));
    uint32_t seed1 = std::rand();
    uint32_t seed2 = std::rand();
    uint32_t seed3 = std::rand();
    uint32_t seed4 = std::rand();
    uint32_t seed5 = std::rand();
    
    std::cout << "[WORLD] Using random seeds: " << seed1 << ", " << seed2 << ", " << seed3 << ", " << seed4 << ", " << seed5 << std::endl;
    
    // Generate each island with random seeds for variety
    m_islandSystem.generateFloatingIslandOrganic(island1ID, seed1, mainRadius);      // Main island (large)
    m_islandSystem.generateFloatingIslandOrganic(island2ID, seed2, smallRadius);     // East island (smaller)
    m_islandSystem.generateFloatingIslandOrganic(island3ID, seed3, smallRadius);     // West island (smaller)
    m_islandSystem.generateFloatingIslandOrganic(island4ID, seed4, smallRadius);     // North island (smaller)
    m_islandSystem.generateFloatingIslandOrganic(island5ID, seed5, smallRadius);     // South island (smaller)

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
                    
                    // Generate mesh WITHOUT LIGHTING during world generation (huge performance boost)
                    // Lighting will be generated later when needed by the client
                    const_cast<VoxelChunk*>(chunk.get())->generateMesh(false);
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
    
    // Set player spawn position (will be read by GameClient when it connects)
    Vec3 islandCenter = m_islandSystem.getIslandCenter(island1ID);
    // Spawn high above island center to ensure dramatic falling entry and avoid spawning inside geometry
    Vec3 playerSpawnPos = Vec3(0.0f, 64.0f, 0.0f);  // 500 units straight up from origin
    m_primaryPlayer->setPosition(playerSpawnPos);

    std::cout << "🎯 Player spawn position: (" << playerSpawnPos.x << ", " << playerSpawnPos.y << ", " << playerSpawnPos.z << ")" << std::endl;
    std::cout << "🏝️  Main island center: (" << islandCenter.x << ", " << islandCenter.y << ", " << islandCenter.z << ")" << std::endl;
}

void GameState::updatePhysics(float deltaTime)
{
    // Update generic entity physics (including fluid particles)
    g_physics.update(deltaTime);
}

void GameState::updatePlayer(float deltaTime)
{
    if (m_primaryPlayer)
    {
        m_primaryPlayer->update(deltaTime);
    }
}
