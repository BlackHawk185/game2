#include "GameServer.h"
#include "../World/ChunkManager.h"
#include "../Network/NetworkMessages.h"
#include "../Time/TimeManager.h"
#include <iostream>

namespace Engine {
namespace Core {

GameServer::GameServer() 
    : m_isRunning(false)
    , m_port(0)
    , m_chunkManager(nullptr)
    , m_networkManager(nullptr)
{
}

GameServer::~GameServer() {
    shutdown();
}

bool GameServer::initialize(uint16_t port) {
    m_port = port;
    
    // Create networking manager
    m_networkManager = new NetworkManager();
    if (!m_networkManager->initializeNetworking()) {
        std::cerr << "Failed to initialize ENet networking!" << std::endl;
        delete m_networkManager;
        m_networkManager = nullptr;
        return false;
    }
    
    if (!m_networkManager->startHosting(port)) {
        std::cerr << "Failed to start hosting on port " << port << std::endl;
        m_networkManager->shutdownNetworking();
        delete m_networkManager;
        m_networkManager = nullptr;
        return false;
    }

    // Create chunk manager
    m_chunkManager = new Engine::World::ChunkManager();
    
    // Generate initial world chunks around spawn (0,0,0)
    generateInitialWorld();

    m_isRunning = true;
    std::cout << "GameServer initialized on port " << port << std::endl;
    return true;
}

void GameServer::shutdown() {
    m_isRunning = false;
    
    if (m_networkManager) {
        m_networkManager->stopHosting();
        m_networkManager->shutdownNetworking();
        delete m_networkManager;
        m_networkManager = nullptr;
    }
    
    // Clean up islands
    for (auto* island : m_islands) {
        delete island;
    }
    m_islands.clear();
    
    if (m_chunkManager) {
        delete m_chunkManager;
        m_chunkManager = nullptr;
    }
    
    std::cout << "GameServer shutdown" << std::endl;
}

void GameServer::update(float deltaTime) {
    if (!m_isRunning || !m_networkManager || !m_chunkManager) {
        return;
    }

    // Update networking
    m_networkManager->update();
    
    // Update world generation and chunks
    m_chunkManager->update(deltaTime);
    
    // Handle client connections and input
    handleClientConnections();
    handleClientInput();
    
    // Update world simulation
    updateWorldSimulation(deltaTime);
    
    // Send world updates to clients
    sendWorldUpdatesToClients();
}

void GameServer::generateInitialWorld() {
    if (!m_chunkManager) {
        return;
    }
    
    // Generate initial world using ChunkManager
    m_chunkManager->generateInitialWorld();
    std::cout << "Generated initial world" << std::endl;
}

void GameServer::handleClientConnections() {
    // TODO: Handle new client connections and disconnections
}

void GameServer::handleClientInput() {
    // TODO: Process client input messages
}

void GameServer::updateWorldSimulation(float deltaTime) {
    // TODO: Update physics, entities, etc.
}

void GameServer::sendWorldUpdatesToClients() {
    if (!m_networkManager || !m_chunkManager) {
        return;
    }

    // Get all dirty chunks that need to be sent to clients
    auto dirtyChunks = m_chunkManager->getAllDirtyChunks();
    
    static int debugCount = 0;
    debugCount++;
    
    // Print debug info every 5 seconds
    if (debugCount % 300 == 0) {
        std::cout << "GameServer: Checking for dirty chunks... Found: " << dirtyChunks.size() << std::endl;
    }
    
    if (!dirtyChunks.empty()) {
        static int updateCount = 0;
        updateCount++;
        
        // Only log every 60 updates (once per second at 60fps)
        if (updateCount % 60 == 0) {
            std::cout << "Sending " << dirtyChunks.size() << " dirty chunks to clients (update #" << updateCount << ")" << std::endl;
        }
        
        for (const auto* chunk : dirtyChunks) {
            // TODO: Implement actual chunk data compression and transmission
            // For now, just log that we would send this chunk (but only occasionally)
            if (updateCount % 60 == 0) {
                std::cout << "Would send chunk data for a chunk" << std::endl;
            }
        }
        
        // Mark chunks as clean after sending
        m_chunkManager->markAllChunksClean();
    }
}

// Player management methods
uint32_t GameServer::addPlayer() {
    // TODO: Implement player addition
    return 0;
}

void GameServer::removePlayer(uint32_t playerID) {
    // TODO: Implement player removal
    m_players.erase(playerID);
}

void GameServer::updatePlayerState(uint32_t playerID, const Vec3& position, const Vec3& velocity) {
    auto it = m_players.find(playerID);
    if (it != m_players.end()) {
        it->second.position = position;
        it->second.velocity = velocity;
    }
}

bool GameServer::validatePlayerMovement(uint32_t playerID, const Vec3& newPosition) {
    // TODO: Implement movement validation
    return true;
}

} // namespace Core
} // namespace Engine
