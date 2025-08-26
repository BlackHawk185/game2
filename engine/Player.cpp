// Player.cpp - Physics-driven player for floating island exploration
#include "Player.h"

#include "Input/Camera.h"
#include "Time/TimeManager.h"
#include "World/IslandChunkSystem.h"
#include "pch.h"

extern IslandChunkSystem g_islandSystem;  // Global system reference

Player::Player()
{
    // No spawn logic here — spawning is handled by the game startup (e.g., main.cpp)
    // Leave `position` at its default (declared in Player.h) so the world/level
    // code can place the player after islands/world are created.
}

void Player::update(float deltaTime)
{
    // Store previous state
    wasOnGround = onGround;

    // Update physics
    updatePhysics(deltaTime);

    // Check for ground collision with voxel world
    checkGroundCollision();

    // Apply appropriate friction
    applyFriction(deltaTime);
}

void Player::applyInput(Vec3 inputDirection, bool jump, float deltaTime)
{
    // Normalize input if needed
    float inputLength = inputDirection.length();
    if (inputLength > 1.0f)
    {
        inputDirection = inputDirection * (1.0f / inputLength);
    }

    // Apply movement force
    float currentMoveSpeed = moveSpeed;
    if (!onGround)
    {
        currentMoveSpeed *= airControl;  // Reduced air control
    }

    // Add movement acceleration
    acceleration.x = inputDirection.x * currentMoveSpeed;
    acceleration.z = inputDirection.z * currentMoveSpeed;

    // Handle jumping
    if (jump && onGround)
    {
        velocity.y = jumpStrength;
        onGround = false;  // Immediately consider airborne
    }
}

void Player::updateCameraFromPlayer(Camera* camera)
{
    if (!camera)
        return;

    // First-person camera: camera at player eye level
    Vec3 eyePosition = position;
    eyePosition.y += 1.6f;  // Eye height offset

    camera->position = eyePosition;
    // Camera orientation stays the same (controlled by mouse)
}

void Player::updatePhysics(float deltaTime)
{
    // Store previous velocity for debugging
    Vec3 oldVelocity = velocity;
    Vec3 oldPosition = position;

    // Apply time bubble effects if time system is available
    float effectiveDeltaTime = deltaTime;
    if (g_timeManager)
    {
        float timeBubbleEffect =
            g_timeManager->getTimeBubbleEffect(position.x, position.y, position.z);
        effectiveDeltaTime *= timeBubbleEffect;
    }

    // Apply acceleration to velocity using effective delta time
    velocity.x += acceleration.x * effectiveDeltaTime;
    velocity.y += acceleration.y * effectiveDeltaTime;
    velocity.z += acceleration.z * effectiveDeltaTime;

    // Apply both player velocity and platform base velocity to position
    position.x += (velocity.x + baseVelocity.x) * effectiveDeltaTime;
    position.y += (velocity.y + baseVelocity.y) * effectiveDeltaTime;
    position.z += (velocity.z + baseVelocity.z) * effectiveDeltaTime;

    // Reset horizontal acceleration (gravity stays constant)
    acceleration.x = 0.0f;
    acceleration.z = 0.0f;
    // Keep gravity: acceleration.y = -15.0f (set in constructor)
}

void Player::checkGroundCollision()
{
    // Proper voxel collision detection
    uint32_t mainIslandID = 1;  // Assuming first island has ID 1

    // Convert player world position to island-local coordinates
    Vec3 islandCenter = g_islandSystem.getIslandCenter(mainIslandID);
    Vec3 localPos = position - islandCenter;

    // Check voxel directly below player feet
    int checkX = static_cast<int>(std::floor(localPos.x));
    int checkY = static_cast<int>(std::floor(localPos.y - 0.1f));  // Check slightly below feet
    int checkZ = static_cast<int>(std::floor(localPos.z));

    // Get voxel at feet position (Vec3 expects floats)
    uint8_t voxelBelow = g_islandSystem.getVoxelFromIsland(
        mainIslandID,
        Vec3(static_cast<float>(checkX), static_cast<float>(checkY), static_cast<float>(checkZ)));

    bool wasInContactWithIsland = false;

    if (voxelBelow != 0)
    {  // Solid voxel found
        onGround = true;
        wasInContactWithIsland = true;
        if (velocity.y < 0)
        {
            velocity.y = 0;
            // Snap player to top of voxel
            position.y = islandCenter.y + checkY + 1.0f;
        }
    }
    else
    {
        onGround = false;
    }

    // Additional collision check for landing (check multiple points below player)
    if (!onGround && velocity.y < 0)
    {
        // Check in a small area around player feet for landing
        for (int dx = -1; dx <= 1; dx++)
        {
            for (int dz = -1; dz <= 1; dz++)
            {
                int testX = checkX + dx;
                int testZ = checkZ + dz;
                uint8_t testVoxel = g_islandSystem.getVoxelFromIsland(
                    mainIslandID, Vec3(static_cast<float>(testX), static_cast<float>(checkY),
                                       static_cast<float>(testZ)));

                if (testVoxel != 0)
                {
                    onGround = true;
                    wasInContactWithIsland = true;
                    velocity.y = 0;
                    position.y = islandCenter.y + checkY + 1.0f;
                    break;
                }
            }
            if (onGround)
                break;
        }
    }

    // Mirror island velocity when in contact with island
    if (wasInContactWithIsland)
    {
        baseVelocity = g_islandSystem.getIslandVelocity(mainIslandID);
    }
    else
    {
        // Not in contact with any island - zero out base velocity
        baseVelocity = Vec3(0.0f, 0.0f, 0.0f);
    }
}

void Player::applyFriction(float deltaTime)
{
    if (onGround)
    {
        // Frame-rate independent ground friction
        // Use float powf to avoid double intermediate and narrowing warnings
        float frictionFactor = std::pow(groundFriction, deltaTime * 60.0f);  // Normalize to 60 FPS
        velocity.x *= frictionFactor;
        velocity.z *= frictionFactor;
    }
    else
    {
        // Frame-rate independent air resistance
        float airFrictionFactor = std::pow(airFriction, deltaTime * 60.0f);  // Normalize to 60 FPS
        velocity.x *= airFrictionFactor;
        velocity.z *= airFrictionFactor;
    }

    // Terminal velocity (prevent infinite falling)
    if (velocity.y < -30.0f)
    {
        velocity.y = -30.0f;
    }
}
