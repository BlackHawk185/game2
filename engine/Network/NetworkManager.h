#pragma once
#include "IntegratedServer.h"
#include "NetworkClient.h"
#include <memory>
#include <string>
#include <cstdint>

/**
 * @brief Network manager that integrates server and client components
 * This is what gets added to the main engine
 */
class NetworkManager {
private:
    std::unique_ptr<IntegratedServer> server;
    std::unique_ptr<NetworkClient> client;
    bool isNetworkingEnabled;
    
public:
    NetworkManager();
    ~NetworkManager();
    
    // Initialize ENet library (call once at startup)
    static bool initializeNetworking();
    static void shutdownNetworking();
    
    // Server mode (hosting)
    bool startHosting(uint16_t port = 7777);
    void stopHosting();
    bool isHosting() const { return server && server->isRunning(); }
    
    // Client mode (joining)
    bool joinServer(const std::string& host, uint16_t port = 7777);
    void leaveServer();
    bool isConnectedToServer() const { return client && client->isConnected(); }
    
    // Called each frame to update networking
    void update();
    
    // Test functions for Phase 1
    void broadcastHelloWorld();
    
    // Movement functions for Phase 2
    void sendPlayerMovement(const Vec3& intendedPosition, const Vec3& velocity, float deltaTime);
    void broadcastPlayerPosition(uint32_t playerId, const Vec3& position, const Vec3& velocity);
    
    // Get components for setting up callbacks
    IntegratedServer* getServer() { return server.get(); }
    NetworkClient* getClient() { return client.get(); }
};
