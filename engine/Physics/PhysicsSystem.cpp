// PhysicsSystem.cpp - Basic collision detection system
#include "PhysicsSystem.h"

#include <iostream>
#include <cmath>

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
        if (!island)
            continue;

        // Convert player position to island-local coordinates
        Vec3 localPlayerPos = playerPos - island->physicsCenter;
        
        // Calculate which chunks the player could potentially collide with
        // Player radius extended by one chunk to be safe
        float checkRadius = playerRadius + VoxelChunk::SIZE;
        
        // Calculate chunk coordinate bounds
        int minChunkX = static_cast<int>(std::floor((localPlayerPos.x - checkRadius) / VoxelChunk::SIZE));
        int maxChunkX = static_cast<int>(std::ceil((localPlayerPos.x + checkRadius) / VoxelChunk::SIZE));
        int minChunkY = static_cast<int>(std::floor((localPlayerPos.y - checkRadius) / VoxelChunk::SIZE));
        int maxChunkY = static_cast<int>(std::ceil((localPlayerPos.y + checkRadius) / VoxelChunk::SIZE));
        int minChunkZ = static_cast<int>(std::floor((localPlayerPos.z - checkRadius) / VoxelChunk::SIZE));
        int maxChunkZ = static_cast<int>(std::ceil((localPlayerPos.z + checkRadius) / VoxelChunk::SIZE));

        // Only check chunks that could potentially contain collisions
        for (int chunkX = minChunkX; chunkX <= maxChunkX; ++chunkX)
        {
            for (int chunkY = minChunkY; chunkY <= maxChunkY; ++chunkY)
            {
                for (int chunkZ = minChunkZ; chunkZ <= maxChunkZ; ++chunkZ)
                {
                    Vec3 chunkCoord(chunkX, chunkY, chunkZ);
                    
                    // Check if this chunk exists in the island
                    auto chunkIt = island->chunks.find(chunkCoord);
                    if (chunkIt == island->chunks.end() || !chunkIt->second)
                        continue;

                    // Calculate chunk world position
                    Vec3 chunkWorldPos = island->physicsCenter + FloatingIsland::chunkCoordToWorldPos(chunkCoord);
                    
                    // Convert player position to chunk-local coordinates
                    Vec3 chunkLocalPlayerPos = playerPos - chunkWorldPos;

                    // Check collision with this chunk
                    Vec3 collisionNormal;
                    if (checkChunkCollision(chunkIt->second.get(), chunkLocalPlayerPos, chunkWorldPos,
                                            collisionNormal, playerRadius))
                    {
                        outNormal = collisionNormal;
                        return true;
                    }
                }
            }
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
        if (!island)
            continue;

        // Convert ray origin to island-local coordinates
        Vec3 localRayOrigin = rayOrigin - island->physicsCenter;
        Vec3 rayEnd = localRayOrigin + (rayDirection * maxDistance);
        
        // Calculate bounding box of ray path
        float minX = std::min(localRayOrigin.x, rayEnd.x);
        float maxX = std::max(localRayOrigin.x, rayEnd.x);
        float minY = std::min(localRayOrigin.y, rayEnd.y);
        float maxY = std::max(localRayOrigin.y, rayEnd.y);
        float minZ = std::min(localRayOrigin.z, rayEnd.z);
        float maxZ = std::max(localRayOrigin.z, rayEnd.z);
        
        // Calculate chunk coordinate bounds that the ray could intersect
        int minChunkX = static_cast<int>(std::floor(minX / VoxelChunk::SIZE));
        int maxChunkX = static_cast<int>(std::ceil(maxX / VoxelChunk::SIZE));
        int minChunkY = static_cast<int>(std::floor(minY / VoxelChunk::SIZE));
        int maxChunkY = static_cast<int>(std::ceil(maxY / VoxelChunk::SIZE));
        int minChunkZ = static_cast<int>(std::floor(minZ / VoxelChunk::SIZE));
        int maxChunkZ = static_cast<int>(std::ceil(maxZ / VoxelChunk::SIZE));

        // Only check chunks that the ray could potentially intersect
        for (int chunkX = minChunkX; chunkX <= maxChunkX; ++chunkX)
        {
            for (int chunkY = minChunkY; chunkY <= maxChunkY; ++chunkY)
            {
                for (int chunkZ = minChunkZ; chunkZ <= maxChunkZ; ++chunkZ)
                {
                    Vec3 chunkCoord(chunkX, chunkY, chunkZ);
                    
                    // Check if this chunk exists in the island
                    auto chunkIt = island->chunks.find(chunkCoord);
                    if (chunkIt == island->chunks.end() || !chunkIt->second)
                        continue;

                    // Calculate chunk world position
                    Vec3 chunkWorldPos = island->physicsCenter + FloatingIsland::chunkCoordToWorldPos(chunkCoord);
                    
                    // Convert ray to chunk-local coordinates
                    Vec3 chunkLocalRayOrigin = rayOrigin - chunkWorldPos;

                    // Check ray collision with this chunk
                    Vec3 localHitPoint, localHitNormal;
                    if (chunkIt->second->checkRayCollision(chunkLocalRayOrigin, rayDirection, maxDistance,
                                                          localHitPoint, localHitNormal))
                    {
                        // Convert back to world coordinates
                        hitPoint = localHitPoint + chunkWorldPos;
                        hitNormal = localHitNormal;
                        return true;
                    }
                }
            }
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
