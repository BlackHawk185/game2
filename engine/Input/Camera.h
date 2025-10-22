// Camera.h - Simple FPS camera for viewing voxel world
#pragma once
#include "Math/Vec3.h"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

class Camera {
public:
    Camera();
    
    void update(float deltaTime);
    void processInput(void* window, float deltaTime); // GLFWwindow* 
    
    void getViewMatrix(float* matrix);
    glm::mat4 getViewMatrix();
    void getProjectionMatrix(float* matrix, float aspect);
    glm::mat4 getProjectionMatrix(float aspect);
    
    // Position and orientation - Set by GameState on initialization
    Vec3 position;
    Vec3 front{0.0f, 0.0f, -1.0f};
    Vec3 up{0.0f, 1.0f, 0.0f};
    Vec3 right{1.0f, 0.0f, 0.0f};
    
    // Camera settings - Point toward origin to see islands
    float yaw = 0.0f;     // Point north initially  
    float pitch = -30.0f; // Look down slightly to see islands below
    float speed = 15.0f;  // Faster movement to explore the sphere
    float sensitivity = 0.1f; // Mouse sensitivity
    
    // Public method to update camera vectors when yaw/pitch are modified externally
    void updateCameraVectors();

private:
    bool firstMouse = true;
    float lastX = 640.0f, lastY = 360.0f;
};
