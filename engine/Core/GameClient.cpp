// GameClient.cpp - Client-side rendering and input implementation
#include "GameClient.h"

#include <GLFW/glfw3.h>
#include <glad/gl.h>
#include <imgui.h>

#include <iostream>
#include <memory>

#include "GameState.h"
#include "../Profiling/Profiler.h"
#include "../World/BlockType.h"

#include "../Network/NetworkManager.h"
#include "../Network/NetworkMessages.h"
#include "../Rendering/Renderer.h"
#include "../Core/Window.h"
#include "../Rendering/MDIRenderer.h"
#include "../Rendering/ModelInstanceRenderer.h"
#include "../Rendering/TextureManager.h"
#include "../Rendering/ShadowMap.h"
#include "../Rendering/CascadedShadowMap.h"
#include "../Rendering/GlobalLightingManager.h"
#include "../Physics/FluidSystem.h"
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

    // Set global day/night cycle pointer
    g_dayNightCycle = &m_dayNightCycle;

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
    // Clear global day/night cycle pointer
    g_dayNightCycle = nullptr;
    
    shutdown();
}

bool GameClient::initialize(bool enableDebug)
{
    if (m_initialized)
    {
        std::cerr << "GameClient already initialized!" << std::endl;
        return false;
    }

    m_debugMode = enableDebug;

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

    // Polling now occurs during window update at end of frame

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

    // NEW: Update day/night cycle for dynamic lighting
    {
        PROFILE_SCOPE("DayNightCycle::update");
        m_dayNightCycle.update(deltaTime);
        
        // Check if sun direction changed significantly
        Vec3 newSunDirection = m_dayNightCycle.getSunDirection();
        float directionChange = (newSunDirection - m_lastSunDirection).length();
        
        if (directionChange > 0.01f) { // Sun moved enough to matter
            // Update global lighting manager with new sun direction
            extern GlobalLightingManager g_globalLighting;
            g_globalLighting.setSunDirection(newSunDirection);
            m_lastSunDirection = newSunDirection;
        }
    }

    // Update model instancing time (wind animation)
    if (g_modelRenderer)
    {
        g_modelRenderer->update(deltaTime);
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

    // Swap buffers and poll events via wrapper
    {
        PROFILE_SCOPE("Window::update");
        if (m_window) m_window->update();
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

    // Cleanup renderers (unique_ptr handles deletion automatically)
    if (g_mdiRenderer)
    {
        g_mdiRenderer->shutdown();
        g_mdiRenderer.reset();
        std::cout << "MDI renderer shutdown" << std::endl;
    }

    if (g_modelRenderer)
    {
        g_modelRenderer->shutdown();
        g_modelRenderer.reset();
    }

    // Cleanup window
    if (m_window)
    {
        m_window->shutdown();
        m_window.reset();
    }

    // Terminate GLFW after window shutdown
    glfwTerminate();

    m_initialized = false;
    // Removed verbose debug output
}

bool GameClient::shouldClose() const
{
    return m_window && m_window->shouldClose();
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
        
        // Update frustum culling
        m_frustumCuller.updateFromCamera(m_camera, aspect, fov);
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
    // Use the Window wrapper for all window/context handling
    m_window = std::make_unique<Engine::Core::Window>();
    if (!m_window->initialize(m_windowWidth, m_windowHeight, "MMORPG Engine - Client", m_debugMode))
    {
        std::cerr << "Failed to initialize window!" << std::endl;
        return false;
    }

    // Set resize callback to update camera/aspect
    m_window->setResizeCallback([this](int width, int height) { this->onWindowResize(width, height); });

    // Set up mouse capture on underlying GLFW window
    glfwSetInputMode(m_window->getHandle(), GLFW_CURSOR, GLFW_CURSOR_DISABLED);

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
    
    // Initialize texture manager (needed by all renderers)
    extern TextureManager* g_textureManager;
    if (!g_textureManager)
    {
        g_textureManager = new TextureManager();
    }
    
    // Initialize MDI renderer for massive chunk batching (16k+ chunks → 1 draw call)
    g_mdiRenderer = std::make_unique<MDIRenderer>();
    if (!g_mdiRenderer->initialize(32768, 50000000, 75000000))
    {
        std::cerr << "⚠️  Failed to initialize MDI renderer - falling back to per-chunk rendering" << std::endl;
        g_mdiRenderer.reset();
    }
    else
    {
        std::cout << "✅ MDI Renderer initialized - ready for massive batching!" << std::endl;
    }

    // Initialize model instancing renderer (decorative GLB like grass)
    g_modelRenderer = std::make_unique<ModelInstanceRenderer>();
    if (!g_modelRenderer->initialize())
    {
        std::cerr << "Failed to initialize ModelInstanceRenderer!" << std::endl;
        g_modelRenderer.reset();
        return false;
    }
    // Load grass model asset
    if (!g_modelRenderer->loadGrassModel("assets/models/grass.glb"))
    {
        std::cerr << "Warning: could not load assets/models/grass.glb. Grass will not render." << std::endl;
    }

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
    // Handle camera movement and collision detection
    handleCameraMovement(deltaTime);

    // Time effects (temporary, will be refactored)
    if (g_timeEffects)
    {
        // TODO: Implement proper time effect methods
        // The actual method names need to be checked in TimeEffects.h
        /*
        if (glfwGetKey(m_window->getHandle(), GLFW_KEY_1) == GLFW_PRESS) g_timeEffects->setSlowMotion();
        if (glfwGetKey(m_window->getHandle(), GLFW_KEY_2) == GLFW_PRESS) g_timeEffects->setBulletTime();
        if (glfwGetKey(m_window->getHandle(), GLFW_KEY_3) == GLFW_PRESS) g_timeEffects->setTimeFreeze();
        if (glfwGetKey(m_window->getHandle(), GLFW_KEY_4) == GLFW_PRESS) g_timeEffects->setSpeedUp();
        if (glfwGetKey(m_window->getHandle(), GLFW_KEY_5) == GLFW_PRESS) g_timeEffects->setTimeReverse();
        if (glfwGetKey(m_window->getHandle(), GLFW_KEY_0) == GLFW_PRESS) g_timeEffects->resetTime();
        if (glfwGetKey(m_window->getHandle(), GLFW_KEY_T) == GLFW_PRESS) g_timeEffects->toggleTimePulse();
        */
    }

    // Fluid particle spawning controls
    static bool wasWaterKeyPressed = false;
    bool isWaterKeyPressed = glfwGetKey(m_window->getHandle(), GLFW_KEY_F) == GLFW_PRESS;
    
    if (isWaterKeyPressed && !wasWaterKeyPressed)
    {
        // Spawn fluid particle in front of camera
        Vec3 spawnPosition = m_camera.position + m_camera.front * 3.0f;
        Vec3 spawnVelocity = m_camera.front * 5.0f;  // Launch forward
        
        EntityID particleEntity = g_fluidSystem.spawnFluidParticle(spawnPosition, spawnVelocity);
        if (particleEntity != INVALID_ENTITY)
        {
            std::cout << "ðŸ’§ Spawned fluid particle " << particleEntity << " at (" 
                      << spawnPosition.x << ", " << spawnPosition.y << ", " << spawnPosition.z << ")" << std::endl;
        }
    }
    wasWaterKeyPressed = isWaterKeyPressed;

    // Debug collision info (press C to debug collision system)
    static bool wasDebugKeyPressed = false;
    bool isDebugKeyPressed = glfwGetKey(m_window->getHandle(), GLFW_KEY_C) == GLFW_PRESS;
    
    if (isDebugKeyPressed && !wasDebugKeyPressed)
    {
        g_physics.debugCollisionInfo(m_camera.position, 0.5f);
    }
    wasDebugKeyPressed = isDebugKeyPressed;

    // Exit
    if (glfwGetKey(m_window->getHandle(), GLFW_KEY_ESCAPE) == GLFW_PRESS)
    {
        m_window->setShouldClose(true);
    }
}

void GameClient::processMouse(float deltaTime)
{
    static double lastX = m_windowWidth / 2.0;
    static double lastY = m_windowHeight / 2.0;
    static bool firstMouse = true;

    double xpos, ypos;
    glfwGetCursorPos(m_window->getHandle(), &xpos, &ypos);

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

    bool leftClick = glfwGetMouseButton(m_window->getHandle(), GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;
    bool rightClick = glfwGetMouseButton(m_window->getHandle(), GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS;

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
                        m_inputState.cachedTargetBlock.islandID, placePos, BlockID::STONE);
                }

                // Apply optimistically for immediate visual feedback
                m_gameState->setVoxel(m_inputState.cachedTargetBlock.islandID, placePos, BlockID::STONE);

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
    
    // Sync island physics to chunk transforms (client-side only)
    {
        PROFILE_SCOPE("Island physics update");
        m_gameState->getIslandSystem()->syncPhysicsToChunks();
    }

    // Shadow depth pass (cascaded)
    {
        renderShadowPass();
    }

    // Render all islands with MDI (single draw call for all chunks)
    {
        PROFILE_SCOPE("MDI_renderAll");
        
        if (!g_mdiRenderer)
        {
            std::cerr << "❌ MDI renderer not initialized! Cannot render world." << std::endl;
            return;
        }
        
        // Get camera matrices
        float aspect = (float)m_windowWidth / (float)m_windowHeight;
        glm::mat4 projectionMatrix = m_camera.getProjectionMatrix(aspect);
        glm::mat4 viewMatrix = m_camera.getViewMatrix();
        
        // Render everything with single draw call!
        g_mdiRenderer->renderAll(viewMatrix, projectionMatrix);
        
        // Print stats periodically
        static auto lastPrint = std::chrono::steady_clock::now();
        auto now = std::chrono::steady_clock::now();
        if (std::chrono::duration_cast<std::chrono::seconds>(now - lastPrint).count() >= 5)
        {
            auto stats = g_mdiRenderer->getStatistics();
            std::cout << "📊 MDI: " << stats.activeChunks << " chunks, " 
                      << (stats.totalVertices / 1000) << "k verts, " 
                      << stats.drawCalls << " draw call(s), "
                      << stats.lastFrameTimeMs << "ms" << std::endl;
            lastPrint = now;
        }
        // Render decorative grass instances per chunk
        if (g_modelRenderer)
        {
            std::vector<std::pair<VoxelChunk*, Vec3>> snapshot;
            m_gameState->getIslandSystem()->getAllChunksWithWorldPos(snapshot);
            float aspect = (float) m_windowWidth / (float) m_windowHeight;
            float fov = 45.0f;
            glm::mat4 projectionMatrix = glm::perspective(fov * 3.14159265358979323846f / 180.0f, aspect, 0.1f, 1000.0f);
            glm::mat4 viewMatrix = m_camera.getViewMatrix();
            
            for (auto& p : snapshot)
            {
                g_modelRenderer->renderGrassChunk(p.first, p.second, viewMatrix, projectionMatrix);
            }
        }
    }

    // Render fluid particles
    {
        PROFILE_SCOPE("renderFluidParticles");
        
        // Get all fluid particles
        auto* fluidStorage = g_ecs.getStorage<FluidParticleComponent>();
        if (fluidStorage)
        {
            std::vector<EntityID> fluidParticles;
            fluidParticles.reserve(fluidStorage->entities.size());
            
            for (EntityID entity : fluidStorage->entities)
            {
                fluidParticles.push_back(entity);
            }
            
            // TODO: Render fluid particles with MDI renderer
            // if (!fluidParticles.empty())
            // {
            //     // Need to implement fluid particle rendering in MDI
            // }
        }
    }

    // Render block highlighter (disabled in core profile; reimplement with modern pipeline if needed)
    (void)0;
}

void GameClient::renderShadowPass()
{
    PROFILE_SCOPE("GameClient::renderShadowPass");
    
    // Single high-resolution shadow pass
    Vec3 sunDir = Vec3(-0.3f, -1.0f, -0.2f).normalized();
    glm::vec3 camPos(m_camera.position.x, m_camera.position.y, m_camera.position.z);
    glm::vec3 lightDir(sunDir.x, sunDir.y, sunDir.z);
    glm::vec3 lightTarget = camPos;
    glm::vec3 lightPos = camPos - lightDir * 100.0f;
    glm::mat4 lightView = glm::lookAt(lightPos, lightTarget, glm::vec3(0,1,0));
    
    // Shadow map coverage (1024×1024 units with 8192×8192 texture = 8 pixels per block)
    const float orthoSize = 512.0f;
    
    // Build light view-projection with texel snapping for stability
    glm::mat4 lightProj = glm::ortho(-orthoSize, orthoSize, -orthoSize, orthoSize, 1.0f, 300.0f);
    
    // Snap to texel grid to prevent shadow shimmering
    glm::vec4 centerLS = lightView * glm::vec4(lightTarget, 1.0f);
    int smWidth = g_csm.getSize(0);
    float texelSize = (2.0f * orthoSize) / float(smWidth);
    glm::vec2 snapped = glm::floor(glm::vec2(centerLS.x, centerLS.y) / texelSize) * texelSize;
    glm::vec2 delta = snapped - glm::vec2(centerLS.x, centerLS.y);
    glm::mat4 snapMat = glm::translate(glm::mat4(1.0f), glm::vec3(-delta.x, -delta.y, 0.0f));
    glm::mat4 lightVP = lightProj * snapMat * lightView;
    
    // Render shadow depth pass
    if (g_mdiRenderer && m_windowWidth > 0 && m_windowHeight > 0)
    {
        g_mdiRenderer->beginDepthPassCascade(0, lightVP);
        g_mdiRenderer->renderDepthCascade(0);
        g_mdiRenderer->endDepthPassCascade(m_windowWidth, m_windowHeight);
    }
    
    // Set lighting data for forward pass
    if (g_mdiRenderer)
    {
        // Currently using single cascade, but API supports multiple
        int cascadeCount = 1;
        glm::mat4 lightVPs[4] = { lightVP, glm::mat4(1.0f), glm::mat4(1.0f), glm::mat4(1.0f) };
        float cascadeSplits[4] = { 300.0f, 0.0f, 0.0f, 0.0f };  // Far plane for cascade 0
        g_mdiRenderer->setLightingData(cascadeCount, lightVPs, cascadeSplits, glm::vec3(lightDir.x, lightDir.y, lightDir.z));
    }
}

void GameClient::handleCameraMovement(float deltaTime)
{
    // Camera movement input
    float moveSpeed = 10.0f;
    Vec3 movement(0, 0, 0);

    if (glfwGetKey(m_window->getHandle(), GLFW_KEY_W) == GLFW_PRESS)
    {
        movement = movement + m_camera.front * moveSpeed * deltaTime;
    }
    if (glfwGetKey(m_window->getHandle(), GLFW_KEY_S) == GLFW_PRESS)
    {
        movement = movement - m_camera.front * moveSpeed * deltaTime;
    }
    if (glfwGetKey(m_window->getHandle(), GLFW_KEY_A) == GLFW_PRESS)
    {
        movement = movement - m_camera.right * moveSpeed * deltaTime;
    }
    if (glfwGetKey(m_window->getHandle(), GLFW_KEY_D) == GLFW_PRESS)
    {
        movement = movement + m_camera.right * moveSpeed * deltaTime;
    }
    if (glfwGetKey(m_window->getHandle(), GLFW_KEY_SPACE) == GLFW_PRESS)
    {
        movement = movement + m_camera.up * moveSpeed * deltaTime;
    }
    if (glfwGetKey(m_window->getHandle(), GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS)
    {
        movement = movement - m_camera.up * moveSpeed * deltaTime;
    }

    // **DIRECT MOVEMENT**: Camera moves freely without collision
    // If you want camera collision, add VelocityComponent to camera and let PhysicsSystem handle it
    Vec3 intendedPosition = m_camera.position + movement;
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

void GameClient::handleCompressedChunkReceived(uint32_t islandID, const Vec3& chunkCoord, const Vec3& islandPosition,
                                               const uint8_t* voxelData, uint32_t dataSize)
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
        
        // Register with MDI renderer for batched rendering
        if (g_mdiRenderer && island)
        {
            Vec3 worldOffset = island->physicsCenter + Vec3(
                chunkCoord.x * VoxelChunk::SIZE,
                chunkCoord.y * VoxelChunk::SIZE,
                chunkCoord.z * VoxelChunk::SIZE
            );
            int mdiIndex = g_mdiRenderer->registerChunk(chunk, worldOffset);
            chunk->setMDIIndex(mdiIndex);  // Store for future transform updates
        }
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

// Window resize callback handled via Engine::Core::Window::setResizeCallback
