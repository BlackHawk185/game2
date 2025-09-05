// PhysicsSystem.cpp - Basic collision detection system
#include "PhysicsSystem.h"
#include "FluidSystem.h"

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
    // Apply physics to all entities with Transform and Velocity components
    updateEntities(deltaTime);
}

void PhysicsSystem::shutdown()
{
    // Removed verbose debug output
}

// **NEW: Collision Detection Implementation**

bool PhysicsSystem::checkEntityCollision(const Vec3& entityPos, Vec3& outNormal, float entityRadius, const FloatingIsland** outIsland)
{
    if (!m_islandSystem)
        return false;

    // Check collision with all active islands
    const auto& islands = m_islandSystem->getIslands();
    for (const auto& islandPair : islands)
    {
        const FloatingIsland* island = &islandPair.second;
        if (!island)
            continue;

        // Convert entity position to island-local coordinates
        Vec3 localEntityPos = entityPos - island->physicsCenter;
        
        // Calculate which chunks the entity could potentially collide with
        float checkRadius = entityRadius + VoxelChunk::SIZE;
        
        // Calculate chunk coordinate bounds
        int minChunkX = static_cast<int>(std::floor((localEntityPos.x - checkRadius) / VoxelChunk::SIZE));
        int maxChunkX = static_cast<int>(std::ceil((localEntityPos.x + checkRadius) / VoxelChunk::SIZE));
        int minChunkY = static_cast<int>(std::floor((localEntityPos.y - checkRadius) / VoxelChunk::SIZE));
        int maxChunkY = static_cast<int>(std::ceil((localEntityPos.y + checkRadius) / VoxelChunk::SIZE));
        int minChunkZ = static_cast<int>(std::floor((localEntityPos.z - checkRadius) / VoxelChunk::SIZE));
        int maxChunkZ = static_cast<int>(std::ceil((localEntityPos.z + checkRadius) / VoxelChunk::SIZE));

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
                    
                    // Convert entity position to chunk-local coordinates
                    Vec3 chunkLocalEntityPos = entityPos - chunkWorldPos;

                    // Check collision with this chunk
                    Vec3 collisionNormal;
                    if (checkChunkCollision(chunkIt->second.get(), chunkLocalEntityPos, chunkWorldPos,
                                            collisionNormal, entityRadius))
                    {
                        outNormal = collisionNormal;
                        if (outIsland)
                            *outIsland = island;
                        return true;
                    }
                }
            }
        }
    }

    return false;
}

bool PhysicsSystem::checkEntityCollisionWithPenetration(const Vec3& entityPos, Vec3& outNormal, float entityRadius,
                                                  const FloatingIsland** outIsland, float* outPenetrationDepth)
{
    if (!m_islandSystem)
        return false;

    // Check collision with all active islands
    const auto& islands = m_islandSystem->getIslands();
    for (const auto& islandPair : islands)
    {
        const FloatingIsland* island = &islandPair.second;
        if (!island)
            continue;

        // Convert entity position to island-local coordinates
        Vec3 localEntityPos = entityPos - island->physicsCenter;
        
        // Calculate which chunks the entity could potentially collide with
        float checkRadius = entityRadius + VoxelChunk::SIZE;
        
        // Calculate chunk coordinate bounds
        int minChunkX = static_cast<int>(std::floor((localEntityPos.x - checkRadius) / VoxelChunk::SIZE));
        int maxChunkX = static_cast<int>(std::ceil((localEntityPos.x + checkRadius) / VoxelChunk::SIZE));
        int minChunkY = static_cast<int>(std::floor((localEntityPos.y - checkRadius) / VoxelChunk::SIZE));
        int maxChunkY = static_cast<int>(std::ceil((localEntityPos.y + checkRadius) / VoxelChunk::SIZE));
        int minChunkZ = static_cast<int>(std::floor((localEntityPos.z - checkRadius) / VoxelChunk::SIZE));
        int maxChunkZ = static_cast<int>(std::ceil((localEntityPos.z + checkRadius) / VoxelChunk::SIZE));

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
                    
                    // Convert entity position to chunk-local coordinates
                    Vec3 chunkLocalEntityPos = entityPos - chunkWorldPos;

                    // Check collision with this chunk
                    Vec3 collisionNormal;
                    float penetrationDepth;
                    if (checkChunkCollisionWithPenetration(chunkIt->second.get(), chunkLocalEntityPos, chunkWorldPos,
                                                          collisionNormal, entityRadius, &penetrationDepth))
                    {
                        outNormal = collisionNormal;
                        if (outIsland)
                            *outIsland = island;
                        if (outPenetrationDepth)
                            *outPenetrationDepth = penetrationDepth;
                        return true;
                    }
                }
            }
        }
    }

    return false;
}bool PhysicsSystem::checkRayCollision(const Vec3& rayOrigin, const Vec3& rayDirection,
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
    // Check if collision mesh needs updating
    const CollisionMesh& collisionMesh = chunk->getCollisionMesh();

    // Only rebuild if absolutely necessary (should rarely happen now)
    if (collisionMesh.needsUpdate)
    {
        // This should only occur if mesh generation was interrupted or failed
        const_cast<VoxelChunk*>(chunk)->buildCollisionMesh();
    }

    // Check if player box intersects with any collision faces
    for (const auto& face : collisionMesh.faces)
    {
        // **BOX-TO-PLANE COLLISION DETECTION** for box entities
        Vec3 faceToPlayer = playerPos - face.position;
        float distanceToPlane = faceToPlayer.dot(face.normal);

        // For box collision, check if any part of the box intersects the face plane
        // Box extends playerRadius in all directions from center
        if (abs(distanceToPlane) <= playerRadius)
        {
            // Project player position onto the face plane
            Vec3 projectedPoint = playerPos - face.normal * distanceToPlane;

            // Check if projected box overlaps with the 1x1 face bounds
            Vec3 faceCenter = face.position;
            Vec3 localPoint = projectedPoint - faceCenter;

            // **BOX OVERLAP CHECK**: Check if box (with radius playerRadius) overlaps face
            bool withinBounds = true;
            if (abs(face.normal.x) > 0.5f)
            {  // X-facing face - check Y,Z box overlap
                withinBounds = (abs(localPoint.y) <= (0.5f + playerRadius)) && 
                              (abs(localPoint.z) <= (0.5f + playerRadius));
            }
            else if (abs(face.normal.y) > 0.5f)
            {  // Y-facing face - check X,Z box overlap  
                withinBounds = (abs(localPoint.x) <= (0.5f + playerRadius)) && 
                              (abs(localPoint.z) <= (0.5f + playerRadius));
            }
            else
            {  // Z-facing face - check X,Y box overlap
                withinBounds = (abs(localPoint.x) <= (0.5f + playerRadius)) && 
                              (abs(localPoint.y) <= (0.5f + playerRadius));
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

bool PhysicsSystem::checkChunkCollisionWithPenetration(const VoxelChunk* chunk, const Vec3& playerPos,
                                               const Vec3& chunkWorldPos, Vec3& outNormal,
                                               float playerRadius, float* outPenetrationDepth)
{
    // Check if collision mesh needs updating
    const CollisionMesh& collisionMesh = chunk->getCollisionMesh();

    // Only rebuild if absolutely necessary (should rarely happen now)
    if (collisionMesh.needsUpdate)
    {
        // This should only occur if mesh generation was interrupted or failed
        const_cast<VoxelChunk*>(chunk)->buildCollisionMesh();
    }

    // Check if player box intersects with any collision faces
    for (const auto& face : collisionMesh.faces)
    {
        // **BOX-TO-PLANE COLLISION DETECTION** for box entities
        Vec3 faceToPlayer = playerPos - face.position;
        float distanceToPlane = faceToPlayer.dot(face.normal);

        // For box collision, check if any part of the box intersects the face plane
        // Box extends playerRadius in all directions from center
        if (abs(distanceToPlane) <= playerRadius)
        {
            // Project player position onto the face plane
            Vec3 projectedPoint = playerPos - face.normal * distanceToPlane;

            // Check if projected box overlaps with the 1x1 face bounds
            Vec3 faceCenter = face.position;
            Vec3 localPoint = projectedPoint - faceCenter;

            // **BOX OVERLAP CHECK**: Check if box (with radius playerRadius) overlaps face
            bool withinBounds = true;
            if (abs(face.normal.x) > 0.5f)
            {  // X-facing face - check Y,Z box overlap
                withinBounds = (abs(localPoint.y) <= (0.5f + playerRadius)) && 
                              (abs(localPoint.z) <= (0.5f + playerRadius));
            }
            else if (abs(face.normal.y) > 0.5f)
            {  // Y-facing face - check X,Z box overlap  
                withinBounds = (abs(localPoint.x) <= (0.5f + playerRadius)) && 
                              (abs(localPoint.z) <= (0.5f + playerRadius));
            }
            else
            {  // Z-facing face - check X,Y box overlap
                withinBounds = (abs(localPoint.x) <= (0.5f + playerRadius)) && 
                              (abs(localPoint.y) <= (0.5f + playerRadius));
            }

            if (withinBounds)
            {
                outNormal = face.normal;

                // Calculate penetration depth for box collision
                if (outPenetrationDepth)
                {
                    // For box collision, penetration depth is box surface to plane distance
                    // Positive distanceToPlane means box center is in front of plane
                    // Negative means behind (inside the collision volume)
                    if (distanceToPlane >= 0)
                    {
                        // Box center is in front but box surface overlaps - calculate overlap
                        *outPenetrationDepth = playerRadius - distanceToPlane;
                    }
                    else
                    {
                        // Box center is behind plane (inside collision volume)
                        *outPenetrationDepth = playerRadius + abs(distanceToPlane);
                    }
                }

                return true;
            }
        }
    }

    return false;
}

// **NEW: Generic Entity Physics Update**

void PhysicsSystem::updateEntities(float deltaTime)
{
    // Get all entities with Transform components
    auto* transformStorage = g_ecs.getStorage<TransformComponent>();
    if (!transformStorage) return;
    
    // Iterate through all entities with TransformComponent
    for (size_t i = 0; i < transformStorage->entities.size(); ++i)
    {
        EntityID entity = transformStorage->entities[i];
        
        // Check if this entity also has VelocityComponent
        auto* velocity = g_ecs.getComponent<VelocityComponent>(entity);
        auto* transform = &transformStorage->components[i];
        
        if (!velocity || !transform) continue;
        
        // Apply gravity
        velocity->velocity.y -= 9.81f * deltaTime;  // Gravity acceleration
        
        // Get entity radius (default to 0.5f, or use FluidParticleComponent if available)
        float entityRadius = 0.5f;
        auto* fluidComp = g_ecs.getComponent<FluidParticleComponent>(entity);
        if (fluidComp)
        {
            entityRadius = fluidComp->radius;
        }
        
        // **AXIS-SEPARATED COLLISION**: Test X, Y, Z movement independently to prevent Y-blocking
        Vec3 currentPos = transform->position;
        Vec3 deltaMovement = velocity->velocity * deltaTime;
        Vec3 finalPosition = currentPos;
        
        // Try X movement
        if (abs(deltaMovement.x) > 0.001f)
        {
            Vec3 testPosX = currentPos + Vec3(deltaMovement.x, 0, 0);
            Vec3 normalX;
            const FloatingIsland* islandX = nullptr;
            float depthX = 0.0f;
            
            if (!checkEntityCollisionWithPenetration(testPosX, normalX, entityRadius, &islandX, &depthX) || depthX <= 0.0f)
            {
                finalPosition.x = testPosX.x; // X movement is safe
            }
            else
            {
                velocity->velocity.x = islandX ? islandX->velocity.x : 0.0f; // Stop X velocity
            }
        }
        
        // Try Z movement  
        if (abs(deltaMovement.z) > 0.001f)
        {
            Vec3 testPosZ = Vec3(finalPosition.x, currentPos.y, currentPos.z + deltaMovement.z);
            Vec3 normalZ;
            const FloatingIsland* islandZ = nullptr;
            float depthZ = 0.0f;
            
            if (!checkEntityCollisionWithPenetration(testPosZ, normalZ, entityRadius, &islandZ, &depthZ) || depthZ <= 0.0f)
            {
                finalPosition.z = testPosZ.z; // Z movement is safe
            }
            else
            {
                velocity->velocity.z = islandZ ? islandZ->velocity.z : 0.0f; // Stop Z velocity
            }
        }
        
        // Try Y movement (falling/jumping) - this should almost always work unless on ground
        if (abs(deltaMovement.y) > 0.001f)
        {
            Vec3 testPosY = Vec3(finalPosition.x, currentPos.y + deltaMovement.y, finalPosition.z);
            Vec3 normalY;
            const FloatingIsland* islandY = nullptr;
            float depthY = 0.0f;
            
            if (!checkEntityCollisionWithPenetration(testPosY, normalY, entityRadius, &islandY, &depthY) || depthY <= 0.0f)
            {
                finalPosition.y = testPosY.y; // Y movement is safe
            }
            else
            {
                velocity->velocity.y = islandY ? islandY->velocity.y : 0.0f; // Stop Y velocity (hit ground/ceiling)
            }
        }
        
        // Apply the final position
        transform->position = finalPosition;
    }
}

// **NEW: Player Collision Wrapper**

bool PhysicsSystem::checkPlayerCollision(const Vec3& playerPos, Vec3& outNormal, float playerRadius)
{
    return checkEntityCollision(playerPos, outNormal, playerRadius);
}

// Debug and testing methods
void PhysicsSystem::debugCollisionInfo(const Vec3& playerPos, float playerRadius)
{
    if (!m_islandSystem)
    {
        std::cout << "PhysicsSystem: No island system connected" << std::endl;
        return;
    }

    std::cout << "=== Collision Debug Info ===" << std::endl;
    std::cout << "Player pos: (" << playerPos.x << ", " << playerPos.y << ", " << playerPos.z << ")" << std::endl;
    std::cout << "Player radius: " << playerRadius << std::endl;

    const auto& islands = m_islandSystem->getIslands();
    std::cout << "Total islands: " << islands.size() << std::endl;

    int totalFaces = 0;
    for (const auto& islandPair : islands)
    {
        const FloatingIsland* island = &islandPair.second;
        if (!island) continue;

        std::cout << "Island " << islandPair.first << " at (" << island->physicsCenter.x << ", " << island->physicsCenter.y << ", " << island->physicsCenter.z << ")" << std::endl;
        std::cout << "  Chunks: " << island->chunks.size() << std::endl;

        for (const auto& chunkPair : island->chunks)
        {
            const VoxelChunk* chunk = chunkPair.second.get();
            if (!chunk) continue;

            int faces = chunk->getCollisionMesh().faces.size();
            totalFaces += faces;
            std::cout << "    Chunk at (" << chunkPair.first.x << ", " << chunkPair.first.y << ", " << chunkPair.first.z << "): " << faces << " collision faces" << std::endl;
        }
    }

    std::cout << "Total collision faces: " << totalFaces << std::endl;
    std::cout << "==========================" << std::endl;
}

int PhysicsSystem::getTotalCollisionFaces() const
{
    if (!m_islandSystem) return 0;

    int total = 0;
    const auto& islands = m_islandSystem->getIslands();
    for (const auto& islandPair : islands)
    {
        const FloatingIsland* island = &islandPair.second;
        if (!island) continue;

        for (const auto& chunkPair : island->chunks)
        {
            const VoxelChunk* chunk = chunkPair.second.get();
            if (chunk)
            {
                total += chunk->getCollisionMesh().faces.size();
            }
        }
    }
    return total;
}
