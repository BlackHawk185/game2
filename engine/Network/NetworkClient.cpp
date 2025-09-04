#include "NetworkClient.h"

#include <cstring>
#include <iostream>
#include <vector>

#include "VoxelCompression.h"

NetworkClient::NetworkClient() : client(nullptr), serverConnection(nullptr), nextSequenceNumber(0)
{
}

NetworkClient::~NetworkClient()
{
    disconnect();

    if (client)
    {
        enet_host_destroy(client);
    }
}

bool NetworkClient::connectToServer(const std::string& host, uint16_t port)
{
    if (serverConnection)
    {
        std::cout << "Already connected to a server!" << std::endl;
        return false;
    }

    // Create client host
    if (!client)
    {
        client = enet_host_create(nullptr, 1, 2, 0, 0);
        if (!client)
        {
            std::cout << "Failed to create ENet client host!" << std::endl;
            return false;
        }
    }

    // Set up server address
    ENetAddress address;
    if (enet_address_set_host(&address, host.c_str()) < 0) {
        std::cout << "NetworkClient: Failed to resolve hostname: " << host << std::endl;
        return false;
    }
    address.port = port;

    // Connect to server
    std::cout << "NetworkClient: Attempting to connect to " << host << ":" << port << std::endl;
    serverConnection = enet_host_connect(client, &address, 2, 0);
    if (!serverConnection)
    {
        std::cout << "NetworkClient: Failed to create connection to server!" << std::endl;
        return false;
    }

    std::cout << "NetworkClient: Connection initiated (asynchronous)" << std::endl;
    return true;
    
    std::cout << "Failed to connect to server!" << std::endl;
    enet_peer_reset(serverConnection);
    serverConnection = nullptr;
    return false;
}

void NetworkClient::disconnect()
{
    if (serverConnection)
    {
        enet_peer_disconnect(serverConnection, 0);

        // Wait for disconnect to complete
        ENetEvent event;
        while (enet_host_service(client, &event, 3000) > 0)
        {
            if (event.type == ENET_EVENT_TYPE_DISCONNECT)
            {
                break;
            }
        }

        enet_peer_reset(serverConnection);
        serverConnection = nullptr;

        if (onDisconnectedFromServer)
        {
            onDisconnectedFromServer();
        }

        std::cout << "Disconnected from server" << std::endl;
    }
}

void NetworkClient::update()
{
    if (!client)
        return;

    ENetEvent event;

    // Process all pending events (non-blocking)
    while (enet_host_service(client, &event, 0) > 0)
    {
        handleServerEvent(event);
    }
}

void NetworkClient::handleServerEvent(const ENetEvent& event)
{
    switch (event.type)
    {
        case ENET_EVENT_TYPE_CONNECT:
        {
            std::cout << "Connected to server successfully!" << std::endl;
            
            if (onConnectedToServer)
            {
                onConnectedToServer();
            }
            break;
        }
        
        case ENET_EVENT_TYPE_DISCONNECT:
        {
            std::cout << "Server disconnected us" << std::endl;
            serverConnection = nullptr;

            if (onDisconnectedFromServer)
            {
                onDisconnectedFromServer();
            }
            break;
        }

        case ENET_EVENT_TYPE_RECEIVE:
        {
            processServerMessage(event.packet);
            enet_packet_destroy(event.packet);
            break;
        }

        default:
            break;
    }
}

void NetworkClient::processServerMessage(ENetPacket* packet)
{
    if (packet->dataLength < sizeof(NetworkMessageType))
        return;

    NetworkMessageType messageType = *(NetworkMessageType*) packet->data;

    switch (messageType)
    {
        case NetworkMessageType::HELLO_WORLD:
        {
            if (packet->dataLength >= sizeof(HelloWorldMessage))
            {
                HelloWorldMessage msg = *(HelloWorldMessage*) packet->data;
                std::cout << "Received from server: " << msg.message << std::endl;

                if (onHelloWorld)
                {
                    onHelloWorld(msg);
                }
            }
            break;
        }

        case NetworkMessageType::PLAYER_POSITION_UPDATE:
        {
            if (packet->dataLength >= sizeof(PlayerPositionUpdate))
            {
                PlayerPositionUpdate update = *(PlayerPositionUpdate*) packet->data;

                if (onPlayerPositionUpdate)
                {
                    onPlayerPositionUpdate(update);
                }
            }
            break;
        }

        case NetworkMessageType::COMPRESSED_CHUNK_DATA:
        {
            if (packet->dataLength >= sizeof(CompressedChunkHeader))
            {
                CompressedChunkHeader header = *(CompressedChunkHeader*) packet->data;

                // The compressed data starts after the header
                const uint8_t* compressedData =
                    reinterpret_cast<const uint8_t*>(packet->data) + sizeof(CompressedChunkHeader);
                size_t availableDataSize =
                    static_cast<size_t>(packet->dataLength) - sizeof(CompressedChunkHeader);

                if (availableDataSize >= static_cast<size_t>(header.compressedSize))
                {
                    // Decompress the voxel data using LZ4
                    std::vector<uint8_t> decompressedData(static_cast<size_t>(header.originalSize));

                    if (VoxelCompression::decompressLZ4(compressedData, header.compressedSize,
                                                        decompressedData.data(),
                                                        header.originalSize))
                    {
                        if (onCompressedChunkReceived)
                        {
                            onCompressedChunkReceived(header.islandID, header.chunkCoord, header.islandPosition,
                                                      decompressedData.data(), header.originalSize);
                        }
                    }
                    else
                    {
                        std::cerr << "Failed to decompress chunk (" << header.chunkCoord.x 
                                  << "," << header.chunkCoord.y << "," << header.chunkCoord.z 
                                  << ") from island " << header.islandID << std::endl;
                    }
                }
                else
                {
                    std::cerr << "Incomplete chunk data packet. Expected: "
                              << header.compressedSize << ", Available: " << availableDataSize
                              << std::endl;
                }
            }
            break;
        }

        case NetworkMessageType::ENTITY_STATE_UPDATE:
        {
            if (packet->dataLength >= sizeof(EntityStateUpdate))
            {
                EntityStateUpdate update = *(EntityStateUpdate*) packet->data;

                if (onEntityStateUpdate)
                {
                    onEntityStateUpdate(update);
                }
            }
            break;
        }

        default:
            std::cout << "Unknown message type from server: " << (int) messageType << std::endl;
            break;
    }
}

void NetworkClient::sendToServer(const void* data, size_t size)
{
    if (!serverConnection)
        return;

    ENetPacket* packet = enet_packet_create(data, size, ENET_PACKET_FLAG_RELIABLE);
    enet_peer_send(serverConnection, 0, packet);
}

void NetworkClient::sendMovementRequest(const Vec3& intendedPosition, const Vec3& velocity, float deltaTime)
{
    PlayerMovementRequest request;
    request.sequenceNumber = nextSequenceNumber++;
    request.intendedPosition = intendedPosition;
    request.velocity = velocity;
    request.deltaTime = deltaTime;
    
    sendToServer(&request, sizeof(request));
}
