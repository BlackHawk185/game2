// GameServer.cpp - Headless game server implementation
#include "GameServer.h"

#include "pch.h"
#include "Profiler.h"

#include "../Network/NetworkMessages.h"
#include "../World/VoxelChunk.h"  // For accessing voxel data

GameServer::GameServer()
{
    // Constructor
    m_networkManager = std::make_unique<NetworkManager>();  // Re-enabled with ENet
}

GameServer::~GameServer()
{
    shutdown();
}

bool GameServer::initialize(float targetTickRate, bool enableNetworking, uint16_t networkPort)
{
    // Removed verbose debug output

    m_targetTickRate = targetTickRate;
    m_fixedDeltaTime = 1.0f / targetTickRate;
    m_networkingEnabled = enableNetworking;

    // Initialize time manager
    m_timeManager = std::make_unique<TimeManager>();

    // Initialize game state
    m_gameState = std::make_unique<GameState>();
    if (!m_gameState->initialize(true))
    {  // Create default world
        std::cerr << "Failed to initialize game state!" << std::endl;
        return false;
    }

    // Log island generation mode once (noise is now default)
    // Connect physics system to island system for server-side collision detection
    g_physics.setIslandSystem(m_gameState->getIslandSystem());

    // Initialize networking if requested
    if (m_networkingEnabled)
    {
        if (!m_networkManager->initializeNetworking())
        {
            std::cerr << "Failed to initialize networking!" << std::endl;
            return false;
        }

        if (!m_networkManager->startHosting(networkPort))
        {
            std::cerr << "Failed to start network server on port " << networkPort << std::endl;
            return false;
        }

        // Set up callback to send world state to new clients
        if (auto server = m_networkManager->getServer())
        {
            server->onClientConnected = [this](ENetPeer* peer)
            {
                // Removed verbose debug output
                sendWorldStateToClient(peer);
            };

            server->onVoxelChangeRequest = [this](ENetPeer* peer, const VoxelChangeRequest& request)
            { this->handleVoxelChangeRequest(peer, request); };
        }

        // Removed verbose debug output
    }

    // Removed verbose debug output

    return true;
}

void GameServer::run()
{
    if (m_running.load())
    {
        std::cerr << "Server is already running!" << std::endl;
        return;
    }

    // Removed verbose debug output
    m_running.store(true);

    serverLoop();
}

void GameServer::runAsync()
{
    if (m_running.load())
    {
        std::cerr << "Server is already running!" << std::endl;
        return;
    }

    // Removed verbose debug output
    m_running.store(true);

    m_serverThread = std::make_unique<std::thread>(&GameServer::serverLoop, this);
}

void GameServer::stop()
{
    if (!m_running.load())
    {
        return;
    }

    std::cout << "⏹️  Stopping GameServer..." << std::endl;
    m_running.store(false);

    // Wait for server thread to finish
    if (m_serverThread && m_serverThread->joinable())
    {
        m_serverThread->join();
        m_serverThread.reset();
    }
}

void GameServer::shutdown()
{
    stop();

    // Clear command queues
    m_pendingVoxelChanges.clear();
    m_pendingPlayerMovements.clear();

    // Shutdown systems
    if (m_gameState)
    {
        m_gameState->shutdown();
        m_gameState.reset();
    }

    m_timeManager.reset();
}

void GameServer::queueVoxelChange(uint32_t islandID, const Vec3& localPos, uint8_t voxelType)
{
    // TODO: Add proper thread-safe queue implementation
    // For now, just apply directly (not thread-safe!)
    VoxelChangeCommand cmd;
    cmd.islandID = islandID;
    cmd.localPos = localPos;
    cmd.voxelType = voxelType;
    m_pendingVoxelChanges.push_back(cmd);
}

void GameServer::queuePlayerMovement(const Vec3& movement)
{
    // TODO: Add proper thread-safe queue implementation
    PlayerMovementCommand cmd;
    cmd.movement = movement;
    m_pendingPlayerMovements.push_back(cmd);
}

void GameServer::serverLoop()
{
    PROFILE_SCOPE("GameServer::serverLoop");
    
    auto lastTime = std::chrono::high_resolution_clock::now();
    float accumulator = 0.0f;

    // Removed verbose debug output

    while (m_running.load())
    {
        PROFILE_SCOPE("Server main loop iteration");
        
        auto currentTime = std::chrono::high_resolution_clock::now();
        float deltaTime = std::chrono::duration<float>(currentTime - lastTime).count();
        lastTime = currentTime;

        // Clamp delta time to prevent spiral of death
        if (deltaTime > 0.25f)
        {
            deltaTime = 0.25f;
        }

        accumulator += deltaTime;

        // Fixed timestep simulation
        while (accumulator >= m_fixedDeltaTime)
        {
            PROFILE_SCOPE("Fixed timestep tick");
            processTick(m_fixedDeltaTime);
            accumulator -= m_fixedDeltaTime;
            m_totalTicks++;
        }

        // Update tick rate statistics
        {
            PROFILE_SCOPE("updateTickRateStats");
            updateTickRateStats(deltaTime);
        }

        // Update profiler (will auto-report every second)
        g_profiler.updateAndReport();

        // Sleep to avoid consuming 100% CPU
        // Target is to wake up several times per fixed timestep for responsiveness
        std::this_thread::sleep_for(std::chrono::microseconds(1000));  // 1ms
    }

    // Removed verbose debug output
}

void GameServer::processTick(float deltaTime)
{
    PROFILE_SCOPE("GameServer::processTick");
    
    // Process queued commands first
    {
        PROFILE_SCOPE("processQueuedCommands");
        processQueuedCommands();
    }

    // Update networking
    if (m_networkingEnabled && m_networkManager)
    {
        PROFILE_SCOPE("NetworkManager::update");
        m_networkManager->update();
    }

    // Update time manager
    if (m_timeManager)
    {
        PROFILE_SCOPE("TimeManager::update");
        m_timeManager->update(deltaTime);
    }

    // Update game simulation
    if (m_gameState)
    {
        PROFILE_SCOPE("GameState::updateSimulation");
        m_gameState->updateSimulation(deltaTime);
    }

    // Broadcast island state updates to clients
    if (m_networkingEnabled && m_networkManager)
    {
        PROFILE_SCOPE("broadcastIslandStates");
        broadcastIslandStates();
    }
}

void GameServer::processQueuedCommands()
{
    // Process voxel changes
    for (const auto& cmd : m_pendingVoxelChanges)
    {
        if (m_gameState)
        {
            m_gameState->setVoxel(cmd.islandID, cmd.localPos, cmd.voxelType);
        }
    }
    m_pendingVoxelChanges.clear();

    // Process player movements
    for (const auto& cmd : m_pendingPlayerMovements)
    {
        if (m_gameState)
        {
            m_gameState->applyPlayerMovement(cmd.movement, m_fixedDeltaTime);
        }
    }
    m_pendingPlayerMovements.clear();
}

void GameServer::updateTickRateStats(float actualDeltaTime)
{
    // Simple moving average for tick rate calculation
    static float tickRateAccumulator = 0.0f;
    static int tickRateSamples = 0;

    if (actualDeltaTime > 0.0f)
    {
        tickRateAccumulator += 1.0f / actualDeltaTime;
        tickRateSamples++;

        // Update every 60 samples (~1 second)
        if (tickRateSamples >= 60)
        {
            m_currentTickRate = tickRateAccumulator / tickRateSamples;
            tickRateAccumulator = 0.0f;
            tickRateSamples = 0;
        }
    }
}

void GameServer::sendWorldStateToClient(ENetPeer* peer)
{
    if (!m_gameState || !m_networkManager)
    {
        std::cerr << "Cannot send world state: missing game state or network manager" << std::endl;
        return;
    }

    auto server = m_networkManager->getServer();
    if (!server)
    {
        std::cerr << "No server instance available" << std::endl;
        return;
    }

    // Get island system from game state
    auto* islandSystem = m_gameState->getIslandSystem();
    if (!islandSystem)
    {
        std::cerr << "No island system available" << std::endl;
        return;
    }

    // Create world state message from current game state
    WorldStateMessage worldState;
    worldState.numIslands = 3;  // We know we have 3 islands from createDefaultWorld

    // Get actual island positions from the island system
    const std::vector<uint32_t>& islandIDs = m_gameState->getAllIslandIDs();
    for (int i = 0; i < 3 && i < islandIDs.size(); i++)
    {
        Vec3 islandCenter = islandSystem->getIslandCenter(islandIDs[i]);
        worldState.islandPositions[i] = islandCenter;
    }

    // Set player spawn position (center of main island)
    worldState.playerSpawnPosition =
        Vec3(16.0f, 16.0f, 16.0f);  // Updated to match island generation

    // Removed verbose debug output

    // Send basic world state first
    server->sendWorldStateToClient(peer, worldState);

    // Now send compressed voxel data for each island
    for (int i = 0; i < 3 && i < islandIDs.size(); i++)
    {
        const FloatingIsland* island = islandSystem->getIsland(islandIDs[i]);
        if (island)
        {
            // Send all chunks for this island
            for (const auto& [chunkCoord, chunk] : island->chunks)
            {
                if (chunk)
                {
                    const uint8_t* voxelData = chunk->getRawVoxelData();
                    uint32_t voxelDataSize = chunk->getVoxelDataSize();

                    // Use the new sendCompressedChunkToClient method with chunk coordinates
                    server->sendCompressedChunkToClient(peer, islandIDs[i], chunkCoord, worldState.islandPositions[i], voxelData, voxelDataSize);
                }
            }
        }
    }
}

void GameServer::handleVoxelChangeRequest(ENetPeer* peer, const VoxelChangeRequest& request)
{
    // Removed verbose debug output

    if (!m_gameState)
    {
        std::cerr << "Cannot handle voxel change: no game state!" << std::endl;
        return;
    }

    // Apply the voxel change to the server's authoritative game state
    m_gameState->setVoxel(request.islandID, request.localPos, request.voxelType);

    // Broadcast the change to all connected clients (including the sender for confirmation)
    if (auto server = m_networkManager->getServer())
    {
        server->broadcastVoxelChange(request.islandID, request.localPos, request.voxelType, 0);
    }
}

void GameServer::broadcastIslandStates()
{
    if (!m_gameState || !m_networkManager)
    {
        return;
    }

    auto server = m_networkManager->getServer();
    if (!server)
    {
        return;
    }

    auto* islandSystem = m_gameState->getIslandSystem();
    if (!islandSystem)
    {
        return;
    }

    // Broadcast state for all islands at a reasonable frequency (e.g., 10Hz for smooth movement)
    static float lastBroadcastTime = 0.0f;
    static int broadcastCount = 0;
    float currentTime = m_timeManager ? m_timeManager->getRealTime() : 0.0f;

    if (currentTime - lastBroadcastTime < 0.1f)
    {  // 10Hz update rate
        return;
    }
    lastBroadcastTime = currentTime;
    broadcastCount++;

    // Get all island IDs and broadcast their current states
    const std::vector<uint32_t>& islandIDs = m_gameState->getAllIslandIDs();
    uint32_t serverTimestamp =
        static_cast<uint32_t>(currentTime * 1000.0f);  // Convert to milliseconds

    // Debug: Print broadcast info every 30 broadcasts (3 seconds at 10Hz)
    if (broadcastCount % 30 == 1)
    {
        // Removed verbose debug output
    }

    for (uint32_t islandID : islandIDs)
    {
        const FloatingIsland* island = islandSystem->getIsland(islandID);
        if (!island)
            continue;

        // Create EntityStateUpdate for this island
        EntityStateUpdate update;
        update.sequenceNumber = static_cast<uint32_t>(
            m_totalTicks);  // Use tick count as sequence (truncated to 32-bit for network)
        update.entityID = islandID;
        update.entityType = 1;  // 1 = Island (as defined in NetworkMessages.h)
        update.position = island->physicsCenter;
        update.velocity = island->velocity;
        update.acceleration = island->acceleration;
        update.serverTimestamp = serverTimestamp;
        update.flags = 0;  // No special flags for islands

        // Removed verbose debug output for individual island broadcasts

        // Broadcast to all connected clients
        server->broadcastEntityState(update);
    }
}

