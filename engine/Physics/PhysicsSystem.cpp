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

    // Verbose per-frame logging disabled

    // Check collision with all active islands
    const auto& islands = m_islandSystem->getIslands();
    for (const auto& islandPair : islands)
    {
        const FloatingIsland* island = &islandPair.second;
        if (!island || !island->mainChunk)
            continue;

        // Convert player position to island-local coordinates
        Vec3 localPlayerPos = playerPos - island->physicsCenter;

        // Verbose per-island logging disabled

        // Check collision with this island's chunk
        Vec3 collisionNormal;
        if (checkChunkCollision(island->mainChunk.get(), localPlayerPos, island->physicsCenter,
                                collisionNormal, playerRadius))
        {
            outNormal = collisionNormal;
            // Collision detected; keep logs minimal
            return true;
        }
    }

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
    // Verbose per-chunk collision logging disabled

    // Check if collision mesh needs updating
    const CollisionMesh& collisionMesh = chunk->getCollisionMesh();
    // Mesh size logs disabled

    if (collisionMesh.needsUpdate)
    {
        // Rebuild collision mesh (const_cast is necessary here since we need to modify the chunk)
        const_cast<VoxelChunk*>(chunk)->buildCollisionMesh();
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
