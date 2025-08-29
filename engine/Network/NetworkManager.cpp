#include "NetworkManager.h"

#include <enet/enet.h>

#include <iostream>

#include "NetworkMessages.h"

#include "../Physics/PhysicsSystem.h"

NetworkManager::NetworkManager() : isNetworkingEnabled(false)
{
    // Create integrated server and client
    server = std::make_unique<IntegratedServer>();
    client = std::make_unique<NetworkClient>();

    // Set up client event handlers
    client->onConnectedToServer = [this]()
    {
        // Removed verbose debug output
    };

    client->onDisconnectedFromServer = [this]()
    { std::cout << "Disconnected from the game" << std::endl; };

    client->onHelloWorld = [this](const HelloWorldMessage& msg)
    { std::cout << "Server says: " << msg.message << std::endl; };

    client->onPlayerPositionUpdate = [this](const PlayerPositionUpdate& update)
    {
        // Debug: Uncomment for position update debugging
        // std::cout << "Player " << update.playerId << " moved to: "
        //           << update.position.x << ", "
        //           << update.position.y << ", "
        //           << update.position.z << std::endl;
    };
}

NetworkManager::~NetworkManager()
{
    stopHosting();
    leaveServer();
}

bool NetworkManager::initializeNetworking()
{
    if (enet_initialize() != 0)
    {
        std::cout << "Failed to initialize ENet!" << std::endl;
        return false;
    }
    // Removed verbose debug output
    return true;
}

void NetworkManager::shutdownNetworking()
{
    enet_deinitialize();
    std::cout << "ENet shut down" << std::endl;
}

bool NetworkManager::startHosting(uint16_t port)
{
    if (!server)
    {
        server = std::make_unique<IntegratedServer>();
    }

    bool success = server->startServer(port);
    if (success)
    {
        isNetworkingEnabled = true;

        // Set up basic callbacks
        server->onClientConnected = [](ENetPeer* peer)
        {
            // Removed verbose debug output
        };

        server->onClientDisconnected = [](ENetPeer* peer)
        { std::cout << "Player left the game" << std::endl; };

        server->onPlayerMovementRequest =
            [this](ENetPeer* peer, const PlayerMovementRequest& request)
        {
            // Debug: Uncomment for movement debugging
            // std::cout << "[SERVER] Player movement request: (" << request.intendedPosition.x << ", "
            //           << request.intendedPosition.y << ", " << request.intendedPosition.z
            //           << ") (velocity: (" << request.velocity.x << ", " << request.velocity.y
            //           << ", " << request.velocity.z << "))" << std::endl;

            // Validate movement against collision
            Vec3 collisionNormal;
            const float PLAYER_RADIUS = 0.5f;

            if (g_physics.checkPlayerCollision(request.intendedPosition, collisionNormal,
                                               PLAYER_RADIUS))
            {
                // Debug: Uncomment for collision debugging
                // std::cout
                //     << "[SERVER] Collision detected, applying friction-based response (normal: ("
                //     << collisionNormal.x << ", " << collisionNormal.y << ", " << collisionNormal.z
                //     << "))" << std::endl;

                // Apply friction-based collision response instead of instant stop
                const float FRICTION_COEFFICIENT =
                    0.3f;  // Friction factor (0 = no friction, 1 = full stop)
                Vec3 frictionVelocity = request.velocity * (1.0f - FRICTION_COEFFICIENT);

                // Project velocity onto collision plane to prevent penetration
                float velocityDotNormal = request.velocity.dot(collisionNormal);
                if (velocityDotNormal < 0)
                {
                    // Only apply friction if moving towards the collision surface
                    Vec3 tangentialVelocity =
                        request.velocity - collisionNormal * velocityDotNormal;
                    frictionVelocity = tangentialVelocity * (1.0f - FRICTION_COEFFICIENT);
                }

                // Send corrected position with friction-applied velocity
                this->broadcastPlayerPosition(0, request.intendedPosition, frictionVelocity);
            }
            else
            {
                // Debug: Uncomment for movement debugging
                // std::cout << "[SERVER] Movement validated, broadcasting to clients" << std::endl;
                // Movement is valid, broadcast to all clients
                this->broadcastPlayerPosition(0, request.intendedPosition, request.velocity);
            }
        };

        // Voxel changes are now handled by GameServer directly via its own callback
    }

    return success;
}

void NetworkManager::stopHosting()
{
    if (server)
    {
        server->stopServer();
        isNetworkingEnabled = false;
    }
}

bool NetworkManager::joinServer(const std::string& host, uint16_t port)
{
    if (!client)
    {
        client = std::make_unique<NetworkClient>();
    }

    bool success = client->connectToServer(host, port);
    if (success)
    {
        isNetworkingEnabled = true;

        // Set up basic callbacks
        client->onConnectedToServer = []()
        { std::cout << "Successfully joined the game!" << std::endl; };

        client->onDisconnectedFromServer = []()
        { std::cout << "Disconnected from the game" << std::endl; };

        client->onHelloWorld = [](const HelloWorldMessage& msg)
        { std::cout << "Server says: " << msg.message << std::endl; };

        client->onPlayerPositionUpdate = [](const PlayerPositionUpdate& update)
        {
            // Debug: Uncomment for position update debugging
            // std::cout << "Player " << update.playerId << " moved to: "
            //           << update.position.x << ", "
            //           << update.position.y << ", "
            //           << update.position.z << std::endl;
        };
    }

    return success;
}

void NetworkManager::leaveServer()
{
    if (client)
    {
        client->disconnect();
        isNetworkingEnabled = false;
    }
}

void NetworkManager::update()
{
    if (server && server->isRunning())
    {
        server->update();
    }

    if (client && client->isConnected())
    {
        client->update();
    }
}

void NetworkManager::broadcastHelloWorld()
{
    if (server && server->isRunning())
    {
        server->broadcastHelloWorld();
    }
}

void NetworkManager::sendPlayerMovement(const Vec3& intendedPosition, const Vec3& velocity,
                                        float deltaTime)
{
    if (client && client->isConnected())
    {
        client->sendMovementRequest(intendedPosition, velocity, deltaTime);
    }
}

void NetworkManager::broadcastPlayerPosition(uint32_t playerId, const Vec3& position,
                                             const Vec3& velocity)
{
    if (server && server->isRunning())
    {
        server->broadcastPlayerPosition(playerId, position, velocity);
    }
}
