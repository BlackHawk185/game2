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
    
    // Position and orientation - now using Vec3!
    Vec3 position{0.0f, 50.0f, 50.0f}; // Start high up and back to see islands
    Vec3 front{0.0f, 0.0f, -1.0f};
    Vec3 up{0.0f, 1.0f, 0.0f};
    Vec3 right{1.0f, 0.0f, 0.0f};
    
    // Camera settings - Point toward origin to see islands
    float yaw = 0.0f;     // Point north initially  
    float pitch = -30.0f; // Look down slightly to see islands below
    float speed = 15.0f;  // Faster movement to explore the sphere
    float sensitivity = 0.1f; // Mouse sensitivity
    float fov = 45.0f;    // Field of view
    float nearPlane = 0.1f;
    float farPlane = 1000.0f;
    
    // Getters for CSM system
    glm::vec3 getPosition() const { return glm::vec3(position.x, position.y, position.z); }
    glm::vec3 getFront() const { return glm::vec3(front.x, front.y, front.z); }
    glm::vec3 getRight() const { return glm::vec3(right.x, right.y, right.z); }
    glm::vec3 getUp() const { return glm::vec3(up.x, up.y, up.z); }
    float getFOV() const { return glm::radians(fov); }
    float getNearPlane() const { return nearPlane; }
    float getFarPlane() const { return farPlane; }
    float getAspectRatio() const { return 16.0f / 9.0f; } // TODO: Make this configurable
    
    // Public method to update camera vectors when yaw/pitch are modified externally
    void updateCameraVectors();

private:
    bool firstMouse = true;
    float lastX = 640.0f, lastY = 360.0f;
};
