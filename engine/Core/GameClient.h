// GameClient.h - Client-side rendering and input handling
// This class manages the presentation layer and user interaction
#pragma once

#include "../Math/Vec3.h"
#include "../Input/Camera.h"
#include "../Culling/FrustumCuller.h"
#include "../World/VoxelRaycaster.h"
#include <memory>

// Forward declarations
class GameState;
struct GLFWwindow;

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
    bool initialize();
    
    /**
     * Connect to a game state (local or remote)
     * @param gameState - The game state to render (can be local server)
     */
    bool connectToGameState(GameState* gameState);
    
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
    // Graphics context
    GLFWwindow* m_window = nullptr;
    int m_windowWidth = 1280;
    int m_windowHeight = 720;
    
    // Game state connection
    GameState* m_gameState = nullptr;  // Not owned by client
    
    // Rendering systems
    Camera m_camera;
    FrustumCuller m_frustumCuller;
    
    // Input state
    struct InputState {
        bool leftMousePressed = false;
        bool rightMousePressed = false;
        float raycastTimer = 0.0f;
        RayHit cachedTargetBlock;  // Cache raycast results for performance
    } m_inputState;
    
    // Client state
    bool m_initialized = false;
    
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
     * Render UI and debug info
     */
    void renderUI();
    
    /**
     * Handle window resize
     */
    void onWindowResize(int width, int height);
    
    // Static callback for GLFW
    static void windowResizeCallback(GLFWwindow* window, int width, int height);
};
