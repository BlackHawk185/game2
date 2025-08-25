#include "NetworkManager.h"
#include "NetworkMessages.h"
#include <enet/enet.h>
#include <iostream>

NetworkManager::NetworkManager() 
    : isNetworkingEnabled(false) {
    // Create integrated server and client
    server = std::make_unique<IntegratedServer>();
    client = std::make_unique<NetworkClient>();
    
    // Set up client event handlers
    client->onConnectedToServer = [this]() {
        std::cout << "Connected to game server" << std::endl;
    };
    
    client->onDisconnectedFromServer = [this]() {
        std::cout << "Disconnected from the game" << std::endl;
    };
    
    client->onHelloWorld = [this](const HelloWorldMessage& msg) {
        std::cout << "Server says: " << msg.message << std::endl;
    };
    
    client->onPlayerPositionUpdate = [this](const PlayerPositionUpdate& update) {
        // Debug: Uncomment for position update debugging
        // std::cout << "Player " << update.playerId << " moved to: " 
        //           << update.position.x << ", " 
        //           << update.position.y << ", " 
        //           << update.position.z << std::endl;
    };
}

NetworkManager::~NetworkManager() {
    stopHosting();
    leaveServer();
}

bool NetworkManager::initializeNetworking() {
    if (enet_initialize() != 0) {
        std::cout << "Failed to initialize ENet!" << std::endl;
        return false;
    }
    std::cout << "ENet initialized successfully" << std::endl;
    return true;
}

void NetworkManager::shutdownNetworking() {
    enet_deinitialize();
    std::cout << "ENet shut down" << std::endl;
}

bool NetworkManager::startHosting(uint16_t port) {
    if (!server) {
        server = std::make_unique<IntegratedServer>();
    }
    
    bool success = server->startServer(port);
    if (success) {
        isNetworkingEnabled = true;
        
        // Set up basic callbacks
        server->onClientConnected = [](ENetPeer* peer) {
            std::cout << "New player joined the game!" << std::endl;
        };
        
        server->onClientDisconnected = [](ENetPeer* peer) {
            std::cout << "Player left the game" << std::endl;
        };
        
        server->onPlayerMovementRequest = [this](ENetPeer* peer, const PlayerMovementRequest& request) {
            // Debug: Uncomment for movement debugging
            // std::cout << "Player wants to move to: " 
            //           << request.intendedPosition.x << ", " 
            //           << request.intendedPosition.y << ", " 
            //           << request.intendedPosition.z << std::endl;
            
            // For now, just accept all movement requests
            // Movement validation will be added in Phase 2
            this->broadcastPlayerPosition(0, request.intendedPosition, request.velocity);
        };
        
        // Voxel changes are now handled by GameServer directly via its own callback
    }
    
    return success;
}

void NetworkManager::stopHosting() {
    if (server) {
        server->stopServer();
        isNetworkingEnabled = false;
    }
}

bool NetworkManager::joinServer(const std::string& host, uint16_t port) {
    if (!client) {
        client = std::make_unique<NetworkClient>();
    }
    
    bool success = client->connectToServer(host, port);
    if (success) {
        isNetworkingEnabled = true;
        
        // Set up basic callbacks
        client->onConnectedToServer = []() {
            std::cout << "Successfully joined the game!" << std::endl;
        };
        
        client->onDisconnectedFromServer = []() {
            std::cout << "Disconnected from the game" << std::endl;
        };
        
        client->onHelloWorld = [](const HelloWorldMessage& msg) {
            std::cout << "Server says: " << msg.message << std::endl;
        };
        
        client->onPlayerPositionUpdate = [](const PlayerPositionUpdate& update) {
            // Debug: Uncomment for position update debugging  
            // std::cout << "Player " << update.playerId << " moved to: " 
            //           << update.position.x << ", " 
            //           << update.position.y << ", " 
            //           << update.position.z << std::endl;
        };
    }
    
    return success;
}

void NetworkManager::leaveServer() {
    if (client) {
        client->disconnect();
        isNetworkingEnabled = false;
    }
}

void NetworkManager::update() {
    if (server && server->isRunning()) {
        server->update();
    }
    
    if (client && client->isConnected()) {
        client->update();
    }
}

void NetworkManager::broadcastHelloWorld() {
    if (server && server->isRunning()) {
        server->broadcastHelloWorld();
    }
}

void NetworkManager::sendPlayerMovement(const Vec3& intendedPosition, const Vec3& velocity, float deltaTime) {
    if (client && client->isConnected()) {
        client->sendMovementRequest(intendedPosition, velocity, deltaTime);
    }
}

void NetworkManager::broadcastPlayerPosition(uint32_t playerId, const Vec3& position, const Vec3& velocity) {
    if (server && server->isRunning()) {
        server->broadcastPlayerPosition(playerId, position, velocity);
    }
}
