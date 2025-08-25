// GameClient.cpp - Client-side rendering and input implementation
#include "GameClient.h"
#include "GameState.h"
#include "../Rendering/Renderer.h"
#include "../Time/TimeEffects.h"
#include <GLFW/glfw3.h>
#include <GL/gl.h>
#include <imgui.h>
#include <iostream>

// External systems that we'll refactor later
extern TimeEffects* g_timeEffects;

GameClient::GameClient() {
    // Constructor
}

GameClient::~GameClient() {
    shutdown();
}

bool GameClient::initialize() {
    if (m_initialized) {
        std::cerr << "GameClient already initialized!" << std::endl;
        return false;
    }
    
    std::cout << "🖼️  Initializing GameClient..." << std::endl;
    
    // Initialize window and graphics
    if (!initializeWindow()) {
        return false;
    }
    
    if (!initializeGraphics()) {
        return false;
    }
    
    m_initialized = true;
    std::cout << "✅ GameClient initialized successfully" << std::endl;
    return true;
}

bool GameClient::connectToGameState(GameState* gameState) {
    if (!gameState) {
        std::cerr << "Cannot connect to null game state!" << std::endl;
        return false;
    }
    
    m_gameState = gameState;
    
    // Set up camera position based on game state
    if (m_gameState->getPrimaryPlayer()) {
        Vec3 playerPos = m_gameState->getPrimaryPlayer()->getPosition();
        m_camera.position = playerPos;
    }
    
    std::cout << "🔗 GameClient connected to game state" << std::endl;
    return true;
}

bool GameClient::update(float deltaTime) {
    if (!m_initialized || !m_gameState) {
        return false;
    }
    
    // Poll window events
    glfwPollEvents();
    
    // Check if window should close
    if (shouldClose()) {
        return false;
    }
    
    // Process input
    processInput(deltaTime);
    
    // Render frame
    render();
    
    // Swap buffers
    glfwSwapBuffers(m_window);
    
    return true;
}

void GameClient::shutdown() {
    if (!m_initialized) {
        return;
    }
    
    std::cout << "🔄 Shutting down GameClient..." << std::endl;
    
    // Disconnect from game state
    m_gameState = nullptr;
    
    // Cleanup graphics
    if (m_window) {
        glfwDestroyWindow(m_window);
        m_window = nullptr;
    }
    
    glfwTerminate();
    
    m_initialized = false;
    std::cout << "✅ GameClient shutdown complete" << std::endl;
}

bool GameClient::shouldClose() const {
    return m_window && glfwWindowShouldClose(m_window);
}

void GameClient::processInput(float deltaTime) {
    if (!m_window || !m_gameState) {
        return;
    }
    
    processKeyboard(deltaTime);
    processMouse(deltaTime);
    processBlockInteraction(deltaTime);
}

void GameClient::render() {
    if (!m_gameState) {
        return;
    }
    
    // Clear screen
    Renderer::clear();
    
    // Set up 3D projection
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    
    float aspect = (float)m_windowWidth / (float)m_windowHeight;
    float fov = 45.0f;
    float nearPlane = 0.1f;
    float farPlane = 1000.0f;
    
    // Manual perspective setup
    float top = nearPlane * tan(fov * 3.14159f / 360.0f);
    float bottom = -top;
    float right = top * aspect;
    float left = -right;
    glFrustum(left, right, bottom, top, nearPlane, farPlane);
    
    // Set up view matrix
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    
    Mat4 viewMatrix = m_camera.getViewMatrix();
    glMultMatrixf(viewMatrix.data());
    
    // Update frustum culling
    m_frustumCuller.updateFromCamera(m_camera, aspect, 45.0f);
    
    // Render world
    renderWorld();
    
    // Render UI
    renderUI();
}

bool GameClient::initializeWindow() {
    if (!glfwInit()) {
        std::cerr << "Failed to initialize GLFW!" << std::endl;
        return false;
    }
    
    // Create window
    m_window = glfwCreateWindow(m_windowWidth, m_windowHeight, "MMORPG Engine - Client", nullptr, nullptr);
    if (!m_window) {
        std::cerr << "Failed to create GLFW window!" << std::endl;
        glfwTerminate();
        return false;
    }
    
    glfwMakeContextCurrent(m_window);
    
    // Set callbacks
    glfwSetWindowUserPointer(m_window, this);
    glfwSetWindowSizeCallback(m_window, windowResizeCallback);
    
    // Set up mouse capture
    glfwSetInputMode(m_window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    
    return true;
}

bool GameClient::initializeGraphics() {
    // Initialize renderer
    if (!Renderer::initialize()) {
        std::cerr << "Failed to initialize renderer!" << std::endl;
        return false;
    }
    
    // Initialize ImGui
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    ImGui::StyleColorsDark();
    
    return true;
}

void GameClient::processKeyboard(float deltaTime) {
    // Camera movement
    float moveSpeed = 10.0f;
    Vec3 movement(0, 0, 0);
    
    if (glfwGetKey(m_window, GLFW_KEY_W) == GLFW_PRESS) {
        movement = movement + m_camera.front * moveSpeed * deltaTime;
    }
    if (glfwGetKey(m_window, GLFW_KEY_S) == GLFW_PRESS) {
        movement = movement - m_camera.front * moveSpeed * deltaTime;
    }
    if (glfwGetKey(m_window, GLFW_KEY_A) == GLFW_PRESS) {
        movement = movement - m_camera.right * moveSpeed * deltaTime;
    }
    if (glfwGetKey(m_window, GLFW_KEY_D) == GLFW_PRESS) {
        movement = movement + m_camera.right * moveSpeed * deltaTime;
    }
    if (glfwGetKey(m_window, GLFW_KEY_SPACE) == GLFW_PRESS) {
        movement = movement + m_camera.up * moveSpeed * deltaTime;
    }
    if (glfwGetKey(m_window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS) {
        movement = movement - m_camera.up * moveSpeed * deltaTime;
    }
    
    // Apply movement
    m_camera.position = m_camera.position + movement;
    
    // Update player position in game state
    if (m_gameState->getPrimaryPlayer()) {
        m_gameState->setPrimaryPlayerPosition(m_camera.position);
    }
    
    // Time effects (temporary, will be refactored)
    if (g_timeEffects) {
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
    if (glfwGetKey(m_window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
        glfwSetWindowShouldClose(m_window, GLFW_TRUE);
    }
}

void GameClient::processMouse(float deltaTime) {
    static double lastX = m_windowWidth / 2.0;
    static double lastY = m_windowHeight / 2.0;
    static bool firstMouse = true;
    
    double xpos, ypos;
    glfwGetCursorPos(m_window, &xpos, &ypos);
    
    if (firstMouse) {
        lastX = xpos;
        lastY = ypos;
        firstMouse = false;
    }
    
    float xoffset = (float)(xpos - lastX);
    float yoffset = (float)(lastY - ypos);  // Reversed since y-coordinates go from bottom to top
    lastX = xpos;
    lastY = ypos;
    
    float sensitivity = 0.1f;
    xoffset *= sensitivity;
    yoffset *= sensitivity;
    
    m_camera.yaw += xoffset;
    m_camera.pitch += yoffset;
    
    // Constrain pitch
    if (m_camera.pitch > 89.0f) m_camera.pitch = 89.0f;
    if (m_camera.pitch < -89.0f) m_camera.pitch = -89.0f;
    
    // Camera vectors are updated internally by the Camera class
    // m_camera.updateCameraVectors();
}

void GameClient::processBlockInteraction(float deltaTime) {
    if (!m_gameState) {
        return;
    }
    
    // Update raycast timer for performance
    m_inputState.raycastTimer += deltaTime;
    if (m_inputState.raycastTimer > 0.1f) {  // 10 FPS raycasting
        m_inputState.cachedTargetBlock = VoxelRaycaster::raycast(
            m_camera.position, m_camera.front, 50.0f, 
            m_gameState->getIslandSystem()
        );
        m_inputState.raycastTimer = 0.0f;
    }
    
    bool leftClick = glfwGetMouseButton(m_window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;
    bool rightClick = glfwGetMouseButton(m_window, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS;
    
    // Left click - break block
    if (leftClick && !m_inputState.leftMousePressed) {
        m_inputState.leftMousePressed = true;
        
        if (m_inputState.cachedTargetBlock.hit) {
            m_gameState->setVoxel(m_inputState.cachedTargetBlock.islandID, 
                                  m_inputState.cachedTargetBlock.localBlockPos, 0);
        }
    } else if (!leftClick) {
        m_inputState.leftMousePressed = false;
    }
    
    // Right click - place block
    if (rightClick && !m_inputState.rightMousePressed) {
        m_inputState.rightMousePressed = true;
        
        if (m_inputState.cachedTargetBlock.hit) {
            Vec3 placePos = VoxelRaycaster::getPlacementPosition(m_inputState.cachedTargetBlock);
            uint8_t existingVoxel = m_gameState->getVoxel(m_inputState.cachedTargetBlock.islandID, placePos);
            
            if (existingVoxel == 0) {
                m_gameState->setVoxel(m_inputState.cachedTargetBlock.islandID, placePos, 1);
            }
        }
    } else if (!rightClick) {
        m_inputState.rightMousePressed = false;
    }
}

void GameClient::renderWorld() {
    if (!m_gameState) {
        return;
    }
    
    // Render all islands
    m_gameState->getIslandSystem()->renderAllIslands();
    
    // Render block highlighter
    if (m_inputState.cachedTargetBlock.hit) {
        auto* island = m_gameState->getIslandSystem()->getIsland(m_inputState.cachedTargetBlock.islandID);
        if (island) {
            Vec3 islandPhysicsPos = island->physicsCenter;
            
            // Draw wireframe box around targeted block
            glDisable(GL_DEPTH_TEST);
            glDisable(GL_LIGHTING);
            glColor3f(1.0f, 1.0f, 0.0f); // Yellow highlight
            glLineWidth(2.0f);
            
            float x = islandPhysicsPos.x + m_inputState.cachedTargetBlock.localBlockPos.x;
            float y = islandPhysicsPos.y + m_inputState.cachedTargetBlock.localBlockPos.y;
            float z = islandPhysicsPos.z + m_inputState.cachedTargetBlock.localBlockPos.z;
            
            glBegin(GL_LINES);
            // Bottom face
            glVertex3f(x, y, z);       glVertex3f(x+1, y, z);
            glVertex3f(x+1, y, z);     glVertex3f(x+1, y, z+1);
            glVertex3f(x+1, y, z+1);   glVertex3f(x, y, z+1);
            glVertex3f(x, y, z+1);     glVertex3f(x, y, z);
            
            // Top face
            glVertex3f(x, y+1, z);     glVertex3f(x+1, y+1, z);
            glVertex3f(x+1, y+1, z);   glVertex3f(x+1, y+1, z+1);
            glVertex3f(x+1, y+1, z+1); glVertex3f(x, y+1, z+1);
            glVertex3f(x, y+1, z+1);   glVertex3f(x, y+1, z);
            
            // Vertical edges
            glVertex3f(x, y, z);       glVertex3f(x, y+1, z);
            glVertex3f(x+1, y, z);     glVertex3f(x+1, y+1, z);
            glVertex3f(x+1, y, z+1);   glVertex3f(x+1, y+1, z+1);
            glVertex3f(x, y, z+1);     glVertex3f(x, y+1, z+1);
            glEnd();
            
            glEnable(GL_DEPTH_TEST);
            glColor3f(1.0f, 1.0f, 1.0f); // Reset color
        }
    }
}

void GameClient::renderUI() {
    // TODO: Implement ImGui UI rendering
    // This will show server connection status, performance metrics, etc.
}

void GameClient::onWindowResize(int width, int height) {
    m_windowWidth = width;
    m_windowHeight = height;
    glViewport(0, 0, width, height);
}

void GameClient::windowResizeCallback(GLFWwindow* window, int width, int height) {
    GameClient* client = static_cast<GameClient*>(glfwGetWindowUserPointer(window));
    if (client) {
        client->onWindowResize(width, height);
    }
}
