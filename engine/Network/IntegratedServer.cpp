#include "IntegratedServer.h"
#include <iostream>
#include <cstring>
#include <algorithm>

IntegratedServer::IntegratedServer() 
    : host(nullptr), nextSequenceNumber(0) {
}

IntegratedServer::~IntegratedServer() {
    stopServer();
}

bool IntegratedServer::startServer(uint16_t port) {
    if (host) {
        std::cout << "Server already running!" << std::endl;
        return false;
    }
    
    ENetAddress address;
    address.host = ENET_HOST_ANY;
    address.port = port;
    
    // Create server host (allow up to 32 clients)
    host = enet_host_create(&address, 32, 2, 0, 0);
    if (!host) {
        std::cout << "Failed to create ENet server host!" << std::endl;
        return false;
    }
    
    std::cout << "Server started on port " << port << std::endl;
    return true;
}

void IntegratedServer::stopServer() {
    if (host) {
        enet_host_destroy(host);
        host = nullptr;
        connectedClients.clear();
        std::cout << "Server stopped" << std::endl;
    }
}

void IntegratedServer::update() {
    if (!host) return;
    
    ENetEvent event;
    
    // Process all pending events (non-blocking)
    while (enet_host_service(host, &event, 0) > 0) {
        handleClientEvent(event);
    }
}

void IntegratedServer::handleClientEvent(const ENetEvent& event) {
    switch (event.type) {
        case ENET_EVENT_TYPE_CONNECT: {
            std::cout << "Client connected from " 
                      << (event.peer->address.host & 0xFF) << "."
                      << ((event.peer->address.host >> 8) & 0xFF) << "."
                      << ((event.peer->address.host >> 16) & 0xFF) << "."
                      << ((event.peer->address.host >> 24) & 0xFF) 
                      << ":" << event.peer->address.port << std::endl;
            
            connectedClients.push_back(event.peer);
            
            if (onClientConnected) {
                onClientConnected(event.peer);
            }
            break;
        }
        
        case ENET_EVENT_TYPE_DISCONNECT: {
            std::cout << "Client disconnected" << std::endl;
            
            // Remove from connected clients list
            auto it = std::find(connectedClients.begin(), connectedClients.end(), event.peer);
            if (it != connectedClients.end()) {
                connectedClients.erase(it);
            }
            
            if (onClientDisconnected) {
                onClientDisconnected(event.peer);
            }
            break;
        }
        
        case ENET_EVENT_TYPE_RECEIVE: {
            processClientMessage(event.peer, event.packet);
            enet_packet_destroy(event.packet);
            break;
        }
        
        default:
            break;
    }
}

void IntegratedServer::processClientMessage(ENetPeer* client, ENetPacket* packet) {
    if (packet->dataLength < sizeof(NetworkMessageType)) return;
    
    NetworkMessageType messageType = *(NetworkMessageType*)packet->data;
    
    switch (messageType) {
        case NetworkMessageType::PLAYER_MOVEMENT_REQUEST: {
            if (packet->dataLength >= sizeof(PlayerMovementRequest)) {
                PlayerMovementRequest request = *(PlayerMovementRequest*)packet->data;
                
                if (onPlayerMovementRequest) {
                    onPlayerMovementRequest(client, request);
                }
            }
            break;
        }
        
        default:
            std::cout << "Unknown message type from client: " << (int)messageType << std::endl;
            break;
    }
}

void IntegratedServer::broadcastHelloWorld() {
    HelloWorldMessage msg;
    strcpy(msg.message, "Hello from server!");
    
    broadcastToAllClients(&msg, sizeof(msg));
}

void IntegratedServer::broadcastPlayerPosition(uint32_t playerId, const Vec3& position, const Vec3& velocity) {
    PlayerPositionUpdate update;
    update.playerId = playerId;
    update.position = position;
    update.velocity = velocity;
    
    broadcastToAllClients(&update, sizeof(update));
}

void IntegratedServer::sendToClient(ENetPeer* client, const void* data, size_t size) {
    ENetPacket* packet = enet_packet_create(data, size, ENET_PACKET_FLAG_RELIABLE);
    enet_peer_send(client, 0, packet);
}

void IntegratedServer::broadcastToAllClients(const void* data, size_t size) {
    if (!host || connectedClients.empty()) return;
    
    ENetPacket* packet = enet_packet_create(data, size, ENET_PACKET_FLAG_RELIABLE);
    
    for (ENetPeer* client : connectedClients) {
        enet_peer_send(client, 0, packet);
    }
}
