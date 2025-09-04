#pragma once

#include "../Network/NetworkManager.h"
#include "../Math/Vec3.h"
#include <vector>
#include <unordered_map>
#include <memory>

// Forward declarations for classes in other namespaces
namespace Engine {
namespace World {
    class Island;
    class ChunkManager;
}
}

namespace Engine {
namespace Core {

/**
 * GameServer - Authoritative game simulation
 * Contains all world state internally
 * Handles world generation, physics validation, and client communication
 * No rendering - purely simulation
 */
class GameServer {
public:
    GameServer();
    ~GameServer();

    // Core lifecycle
    bool initialize(uint16_t port = 7777);
    void update(float deltaTime);
    void shutdown();

    // Server state
    bool isRunning() const { return m_isRunning; }
    uint16_t getPort() const { return m_port; }

private:
    // Core server state
    bool m_isRunning;
    uint16_t m_port;
    
    // World state (server owns this)
    std::vector<Engine::World::Island*> m_islands;           // Simplified to raw pointers
    Engine::World::ChunkManager* m_chunkManager;             // Simplified to raw pointer
    
    // Networking
    NetworkManager* m_networkManager;         // Simplified to raw pointer
    
    // Connected players
    struct PlayerState {
        uint32_t playerID;
        Vec3 position;
        Vec3 velocity;
        bool isActive;
    };
    std::unordered_map<uint32_t, PlayerState> m_players;
    
    // Internal methods
    void handleClientConnections();
    void handleClientInput();
    void updateWorldSimulation(float deltaTime);
    void sendWorldUpdatesToClients();
    void generateInitialWorld();
    
    // Player management
    uint32_t addPlayer();
    void removePlayer(uint32_t playerID);
    void updatePlayerState(uint32_t playerID, const Vec3& position, const Vec3& velocity);
    bool validatePlayerMovement(uint32_t playerID, const Vec3& newPosition);
};

} // namespace Core
} // namespace Engine
