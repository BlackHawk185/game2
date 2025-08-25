// main.cpp - Dynamic Floating Island Voxel Engine (Simplified)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <GLFW/glfw3.h>
#include <GL/gl.h>
#include <imgui.h>
#include <iostream>
#include <chrono>
#include <cstring>
#include <cctype>

#include "../World/VoxelChunk.h"
#include "../Rendering/Renderer.h"
#include "../Input/Camera.h"
#include "../Player.h"
#include "../World/IslandChunkSystem.h"
#include "../Threading/JobSystem.h"
#include "../Culling/FrustumCuller.h"
#include "../World/VoxelRaycaster.h"
#include "../Time/TimeManager.h"
#include "../Time/TimeEffects.h"

// Global player pointer for callbacks
Player* g_player = nullptr;

int main(int argc, char* argv[]) {
    // Parse command line arguments for future networking
    bool isClientOnly = false;
    std::string serverAddress = "";
    uint16_t serverPort = 7777;
    
    // Default behavior: Host own server and connect to it (integrated mode)
    // --client <address> [port]: Connect to remote server only (skip local server)
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--client") == 0 && i + 1 < argc) {
            isClientOnly = true;
            serverAddress = argv[i + 1];
            i++; // Skip next argument since we consumed it
            
            // Optional port argument
            if (i + 1 < argc && isdigit(argv[i + 1][0])) {
                serverPort = static_cast<uint16_t>(atoi(argv[i + 1]));
                i++; // Skip port argument
            }
        }
    }
    
    std::cout << "🏝️ Dynamic Floating Island Engine" << std::endl;
    if (isClientOnly) {
        std::cout << "🌐 Client Mode: Would connect to " << serverAddress << ":" << serverPort << " (networking not implemented yet)" << std::endl;
    } else {
        std::cout << "🖥️ Integrated Mode: Would host local server + client (networking not implemented yet)" << std::endl;
    }
    if (!glfwInit()) {
        std::cerr << "Failed to initialize GLFW!" << std::endl;
        return 1;
    }
    
    GLFWwindow* window = glfwCreateWindow(1280, 720, "🏝️ Dynamic Floating Island Engine", nullptr, nullptr);
    if (!window) {
        std::cerr << "Failed to create window!" << std::endl;
        glfwTerminate();
        return 1;
    }

    glfwMakeContextCurrent(window);
    glfwSwapInterval(1); // Enable vsync
    
    // **VIEWPORT SCALING CALLBACK**
    glfwSetFramebufferSizeCallback(window, [](GLFWwindow* window, int width, int height) {
        glViewport(0, 0, width, height);
        std::cout << "🖼️ Viewport resized to " << width << "x" << height << std::endl;
    });
    
    // **WINDOW CLOSE CALLBACK** - Ensure proper shutdown
    glfwSetWindowCloseCallback(window, [](GLFWwindow* window) {
        std::cout << "🚪 Window closing - initiating clean shutdown..." << std::endl;
        glfwSetWindowShouldClose(window, GLFW_TRUE);
    });
    
    // **ESCAPE KEY TO CLOSE & TIME MANIPULATION CONTROLS**
    glfwSetKeyCallback(window, [](GLFWwindow* window, int key, int scancode, int action, int mods) {
        if (action == GLFW_PRESS) {
            switch (key) {
                case GLFW_KEY_ESCAPE:
                    std::cout << "⌨️ Escape pressed - closing window..." << std::endl;
                    glfwSetWindowShouldClose(window, GLFW_TRUE);
                    break;
                    
                case GLFW_KEY_1:
                    if (g_timeEffects) {
                        g_timeEffects->activateSlowMotion(3.0f, 0.5f);
                    }
                    break;
                    
                case GLFW_KEY_2:
                    if (g_timeEffects) {
                        g_timeEffects->activateBulletTime(4.0f, 0.3f);
                    }
                    break;
                    
                case GLFW_KEY_3:
                    if (g_timeEffects) {
                        g_timeEffects->activateTimeFreeze(1.0f);
                    }
                    break;
                    
                case GLFW_KEY_4:
                    if (g_timeEffects) {
                        g_timeEffects->activateSpeedBoost(3.0f, 2.0f);
                    }
                    break;
                    
                case GLFW_KEY_5:
                    if (g_timeEffects && g_timeManager && g_player) {
                        // Create a temporal bubble at player position
                        Vec3 playerPos = g_player->getPosition();
                        g_timeEffects->createTemporalBubble("player_bubble", 
                            playerPos.x, playerPos.y, playerPos.z, 
                            15.0f, 0.2f, 5.0f);
                    }
                    break;
                    
                case GLFW_KEY_0:
                    if (g_timeEffects) {
                        g_timeEffects->stopAllEffects();
                    }
                    break;
                    
                case GLFW_KEY_T:
                    if (g_timeManager) {
                        g_timeManager->debugPrintTimeInfo();
                        if (g_timeEffects) {
                            g_timeEffects->debugPrintActiveEffects();
                        }
                    }
                    break;
            }
        }
    });

    // Set initial viewport
    int width, height;
    glfwGetFramebufferSize(window, &width, &height);
    glViewport(0, 0, width, height);

    // Initialize renderer
    if (!Renderer::initialize()) {
        std::cerr << "Failed to initialize renderer!" << std::endl;
        return 1;
    }
    
    // Physics are managed by IslandChunkSystem
    // (Island-specific physics initialization happens in IslandChunkSystem)
    
    // **INITIALIZE THREADING INFRASTRUCTURE**
    if (!g_jobSystem.initialize()) {
        std::cerr << "Failed to initialize job system!" << std::endl;
        return 1;
    }

    // **INITIALIZE TIME MANAGEMENT SYSTEM**
    g_timeManager = new TimeManager();
    g_timeEffects = new TimeEffects();

    // Initialize ImGui
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    ImGui::StyleColorsDark();

    // Create camera and player for physics-driven exploration
    Camera camera;
    Player player;
    g_player = &player;  // Set global pointer for callbacks
    
    // Create 3 islands in a triangle formation, close to each other
    uint32_t island1ID = g_islandSystem.createIsland(Vec3(0, 0, 0));      // Center island
    uint32_t island2ID = g_islandSystem.createIsland(Vec3(40, 5, 30));    // Right-forward island  
    uint32_t island3ID = g_islandSystem.createIsland(Vec3(-40, -5, 30));  // Left-forward island
    
    // Generate each island with different seeds for variety
    g_islandSystem.generateFloatingIsland(island1ID, 12345, 32.0f);
    g_islandSystem.generateFloatingIsland(island2ID, 54321, 32.0f);
    g_islandSystem.generateFloatingIsland(island3ID, 98765, 32.0f);
    
    // Calculate proper player spawn position on the center island
    // Island terrain is generated with center at (16, 10, 16) within the 32x32x32 chunk
    // Since chunk is positioned at (0,0,0), the island surface is around Y=10-22 in world coords
    // Place player at island center (16, 0, 16) + reasonable height above surface
    Vec3 islandCenter = g_islandSystem.getIslandCenter(island1ID);
    Vec3 playerSpawnPos = Vec3(16.0f, 16.0f, 16.0f);
    
    player.setPosition(playerSpawnPos);
    
    if (isClientOnly) {
        std::cout << "Engine initialized. Ready to connect to " << serverAddress << ":" << serverPort << " (when networking is implemented)" << std::endl;
        std::cout << "🎮 Controls: WASD+mouse to move, SPACE to jump, 1-5/0/T for time effects, ESC to exit." << std::endl;
    } else {
        std::cout << "Engine initialized. Ready for integrated mode (when networking is implemented)" << std::endl;
        std::cout << "🎮 Controls: WASD+mouse to move, SPACE to jump, 1-5/0/T for time effects, ESC to exit." << std::endl;
        std::cout << "💡 Use --client <address> [port] to connect to remote servers instead" << std::endl;
    }
    
    // Timing
    auto lastTime = std::chrono::high_resolution_clock::now();
    
    // Main loop - SHOW THE ISLAND!
    while (!glfwWindowShouldClose(window)) {
    // Calculate real deltaTime
    auto currentTime = std::chrono::high_resolution_clock::now();
    float realDeltaTime = std::chrono::duration<float>(currentTime - lastTime).count();
    lastTime = currentTime;
    // Clamp deltaTime to avoid physics lurches (max 0.05s = 20 FPS)
    if (realDeltaTime > 0.05f) realDeltaTime = 0.05f;
    
    // **UPDATE TIME SYSTEM** - This calculates scaled time for all systems
    g_timeManager->update(realDeltaTime);
    g_timeEffects->update(realDeltaTime);
    
    // Get scaled delta time for gameplay systems
    float gameplayDeltaTime = DELTA_TIME(GAMEPLAY);
        
        glfwPollEvents();
        
        // **DYNAMIC ASPECT RATIO** - Get current window size
        int windowWidth, windowHeight;
        glfwGetFramebufferSize(window, &windowWidth, &windowHeight);
        float aspect = (windowHeight > 0) ? (float)windowWidth / (float)windowHeight : 1.0f;
        
        // **UPDATE CAMERA FIRST** - Process mouse input before player movement to avoid frame delay
        // Use real delta time for camera (UI-like, unaffected by time scaling)
        camera.processInput(window, realDeltaTime);
        
        // **UPDATE PHYSICS**
        g_islandSystem.updateIslandPhysics(gameplayDeltaTime);
        g_islandSystem.syncPhysicsToChunks();
        
        // **UPDATE PLAYER PHYSICS**
        // Collect player input using current camera direction (now updated)
        Vec3 playerInput(0, 0, 0);
        if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) {
            playerInput = playerInput + camera.front;
        }
        if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) {
            playerInput = playerInput - camera.front;
        }
        if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) {
            playerInput = playerInput - camera.right;
        }
        if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) {
            playerInput = playerInput + camera.right;
        }
        bool jump = (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS);
        
        // Apply input to player using scaled time
        player.applyInput(playerInput, jump, gameplayDeltaTime);
        player.update(gameplayDeltaTime);
        
        // Update camera to follow player position (first-person)
        player.updateCameraFromPlayer(&camera);
        
        // **UPDATE FRUSTUM CULLING**
        g_frustumCuller.updateFromCamera(camera, aspect, 45.0f);
        
        // **BLOCK BREAKING/PLACING INPUT**
        static bool leftPressed = false, rightPressed = false;
        static RayHit cachedTargetBlock; // Cache the raycast result
        static float raycastTimer = 0.0f; // Timer for raycast frequency
        
        bool leftClick = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;
        bool rightClick = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS;
        
        // **PERFORMANCE FIX: Only raycast 10 times per second instead of every frame**
        raycastTimer += realDeltaTime;
        if (raycastTimer > 0.1f) { // 10 FPS raycasting
            cachedTargetBlock = VoxelRaycaster::raycast(camera.position, camera.front, 50.0f, &g_islandSystem);
            raycastTimer = 0.0f;
        }
        RayHit& targetBlock = cachedTargetBlock;
        
        // Only trigger on press (not hold)
        if (leftClick && !leftPressed) {
            leftPressed = true;
            
            if (targetBlock.hit) {
                g_islandSystem.setVoxelInIsland(targetBlock.islandID, targetBlock.localBlockPos, 0); // Remove block (set to air)
            }
        } else if (!leftClick) {
            leftPressed = false;
        }
        
        if (rightClick && !rightPressed) {
            rightPressed = true;
            
            if (targetBlock.hit) {
                // Place block adjacent to hit position (in direction of normal)
                Vec3 placePos = VoxelRaycaster::getPlacementPosition(targetBlock);
                
                // Check if placement position is valid
                int existingVoxel = g_islandSystem.getVoxelFromIsland(targetBlock.islandID, placePos);
                
                if (existingVoxel == 0) {
                    g_islandSystem.setVoxelInIsland(targetBlock.islandID, placePos, 1); // Place block
                }
            }
        } else if (!rightClick) {
            rightPressed = false;
        }
        
        // **DRAIN COMPLETED JOBS (Non-blocking)**
        std::vector<JobResult> completedJobs;
        g_jobSystem.drainCompletedJobs(completedJobs, 50);
        for (const auto& result : completedJobs) {
            // TODO: Process completed work (mesh uploads, physics updates, etc.)
            // For now, just count them
        }
        
        // Clear screen
        Renderer::clear();
        
        // Set up 3D projection
        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();
        float fov = 45.0f;
        float nearPlane = 0.1f;
        float farPlane = 1000.0f;
        
        // Manual perspective setup
        float top = nearPlane * tan(fov * 3.14159f / 360.0f);
        float bottom = -top;
        float right = top * aspect;
        float left = -right;
        glFrustum(left, right, bottom, top, nearPlane, farPlane);
        
        // Set up view matrix (camera) - NOW USING CLEAN VEC3 MATH!
        glMatrixMode(GL_MODELVIEW);
        glLoadIdentity();
        
        // Get proper view matrix from camera using new Vec3 system
        Mat4 viewMatrix = camera.getViewMatrix();
        glMultMatrixf(viewMatrix.data());
        
        // **RENDER THE PHYSICS-DRIVEN ISLANDS**
        g_islandSystem.renderAllIslands();
        
        // **RENDER BLOCK HIGHLIGHTER**
        if (targetBlock.hit) {
            // **ISLAND-CENTRIC HIGHLIGHTING**: Get the island's physics center
            FloatingIsland* island = g_islandSystem.getIsland(targetBlock.islandID);
            if (island) {
                Vec3 islandPhysicsPos = island->physicsCenter;
                
                // Draw wireframe box around targeted block IN ISLAND'S LOCAL SPACE
                glDisable(GL_DEPTH_TEST);
                glDisable(GL_LIGHTING);
                glColor3f(1.0f, 1.0f, 0.0f); // Yellow highlight
                glLineWidth(2.0f);
                
                // Render at: island physics position + local block position
                float x = islandPhysicsPos.x + targetBlock.localBlockPos.x;
                float y = islandPhysicsPos.y + targetBlock.localBlockPos.y;
                float z = islandPhysicsPos.z + targetBlock.localBlockPos.z;
                
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
        
        glfwSwapBuffers(window);
    }

    std::cout << "\n👋 Shutting down floating island engine..." << std::endl;
    
    // **SHUTDOWN TIME SYSTEM**
    delete g_timeEffects;
    delete g_timeManager;
    g_timeEffects = nullptr;
    g_timeManager = nullptr;
    
    // **SHUTDOWN THREADING INFRASTRUCTURE**
    g_jobSystem.shutdown();
    // Physics shutdown is handled by IslandChunkSystem destructor
    
    Renderer::shutdown();
    ImGui::DestroyContext();
    glfwDestroyWindow(window);
    glfwTerminate();
    
    return 0;
}
