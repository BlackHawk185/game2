// PlayerController.cpp - Unified player input, physics, and camera control
#include "PlayerController.h"

#include <GLFW/glfw3.h>
#include <cmath>
#include <iostream>

#include "../World/IslandChunkSystem.h"

PlayerController::PlayerController()
{
    // Initialize camera with default values
}

PlayerController::~PlayerController()
{
}

void PlayerController::initialize(const Vec3& initialPosition)
{
    // Set initial position
    m_physicsPosition = initialPosition;
    m_camera.position = initialPosition + Vec3(0.0f, m_eyeHeightOffset, 0.0f);
    
    // Initialize velocities
    m_playerVelocity = Vec3(0.0f, 0.0f, 0.0f);
    m_isGrounded = false;
    m_jumpPressed = false;
}

void PlayerController::update(GLFWwindow* window, float deltaTime, IslandChunkSystem* islandSystem)
{
    if (m_noclipMode)
    {
        updateNoclip(window, deltaTime);
    }
    else
    {
        updatePhysics(window, deltaTime, islandSystem);
    }
    
    // Always update camera position based on physics position
    updateCameraPosition(deltaTime);
}

void PlayerController::processMouse(GLFWwindow* window)
{
    if (m_uiBlocking)
    {
        return; // Don't process mouse if UI is open
    }
    
    static bool firstMouse = true;
    static double lastX = 640.0;
    static double lastY = 360.0;
    
    double mouseX, mouseY;
    glfwGetCursorPos(window, &mouseX, &mouseY);
    
    if (firstMouse)
    {
        lastX = mouseX;
        lastY = mouseY;
        firstMouse = false;
        return;
    }
    
    float xOffset = static_cast<float>(mouseX - lastX);
    float yOffset = static_cast<float>(lastY - mouseY); // Reversed for correct Y axis
    lastX = mouseX;
    lastY = mouseY;
    
    xOffset *= m_camera.sensitivity;
    yOffset *= m_camera.sensitivity;
    
    m_camera.yaw += xOffset;
    m_camera.pitch += yOffset;
    
    // Constrain pitch to prevent gimbal lock
    if (m_camera.pitch > 89.0f)
        m_camera.pitch = 89.0f;
    if (m_camera.pitch < -89.0f)
        m_camera.pitch = -89.0f;
    
    // Update camera vectors based on new yaw/pitch
    m_camera.updateCameraVectors();
}

Vec3 PlayerController::getEyePosition() const
{
    return m_physicsPosition + Vec3(0.0f, m_eyeHeightOffset, 0.0f);
}

void PlayerController::setPosition(const Vec3& position)
{
    m_physicsPosition = position;
    m_camera.position = position + Vec3(0.0f, m_eyeHeightOffset, 0.0f);
    m_playerVelocity = Vec3(0.0f, 0.0f, 0.0f);
}

void PlayerController::updateNoclip(GLFWwindow* window, float deltaTime)
{
    // Free-flying movement for debugging
    Vec3 movement(0, 0, 0);
    
    if (!m_uiBlocking)
    {
        float flySpeed = 30.0f;
        
        if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
            movement = movement + m_camera.front * flySpeed * deltaTime;
        if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
            movement = movement - m_camera.front * flySpeed * deltaTime;
        if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
            movement = movement - m_camera.right * flySpeed * deltaTime;
        if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
            movement = movement + m_camera.right * flySpeed * deltaTime;
        if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS)
            movement = movement + m_camera.up * flySpeed * deltaTime;
        if (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS)
            movement = movement - m_camera.up * flySpeed * deltaTime;
    }
    
    m_physicsPosition = m_physicsPosition + movement;
    m_camera.position = m_physicsPosition; // No eye offset in noclip
}

void PlayerController::updatePhysics(GLFWwindow* window, float deltaTime, IslandChunkSystem* islandSystem)
{
    (void)islandSystem; // Reserved for future island-specific physics interactions
    
    // ==========================================
    // PHASE 1: GATHER INPUT
    // ==========================================
    
    Vec3 inputDirection = getInputDirection(window);
    
    // Check for jump input
    bool jumpThisFrame = false;
    if (!m_uiBlocking)
    {
        jumpThisFrame = glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS;
    }
    
    // ==========================================
    // PHASE 2: DETECT GROUND STATE
    // ==========================================
    
    const float raycastMargin = 0.1f;
    GroundInfo groundInfo = g_physics.detectGroundCapsule(m_physicsPosition, m_capsuleRadius,
                                                          m_capsuleHeight, raycastMargin);
    m_isGrounded = groundInfo.isGrounded;
    
    // ==========================================
    // PHASE 3: APPLY PHYSICS
    // ==========================================
    
    // Apply gravity
    m_playerVelocity.y -= m_gravity * deltaTime;
    
    // Ground physics and jumping
    if (m_isGrounded)
    {
        // Stop falling when on ground
        if (m_playerVelocity.y < 0)
        {
            m_playerVelocity.y = 0;
        }
        
        // Handle jump
        if (jumpThisFrame && !m_jumpPressed)
        {
            m_playerVelocity.y = m_jumpStrength;
        }
        
        // Apply ground friction
        m_playerVelocity.x *= m_groundFriction;
        m_playerVelocity.z *= m_groundFriction;
    }
    else
    {
        // Check for climbing - if holding space in air and moving into a wall
        if (jumpThisFrame && inputDirection.lengthSquared() > 0.01f)
        {
            // Try moving forward slightly to detect wall collision
            Vec3 forwardTest = m_physicsPosition + (inputDirection.normalized() * 0.3f);
            Vec3 climbNormal;
            if (g_physics.checkCapsuleCollision(forwardTest, m_capsuleRadius, m_capsuleHeight, climbNormal, nullptr))
            {
                // Wall detected! Check if we can climb over it (max 3 blocks)
                // Check 1 block forward and 1 block above player's top
                Vec3 climbCheckPos = m_physicsPosition + (inputDirection.normalized() * 1.0f);
                climbCheckPos.y += (m_capsuleHeight * 0.5f) + 1.0f; // Top of capsule + 1 block
                
                Vec3 topCheckNormal;
                if (!g_physics.checkCapsuleCollision(climbCheckPos, m_capsuleRadius, m_capsuleHeight, topCheckNormal, nullptr))
                {
                    // No obstruction above - can climb!
                    m_playerVelocity.y = m_climbSpeed;
                }
                // else: Wall is too tall (>3 blocks), don't allow climbing
            }
        }
        
        // Apply air resistance
        m_playerVelocity.x *= m_airFriction;
        m_playerVelocity.z *= m_airFriction;
    }
    
    m_jumpPressed = jumpThisFrame;
    
    // Apply input acceleration
    float controlStrength = m_isGrounded ? 1.0f : m_airControl;
    Vec3 targetHorizontalVelocity = inputDirection * m_moveSpeed;
    Vec3 currentHorizontalVelocity = Vec3(m_playerVelocity.x, 0, m_playerVelocity.z);
    
    Vec3 velocityDelta = (targetHorizontalVelocity - currentHorizontalVelocity) * controlStrength * 10.0f * deltaTime;
    m_playerVelocity.x += velocityDelta.x;
    m_playerVelocity.z += velocityDelta.z;
    
    // Calculate intended movement
    Vec3 intendedMovement = m_playerVelocity * deltaTime;
    
    // ==========================================
    // PHASE 4: COLLISION DETECTION
    // ==========================================
    
    Vec3 intendedPosition = m_physicsPosition + intendedMovement;
    
    Vec3 collisionNormal;
    const FloatingIsland* collidingIsland = nullptr;
    if (g_physics.checkCapsuleCollision(intendedPosition, m_capsuleRadius, m_capsuleHeight,
                                       collisionNormal, &collidingIsland))
    {
        // Collision detected - use axis-separated movement
        Vec3 relativeMovement = intendedMovement;
        
        if (collidingIsland)
        {
            // Move relative to the island to prevent clipping through moving walls
            // Only apply to horizontal (X/Z) axes to avoid choppy vertical movement (jumping/climbing)
            Vec3 islandMovement = collidingIsland->velocity * deltaTime;
            relativeMovement.x -= islandMovement.x;
            relativeMovement.z -= islandMovement.z;
            // relativeMovement.y stays unchanged - vertical movement should be independent
        }
        
        // Try axis-separated movement
        // Process Y first to allow jumping/climbing to work smoothly
        Vec3 testPos;
        
        // Try Y (vertical movement - jumping/falling/climbing)
        testPos = m_physicsPosition + Vec3(0, relativeMovement.y, 0);
        if (!g_physics.checkCapsuleCollision(testPos, m_capsuleRadius, m_capsuleHeight, collisionNormal, nullptr))
        {
            m_physicsPosition = testPos;
        }
        else
        {
            m_playerVelocity.y = 0;
        }
        
        // Try X (with step-up if blocked)
        testPos = m_physicsPosition + Vec3(relativeMovement.x, 0, 0);
        if (!g_physics.checkCapsuleCollision(testPos, m_capsuleRadius, m_capsuleHeight, collisionNormal, nullptr))
        {
            m_physicsPosition = testPos;
        }
        else if (m_isGrounded) // Only step up when grounded
        {
            // Try stepping up - raise by max step height and try horizontal movement again
            Vec3 steppedPos = m_physicsPosition + Vec3(relativeMovement.x, m_maxStepHeight, 0);
            if (!g_physics.checkCapsuleCollision(steppedPos, m_capsuleRadius, m_capsuleHeight, collisionNormal, nullptr))
            {
                // Check if there's ground beneath the stepped position (within step height range)
                GroundInfo stepGroundInfo = g_physics.detectGroundCapsule(steppedPos, m_capsuleRadius, m_capsuleHeight, m_maxStepHeight + 0.5f);
                if (stepGroundInfo.isGrounded && stepGroundInfo.distanceToGround <= (m_maxStepHeight + 0.1f))
                {
                    // Valid step-up! Place player on top of the step
                    std::cout << "STEP-UP: distance=" << stepGroundInfo.distanceToGround << std::endl;
                    m_physicsPosition = steppedPos;
                    m_physicsPosition.y -= stepGroundInfo.distanceToGround; // Settle onto the step surface
                }
                else
                {
                    // No ground beneath - block movement
                    std::cout << "STEP-UP FAILED: grounded=" << stepGroundInfo.isGrounded << " distance=" << stepGroundInfo.distanceToGround << std::endl;
                    m_playerVelocity.x = 0;
                }
            }
            else
            {
                // Can't step up - blocked
                std::cout << "STEP-UP BLOCKED: collision at stepped position" << std::endl;
                m_playerVelocity.x = 0;
            }
        }
        else
        {
            std::cout << "NOT GROUNDED - no step-up attempt" << std::endl;
            m_playerVelocity.x = 0;
        }
        
        // Try Z (with step-up if blocked)
        testPos = m_physicsPosition + Vec3(0, 0, relativeMovement.z);
        if (!g_physics.checkCapsuleCollision(testPos, m_capsuleRadius, m_capsuleHeight, collisionNormal, nullptr))
        {
            m_physicsPosition = testPos;
        }
        else if (m_isGrounded) // Only step up when grounded
        {
            // Try stepping up - raise by max step height and try horizontal movement again
            Vec3 steppedPos = m_physicsPosition + Vec3(0, m_maxStepHeight, relativeMovement.z);
            if (!g_physics.checkCapsuleCollision(steppedPos, m_capsuleRadius, m_capsuleHeight, collisionNormal, nullptr))
            {
                // Check if there's ground beneath the stepped position (within step height range)
                GroundInfo stepGroundInfo = g_physics.detectGroundCapsule(steppedPos, m_capsuleRadius, m_capsuleHeight, m_maxStepHeight + 0.5f);
                if (stepGroundInfo.isGrounded && stepGroundInfo.distanceToGround <= (m_maxStepHeight + 0.1f))
                {
                    // Valid step-up! Place player on top of the step
                    m_physicsPosition = steppedPos;
                    m_physicsPosition.y -= stepGroundInfo.distanceToGround; // Settle onto the step surface
                }
                else
                {
                    // No ground beneath - block movement
                    m_playerVelocity.z = 0;
                }
            }
            else
            {
                // Can't step up - blocked
                m_playerVelocity.z = 0;
            }
        }
        else
        {
            m_playerVelocity.z = 0;
        }
        
        // Apply island movement
        if (collidingIsland)
        {
            m_physicsPosition = m_physicsPosition + (collidingIsland->velocity * deltaTime);
        }
    }
    else
    {
        // No collision - move freely
        m_physicsPosition = intendedPosition;
        
        // If grounded, move with the island
        if (m_isGrounded && groundInfo.standingOnIslandID != 0)
        {
            m_physicsPosition = m_physicsPosition + (groundInfo.groundVelocity * deltaTime);
        }
    }
    
    // ==========================================
    // PHASE 5: UPDATE PILOTING STATE
    // ==========================================
    
    if (m_isGrounded)
    {
        m_pilotedIslandID = groundInfo.standingOnIslandID;
    }
    else
    {
        if (!m_isPiloting)
        {
            m_pilotedIslandID = 0;
        }
    }
}

Vec3 PlayerController::getInputDirection(GLFWwindow* window) const
{
    Vec3 inputDirection(0, 0, 0);
    
    if (m_uiBlocking)
    {
        return inputDirection; // No input if UI is blocking
    }
    
    // If piloting and grounded, A/D rotate the vehicle
    if (m_isPiloting && m_isGrounded && m_pilotedIslandID != 0)
    {
        // Only W/S for forward/backward when piloting
        if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
        {
            inputDirection = inputDirection + m_camera.front;
        }
        if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
        {
            inputDirection = inputDirection - m_camera.front;
        }
        // Note: A/D handled separately for rotation in GameClient
    }
    else
    {
        // Normal WASD movement
        if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
        {
            inputDirection = inputDirection + m_camera.front;
        }
        if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
        {
            inputDirection = inputDirection - m_camera.front;
        }
        if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
        {
            inputDirection = inputDirection - m_camera.right;
        }
        if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
        {
            inputDirection = inputDirection + m_camera.right;
        }
    }
    
    // Flatten to horizontal plane
    inputDirection.y = 0;
    if (inputDirection.length() > 0.001f)
    {
        inputDirection = inputDirection.normalized();
    }
    
    return inputDirection;
}

void PlayerController::updateCameraPosition(float deltaTime)
{
    Vec3 eyePosition = m_physicsPosition + Vec3(0.0f, m_eyeHeightOffset, 0.0f);
    
    if (m_disableCameraSmoothing || m_noclipMode)
    {
        // Snap camera directly to target position
        m_camera.position = eyePosition;
    }
    else
    {
        // Smooth camera interpolation to hide jitter
        Vec3 positionDelta = eyePosition - m_camera.position;
        float smoothingFactor = 1.0f - std::pow(m_cameraSmoothing, deltaTime * 60.0f);
        m_camera.position = m_camera.position + positionDelta * smoothingFactor;
    }
}
