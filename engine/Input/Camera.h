// Camera.h - Simple FPS camera for viewing voxel world
#pragma once
#include "Math/Vec3.h"

class Camera {
public:
    Camera();
    
    void update(float deltaTime);
    void processInput(void* window, float deltaTime); // GLFWwindow* 
    
    void getViewMatrix(float* matrix);
    Mat4 getViewMatrix();  // New clean version
    void getProjectionMatrix(float* matrix, float aspect);
    Mat4 getProjectionMatrix(float aspect); // New clean version
    
    // Position and orientation - now using Vec3!
    Vec3 position{16.0f, 32.0f, 16.0f}; // Start ON TOP of the island surface!
    Vec3 front{0.0f, 0.0f, -1.0f};
    Vec3 up{0.0f, 1.0f, 0.0f};
    Vec3 right{1.0f, 0.0f, 0.0f};
    
    // Camera settings - Point forward for first-person gameplay
    float yaw = 0.0f;     // Point north initially
    float pitch = 0.0f;   // Look straight ahead for first-person exploration
    float speed = 15.0f;  // Faster movement to explore the sphere
    float sensitivity = 0.1f; // Mouse sensitivity
    
    // Public method to update camera vectors when yaw/pitch are modified externally
    void updateCameraVectors();

private:
    bool firstMouse = true;
    float lastX = 640.0f, lastY = 360.0f;
};
