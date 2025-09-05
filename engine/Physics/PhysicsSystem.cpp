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

                // Calculate penetration depth
                if (outPenetrationDepth)
                {
                    // Penetration depth is how much the sphere overlaps with the plane
                    // Positive distanceToPlane means player is in front of plane
                    // Negative means behind (inside the collision volume)
                    if (distanceToPlane >= 0)
                    {
                        // Player is in front but within radius - calculate overlap
                        *outPenetrationDepth = playerRadius - distanceToPlane;
                    }
                    else
                    {
                        // Player is behind plane (inside collision volume)
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
        
        // Calculate intended new position
        Vec3 intendedPosition = transform->position + velocity->velocity * deltaTime;

        // **PREVENTIVE COLLISION SYSTEM** - Check intended position first
        Vec3 intendedCollisionNormal;
        const FloatingIsland* intendedCollidedIsland = nullptr;
        float intendedPenetrationDepth = 0.0f;
        bool willCollideAtIntended = checkEntityCollisionWithPenetration(
            intendedPosition, intendedCollisionNormal, entityRadius,
            &intendedCollidedIsland, &intendedPenetrationDepth);

        if (willCollideAtIntended && intendedPenetrationDepth > 0.0f)
        {
            // **BLOCKING RESPONSE**: Stop ALL movement when collision detected
            // Kill velocity towards the surface to prevent re-penetration
            float normalVelocity = velocity->velocity.dot(intendedCollisionNormal);
            if (normalVelocity < 0) {
                velocity->velocity -= intendedCollisionNormal * normalVelocity;
            }

            // **BARRIER APPROACH**: Don't move into collision - stay at current position
            // This prevents any penetration from occurring on ALL axes
            transform->position = transform->position; // Explicit no-op for clarity
        }
        else
        {
            // No collision at intended position - safe to move
            transform->position = intendedPosition;
        }

        // **ISLAND VELOCITY INHERITANCE**: Apply friction-based velocity matching
        // Check for both penetration and proximity to island surfaces
        Vec3 currentCollisionNormal;
        const FloatingIsland* currentIsland = nullptr;
        float currentPenetrationDepth = 0.0f;
        bool isOnIsland = checkEntityCollisionWithPenetration(
            transform->position, currentCollisionNormal, entityRadius,
            &currentIsland, &currentPenetrationDepth);

        // If not penetrating, check proximity with slightly larger radius
        if (!isOnIsland)
        {
            Vec3 proximityNormal;
            const FloatingIsland* proximityIsland = nullptr;
            isOnIsland = checkEntityCollision(
                transform->position, proximityNormal, entityRadius + 0.1f, // Small proximity threshold
                &proximityIsland);
            if (isOnIsland)
            {
                currentIsland = proximityIsland;
            }
        }

        // Apply gradual island velocity inheritance if on or near any island
        if (isOnIsland && currentIsland)
        {
            // Apply gradual island velocity inheritance (10% per tick)
            // This allows wind/other forces to overcome slow islands while fast islands dominate
            Vec3 velocityDifference = currentIsland->velocity - velocity->velocity;
            // Only apply to XZ plane, preserve Y for jumping/falling
            velocity->velocity.x += velocityDifference.x * 0.1f;
            velocity->velocity.z += velocityDifference.z * 0.1f;
        }

        // **SIMPLIFIED FALLBACK: Teleport out of blocks** (rare edge cases only)
        Vec3 fallbackCollisionNormal;
        const FloatingIsland* fallbackIsland = nullptr;
        float fallbackPenetrationDepth = 0.0f;
        bool isCurrentlyStuck = checkEntityCollisionWithPenetration(
            transform->position, fallbackCollisionNormal, entityRadius,
            &fallbackIsland, &fallbackPenetrationDepth);

        if (isCurrentlyStuck && fallbackPenetrationDepth > 0.0f)
        {
            // **TELEPORT OUT**: Simple fix - move completely out of penetration
            transform->position += fallbackCollisionNormal * (fallbackPenetrationDepth + 0.01f);

            // Kill velocity towards the surface to prevent re-penetration
            float normalVelocity = velocity->velocity.dot(fallbackCollisionNormal);
            if (normalVelocity < 0) {
                velocity->velocity -= fallbackCollisionNormal * normalVelocity;
            }
        }
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
