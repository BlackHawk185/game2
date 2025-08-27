// PhysicsSystem.cpp - Basic collision detection system
#include "PhysicsSystem.h"

#include <iostream>

#include "../World/IslandChunkSystem.h"
#include "../World/VoxelChunk.h"

PhysicsSystem g_physics;

PhysicsSystem::PhysicsSystem() {}

PhysicsSystem::~PhysicsSystem()
{
    shutdown();
}

bool PhysicsSystem::initialize()
{
    // Removed verbose debug output
    return true;
}

void PhysicsSystem::update(float deltaTime)
{
    // Stub - no physics simulation yet
}

void PhysicsSystem::shutdown()
{
    // Removed verbose debug output
}

// **NEW: Collision Detection Implementation**

bool PhysicsSystem::checkPlayerCollision(const Vec3& playerPos, Vec3& outNormal, float playerRadius)
{
    if (!m_islandSystem)
        return false;

    std::cout << "[PHYSICS] Checking player collision at (" << playerPos.x << ", " << playerPos.y
              << ", " << playerPos.z << ") (radius: " << playerRadius << ")" << std::endl;

    // Check collision with all active islands
    const auto& islands = m_islandSystem->getIslands();
    for (const auto& islandPair : islands)
    {
        const FloatingIsland* island = &islandPair.second;
        if (!island || !island->mainChunk)
            continue;

        // Convert player position to island-local coordinates
        Vec3 localPlayerPos = playerPos - island->physicsCenter;

        std::cout << "[PHYSICS] Checking island " << islandPair.first << " at ("
                  << island->physicsCenter.x << ", " << island->physicsCenter.y << ", "
                  << island->physicsCenter.z << ") (local pos: (" << localPlayerPos.x << ", "
                  << localPlayerPos.y << ", " << localPlayerPos.z << "))" << std::endl;

        // Check collision with this island's chunk
        Vec3 collisionNormal;
        if (checkChunkCollision(island->mainChunk.get(), localPlayerPos, island->physicsCenter,
                                collisionNormal, playerRadius))
        {
            outNormal = collisionNormal;
            std::cout << "[PHYSICS] COLLISION DETECTED with island " << islandPair.first
                      << " (normal: " << collisionNormal.x << ", " << collisionNormal.y << ", "
                      << collisionNormal.z << ")" << std::endl;
            return true;
        }
    }

    std::cout << "[PHYSICS] No collision detected" << std::endl;
    return false;
}

bool PhysicsSystem::checkRayCollision(const Vec3& rayOrigin, const Vec3& rayDirection,
                                      float maxDistance, Vec3& hitPoint, Vec3& hitNormal)
{
    if (!m_islandSystem)
        return false;

    // Check ray collision with all active islands
    const auto& islands = m_islandSystem->getIslands();
    for (const auto& islandPair : islands)
    {
        const FloatingIsland* island = &islandPair.second;
        if (!island || !island->mainChunk)
            continue;

        // Convert ray to island-local coordinates
        Vec3 localRayOrigin = rayOrigin - island->physicsCenter;

        // Check ray collision with this island's chunk
        Vec3 localHitPoint, localHitNormal;
        if (island->mainChunk->checkRayCollision(localRayOrigin, rayDirection, maxDistance,
                                                 localHitPoint, localHitNormal))
        {
            // Convert back to world coordinates
            hitPoint = localHitPoint + island->physicsCenter;
            hitNormal = localHitNormal;
            return true;
        }
    }

    return false;
}

bool PhysicsSystem::checkChunkCollision(const VoxelChunk* chunk, const Vec3& playerPos,
                                        const Vec3& chunkWorldPos, Vec3& outNormal,
                                        float playerRadius)
{
    std::cout << "[PHYSICS] Checking chunk collision at world pos (" << chunkWorldPos.x << ", "
              << chunkWorldPos.y << ", " << chunkWorldPos.z << ") (local player pos: ("
              << playerPos.x << ", " << playerPos.y << ", " << playerPos.z << "))" << std::endl;

    // Check if collision mesh needs updating
    const CollisionMesh& collisionMesh = chunk->getCollisionMesh();
    std::cout << "[PHYSICS] Collision mesh has " << collisionMesh.faces.size()
              << " faces (needsUpdate: " << (collisionMesh.needsUpdate ? "true" : "false") << ")"
              << std::endl;

    if (collisionMesh.needsUpdate)
    {
        std::cout << "[PHYSICS] Building collision mesh..." << std::endl;
        // Rebuild collision mesh (const_cast is necessary here since we need to modify the chunk)
        const_cast<VoxelChunk*>(chunk)->buildCollisionMesh();
        std::cout << "[PHYSICS] Collision mesh built with "
                  << chunk->getCollisionMesh().faces.size() << " faces" << std::endl;
    }

    // Check if player sphere intersects with any collision faces
    for (const auto& face : collisionMesh.faces)
    {
        // Simple sphere-plane collision detection
        Vec3 faceToPlayer = playerPos - face.position;
        float distanceToPlane = faceToPlayer.dot(face.normal);

        // If player is within radius of the face plane
        if (abs(distanceToPlane) <= playerRadius)
        {
            // Project player position onto the face plane
            Vec3 projectedPoint = playerPos - face.normal * distanceToPlane;

            // Check if projected point is within the 1x1 face bounds
            Vec3 faceCenter = face.position;
            Vec3 localPoint = projectedPoint - faceCenter;

            // Determine which axes to check based on face normal
            bool withinBounds = true;
            if (abs(face.normal.x) > 0.5f)
            {  // X-facing face
                withinBounds = abs(localPoint.y) <= 0.5f && abs(localPoint.z) <= 0.5f;
            }
            else if (abs(face.normal.y) > 0.5f)
            {  // Y-facing face
                withinBounds = abs(localPoint.x) <= 0.5f && abs(localPoint.z) <= 0.5f;
            }
            else
            {  // Z-facing face
                withinBounds = abs(localPoint.x) <= 0.5f && abs(localPoint.y) <= 0.5f;
            }

            if (withinBounds)
            {
                outNormal = face.normal;
                return true;
            }
        }
    }

    return false;
}

// **EXISTING STUB METHODS (unchanged)**

uint32_t PhysicsSystem::createFloatingIslandBody(const Vec3& position, float mass)
{
    // Stub - return dummy ID
    static uint32_t nextID = 1;
    return nextID++;
}

uint32_t PhysicsSystem::createStaticBox(const Vec3& position, const Vec3& halfExtent)
{
    // Stub - return dummy ID
    static uint32_t nextID = 1000;
    return nextID++;
}

void PhysicsSystem::addBuoyancyForce(uint32_t bodyID, float buoyancy)
{
    // Stub - does nothing
}

Vec3 PhysicsSystem::getBodyPosition(uint32_t bodyID)
{
    // Stub - return zero position
    return Vec3(0, 0, 0);
}

void PhysicsSystem::setBodyPosition(uint32_t bodyID, const Vec3& position)
{
    // Stub - does nothing
}

void PhysicsSystem::addForce(uint32_t bodyID, const Vec3& force)
{
    // Stub - does nothing
}
