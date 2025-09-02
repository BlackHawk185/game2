// GameClient.cpp - Client-side rendering and input implementation
#include "GameClient.h"

#include <GLFW/glfw3.h>
#include <glad/gl.h>
#include <imgui.h>

#include <iostream>
#include <memory>

#include "GameState.h"
#include "Profiler.h"

#include "../Network/NetworkManager.h"
#include "../Network/NetworkMessages.h"
#include "../Rendering/Renderer.h"
#include "../Rendering/VBORenderer.h"  // RE-ENABLED
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include "../Time/TimeEffects.h"
#include "../World/VoxelChunk.h"  // For accessing voxel data

// External systems that we'll refactor later
extern TimeEffects* g_timeEffects;

GameClient::GameClient()
{
    // Constructor
    m_networkManager = std::make_unique<NetworkManager>();  // Re-enabled with ENet

    // Set up network callbacks
    if (auto client = m_networkManager->getClient())
    {
        client->onWorldStateReceived = [this](const WorldStateMessage& worldState)
        { this->handleWorldStateReceived(worldState); };

        client->onCompressedIslandReceived = [this](uint32_t islandID, const Vec3& position,
                                                    const uint8_t* voxelData, uint32_t dataSize)
        { this->handleCompressedIslandReceived(islandID, position, voxelData, dataSize); };

        client->onCompressedChunkReceived = [this](uint32_t islandID, const Vec3& chunkCoord, const Vec3& islandPosition,
                                                   const uint8_t* voxelData, uint32_t dataSize)
        { this->handleCompressedChunkReceived(islandID, chunkCoord, islandPosition, voxelData, dataSize); };

        client->onVoxelChangeReceived = [this](const VoxelChangeUpdate& update)
        { this->handleVoxelChangeReceived(update); };

        client->onEntityStateUpdate = [this](const EntityStateUpdate& update)
        { this->handleEntityStateUpdate(update); };
    }
}

GameClient::~GameClient()
{
    shutdown();
}

bool GameClient::initialize()
{
    if (m_initialized)
    {
        std::cerr << "GameClient already initialized!" << std::endl;
        return false;
    }

    // Removed verbose debug output

    // Initialize window and graphics
    if (!initializeWindow())
    {
        return false;
    }

    if (!initializeGraphics())
    {
        return false;
    }

    m_initialized = true;
    // Removed verbose debug output
    return true;
}

bool GameClient::connectToGameState(GameState* gameState)
{
    if (!gameState)
    {
        std::cerr << "Cannot connect to null game state!" << std::endl;
        return false;
    }

    m_gameState = gameState;
    m_isRemoteClient = false;  // Local connection

    // **NEW: Connect physics system to island system for collision detection**
    if (gameState && gameState->getIslandSystem())
    {
        g_physics.setIslandSystem(gameState->getIslandSystem());
    }

    // Set up camera position based on game state
    if (m_gameState->getPrimaryPlayer())
    {
        Vec3 playerPos = m_gameState->getPrimaryPlayer()->getPosition();
        m_camera.position = playerPos;
    }

    // Removed verbose debug output
    return true;
}

bool GameClient::connectToRemoteServer(const std::string& serverAddress, uint16_t serverPort)
{
    if (!m_networkManager)
    {
        std::cerr << "Network manager not initialized!" << std::endl;
        return false;
    }

    // Initialize networking
    if (!m_networkManager->initializeNetworking())
    {
        std::cerr << "Failed to initialize networking!" << std::endl;
        return false;
    }

    // Removed verbose debug output

    // Connect to remote server
    if (!m_networkManager->joinServer(serverAddress, serverPort))
    {
        std::cerr << "Failed to connect to remote server!" << std::endl;
        return false;
    }

    m_isRemoteClient = true;

    // Initial world state will be received from server after handshake protocol
    // For now, create a minimal local state for rendering
    // This will be replaced by data received from server

    // Removed verbose debug output
    return true;
}

bool GameClient::update(float deltaTime)
{
    PROFILE_SCOPE("GameClient::update");
    
    if (!m_initialized)
    {
        return false;
    }

    // Poll window events
    {
        PROFILE_SCOPE("glfwPollEvents");
        glfwPollEvents();
    }

    // Check if window should close
    if (shouldClose())
    {
        return false;
    }

    // Update networking if remote client
    if (m_isRemoteClient && m_networkManager)
    {
        PROFILE_SCOPE("NetworkManager::update");
        m_networkManager->update();
    }

    // Update client-side physics for smooth island movement
    if (m_gameState)
    {
        PROFILE_SCOPE("Island physics update");
        auto* islandSystem = m_gameState->getIslandSystem();
        if (islandSystem)
        {
            // Run client-side island physics between server updates
            // This provides smooth movement using server-provided velocities
            islandSystem->updateIslandPhysics(deltaTime);
        }
    }

    // Process input
    {
        PROFILE_SCOPE("processInput");
        processInput(deltaTime);
    }

    // Render frame
    {
        PROFILE_SCOPE("render");
        render();
    }

    // Swap buffers
    {
        PROFILE_SCOPE("glfwSwapBuffers");
        glfwSwapBuffers(m_window);
    }

    // Update profiler (will auto-report every second)
    g_profiler.updateAndReport();

    return true;
}

void GameClient::shutdown()
{
    if (!m_initialized)
    {
        return;
    }

    // Removed verbose debug output

    // Disconnect from game state
    m_gameState = nullptr;

    // Cleanup VBO renderer
    // RE-ENABLED FOR TESTING
    if (g_vboRenderer)
    {
        g_vboRenderer->shutdown();
        delete g_vboRenderer;
        g_vboRenderer = nullptr;
        std::cout << "VBO renderer shutdown" << std::endl;
    }

    // Cleanup graphics
    if (m_window)
    {
        glfwDestroyWindow(m_window);
        m_window = nullptr;
    }

    glfwTerminate();

    m_initialized = false;
    // Removed verbose debug output
}

bool GameClient::shouldClose() const
{
    return m_window && glfwWindowShouldClose(m_window);
}

void GameClient::processInput(float deltaTime)
{
    if (!m_window)
    {
        return;
    }

    processKeyboard(deltaTime);
    processMouse(deltaTime);

    // Only process block interaction if we have a local game state
    if (m_gameState)
    {
        processBlockInteraction(deltaTime);
    }
}

void GameClient::render()
{
    PROFILE_SCOPE("GameClient::render");
    
    // Clear screen
    {
        PROFILE_SCOPE("Renderer::clear");
        Renderer::clear();
    }

    // Set up 3D projection
    {
        PROFILE_SCOPE("Setup 3D projection");
        
        float aspect = (float) m_windowWidth / (float) m_windowHeight;
        float fov = 45.0f;
        float nearPlane = 0.1f;
        float farPlane = 1000.0f;

        // Create modern projection matrix (glm expects radians)
        glm::mat4 projectionMatrix = glm::perspective(fov * 3.14159265358979323846f / 180.0f, aspect, nearPlane, farPlane);
        glm::mat4 viewMatrix = m_camera.getViewMatrix();
        
        // Set matrices for VBO renderer (modern OpenGL)
        if (g_vboRenderer) {
            g_vboRenderer->setProjectionMatrix(projectionMatrix);
            g_vboRenderer->setViewMatrix(viewMatrix);
        }
        
        // Update frustum culling
        m_frustumCuller.updateFromCamera(m_camera, aspect, 45.0f);
    }

    // Render world (only if we have local game state)
    if (m_gameState)
    {
        PROFILE_SCOPE("renderWorld");
        renderWorld();
    }
    else if (m_isRemoteClient)
    {
        // Render waiting screen for remote clients
        PROFILE_SCOPE("renderWaitingScreen");
        renderWaitingScreen();
    }

    // Render UI
    {
        PROFILE_SCOPE("renderUI");
        renderUI();
    }
}

bool GameClient::initializeWindow()
{
    if (!glfwInit())
    {
        std::cerr << "Failed to initialize GLFW!" << std::endl;
        return false;
    }

    // Request OpenGL 4.6 Core context
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, GLFW_TRUE);

    // Create window
    m_window =
        glfwCreateWindow(m_windowWidth, m_windowHeight, "MMORPG Engine - Client", nullptr, nullptr);
    if (!m_window)
    {
        std::cerr << "Failed to create GLFW window!" << std::endl;
        glfwTerminate();
        return false;
    }

    glfwMakeContextCurrent(m_window);

    // Load OpenGL via glad2
    if (gladLoadGL(glfwGetProcAddress) == 0)
    {
        std::cerr << "Failed to initialize glad (OpenGL 4.6)" << std::endl;
        glfwDestroyWindow(m_window);
        glfwTerminate();
        return false;
    }

    // Set callbacks
    glfwSetWindowUserPointer(m_window, this);
    glfwSetWindowSizeCallback(m_window, windowResizeCallback);

    // Set up mouse capture
    glfwSetInputMode(m_window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

    return true;
}

bool GameClient::initializeGraphics()
{
    // Initialize renderer
    if (!Renderer::initialize())
    {
        std::cerr << "Failed to initialize renderer!" << std::endl;
        return false;
    }

    // Initialize VBO renderer for modern OpenGL rendering
    // RE-ENABLED FOR TESTING
    g_vboRenderer = new VBORenderer();
    if (!g_vboRenderer->initialize())
    {
        std::cerr << "Failed to initialize VBO renderer!" << std::endl;
        delete g_vboRenderer;
        g_vboRenderer = nullptr;
        return false;
    }
    std::cout << "VBO renderer initialized successfully" << std::endl;

    // Initialize ImGui
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    (void) io;
    ImGui::StyleColorsDark();

    return true;
}

void GameClient::processKeyboard(float deltaTime)
{
    // Camera movement
    float moveSpeed = 10.0f;
    Vec3 movement(0, 0, 0);

    if (glfwGetKey(m_window, GLFW_KEY_W) == GLFW_PRESS)
    {
        movement = movement + m_camera.front * moveSpeed * deltaTime;
    }
    if (glfwGetKey(m_window, GLFW_KEY_S) == GLFW_PRESS)
    {
        movement = movement - m_camera.front * moveSpeed * deltaTime;
    }
    if (glfwGetKey(m_window, GLFW_KEY_A) == GLFW_PRESS)
    {
        movement = movement - m_camera.right * moveSpeed * deltaTime;
    }
    if (glfwGetKey(m_window, GLFW_KEY_D) == GLFW_PRESS)
    {
        movement = movement + m_camera.right * moveSpeed * deltaTime;
    }
    if (glfwGetKey(m_window, GLFW_KEY_SPACE) == GLFW_PRESS)
    {
        movement = movement + m_camera.up * moveSpeed * deltaTime;
    }
    if (glfwGetKey(m_window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS)
    {
        movement = movement - m_camera.up * moveSpeed * deltaTime;
    }

    // **NEW: Collision Detection**
    Vec3 intendedPosition = m_camera.position + movement;
    const float PLAYER_RADIUS = 0.5f;  // Player collision radius

    // Verbose per-frame movement logging disabled

    // Check for collision with islands
    Vec3 collisionNormal;
    bool hasCollision = false;

    if (g_physics.checkPlayerCollision(intendedPosition, collisionNormal, PLAYER_RADIUS))
    {
        // Initial collision detected; suppress verbose log
        // Collision detected - implement friction-based sliding collision
        const float FRICTION_COEFFICIENT =
            0.3f;  // Friction factor (0 = no friction, 1 = full stop)

        // Project movement onto the collision plane
        float dotProduct = movement.dot(collisionNormal);
        Vec3 slideMovement = movement - collisionNormal * dotProduct;

        // Apply friction to the sliding movement
        slideMovement *= (1.0f - FRICTION_COEFFICIENT);

        // Apply sliding movement with friction
        intendedPosition = m_camera.position + slideMovement;

        // Check if sliding movement also collides
        if (g_physics.checkPlayerCollision(intendedPosition, collisionNormal, PLAYER_RADIUS))
        {
            // Sliding also collides; suppress verbose log
            // If sliding also collides, apply stronger friction instead of blocking completely
            const float STRONG_FRICTION = 0.7f;
            Vec3 strongFrictionMovement = movement * (1.0f - STRONG_FRICTION);
            intendedPosition = m_camera.position + strongFrictionMovement;
        }
        else
        {
            // Slide movement succeeded; suppress verbose log
        }

        hasCollision = true;
    }
    else
    {
        // No collision; suppress verbose log
    }

    // Apply movement (with collision response)
    m_camera.position = intendedPosition;

    // Update player position in game state if local
    if (m_gameState && m_gameState->getPrimaryPlayer())
    {
        m_gameState->setPrimaryPlayerPosition(m_camera.position);
    }

    // Send movement to server if we're a remote client
    if (m_isRemoteClient && m_networkManager && movement.length() > 0.001f)
    {
        Vec3 velocity = movement / deltaTime;  // Calculate velocity
        m_networkManager->sendPlayerMovement(m_camera.position, velocity, deltaTime);
    }

    // Time effects (temporary, will be refactored)
    if (g_timeEffects)
    {
        // TODO: Implement proper time effect methods
        // The actual method names need to be checked in TimeEffects.h
        /*
        if (glfwGetKey(m_window, GLFW_KEY_1) == GLFW_PRESS) g_timeEffects->setSlowMotion();
        if (glfwGetKey(m_window, GLFW_KEY_2) == GLFW_PRESS) g_timeEffects->setBulletTime();
        if (glfwGetKey(m_window, GLFW_KEY_3) == GLFW_PRESS) g_timeEffects->setTimeFreeze();
        if (glfwGetKey(m_window, GLFW_KEY_4) == GLFW_PRESS) g_timeEffects->setSpeedUp();
        if (glfwGetKey(m_window, GLFW_KEY_5) == GLFW_PRESS) g_timeEffects->setTimeReverse();
        if (glfwGetKey(m_window, GLFW_KEY_0) == GLFW_PRESS) g_timeEffects->resetTime();
        if (glfwGetKey(m_window, GLFW_KEY_T) == GLFW_PRESS) g_timeEffects->toggleTimePulse();
        */
    }

    // Exit
    if (glfwGetKey(m_window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
    {
        glfwSetWindowShouldClose(m_window, GLFW_TRUE);
    }
}

void GameClient::processMouse(float deltaTime)
{
    static double lastX = m_windowWidth / 2.0;
    static double lastY = m_windowHeight / 2.0;
    static bool firstMouse = true;

    double xpos, ypos;
    glfwGetCursorPos(m_window, &xpos, &ypos);

    if (firstMouse)
    {
        lastX = xpos;
        lastY = ypos;
        firstMouse = false;
    }

    float xoffset = (float) (xpos - lastX);
    float yoffset = (float) (lastY - ypos);  // Reversed since y-coordinates go from bottom to top
    lastX = xpos;
    lastY = ypos;

    float sensitivity = 0.1f;
    xoffset *= sensitivity;
    yoffset *= sensitivity;

    m_camera.yaw += xoffset;
    m_camera.pitch += yoffset;

    // Constrain pitch
    if (m_camera.pitch > 89.0f)
        m_camera.pitch = 89.0f;
    if (m_camera.pitch < -89.0f)
        m_camera.pitch = -89.0f;

    // Update camera vectors after changing yaw/pitch - this was missing!
    m_camera.updateCameraVectors();
    // We need to manually call this since we're directly modifying yaw/pitch
    // TODO: Refactor to use Camera's processInput() method instead of duplicating logic
    // m_camera.updateCameraVectors();
}

void GameClient::processBlockInteraction(float deltaTime)
{
    if (!m_gameState)
    {
        return;
    }

    // Update raycast timer for performance
    m_inputState.raycastTimer += deltaTime;
    if (m_inputState.raycastTimer > 0.05f)
    {  // 20 FPS raycasting for more responsive block selection
        m_inputState.cachedTargetBlock = VoxelRaycaster::raycast(
            m_camera.position, m_camera.front, 50.0f, m_gameState->getIslandSystem());
        m_inputState.raycastTimer = 0.0f;
    }

    bool leftClick = glfwGetMouseButton(m_window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;
    bool rightClick = glfwGetMouseButton(m_window, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS;

    // Left click - break block
    if (leftClick && !m_inputState.leftMousePressed)
    {
        m_inputState.leftMousePressed = true;

        if (m_inputState.cachedTargetBlock.hit)
        {
            // Send network request for block break
            if (m_networkManager && m_networkManager->getClient() &&
                m_networkManager->getClient()->isConnected())
            {
                m_networkManager->getClient()->sendVoxelChangeRequest(
                    m_inputState.cachedTargetBlock.islandID,
                    m_inputState.cachedTargetBlock.localBlockPos, 0);
            }

            // Apply optimistically for immediate visual feedback
            m_gameState->setVoxel(m_inputState.cachedTargetBlock.islandID,
                                  m_inputState.cachedTargetBlock.localBlockPos, 0);

            // Clear the cached target block immediately to remove the yellow outline
            m_inputState.cachedTargetBlock = RayHit();

            // **FIXED**: Force immediate raycast to update block selection after breaking
            m_inputState.cachedTargetBlock = VoxelRaycaster::raycast(
                m_camera.position, m_camera.front, 50.0f, m_gameState->getIslandSystem());
            m_inputState.raycastTimer = 0.0f;
        }
    }
    else if (!leftClick)
    {
        m_inputState.leftMousePressed = false;
    }

    // Right click - place block
    if (rightClick && !m_inputState.rightMousePressed)
    {
        m_inputState.rightMousePressed = true;

        if (m_inputState.cachedTargetBlock.hit)
        {
            Vec3 placePos = VoxelRaycaster::getPlacementPosition(m_inputState.cachedTargetBlock);
            uint8_t existingVoxel =
                m_gameState->getVoxel(m_inputState.cachedTargetBlock.islandID, placePos);

            if (existingVoxel == 0)
            {
                // Send network request for block place
                if (m_networkManager && m_networkManager->getClient() &&
                    m_networkManager->getClient()->isConnected())
                {
                    m_networkManager->getClient()->sendVoxelChangeRequest(
                        m_inputState.cachedTargetBlock.islandID, placePos, 1);
                }

                // Apply optimistically for immediate visual feedback
                m_gameState->setVoxel(m_inputState.cachedTargetBlock.islandID, placePos, 1);

                // Clear the cached target block to refresh the selection
                m_inputState.cachedTargetBlock = RayHit();

                // **FIXED**: Force immediate raycast to update block selection after placing
                m_inputState.cachedTargetBlock = VoxelRaycaster::raycast(
                    m_camera.position, m_camera.front, 50.0f, m_gameState->getIslandSystem());
                m_inputState.raycastTimer = 0.0f;
            }
        }
    }
    else if (!rightClick)
    {
        m_inputState.rightMousePressed = false;
    }
}

void GameClient::renderWorld()
{
    PROFILE_SCOPE("GameClient::renderWorld");
    
    if (!m_gameState)
    {
        return;
    }

    // Render all islands
    {
        PROFILE_SCOPE("renderAllIslands");
        m_gameState->getIslandSystem()->renderAllIslands();
    }

    // Render block highlighter (disabled in core profile; reimplement with modern pipeline if needed)
    (void)0;
}

void GameClient::renderWaitingScreen()
{
    // Simple text rendering for waiting screen
    // TODO: Replace with proper ImGui or text rendering system
    // For now, just clear to a different color to show we're in remote mode
    glClearColor(0.1f, 0.1f, 0.3f, 1.0f);  // Dark blue background
}

void GameClient::renderUI()
{
    // TODO: Implement ImGui UI rendering
    // This will show server connection status, performance metrics, etc.
    if (m_isRemoteClient)
    {
        // Show connection status for remote clients
        // TODO: Add proper UI text rendering
    }
}

void GameClient::onWindowResize(int width, int height)
{
    m_windowWidth = width;
    m_windowHeight = height;
    glViewport(0, 0, width, height);
}

void GameClient::handleWorldStateReceived(const WorldStateMessage& worldState)
{
    // Removed verbose debug output

    // Create a new GameState for the client based on server data
    m_gameState =
        new GameState();  // Note: This creates a raw pointer, we might want to use unique_ptr later

    if (!m_gameState->initialize(false))
    {  // Don't create default world, we'll use server data
        std::cerr << "Failed to initialize client game state!" << std::endl;
        delete m_gameState;
        m_gameState = nullptr;
        return;
    }

    // Set camera position to the spawn location
    m_camera.position = worldState.playerSpawnPosition;
    m_camera.position.y += 2.0f;  // Position camera slightly above spawn point

    // Removed verbose debug output
}

void GameClient::handleCompressedIslandReceived(uint32_t islandID, const Vec3& position,
                                                const uint8_t* voxelData, uint32_t dataSize)
{
    if (!m_gameState)
    {
        std::cerr << "Cannot handle island data: No game state initialized" << std::endl;
        return;
    }

    // Removed verbose debug output

    auto* islandSystem = m_gameState->getIslandSystem();
    if (!islandSystem)
    {
        std::cerr << "No island system available" << std::endl;
        return;
    }

    // Create the island at the specified position with the server's ID
    // We'll create it locally and map the server ID to our local island
    uint32_t localIslandID = islandSystem->createIsland(position);
    // Removed verbose debug output

    // Get the island and apply the voxel data
    FloatingIsland* island = islandSystem->getIsland(localIslandID);
    if (island)
    {
        // Create the main chunk if it doesn't exist (client islands don't auto-generate chunks)
        // For backward compatibility, use the origin chunk (0,0,0)
        Vec3 originChunk(0, 0, 0);
        if (island->chunks.find(originChunk) == island->chunks.end())
        {
            islandSystem->addChunkToIsland(localIslandID, originChunk);
        }

        VoxelChunk* chunk = islandSystem->getChunkFromIsland(localIslandID, originChunk);
        if (chunk)
        {
            // Apply the voxel data directly - this replaces any procedural generation
            chunk->setRawVoxelData(voxelData, dataSize);
            chunk->generateMesh();        // Regenerate the mesh with the new data
            chunk->buildCollisionMesh();  // Build collision faces from vertices
        }
        else
        {
            std::cerr << "Failed to create main chunk for island " << localIslandID << std::endl;
        }
    }
    else
    {
        std::cerr << "Failed to retrieve island with local ID: " << localIslandID << std::endl;
    }
}

void GameClient::handleCompressedChunkReceived(uint32_t islandID, const Vec3& chunkCoord, const Vec3& islandPosition, const uint8_t* voxelData, uint32_t dataSize)
{
    if (!m_gameState)
    {
        std::cerr << "Cannot handle chunk data: No game state initialized" << std::endl;
        return;
    }

    auto* islandSystem = m_gameState->getIslandSystem();
    if (!islandSystem)
    {
        std::cerr << "No island system available" << std::endl;
        return;
    }

    // Create or get the island 
    FloatingIsland* island = islandSystem->getIsland(islandID);
    if (!island)
    {
        // Create the island if it doesn't exist
        uint32_t localIslandID = islandSystem->createIsland(islandPosition);
        island = islandSystem->getIsland(localIslandID);
        
        if (!island)
        {
            std::cerr << "Failed to create island " << islandID << std::endl;
            return;
        }
    }

    // Add chunk to island if it doesn't exist
    VoxelChunk* chunk = islandSystem->getChunkFromIsland(islandID, chunkCoord);
    if (!chunk)
    {
        islandSystem->addChunkToIsland(islandID, chunkCoord);
        chunk = islandSystem->getChunkFromIsland(islandID, chunkCoord);
    }

    if (chunk)
    {
        // Apply the voxel data directly
        chunk->setRawVoxelData(voxelData, dataSize);
        chunk->generateMesh();        // Regenerate the mesh with the new data
        chunk->buildCollisionMesh();  // Build collision faces from vertices
    }
    else
    {
        std::cerr << "Failed to create chunk " << chunkCoord.x << "," << chunkCoord.y << "," << chunkCoord.z 
                  << " for island " << islandID << std::endl;
    }
}

void GameClient::handleVoxelChangeReceived(const VoxelChangeUpdate& update)
{
    if (!m_gameState)
    {
        std::cerr << "Cannot apply voxel change: no game state!" << std::endl;
        return;
    }

    // Removed verbose debug output

    // Apply the authoritative voxel change from server
    m_gameState->setVoxel(update.islandID, update.localPos, update.voxelType);

    // **FIXED**: Always force immediate raycast update when server sends voxel changes
    // This ensures block selection is immediately accurate after server updates
    m_inputState.cachedTargetBlock = VoxelRaycaster::raycast(
        m_camera.position, m_camera.front, 50.0f, m_gameState->getIslandSystem());
    m_inputState.raycastTimer = 0.0f;

    // The setVoxel call should automatically trigger mesh regeneration in GameState
}

void GameClient::handleEntityStateUpdate(const EntityStateUpdate& update)
{
    if (!m_gameState)
    {
        return;
    }

    // Handle different entity types
    switch (update.entityType)
    {
        case 1:
        {  // Island
            // Removed verbose debug output

            auto* islandSystem = m_gameState->getIslandSystem();
            if (islandSystem)
            {
                FloatingIsland* island = islandSystem->getIsland(update.entityID);
                if (island)
                {
                    // Apply server-authoritative velocity for client-side physics simulation
                    // This allows smooth movement while maintaining server authority

                    Vec3 currentPos = island->physicsCenter;
                    Vec3 serverPos = update.position;
                    Vec3 positionError = serverPos - currentPos;

                    // Calculate position error magnitude
                    float errorMagnitude = sqrtf(positionError.x * positionError.x +
                                                 positionError.y * positionError.y +
                                                 positionError.z * positionError.z);

                    // Set velocity from server for physics simulation
                    island->velocity = update.velocity;
                    island->acceleration = update.acceleration;

                    // Apply position correction based on error magnitude
                    if (errorMagnitude > 2.0f)
                    {
                        // Large error: snap to server position (teleport/respawn case)
                        island->physicsCenter = serverPos;
                    }
                    else if (errorMagnitude > 0.1f)
                    {
                        // Small to medium error: apply gradual correction
                        // Add a correction velocity component to smoothly move toward server
                        // position
                        Vec3 correctionVelocity = positionError * 0.8f;  // Correction strength
                        island->velocity = island->velocity + correctionVelocity;
                    }
                    // If error is very small (< 0.1f), just use server velocity as-is

                    // Mark for physics update synchronization
                    island->needsPhysicsUpdate = true;
                }
            }
            break;
        }
        case 0:  // Player (future implementation)
        case 2:  // NPC (future implementation)
        default:
            // Handle other entity types in the future
            break;
    }
}

void GameClient::windowResizeCallback(GLFWwindow* window, int width, int height)
{
    GameClient* client = static_cast<GameClient*>(glfwGetWindowUserPointer(window));
    if (client)
    {
        client->onWindowResize(width, height);
    }
}
