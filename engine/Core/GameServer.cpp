// GameServer.cpp - Headless game server implementation
#include "GameServer.h"
#include "../Network/NetworkMessages.h"
#include <iostream>
#include <chrono>
#include <thread>

GameServer::GameServer() {
    // Constructor
    m_networkManager = std::make_unique<NetworkManager>();  // Re-enabled with ENet
}

GameServer::~GameServer() {
    shutdown();
}

bool GameServer::initialize(float targetTickRate, bool enableNetworking, uint16_t networkPort) {
    std::cout << "🖥️  Initializing GameServer (headless mode)..." << std::endl;
    
    m_targetTickRate = targetTickRate;
    m_fixedDeltaTime = 1.0f / targetTickRate;
    m_networkingEnabled = enableNetworking;
    
    // Initialize time manager
    m_timeManager = std::make_unique<TimeManager>();
    
    // Initialize game state
    m_gameState = std::make_unique<GameState>();
    if (!m_gameState->initialize(true)) {  // Create default world
        std::cerr << "Failed to initialize game state!" << std::endl;
        return false;
    }
    
    // Initialize networking if requested
    if (m_networkingEnabled) {
        if (!m_networkManager->initializeNetworking()) {
            std::cerr << "Failed to initialize networking!" << std::endl;
            return false;
        }
        
        if (!m_networkManager->startHosting(networkPort)) {
            std::cerr << "Failed to start network server on port " << networkPort << std::endl;
            return false;
        }
        
        // Set up callback to send world state to new clients
        if (auto server = m_networkManager->getServer()) {
            server->onClientConnected = [this](ENetPeer* peer) {
                std::cout << "New player joined the game!" << std::endl;
                sendWorldStateToClient(peer);
            };
        }
        
        std::cout << "✅ Network server started on port " << networkPort << std::endl;
    }
    
    std::cout << "✅ GameServer initialized successfully" << std::endl;
    std::cout << "   Target tick rate: " << targetTickRate << " Hz" << std::endl;
    std::cout << "   Fixed delta time: " << m_fixedDeltaTime << " seconds" << std::endl;
    std::cout << "   Networking: " << (m_networkingEnabled ? "ENABLED" : "DISABLED") << std::endl;
    
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
    
    // Update networking
    if (m_networkingEnabled && m_networkManager) {
        m_networkManager->update();
    }
    
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

void GameServer::sendWorldStateToClient(ENetPeer* peer) {
    if (!m_gameState || !m_networkManager) {
        std::cerr << "Cannot send world state: missing game state or network manager" << std::endl;
        return;
    }
    
    // Create world state message from current game state
    WorldStateMessage worldState;
    worldState.numIslands = 3;  // We know we have 3 islands from createDefaultWorld
    
    // Get island positions (for now, use hardcoded positions from createDefaultWorld)
    worldState.islandPositions[0] = Vec3(0, 0, 0);     // Main island
    worldState.islandPositions[1] = Vec3(50, 10, 30);  // Second island  
    worldState.islandPositions[2] = Vec3(-30, 5, 40);  // Third island
    
    // Set player spawn position (center of main island)
    worldState.playerSpawnPosition = Vec3(8, 10, 8);
    
    std::cout << "Sending world state to client: " << worldState.numIslands << " islands" << std::endl;
    
    // Send through network manager
    if (auto server = m_networkManager->getServer()) {
        server->sendWorldStateToClient(peer, worldState);
    }
}
