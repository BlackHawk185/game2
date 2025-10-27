// GameClient.h - Client-side rendering and input handling
// This class manages the presentation layer and user interaction
#pragma once

#include "../Math/Vec3.h"
#include "../Input/Camera.h"
#include "../Culling/FrustumCuller.h"
#include "../World/VoxelRaycaster.h"
#include "../World/ElementRecipes.h"  // NEW: Element-based crafting system
#include "../Network/NetworkManager.h"  // Re-enabled with ENet integration working
#include "../Time/DayNightCycle.h"  // NEW: Day/night cycle integration
#include <memory>
#include <string>

// Forward declarations
class GameState;
class BlockHighlightRenderer;
class HUD;
class Inventory;  // DEPRECATED: Old block-based inventory
class PeriodicTableUI;
struct GLFWwindow;
struct VoxelChangeUpdate;
struct WorldStateMessage;

namespace Engine { namespace Core { class Window; } }

/**
 * GameClient handles the presentation layer of the game.
 * It manages rendering, input, and UI, but does not own the game state.
 * 
 * The client can either:
 * 1. Connect to a local GameServer (integrated mode)
 * 2. Connect to a remote server (client-only mode)
 * 3. Work with a shared GameState directly (current transition mode)
 */
class GameClient {
public:
    GameClient();
    ~GameClient();
    
    // ================================
    // CLIENT LIFECYCLE
    // ================================
    
    /**
     * Initialize the client (creates window, graphics context, etc.)
     */
    bool initialize(bool enableDebug = false);
    
    /**
     * Connect to a game state (local or remote)
     * @param gameState - The game state to render (can be local server)
     */
    bool connectToGameState(GameState* gameState);
    
    /**
     * Connect to a remote server
     * @param serverAddress - The server address to connect to
     * @param serverPort - The server port to connect to
     */
    bool connectToRemoteServer(const std::string& serverAddress, uint16_t serverPort);
    
    /**
     * Main client loop - handles input, rendering, and presentation
     * Returns false when the client should exit
     */
    bool update(float deltaTime);
    
    /**
     * Shutdown the client
     */
    void shutdown();
    
    // ================================
    // INPUT HANDLING
    // ================================
    
    /**
     * Process input and generate commands
     * These commands will be sent to the game state/server
     */
    void processInput(float deltaTime);
    
    /**
     * Check if the client window should close
     */
    bool shouldClose() const;
    
    // ================================
    // RENDERING
    // ================================
    
    /**
     * Render the current game state
     */
    void render();
    
    /**
     * Get the current camera for external use
     */
    Camera& getCamera() { return m_camera; }
    const Camera& getCamera() const { return m_camera; }
    
private:
    // Graphics window/context
    std::unique_ptr<Engine::Core::Window> m_window;
    int m_windowWidth = 1280;
    int m_windowHeight = 720;
    
    // Game state connection
    GameState* m_gameState = nullptr;  // Not owned by client
    
    // Networking - Re-enabled with ENet integration
    std::unique_ptr<NetworkManager> m_networkManager;
    bool m_isRemoteClient = false;
    
    // Rendering systems
    Camera m_camera;
    FrustumCuller m_frustumCuller;
    std::unique_ptr<BlockHighlightRenderer> m_blockHighlighter;
    std::unique_ptr<HUD> m_hud;
    std::unique_ptr<Inventory> m_inventory;  // DEPRECATED: Old block-based inventory
    std::unique_ptr<PeriodicTableUI> m_periodicTableUI;  // NEW: Periodic table for hotbar binding
    
    // NEW: Day/night cycle system for dynamic lighting
    DayNightCycle m_dayNightCycle;
    Vec3 m_lastSunDirection{0.0f, 0.0f, 0.0f};  // Track sun direction changes
    
    // FPS tracking
    float m_lastFrameDeltaTime = 0.016f; // Start at ~60 FPS
    
    // Player physics state
    Vec3 m_playerVelocity{0.0f, 0.0f, 0.0f};        // Player's own velocity (input, gravity, jumps)
    Vec3 m_physicsPosition{0.0f, 0.0f, 0.0f};       // Actual hitbox position (can jitter)
    bool m_isGrounded = false;
    bool m_jumpPressed = false;
    bool m_noclipMode = false;          // Debug: disable physics
    bool m_disableCameraSmoothing = false;  // Debug: disable camera LERP to see raw physics
    bool m_isPiloting = false;          // Is player currently piloting a vehicle?
    uint32_t m_pilotedIslandID = 0;     // Which island is being piloted
    float m_moveSpeed = 24.0f;          // Walk speed (adjusted 1.5x for larger world scale)
    float m_jumpStrength = 8.0f;       // Jump velocity (adjusted 1.5x for larger world scale)
    float m_gravity = 20.0f;            // Gravity acceleration
    float m_airControl = 0.2f;          // How much control in air (reduced from 0.4)
    float m_groundFriction = 0.85f;     // Ground friction multiplier
    float m_airFriction = 0.94f;        // Air resistance (reduced from 0.98 for more drag)
    float m_cameraSmoothing = 0.15f;    // Camera interpolation speed (lower = smoother)
    
    // Capsule collision dimensions (player is 3 blocks tall, ~1.1 blocks wide)
    float m_capsuleRadius = 0.55f;      // Horizontal radius (requires 2-block gap, won't fit through 1)
    float m_capsuleHeight = 3.0f;       // Total height including caps
    float m_capsuleCylinderHeight = 1.9f; // Height of cylindrical portion (3.0 - 2*0.55)
    
    // Input state
    struct InputState {
        bool leftMousePressed = false;
        bool rightMousePressed = false;
        float raycastTimer = 0.0f;
        RayHit cachedTargetBlock;  // Cache raycast results for performance
    } m_inputState;
    
    // NEW: Element-based crafting system
    ElementQueue m_elementQueue;
    const BlockRecipe* m_lockedRecipe = nullptr;  // Recipe locked in for placement
    std::array<Element, 9> m_hotbarElements;  // Customizable hotbar (keys 1-9)
    
    // Client state
    bool m_initialized = false;
    bool m_debugMode = false;
    
    // ================================
    // INTERNAL METHODS
    // ================================
    
    /**
     * Initialize GLFW and create window
     */
    bool initializeWindow();
    
    /**
     * Initialize graphics systems (ImGui, etc.)
     */
    bool initializeGraphics();
    
    /**
     * Process keyboard and mouse input
     */
    void processKeyboard(float deltaTime);
    void processMouse(float deltaTime);
    void processBlockInteraction(float deltaTime);
    
    /**
     * Render the 3D world
     */
    void renderWorld();
    
    /**
     * Render shadow depth pass for cascaded shadow mapping
     */
    void renderShadowPass();
    
    /**
     * Handle camera movement and collision detection
     */
    void handleCameraMovement(float deltaTime);
    
    /**
     * Handle received world state from server
     */
    void handleWorldStateReceived(const WorldStateMessage& worldState);
    
    /**
     * Handle received compressed island data from server
     */
    void handleCompressedIslandReceived(uint32_t islandID, const Vec3& position, const uint8_t* voxelData, uint32_t dataSize);
    
    /**
     * Handle received compressed chunk data from server
     */
    void handleCompressedChunkReceived(uint32_t islandID, const Vec3& chunkCoord, const Vec3& islandPosition, const uint8_t* voxelData, uint32_t dataSize);
    
    /**
     * Handle received voxel change updates from server
     */
    void handleVoxelChangeReceived(const VoxelChangeUpdate& update);
    
    /**
     * Handle received entity state updates from server
     */
    void handleEntityStateUpdate(const EntityStateUpdate& update);
    
    /**
     * CRITICAL: Centralized spawn function - ONLY place where player position should be set
     * This ensures m_camera.position and m_physicsPosition stay in sync
     */
    void spawnPlayerAt(const Vec3& worldPosition);
    
    /**
     * Render waiting screen for remote clients
     */
    void renderWaitingScreen();
    
    /**
     * Render UI and debug info
     */
    void renderUI();
    
    /**
     * Handle window resize
     */
    void onWindowResize(int width, int height);

    // Lighting neighborhood tracking
    Vec3 m_lastChunkCoord{999999,999999,999999};

}; 
