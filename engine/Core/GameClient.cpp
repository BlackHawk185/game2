// GameClient.cpp - Client-side rendering and input implementation
#include "GameClient.h"

#include <GLFW/glfw3.h>
#include <glad/gl.h>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

#include <iostream>
#include <memory>

#include "GameState.h"
#include "../Profiling/Profiler.h"
#include "../World/BlockType.h"

#include "../Network/NetworkManager.h"
#include "../Network/NetworkMessages.h"
#include "../Rendering/Renderer.h"
#include "../Rendering/BlockHighlightRenderer.h"
#include "../UI/HUD.h"
#include "../UI/Inventory.h"  // DEPRECATED: Old block-based inventory
#include "../UI/PeriodicTableUI.h"  // NEW: Periodic table UI for hotbar binding
#include "../Core/Window.h"
#include "../Rendering/MDIRenderer.h"
#include "../Rendering/ModelInstanceRenderer.h"
#include "../Rendering/TextureManager.h"
#include "../Rendering/ShadowMap.h"
#include "../Rendering/CascadedShadowMap.h"
#include "../Rendering/GlobalLightingManager.h"
#include "../Physics/PhysicsSystem.h"  // For ground detection
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
    
    // Initialize default hotbar elements (keys 1-9)
    m_hotbarElements = {
        Element::H,   // 1 - Hydrogen
        Element::C,   // 2 - Carbon
        Element::O,   // 3 - Oxygen
        Element::Si,  // 4 - Silicon
        Element::Na,  // 5 - Sodium
        Element::Cl,  // 6 - Chlorine
        Element::Ca,  // 7 - Calcium
        Element::Fe,  // 8 - Iron
        Element::Cu   // 9 - Copper
    };

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

    // Set up camera position at spawn point
    Vec3 playerSpawnPos = Vec3(0.0f, 64.0f, 0.0f);  // Default spawn position
    m_playerController.setPosition(playerSpawnPos);

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
    
    // Track frame time for FPS calculation
    m_lastFrameDeltaTime = deltaTime;

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
    
    // Cleanup ImGui
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

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
    
    // Update player controller (handles movement, physics, and camera)
    if (m_gameState)
    {
        // Tell PlayerController if UI is blocking input
        bool uiBlocking = (m_periodicTableUI && m_periodicTableUI->isOpen());
        m_playerController.setUIBlocking(uiBlocking);
        
        // Process mouse input
        m_playerController.processMouse(m_window->getHandle());
        
        // Update player controller (physics and camera)
        m_playerController.update(m_window->getHandle(), deltaTime, m_gameState->getIslandSystem());
        
        // Send movement to server if remote client
        if (m_isRemoteClient && m_networkManager)
        {
            Vec3 pos = m_playerController.getPosition();
            Vec3 vel = m_playerController.getVelocity();
            m_networkManager->sendPlayerMovement(pos, vel, deltaTime);
        }
    }

    // Process block interaction
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
        m_frustumCuller.updateFromCamera(m_playerController.getCamera(), aspect, fov);
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
    
    // Initialize MDI renderer with fixed allocation (each chunk gets a fixed buffer slice)
    g_mdiRenderer = std::make_unique<MDIRenderer>();
    if (!g_mdiRenderer->initialize(32768))  // 32k chunks max with fixed allocation
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
    
    // Load all OBJ-type block models from registry
    auto& registry = BlockTypeRegistry::getInstance();
    for (const auto& blockType : registry.getAllBlockTypes())
    {
        if (blockType.renderType == BlockRenderType::OBJ && !blockType.assetPath.empty())
        {
            if (!g_modelRenderer->loadModel(blockType.id, blockType.assetPath))
            {
                std::cerr << "Warning: Failed to load model for '" << blockType.name 
                          << "' from " << blockType.assetPath << std::endl;
            }
        }
    }

    // Initialize block highlighter for selected block wireframe
    m_blockHighlighter = std::make_unique<BlockHighlightRenderer>();
    if (!m_blockHighlighter->initialize())
    {
        std::cerr << "Warning: Failed to initialize BlockHighlightRenderer" << std::endl;
        m_blockHighlighter.reset();
    }
    
    // Initialize HUD overlay
    m_hud = std::make_unique<HUD>();
    
    // Initialize Inventory (hotbar) - DEPRECATED
    m_inventory = std::make_unique<Inventory>();
    
    // Initialize Periodic Table UI
    m_periodicTableUI = std::make_unique<PeriodicTableUI>();

    // Initialize ImGui
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    (void) io;
    io.ConfigFlags |= ImGuiConfigFlags_NavNoCaptureKeyboard; // Don't capture keyboard
    ImGui::StyleColorsDark();
    
    // Initialize ImGui backends
    ImGui_ImplGlfw_InitForOpenGL(m_window->getHandle(), true);
    ImGui_ImplOpenGL3_Init("#version 460");

    return true;
}

void GameClient::processKeyboard(float deltaTime)
{
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
    
    // Tab key - toggle periodic table UI
    {
        static bool tabKeyPressed = false;
        bool isTabPressed = glfwGetKey(m_window->getHandle(), GLFW_KEY_TAB) == GLFW_PRESS;
        
        if (isTabPressed && !tabKeyPressed) {
            m_periodicTableUI->toggle();
            
            // Toggle mouse cursor and camera control
            if (m_periodicTableUI->isOpen()) {
                glfwSetInputMode(m_window->getHandle(), GLFW_CURSOR, GLFW_CURSOR_NORMAL);
                std::cout << "Periodic table opened (mouse visible)" << std::endl;
            } else {
                glfwSetInputMode(m_window->getHandle(), GLFW_CURSOR, GLFW_CURSOR_DISABLED);
                std::cout << "Periodic table closed (mouse captured)" << std::endl;
            }
        }
        
        tabKeyPressed = isTabPressed;
    }
    
    // NEW: Element-based crafting system (keys 1-9 add elements to queue)
    // Skip if periodic table is open (it handles key input itself)
    if (!m_periodicTableUI->isOpen())
    {
        static bool numberKeysPressed[10] = {false};  // 0-9
        
        // Keys 1-9: Add elements from customizable hotbar
        for (int i = 0; i < 9; ++i)
        {
            int key = GLFW_KEY_1 + i;  // GLFW_KEY_1 through GLFW_KEY_9
            bool isPressed = glfwGetKey(m_window->getHandle(), key) == GLFW_PRESS;
            
            if (isPressed && !numberKeysPressed[i])
            {
                // Auto-unlock previous recipe when starting a new element sequence
                if (m_elementQueue.isEmpty() && m_lockedRecipe) {
                    m_lockedRecipe = nullptr;
                    std::cout << "Previous recipe unlocked (starting new craft)" << std::endl;
                }
                
                // Add element from customizable hotbar
                Element elem = m_hotbarElements[i];
                m_elementQueue.addElement(elem);
                
                // Check if this matches a recipe
                auto& recipeSystem = ElementRecipeSystem::getInstance();
                const BlockRecipe* recipe = recipeSystem.matchRecipe(m_elementQueue);
                
                if (recipe) {
                    std::cout << "✓ Recipe matched: " << recipe->name << " (" << recipe->formula << ")" << std::endl;
                } else {
                    std::cout << "Element added: " << ElementRecipeSystem::getElementSymbol(elem) 
                              << " (Queue: " << m_elementQueue.toFormula() << ")" << std::endl;
                }
            }
            
            numberKeysPressed[i] = isPressed;
        }
        
        // Key 0: Clear element queue
        bool isZeroPressed = glfwGetKey(m_window->getHandle(), GLFW_KEY_0) == GLFW_PRESS;
        if (isZeroPressed && !numberKeysPressed[9])
        {
            m_elementQueue.clear();
            m_lockedRecipe = nullptr;
            std::cout << "Element queue cleared" << std::endl;
        }
        numberKeysPressed[9] = isZeroPressed;
    }

    // Debug collision info (press C to debug collision system)
    static bool wasDebugKeyPressed = false;
    bool isDebugKeyPressed = glfwGetKey(m_window->getHandle(), GLFW_KEY_C) == GLFW_PRESS;
    
    if (isDebugKeyPressed && !wasDebugKeyPressed)
    {
        g_physics.debugCollisionInfo(m_playerController.getCamera().position, 0.5f);
    }
    wasDebugKeyPressed = isDebugKeyPressed;
    
    // Toggle HUD debug info (press F3)
    static bool wasF3KeyPressed = false;
    bool isF3KeyPressed = glfwGetKey(m_window->getHandle(), GLFW_KEY_F3) == GLFW_PRESS;
    
    if (isF3KeyPressed && !wasF3KeyPressed && m_hud)
    {
        m_hud->toggleDebugInfo();
    }
    wasF3KeyPressed = isF3KeyPressed;
    
    // Toggle noclip mode (press N for debug flying)
    static bool wasNoclipKeyPressed = false;
    bool isNoclipKeyPressed = glfwGetKey(m_window->getHandle(), GLFW_KEY_N) == GLFW_PRESS;
    
    if (isNoclipKeyPressed && !wasNoclipKeyPressed)
    {
        m_playerController.setNoclipMode(!m_playerController.isNoclipMode());
        std::cout << (m_playerController.isNoclipMode() ? "🕊️ Noclip enabled (flying)" : "🚶 Physics enabled (walking)") << std::endl;
    }
    wasNoclipKeyPressed = isNoclipKeyPressed;

    // Toggle camera smoothing (press L to see raw physics - helpful for debugging)
    static bool wasSmoothingKeyPressed = false;
    bool isSmoothingKeyPressed = glfwGetKey(m_window->getHandle(), GLFW_KEY_L) == GLFW_PRESS;
    
    if (isSmoothingKeyPressed && !wasSmoothingKeyPressed)
    {
        m_playerController.setCameraSmoothing(!m_playerController.isCameraSmoothingEnabled());
        std::cout << (m_playerController.isCameraSmoothingEnabled() ? "📹 Camera smoothing enabled (smooth)" : "📹 Camera smoothing disabled (raw physics)") << std::endl;
    }
    wasSmoothingKeyPressed = isSmoothingKeyPressed;

    // Toggle piloting (press E to pilot the island/vehicle you're standing on)
    static bool wasEKeyPressed = false;
    bool isEKeyPressed = glfwGetKey(m_window->getHandle(), GLFW_KEY_E) == GLFW_PRESS;
    
    if (isEKeyPressed && !wasEKeyPressed)
    {
        m_playerController.setPiloting(!m_playerController.isPiloting(), m_playerController.getPilotedIslandID());
        if (m_playerController.isPiloting())
        {
            std::cout << "🚀 Piloting ENABLED - WASD rotates, arrows for thrust" << std::endl;
        }
        else
        {
            std::cout << "🚶 Piloting DISABLED - normal movement" << std::endl;
        }
    }
    wasEKeyPressed = isEKeyPressed;

    // Exit
    if (glfwGetKey(m_window->getHandle(), GLFW_KEY_ESCAPE) == GLFW_PRESS)
    {
        m_window->setShouldClose(true);
    }
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
            m_playerController.getCamera().position, m_playerController.getCamera().front, 50.0f, m_gameState->getIslandSystem());
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

            // Server will respond with authoritative update - no client-side optimistic update

            // Clear the cached target block immediately to remove the yellow outline
            m_inputState.cachedTargetBlock = RayHit();

            // **FIXED**: Force immediate raycast to update block selection after breaking
            m_inputState.cachedTargetBlock = VoxelRaycaster::raycast(
                m_playerController.getCamera().position, m_playerController.getCamera().front, 50.0f, m_gameState->getIslandSystem());
            m_inputState.raycastTimer = 0.0f;
        }
    }
    else if (!leftClick)
    {
        m_inputState.leftMousePressed = false;
    }

    // Right click - place block or lock/switch recipe
    if (rightClick && !m_inputState.rightMousePressed)
    {
        m_inputState.rightMousePressed = true;

        // If we have a queued element sequence, try to lock/switch to it
        if (!m_elementQueue.isEmpty())
        {
            auto& recipeSystem = ElementRecipeSystem::getInstance();
            const BlockRecipe* newRecipe = recipeSystem.matchRecipe(m_elementQueue);
            
            if (newRecipe) {
                // Valid recipe - lock/switch to it
                m_lockedRecipe = newRecipe;
                std::cout << "🔒 Recipe locked: " << m_lockedRecipe->name 
                          << " (" << m_lockedRecipe->formula << ")" << std::endl;
                m_elementQueue.clear();
            } else {
                // Invalid recipe - clear the queue
                std::cout << "❌ No recipe matches " << m_elementQueue.toFormula() << " - clearing queue" << std::endl;
                m_elementQueue.clear();
            }
        }
        // If no queue but we have a locked recipe and valid target, place block
        else if (m_lockedRecipe && m_inputState.cachedTargetBlock.hit)
        {
            Vec3 placePos = VoxelRaycaster::getPlacementPosition(m_inputState.cachedTargetBlock);
            uint8_t existingVoxel =
                m_gameState->getVoxel(m_inputState.cachedTargetBlock.islandID, placePos);

            if (existingVoxel == 0)
            {
                // Use locked recipe block
                uint8_t blockToPlace = m_lockedRecipe->blockID;
                
                // Send network request for block place
                if (m_networkManager && m_networkManager->getClient() &&
                    m_networkManager->getClient()->isConnected())
                {
                    m_networkManager->getClient()->sendVoxelChangeRequest(
                        m_inputState.cachedTargetBlock.islandID, placePos, blockToPlace);
                }

                // Server will respond with authoritative update - no client-side optimistic update
                
                // Keep recipe locked for continuous placement
                std::cout << "Block placed (" << m_lockedRecipe->name << " still locked)" << std::endl;

                // Clear the cached target block to refresh the selection
                m_inputState.cachedTargetBlock = RayHit();

                // **FIXED**: Force immediate raycast to update block selection after placing
                m_inputState.cachedTargetBlock = VoxelRaycaster::raycast(
                    m_playerController.getCamera().position, m_playerController.getCamera().front, 50.0f, m_gameState->getIslandSystem());
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
    
    // Process pending mesh updates from game logic thread (MUST be on render thread for OpenGL)
    if (g_mdiRenderer)
    {
        PROFILE_SCOPE("Process pending mesh updates");
        g_mdiRenderer->processPendingUpdates();
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
        glm::mat4 projectionMatrix = m_playerController.getCamera().getProjectionMatrix(aspect);
        glm::mat4 viewMatrix = m_playerController.getCamera().getViewMatrix();
        
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
            
            // Use Camera's projection matrix (same FOV as voxel rendering)
            float aspect = (float) m_windowWidth / (float) m_windowHeight;
            glm::mat4 projectionMatrix = m_playerController.getCamera().getProjectionMatrix(aspect);
            glm::mat4 viewMatrix = m_playerController.getCamera().getViewMatrix();
            
            // Render all OBJ-type blocks (grass, trees, lamps, QFG, etc.)
            auto& registry = BlockTypeRegistry::getInstance();
            for (const auto& blockType : registry.getAllBlockTypes())
            {
                if (blockType.renderType == BlockRenderType::OBJ)
                {
                    for (auto& p : snapshot)
                    {
                        g_modelRenderer->renderModelChunk(blockType.id, p.first, p.second, viewMatrix, projectionMatrix);
                    }
                }
            }
        }
        
        // Render block highlight (yellow wireframe cube on selected block)
        if (m_blockHighlighter && m_inputState.cachedTargetBlock.hit)
        {
            PROFILE_SCOPE("renderBlockHighlight");
            
            // Convert island-relative position to world position for rendering
            auto& islands = m_gameState->getIslandSystem()->getIslands();
            auto it = islands.find(m_inputState.cachedTargetBlock.islandID);
            if (it != islands.end())
            {
                Vec3 worldBlockPos = m_inputState.cachedTargetBlock.localBlockPos + it->second.physicsCenter;
                
                // Use Camera's projection matrix (same FOV as voxel rendering)
                float aspect = (float) m_windowWidth / (float) m_windowHeight;
                glm::mat4 projectionMatrix = m_playerController.getCamera().getProjectionMatrix(aspect);
                glm::mat4 viewMatrix = m_playerController.getCamera().getViewMatrix();
                
                // Convert glm matrices to float arrays for BlockHighlightRenderer
                m_blockHighlighter->render(worldBlockPos, glm::value_ptr(viewMatrix), glm::value_ptr(projectionMatrix));
            }
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
    glm::vec3 camPos(m_playerController.getCamera().position.x, m_playerController.getCamera().position.y, m_playerController.getCamera().position.z);
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

void GameClient::renderWaitingScreen()
{
    // Simple text rendering for waiting screen
    // TODO: Replace with proper ImGui or text rendering system
    // For now, just clear to a different color to show we're in remote mode
    glClearColor(0.1f, 0.1f, 0.3f, 1.0f);  // Dark blue background
}

void GameClient::renderUI()
{
    // Start ImGui frame
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
    
    // Render HUD
    if (m_hud)
    {
        // Update HUD state
        m_hud->setPlayerPosition(m_playerController.getCamera().position.x, m_playerController.getCamera().position.y, m_playerController.getCamera().position.z);
        m_hud->setPlayerHealth(100.0f, 100.0f); // TODO: Wire up actual health system
        
        // Calculate FPS from delta time
        float fps = (m_lastFrameDeltaTime > 0.0001f) ? (1.0f / m_lastFrameDeltaTime) : 60.0f;
        m_hud->setFPS(fps);
        
        // Set current block in hand (TODO: get from inventory/hotbar)
        m_hud->setCurrentBlock("Stone");
        
        // Set target block (block player is looking at) with elemental formula
        if (m_inputState.cachedTargetBlock.hit && m_gameState)
        {
            uint8_t blockID = m_gameState->getVoxel(
                m_inputState.cachedTargetBlock.islandID,
                m_inputState.cachedTargetBlock.localBlockPos
            );
            
            auto& registry = BlockTypeRegistry::getInstance();
            const BlockTypeInfo* blockInfo = registry.getBlockType(blockID);
            
            // Try to find recipe for this block to show its formula
            std::string formula = "";
            auto& recipeSystem = ElementRecipeSystem::getInstance();
            for (const auto& recipe : recipeSystem.getAllRecipes()) {
                if (recipe.blockID == blockID) {
                    formula = recipe.formula;
                    break;
                }
            }
            
            if (blockInfo)
            {
                m_hud->setTargetBlock(blockInfo->name, formula);
            }
            else
            {
                m_hud->clearTargetBlock();
            }
        }
        else
        {
            m_hud->clearTargetBlock();
        }
        
        // Render HUD overlay
        m_hud->render(m_lastFrameDeltaTime);
        
        // NEW: Render element queue hotbar (with customizable elements)
        m_hud->renderElementQueue(m_elementQueue, m_lockedRecipe, m_hotbarElements);
        
        // Render periodic table UI if open
        if (m_periodicTableUI->isOpen()) {
            m_periodicTableUI->render(m_hotbarElements);
        }
    }
    
    // Show connection status for remote clients
    if (m_isRemoteClient)
    {
        // Additional remote client UI could go here
    }
    
    // Finalize ImGui frame and render
    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
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

    // Spawn player at server-provided location
    Vec3 spawnPos = worldState.playerSpawnPosition;
    spawnPos.y += 2.0f;  // Position camera slightly above spawn point
    m_playerController.setPosition(spawnPos);
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
        // Create the island with the server's ID to keep them in sync
        islandSystem->createIsland(islandPosition, islandID);
        island = islandSystem->getIsland(islandID);

        if (!island)
        {
            std::cerr << "Failed to create island " << islandID << std::endl;
            return;
        }
        
        std::cout << "📦 Created new island " << islandID << " from server" << std::endl;
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
        
        // Queue chunk registration for render thread (thread-safe)
        if (g_mdiRenderer && island)
        {
            Vec3 worldOffset = island->physicsCenter + Vec3(
                chunkCoord.x * VoxelChunk::SIZE,
                chunkCoord.y * VoxelChunk::SIZE,
                chunkCoord.z * VoxelChunk::SIZE
            );
            
            // Use queued registration to avoid OpenGL cross-thread issues
            // The processPendingUpdates() will check if already registered
            g_mdiRenderer->queueChunkRegistration(chunk, worldOffset);
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

    // Apply the authoritative voxel change from server
    m_gameState->setVoxel(update.islandID, update.localPos, update.voxelType);

    // Update MDI renderer (client-side only - server doesn't render)
    if (g_mdiRenderer)
    {
        auto* islandSystem = m_gameState->getIslandSystem();
        Vec3 chunkCoord = FloatingIsland::islandPosToChunkCoord(update.localPos);
        auto* chunk = islandSystem->getChunkFromIsland(update.islandID, chunkCoord);
        if (chunk)
        {
            if (chunk->getMDIIndex() < 0)
            {
                // New chunk - register it
                Vec3 islandCenter = islandSystem->getIslandCenter(update.islandID);
                Vec3 worldOffset = islandCenter + Vec3(
                    chunkCoord.x * VoxelChunk::SIZE,
                    chunkCoord.y * VoxelChunk::SIZE,
                    chunkCoord.z * VoxelChunk::SIZE
                );
                g_mdiRenderer->queueChunkRegistration(chunk, worldOffset);
            }
            else
            {
                // Existing chunk - update mesh
                g_mdiRenderer->queueChunkMeshUpdate(chunk->getMDIIndex(), chunk);
            }
        }
    }

    // **FIXED**: Always force immediate raycast update when server sends voxel changes
    // This ensures block selection is immediately accurate after server updates
    m_inputState.cachedTargetBlock = VoxelRaycaster::raycast(
        m_playerController.getCamera().position, m_playerController.getCamera().front, 50.0f, m_gameState->getIslandSystem());
    m_inputState.raycastTimer = 0.0f;
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

// ================================
// CRITICAL: Centralized Player Spawn Function
// ================================
// This is the ONLY function that should set player position
// Ensures m_playerController.getCamera().position and m_physicsPosition stay in sync
// spawnPlayerAt() removed - use PlayerController::setPosition()

// Window resize callback handled via Engine::Core::Window::setResizeCallback

