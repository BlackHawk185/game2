#pragma once
#include <enet/enet.h>
#include "NetworkMessages.h"
#include <vector>
#include <functional>
#include <cstdint>

/**
 * @brief Simple server component that can be added to existing engine
 * Only active when hosting a game
 */
class IntegratedServer {
private:
    ENetHost* host;
    std::vector<ENetPeer*> connectedClients;
    uint32_t nextSequenceNumber;
    
public:
    IntegratedServer();
    ~IntegratedServer();
    
    // Server lifecycle
    bool startServer(uint16_t port = 7777);
    void stopServer();
    bool isRunning() const { return host != nullptr; }
    
    // Called each frame to handle networking
    void update();
    
    // Send messages to clients
    void broadcastHelloWorld();
    void broadcastPlayerPosition(uint32_t playerId, const Vec3& position, const Vec3& velocity);
    void sendWorldStateToClient(ENetPeer* client, const WorldStateMessage& worldState);
    void sendCompressedIslandToClient(ENetPeer* client, uint32_t islandID, const Vec3& position, const uint8_t* voxelData, uint32_t voxelDataSize);
    
    // NEW: Send individual chunk with coordinates for multi-chunk islands
    void sendCompressedChunkToClient(ENetPeer* client, uint32_t islandID, const Vec3& chunkCoord, const Vec3& islandPosition, const uint8_t* voxelData, uint32_t voxelDataSize);
    
    void broadcastVoxelChange(uint32_t islandID, const Vec3& localPos, uint8_t voxelType, uint32_t authorPlayerId);
    void broadcastEntityState(const EntityStateUpdate& entityState);
    void sendToClient(ENetPeer* client, const void* data, size_t size);
    void broadcastToAllClients(const void* data, size_t size);
    
    // Get connected clients for iteration
    const std::vector<ENetPeer*>& getConnectedClients() const { return connectedClients; }
    
    // Callbacks for when clients connect/disconnect
    std::function<void(ENetPeer*)> onClientConnected;
    std::function<void(ENetPeer*)> onClientDisconnected;
    std::function<void(ENetPeer*, const PlayerMovementRequest&)> onPlayerMovementRequest;
    std::function<void(ENetPeer*, const VoxelChangeRequest&)> onVoxelChangeRequest;
    
private:
    void handleClientEvent(const ENetEvent& event);
    void processClientMessage(ENetPeer* client, ENetPacket* packet);
};
