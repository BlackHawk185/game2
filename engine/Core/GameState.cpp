// GameState.cpp - Core game world state management implementation
#include "GameState.h"
#include "../Threading/JobSystem.h"
#include <iostream>

// External job system (we'll refactor this later)
extern JobSystem g_jobSystem;

GameState::GameState() {
    // Constructor - minimal initialization
}

GameState::~GameState() {
    shutdown();
}

bool GameState::initialize(bool shouldCreateDefaultWorld) {
    if (m_initialized) {
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
    
    // Create default world if requested
    if (shouldCreateDefaultWorld) {
        createDefaultWorld();
    }
    
    m_initialized = true;
    std::cout << "✅ GameState initialized successfully" << std::endl;
    return true;
}

void GameState::shutdown() {
    if (!m_initialized) {
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

void GameState::updateSimulation(float deltaTime) {
    if (!m_initialized) {
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

void GameState::setPrimaryPlayerPosition(const Vec3& position) {
    if (m_primaryPlayer) {
        m_primaryPlayer->setPosition(position);
    }
}

void GameState::applyPlayerMovement(const Vec3& movement, float deltaTime) {
    if (m_primaryPlayer) {
        // For now, just update position directly
        // Later this will go through proper physics movement
        Vec3 currentPos = m_primaryPlayer->getPosition();
        m_primaryPlayer->setPosition(currentPos + movement * deltaTime);
    }
}

bool GameState::setVoxel(uint32_t islandID, const Vec3& localPos, uint8_t voxelType) {
    // Delegate to island system
    m_islandSystem.setVoxelInIsland(islandID, localPos, voxelType);
    return true;  // Error handling will be added when async operations are implemented
}

uint8_t GameState::getVoxel(uint32_t islandID, const Vec3& localPos) const {
    // Delegate to island system
    return m_islandSystem.getVoxelFromIsland(islandID, localPos);
}

Vec3 GameState::getIslandCenter(uint32_t islandID) const {
    return m_islandSystem.getIslandCenter(islandID);
}

void GameState::createDefaultWorld() {
    std::cout << "🏝️ Creating default world (3 floating islands)..." << std::endl;
    
    // Create 3 islands in a triangle formation
    uint32_t island1ID = m_islandSystem.createIsland(Vec3(0, 0, 0));      // Center island
    uint32_t island2ID = m_islandSystem.createIsland(Vec3(40, 5, 30));    // Right-forward island  
    uint32_t island3ID = m_islandSystem.createIsland(Vec3(-40, -5, 30));  // Left-forward island
    
    // Track the islands
    m_islandIDs.push_back(island1ID);
    m_islandIDs.push_back(island2ID);
    m_islandIDs.push_back(island3ID);
    
    // Generate each island with different seeds for variety
    m_islandSystem.generateFloatingIsland(island1ID, 12345, 32.0f);
    m_islandSystem.generateFloatingIsland(island2ID, 54321, 32.0f);
    m_islandSystem.generateFloatingIsland(island3ID, 98765, 32.0f);
    
    // Position primary player on the center island
    Vec3 islandCenter = m_islandSystem.getIslandCenter(island1ID);
    Vec3 playerSpawnPos = Vec3(16.0f, 16.0f, 16.0f);  // Relative to island center
    m_primaryPlayer->setPosition(playerSpawnPos);
    
    std::cout << "✅ Default world created with " << m_islandIDs.size() << " islands" << std::endl;
}

void GameState::updatePhysics(float deltaTime) {
    // Physics integration with Jolt Physics will be implemented when needed for gameplay
}

void GameState::updatePlayer(float deltaTime) {
    if (m_primaryPlayer) {
        m_primaryPlayer->update(deltaTime);
    }
}
