// GameServer.cpp - Headless game server implementation
#include "GameServer.h"
#include <iostream>
#include <chrono>
#include <thread>

GameServer::GameServer() {
    // Constructor
}

GameServer::~GameServer() {
    shutdown();
}

bool GameServer::initialize(float targetTickRate) {
    std::cout << "🖥️  Initializing GameServer (headless mode)..." << std::endl;
    
    m_targetTickRate = targetTickRate;
    m_fixedDeltaTime = 1.0f / targetTickRate;
    
    // Initialize time manager
    m_timeManager = std::make_unique<TimeManager>();
    
    // Initialize game state
    m_gameState = std::make_unique<GameState>();
    if (!m_gameState->initialize(true)) {  // Create default world
        std::cerr << "Failed to initialize game state!" << std::endl;
        return false;
    }
    
    std::cout << "✅ GameServer initialized successfully" << std::endl;
    std::cout << "   Target tick rate: " << targetTickRate << " Hz" << std::endl;
    std::cout << "   Fixed delta time: " << m_fixedDeltaTime << " seconds" << std::endl;
    
    return true;
}

void GameServer::run() {
    if (m_running.load()) {
        std::cerr << "Server is already running!" << std::endl;
        return;
    }
    
    std::cout << "🚀 Starting GameServer simulation loop..." << std::endl;
    m_running.store(true);
    
    serverLoop();
}

void GameServer::runAsync() {
    if (m_running.load()) {
        std::cerr << "Server is already running!" << std::endl;
        return;
    }
    
    std::cout << "🚀 Starting GameServer simulation loop (async)..." << std::endl;
    m_running.store(true);
    
    m_serverThread = std::make_unique<std::thread>(&GameServer::serverLoop, this);
}

void GameServer::stop() {
    if (!m_running.load()) {
        return;
    }
    
    std::cout << "⏹️  Stopping GameServer..." << std::endl;
    m_running.store(false);
    
    // Wait for server thread to finish
    if (m_serverThread && m_serverThread->joinable()) {
        m_serverThread->join();
        m_serverThread.reset();
    }
    
    std::cout << "✅ GameServer stopped" << std::endl;
    std::cout << "   Total ticks processed: " << m_totalTicks << std::endl;
}

void GameServer::shutdown() {
    stop();
    
    std::cout << "🔄 Shutting down GameServer..." << std::endl;
    
    // Clear command queues
    m_pendingVoxelChanges.clear();
    m_pendingPlayerMovements.clear();
    
    // Shutdown systems
    if (m_gameState) {
        m_gameState->shutdown();
        m_gameState.reset();
    }
    
    m_timeManager.reset();
    
    std::cout << "✅ GameServer shutdown complete" << std::endl;
}

void GameServer::queueVoxelChange(uint32_t islandID, const Vec3& localPos, uint8_t voxelType) {
    // TODO: Add proper thread-safe queue implementation
    // For now, just apply directly (not thread-safe!)
    VoxelChangeCommand cmd;
    cmd.islandID = islandID;
    cmd.localPos = localPos;
    cmd.voxelType = voxelType;
    m_pendingVoxelChanges.push_back(cmd);
}

void GameServer::queuePlayerMovement(const Vec3& movement) {
    // TODO: Add proper thread-safe queue implementation
    PlayerMovementCommand cmd;
    cmd.movement = movement;
    m_pendingPlayerMovements.push_back(cmd);
}

void GameServer::serverLoop() {
    auto lastTime = std::chrono::high_resolution_clock::now();
    float accumulator = 0.0f;
    
    std::cout << "🔄 Server simulation loop started" << std::endl;
    
    while (m_running.load()) {
        auto currentTime = std::chrono::high_resolution_clock::now();
        float deltaTime = std::chrono::duration<float>(currentTime - lastTime).count();
        lastTime = currentTime;
        
        // Clamp delta time to prevent spiral of death
        if (deltaTime > 0.25f) {
            deltaTime = 0.25f;
        }
        
        accumulator += deltaTime;
        
        // Fixed timestep simulation
        while (accumulator >= m_fixedDeltaTime) {
            processTick(m_fixedDeltaTime);
            accumulator -= m_fixedDeltaTime;
            m_totalTicks++;
        }
        
        // Update tick rate statistics
        updateTickRateStats(deltaTime);
        
        // Sleep to avoid consuming 100% CPU
        // Target is to wake up several times per fixed timestep for responsiveness
        std::this_thread::sleep_for(std::chrono::microseconds(1000)); // 1ms
    }
    
    std::cout << "🔄 Server simulation loop ended" << std::endl;
}

void GameServer::processTick(float deltaTime) {
    // Process queued commands first
    processQueuedCommands();
    
    // Update time manager
    if (m_timeManager) {
        m_timeManager->update(deltaTime);
    }
    
    // Update game simulation
    if (m_gameState) {
        m_gameState->updateSimulation(deltaTime);
    }
    
    // TODO: Here we would broadcast state updates to clients
    // TODO: Here we would handle network message processing
}

void GameServer::processQueuedCommands() {
    // Process voxel changes
    for (const auto& cmd : m_pendingVoxelChanges) {
        if (m_gameState) {
            m_gameState->setVoxel(cmd.islandID, cmd.localPos, cmd.voxelType);
        }
    }
    m_pendingVoxelChanges.clear();
    
    // Process player movements
    for (const auto& cmd : m_pendingPlayerMovements) {
        if (m_gameState) {
            m_gameState->applyPlayerMovement(cmd.movement, m_fixedDeltaTime);
        }
    }
    m_pendingPlayerMovements.clear();
}

void GameServer::updateTickRateStats(float actualDeltaTime) {
    // Simple moving average for tick rate calculation
    static float tickRateAccumulator = 0.0f;
    static int tickRateSamples = 0;
    
    if (actualDeltaTime > 0.0f) {
        tickRateAccumulator += 1.0f / actualDeltaTime;
        tickRateSamples++;
        
        // Update every 60 samples (~1 second)
        if (tickRateSamples >= 60) {
            m_currentTickRate = tickRateAccumulator / tickRateSamples;
            tickRateAccumulator = 0.0f;
            tickRateSamples = 0;
        }
    }
}
