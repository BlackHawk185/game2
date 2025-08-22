// Camera.cpp - FPS Camera with Vec3 math system
#include "Camera.h"
#include <GLFW/glfw3.h>
#define _USE_MATH_DEFINES
#include <cmath>
#include <iostream>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

Camera::Camera() {
    updateCameraVectors();
}

void Camera::updateCameraVectors() {
    // Calculate front vector from yaw and pitch using Vec3 - much cleaner!
    Vec3 newFront;
    newFront.x = cos(yaw * M_PI / 180.0f) * cos(pitch * M_PI / 180.0f);
    newFront.y = sin(pitch * M_PI / 180.0f);
    newFront.z = sin(yaw * M_PI / 180.0f) * cos(pitch * M_PI / 180.0f);
    front = newFront.normalized();
    
    // Calculate right and up vectors using Vec3 cross products
    Vec3 worldUp(0.0f, 1.0f, 0.0f);
    right = front.cross(worldUp).normalized();
    up = right.cross(front).normalized();
}

void Camera::processInput(void* window, float deltaTime) {
    GLFWwindow* glfwWindow = (GLFWwindow*)window;
    
    float velocity = speed * deltaTime;
    
    // Movement with Vec3 math - so much cleaner!
    if (glfwGetKey(glfwWindow, GLFW_KEY_W) == GLFW_PRESS) {
        position += front * velocity;
    }
    if (glfwGetKey(glfwWindow, GLFW_KEY_S) == GLFW_PRESS) {
        position -= front * velocity;
    }
    if (glfwGetKey(glfwWindow, GLFW_KEY_A) == GLFW_PRESS) {
        position -= right * velocity;
    }
    if (glfwGetKey(glfwWindow, GLFW_KEY_D) == GLFW_PRESS) {
        position += right * velocity;
    }
    // Mouse look control - Always on! ESC to release for UI access
    
    static bool mouseGrabbed = true; // Start with mouse grabbed
    static bool firstRun = true;
    
    // Initialize mouse capture on first run
    if (firstRun) {
        glfwSetInputMode(glfwWindow, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
        firstRun = false;
        firstMouse = true;
    }
    
    if (glfwGetKey(glfwWindow, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
        mouseGrabbed = false;
        glfwSetInputMode(glfwWindow, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
    }
    
    // Always process mouse look when grabbed (which is most of the time)
    if (mouseGrabbed) {
        double mouseX, mouseY;
        glfwGetCursorPos(glfwWindow, &mouseX, &mouseY);
        
        if (firstMouse) {
            lastX = mouseX;
            lastY = mouseY;
            firstMouse = false;
        }
        
        float xOffset = mouseX - lastX;
        float yOffset = lastY - mouseY; // Reversed since y-coordinates go from bottom to top
        lastX = mouseX;
        lastY = mouseY;
        
        xOffset *= sensitivity;
        yOffset *= sensitivity;
        
        yaw += xOffset;
        pitch += yOffset;
        
        // Constrain pitch
        if (pitch > 89.0f) pitch = 89.0f;
        if (pitch < -89.0f) pitch = -89.0f;
        
        updateCameraVectors();
    }
}

// Old array-based interface for compatibility
void Camera::getViewMatrix(float* matrix) {
    Mat4 viewMat = getViewMatrix();
    for (int i = 0; i < 16; i++) {
        matrix[i] = viewMat.data()[i];
    }
}

// New clean Vec3-based view matrix
Mat4 Camera::getViewMatrix() {
    Vec3 center = position + front;
    return Mat4::lookAt(position, center, up);
}

// Old array-based interface for compatibility  
void Camera::getProjectionMatrix(float* matrix, float aspect) {
    Mat4 projMat = getProjectionMatrix(aspect);
    for (int i = 0; i < 16; i++) {
        matrix[i] = projMat.data()[i];
    }
}

// New clean Vec3-based projection matrix
Mat4 Camera::getProjectionMatrix(float aspect) {
    float fov = 45.0f * M_PI / 180.0f; // Convert to radians
    return Mat4::perspective(fov, aspect, 0.1f, 1000.0f);
}

void Camera::update(float deltaTime) {
    // Update camera state if needed
}
