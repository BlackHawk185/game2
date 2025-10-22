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
                                        const Vec3& /*chunkWorldPos*/, Vec3& outNormal,
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

            // **BOX OVERLAP CHECK**: Check if box (with radius playerRadius) overlaps 1x1 face
            // Face extends ±0.5 from its center. Player extends ±playerRadius from projectedPoint.
            // They overlap if the distance between centers is less than sum of half-extents.
            bool withinBounds = true;
            if (abs(face.normal.x) > 0.5f)
            {  // X-facing face - check Y,Z box overlap
                withinBounds = (abs(localPoint.y) <= (0.5f + playerRadius)) && 
                              (abs(localPoint.z) <= (0.5f + playerRadius));
            }
            else if (abs(face.normal.z) > 0.5f)
            {  // Z-facing face - check X,Y box overlap
                withinBounds = (abs(localPoint.x) <= (0.5f + playerRadius)) && 
                              (abs(localPoint.y) <= (0.5f + playerRadius));
            }
            else
            {  // Y-facing face - check X,Z box overlap  
                withinBounds = (abs(localPoint.x) <= (0.5f + playerRadius)) && 
                              (abs(localPoint.z) <= (0.5f + playerRadius));
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
                                               const Vec3& /*chunkWorldPos*/, Vec3& outNormal,
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

            // **BOX OVERLAP CHECK**: Check if box (with radius playerRadius) overlaps 1x1 face
            // Face extends ±0.5 from its center. Player extends ±playerRadius from projectedPoint.
            // They overlap if the distance between centers is less than sum of half-extents.
            bool withinBounds = true;
            if (abs(face.normal.x) > 0.5f)
            {  // X-facing face - check Y,Z box overlap
                withinBounds = (abs(localPoint.y) <= (0.5f + playerRadius)) && 
                              (abs(localPoint.z) <= (0.5f + playerRadius));
            }
            else if (abs(face.normal.z) > 0.5f)
            {  // Z-facing face - check X,Y box overlap
                withinBounds = (abs(localPoint.x) <= (0.5f + playerRadius)) && 
                              (abs(localPoint.y) <= (0.5f + playerRadius));
            }
            else
            {  // Y-facing face - check X,Z box overlap  
                withinBounds = (abs(localPoint.x) <= (0.5f + playerRadius)) && 
                              (abs(localPoint.z) <= (0.5f + playerRadius));
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

// **NEW: Ground Detection for Moving Platform Physics**
GroundInfo PhysicsSystem::detectGround(const Vec3& playerPos, float playerRadius, float rayLength)
{
    GroundInfo info;
    info.isGrounded = false;
    info.distanceToGround = 999.0f;
    
    if (!m_islandSystem)
        return info;
    
    // Raycast downward from player position
    Vec3 rayOrigin = playerPos;
    Vec3 rayDirection = Vec3(0, -1, 0); // Straight down
    Vec3 hitPoint, hitNormal;
    
    // Check all islands for ground collision
    const auto& islands = m_islandSystem->getIslands();
    
    float closestDistance = 999.0f;
    const FloatingIsland* closestIsland = nullptr;
    Vec3 closestHitPoint, closestHitNormal;
    
    for (const auto& islandPair : islands)
    {
        const FloatingIsland* island = &islandPair.second;
        if (!island)
            continue;
        
        // Convert ray to island-local coordinates
        Vec3 localRayOrigin = rayOrigin - island->physicsCenter;
        
        // Calculate which chunks the ray could intersect
        float checkRadius = playerRadius + rayLength + 1.0f; // Add buffer
        int minChunkX = static_cast<int>(std::floor((localRayOrigin.x - checkRadius) / VoxelChunk::SIZE));
        int maxChunkX = static_cast<int>(std::ceil((localRayOrigin.x + checkRadius) / VoxelChunk::SIZE));
        int minChunkY = static_cast<int>(std::floor((localRayOrigin.y - rayLength - 1.0f) / VoxelChunk::SIZE));
        int maxChunkY = static_cast<int>(std::ceil(localRayOrigin.y / VoxelChunk::SIZE));
        int minChunkZ = static_cast<int>(std::floor((localRayOrigin.z - checkRadius) / VoxelChunk::SIZE));
        int maxChunkZ = static_cast<int>(std::ceil((localRayOrigin.z + checkRadius) / VoxelChunk::SIZE));
        
        // Check chunks that could contain ground
        for (int chunkX = minChunkX; chunkX <= maxChunkX; ++chunkX)
        {
            for (int chunkY = minChunkY; chunkY <= maxChunkY; ++chunkY)
            {
                for (int chunkZ = minChunkZ; chunkZ <= maxChunkZ; ++chunkZ)
                {
                    Vec3 chunkCoord(chunkX, chunkY, chunkZ);
                    
                    auto chunkIt = island->chunks.find(chunkCoord);
                    if (chunkIt == island->chunks.end() || !chunkIt->second)
                        continue;
                    
                    Vec3 chunkWorldPos = island->physicsCenter + FloatingIsland::chunkCoordToWorldPos(chunkCoord);
                    Vec3 chunkLocalRayOrigin = rayOrigin - chunkWorldPos;
                    
                    Vec3 localHitPoint, localHitNormal;
                    if (chunkIt->second->checkRayCollision(chunkLocalRayOrigin, rayDirection, rayLength,
                                                          localHitPoint, localHitNormal))
                    {
                        Vec3 worldHitPoint = localHitPoint + chunkWorldPos;
                        float distance = (worldHitPoint - rayOrigin).length();
                        
                        // Only consider upward-facing surfaces (normal.y > 0.5)
                        if (localHitNormal.y > 0.5f && distance < closestDistance)
                        {
                            closestDistance = distance;
                            closestIsland = island;
                            closestHitPoint = worldHitPoint;
                            closestHitNormal = localHitNormal;
                        }
                    }
                }
            }
        }
    }
    
    // If we found ground within range
    if (closestIsland && closestDistance < rayLength)
    {
        info.isGrounded = true;
        info.standingOnIslandID = closestIsland->islandID;
        info.groundNormal = closestHitNormal;
        info.groundVelocity = closestIsland->velocity; // Transfer island velocity
        info.groundContactPoint = closestHitPoint;
        info.distanceToGround = closestDistance;
    }
    
    return info;
}

// Find contact point along movement vector using binary search
// Returns the fraction of movement possible (0.0 to 1.0)
float PhysicsSystem::findContactPoint(const Vec3& fromPos, const Vec3& toPos, float entityRadius, Vec3* outContactNormal)
{
    // First check if destination is clear
    Vec3 testNormal;
    if (!checkEntityCollision(toPos, testNormal, entityRadius, nullptr))
    {
        // Full movement is safe
        return 1.0f;
    }
    
    // Check if we're already colliding at start
    Vec3 startNormal;
    if (checkEntityCollision(fromPos, startNormal, entityRadius, nullptr))
    {
        // Already in collision - check if we're moving away from the surface
        Vec3 movement = toPos - fromPos;
        float movementAlongNormal = movement.dot(startNormal);
        
        if (movementAlongNormal >= 0.0f)
        {
            // Moving away from surface - allow full movement and let destination check handle it
            return 1.0f;
        }
        
        // Moving INTO surface while already overlapping
        // This shouldn't happen in a well-behaved system, but if it does, block movement
        if (outContactNormal)
            *outContactNormal = startNormal;
        return 0.0f;
    }
    
    // Binary search for contact point between fromPos (safe) and toPos (collision)
    float minT = 0.0f;  // Safe position
    float maxT = 1.0f;  // Collision position
    float bestT = 0.0f;
    Vec3 bestNormal;
    
    // 16 iterations gives us precision of 1/65536 of movement distance
    for (int i = 0; i < 16; ++i)
    {
        float midT = (minT + maxT) * 0.5f;
        Vec3 testPos = fromPos + (toPos - fromPos) * midT;
        
        if (checkEntityCollision(testPos, testNormal, entityRadius, nullptr))
        {
            // This position collides - search earlier
            maxT = midT;
            bestNormal = testNormal;
        }
        else
        {
            // This position is safe - can go further
            minT = midT;
            bestT = midT;
        }
    }
    
    if (outContactNormal && bestT < 1.0f)
    {
        *outContactNormal = bestNormal;
    }
    
    return bestT;
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

// ============================================================================
// CAPSULE COLLISION SYSTEM
// ============================================================================
// Capsule is a cylinder with hemispherical caps on top and bottom
// Perfect for humanoid character collision (narrow width, proper height)

bool PhysicsSystem::checkChunkCapsuleCollision(const VoxelChunk* chunk, const Vec3& capsuleCenter,
                                               const Vec3& /*chunkWorldPos*/, Vec3& outNormal,
                                               float radius, float height)
{
    const CollisionMesh& collisionMesh = chunk->getCollisionMesh();
    
    if (collisionMesh.needsUpdate)
    {
        const_cast<VoxelChunk*>(chunk)->buildCollisionMesh();
    }
    
    // Capsule breakdown:
    // - Total height: height
    // - Cylinder height: height - 2*radius (middle section)
    // - Top sphere center: capsuleCenter + (0, cylinderHeight/2, 0)
    // - Bottom sphere center: capsuleCenter - (0, cylinderHeight/2, 0)
    
    float cylinderHalfHeight = (height - 2.0f * radius) * 0.5f;
    Vec3 topSphereCenter = capsuleCenter + Vec3(0, cylinderHalfHeight, 0);
    Vec3 bottomSphereCenter = capsuleCenter - Vec3(0, cylinderHalfHeight, 0);
    
    for (const auto& face : collisionMesh.faces)
    {
        Vec3 faceToCenter = capsuleCenter - face.position;
        float distanceToPlane = faceToCenter.dot(face.normal);
        
        // Check if capsule intersects this face's plane
        // For a capsule, we need to check:
        // 1. The cylindrical middle section (XZ circle at multiple Y heights)
        // 2. The top hemisphere
        // 3. The bottom hemisphere
        
        // Quick reject: if center is too far from plane
        if (abs(distanceToPlane) > (height * 0.5f + 0.1f))
            continue;
        
        // Determine which part of the capsule to test based on face position
        Vec3 closestPointOnAxis;
        
        // Project face position onto capsule's vertical axis
        float yOffset = (face.position.y - capsuleCenter.y);
        
        if (yOffset > cylinderHalfHeight)
        {
            // Check top hemisphere
            closestPointOnAxis = topSphereCenter;
        }
        else if (yOffset < -cylinderHalfHeight)
        {
            // Check bottom hemisphere
            closestPointOnAxis = bottomSphereCenter;
        }
        else
        {
            // Check cylinder - clamp to cylinder height
            closestPointOnAxis = capsuleCenter + Vec3(0, yOffset, 0);
        }
        
        // Now do sphere-to-face test from closestPointOnAxis
        Vec3 faceToPoint = closestPointOnAxis - face.position;
        float distToPlane = faceToPoint.dot(face.normal);
        
        if (abs(distToPlane) <= radius)
        {
            // Project point onto face plane
            Vec3 projectedPoint = closestPointOnAxis - face.normal * distToPlane;
            Vec3 localPoint = projectedPoint - face.position;
            
            // Check if projected point overlaps with 1x1 face bounds
            bool withinBounds = true;
            if (abs(face.normal.x) > 0.5f)
            {
                // X-facing face - check YZ circle overlap
                withinBounds = (abs(localPoint.y) <= (0.5f + radius)) && 
                              (abs(localPoint.z) <= (0.5f + radius));
            }
            else if (abs(face.normal.z) > 0.5f)
            {
                // Z-facing face - check XY circle overlap
                withinBounds = (abs(localPoint.x) <= (0.5f + radius)) && 
                              (abs(localPoint.y) <= (0.5f + radius));
            }
            else
            {
                // Y-facing face - check XZ circle overlap
                withinBounds = (abs(localPoint.x) <= (0.5f + radius)) && 
                              (abs(localPoint.z) <= (0.5f + radius));
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

bool PhysicsSystem::checkCapsuleCollision(const Vec3& capsuleCenter, float radius, float height,
                                         Vec3& outNormal, const FloatingIsland** outIsland)
{
    if (!m_islandSystem)
        return false;
    
    const auto& islands = m_islandSystem->getIslands();
    for (const auto& islandPair : islands)
    {
        const FloatingIsland* island = &islandPair.second;
        if (!island)
            continue;
        
        Vec3 localPos = capsuleCenter - island->physicsCenter;
        
        // Calculate chunk bounds - capsule can span multiple chunks vertically
        float checkRadius = radius + VoxelChunk::SIZE;
        float checkHeight = height * 0.5f + VoxelChunk::SIZE;
        
        int minChunkX = static_cast<int>(std::floor((localPos.x - checkRadius) / VoxelChunk::SIZE));
        int maxChunkX = static_cast<int>(std::ceil((localPos.x + checkRadius) / VoxelChunk::SIZE));
        int minChunkY = static_cast<int>(std::floor((localPos.y - checkHeight) / VoxelChunk::SIZE));
        int maxChunkY = static_cast<int>(std::ceil((localPos.y + checkHeight) / VoxelChunk::SIZE));
        int minChunkZ = static_cast<int>(std::floor((localPos.z - checkRadius) / VoxelChunk::SIZE));
        int maxChunkZ = static_cast<int>(std::ceil((localPos.z + checkRadius) / VoxelChunk::SIZE));
        
        for (int chunkX = minChunkX; chunkX <= maxChunkX; ++chunkX)
        {
            for (int chunkY = minChunkY; chunkY <= maxChunkY; ++chunkY)
            {
                for (int chunkZ = minChunkZ; chunkZ <= maxChunkZ; ++chunkZ)
                {
                    Vec3 chunkCoord(chunkX, chunkY, chunkZ);
                    auto chunkIt = island->chunks.find(chunkCoord);
                    if (chunkIt == island->chunks.end() || !chunkIt->second)
                        continue;
                    
                    Vec3 chunkWorldPos = island->physicsCenter + FloatingIsland::chunkCoordToWorldPos(chunkCoord);
                    Vec3 chunkLocalPos = capsuleCenter - chunkWorldPos;
                    
                    Vec3 collisionNormal;
                    if (checkChunkCapsuleCollision(chunkIt->second.get(), chunkLocalPos, chunkWorldPos,
                                                   collisionNormal, radius, height))
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

float PhysicsSystem::findCapsuleContactPoint(const Vec3& fromPos, const Vec3& toPos,
                                             float radius, float height, Vec3* outContactNormal)
{
    // Check destination first
    Vec3 testNormal;
    if (!checkCapsuleCollision(toPos, radius, height, testNormal, nullptr))
        return 1.0f;  // No collision at destination
    
    // Check if already colliding at start
    Vec3 startNormal;
    if (checkCapsuleCollision(fromPos, radius, height, startNormal, nullptr))
    {
        // Already colliding - check if moving away from surface
        Vec3 movement = toPos - fromPos;
        float movementAlongNormal = movement.dot(startNormal);
        if (movementAlongNormal >= 0.0f)
        {
            // Moving away from surface - allow movement
            if (outContactNormal)
                *outContactNormal = startNormal;
            return 1.0f;
        }
        // Moving into surface - block immediately
        if (outContactNormal)
            *outContactNormal = startNormal;
        return 0.0f;
    }
    
    // Binary search for contact point
    Vec3 searchStart = fromPos;
    Vec3 searchEnd = toPos;
    
    for (int i = 0; i < 16; ++i)  // 16 iterations = 1/65536 precision
    {
        Vec3 midPoint = (searchStart + searchEnd) * 0.5f;
        
        if (checkCapsuleCollision(midPoint, radius, height, testNormal, nullptr))
        {
            // Collision at midpoint - search earlier half
            searchEnd = midPoint;
        }
        else
        {
            // No collision - search later half
            searchStart = midPoint;
        }
    }
    
    // Calculate final contact fraction
    Vec3 totalMovement = toPos - fromPos;
    Vec3 safeMovement = searchStart - fromPos;
    
    float totalDistance = totalMovement.length();
    if (totalDistance < 0.0001f)
        return 1.0f;
    
    float safeDistance = safeMovement.length();
    float contactT = safeDistance / totalDistance;
    
    // Get the normal at the contact point
    if (outContactNormal)
    {
        checkCapsuleCollision(searchEnd, radius, height, *outContactNormal, nullptr);
    }
    
    return contactT;
}

GroundInfo PhysicsSystem::detectGroundCapsule(const Vec3& capsuleCenter, float radius, float height, float rayMargin)
{
    GroundInfo info;
    info.isGrounded = false;
    
    if (!m_islandSystem)
        return info;
    
    // Raycast from bottom of capsule downward
    // Bottom of capsule is at: capsuleCenter.y - (height/2 - radius)
    float cylinderHalfHeight = (height - 2.0f * radius) * 0.5f;
    float bottomY = capsuleCenter.y - cylinderHalfHeight - radius;
    
    Vec3 rayOrigin = Vec3(capsuleCenter.x, bottomY, capsuleCenter.z);
    Vec3 rayDirection = Vec3(0, -1, 0);
    float rayLength = rayMargin;
    
    // Check all islands
    const auto& islands = m_islandSystem->getIslands();
    for (const auto& islandPair : islands)
    {
        const FloatingIsland* island = &islandPair.second;
        if (!island)
            continue;
        
        Vec3 localRayOrigin = rayOrigin - island->physicsCenter;
        
        // Calculate which chunks to check
        float checkRadius = radius + VoxelChunk::SIZE;
        int minChunkX = static_cast<int>(std::floor((localRayOrigin.x - checkRadius) / VoxelChunk::SIZE));
        int maxChunkX = static_cast<int>(std::ceil((localRayOrigin.x + checkRadius) / VoxelChunk::SIZE));
        int minChunkY = static_cast<int>(std::floor((localRayOrigin.y - rayLength) / VoxelChunk::SIZE));
        int maxChunkY = static_cast<int>(std::ceil(localRayOrigin.y / VoxelChunk::SIZE));
        int minChunkZ = static_cast<int>(std::floor((localRayOrigin.z - checkRadius) / VoxelChunk::SIZE));
        int maxChunkZ = static_cast<int>(std::ceil((localRayOrigin.z + checkRadius) / VoxelChunk::SIZE));
        
        for (int chunkX = minChunkX; chunkX <= maxChunkX; ++chunkX)
        {
            for (int chunkY = minChunkY; chunkY <= maxChunkY; ++chunkY)
            {
                for (int chunkZ = minChunkZ; chunkZ <= maxChunkZ; ++chunkZ)
                {
                    Vec3 chunkCoord(chunkX, chunkY, chunkZ);
                    auto chunkIt = island->chunks.find(chunkCoord);
                    if (chunkIt == island->chunks.end() || !chunkIt->second)
                        continue;
                    
                    Vec3 chunkWorldPos = island->physicsCenter + FloatingIsland::chunkCoordToWorldPos(chunkCoord);
                    Vec3 chunkLocalRayOrigin = rayOrigin - chunkWorldPos;
                    
                    // Check collision faces in this chunk
                    const CollisionMesh& collisionMesh = chunkIt->second->getCollisionMesh();
                    
                    if (collisionMesh.needsUpdate)
                    {
                        const_cast<VoxelChunk*>(chunkIt->second.get())->buildCollisionMesh();
                    }
                    
                    // Check each face for ground intersection
                    for (const auto& face : collisionMesh.faces)
                    {
                        // Only check upward-facing surfaces (ground)
                        if (face.normal.y < 0.7f)
                            continue;
                        
                        // Ray-plane intersection
                        float denom = rayDirection.dot(face.normal);
                        if (abs(denom) < 0.0001f)
                            continue;
                        
                        Vec3 planeToRay = face.position - chunkLocalRayOrigin;
                        float t = planeToRay.dot(face.normal) / denom;
                        
                        if (t < 0 || t > rayLength)
                            continue;
                        
                        Vec3 hitPoint = chunkLocalRayOrigin + rayDirection * t;
                        Vec3 localPoint = hitPoint - face.position;
                        
                        // Check if hit point is within face bounds (1x1 square)
                        if (abs(localPoint.x) <= (0.5f + radius) && 
                            abs(localPoint.z) <= (0.5f + radius))
                        {
                            info.isGrounded = true;
                            info.standingOnIslandID = island->islandID;
                            info.groundNormal = face.normal;
                            info.groundVelocity = island->velocity;
                            info.groundContactPoint = hitPoint + chunkWorldPos;
                            info.distanceToGround = t;
                            return info;  // Return first hit
                        }
                    }
                }
            }
        }
    }
    
    return info;
}
