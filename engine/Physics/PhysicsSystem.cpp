// PhysicsSystem.cpp - Basic collision detection system
#include "PhysicsSystem.h"

#include <iostream>
#include <cmath>

#include "../World/IslandChunkSystem.h"
#include "../World/VoxelChunk.h"
#include "../World/BlockType.h"  // For BlockTypeRegistry and BlockTypeInfo

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

void PhysicsSystem::updateEntities(float deltaTime)
{
    // Physics updates now handled by PlayerController using capsule collision
    // This function intentionally left minimal - entity physics is application-specific
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
        
        // Transform world-space capsule to island-local space (accounts for rotation!)
        Vec3 localPos = island->worldToLocal(capsuleCenter);
        
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
                    
                    // Calculate chunk position in island-local space
                    Vec3 chunkLocalOffset = FloatingIsland::chunkCoordToWorldPos(chunkCoord);
                    Vec3 capsuleInChunkLocal = localPos - chunkLocalOffset;
                    
                    Vec3 collisionNormalLocal;
                    // NOTE: We're passing world positions for backward compatibility
                    // but collision detection now happens in island-local space
                    Vec3 chunkWorldPos = island->physicsCenter + chunkLocalOffset;
                    if (checkChunkCapsuleCollision(chunkIt->second.get(), capsuleInChunkLocal, chunkWorldPos,
                                                   collisionNormalLocal, radius, height))
                    {
                        // Transform collision normal from island-local to world space
                        outNormal = island->localDirToWorld(collisionNormalLocal);
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
        
        // Transform world-space ray to island-local space (accounts for rotation!)
        Vec3 localRayOrigin = island->worldToLocal(rayOrigin);
        Vec3 localRayDirection = island->worldDirToLocal(rayDirection);
        
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
                    
                    // Calculate chunk position in island-local space
                    Vec3 chunkLocalOffset = FloatingIsland::chunkCoordToWorldPos(chunkCoord);
                    Vec3 rayInChunkLocal = localRayOrigin - chunkLocalOffset;
                    
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
                        
                        // Ray-plane intersection (using local ray direction)
                        float denom = localRayDirection.dot(face.normal);
                        if (abs(denom) < 0.0001f)
                            continue;
                        
                        Vec3 planeToRay = face.position - rayInChunkLocal;
                        float t = planeToRay.dot(face.normal) / denom;
                        
                        if (t < 0 || t > rayLength)
                            continue;
                        
                        Vec3 hitPoint = rayInChunkLocal + localRayDirection * t;
                        Vec3 localPoint = hitPoint - face.position;
                        
                        // Check if hit point is within face bounds (1x1 square)
                        if (abs(localPoint.x) <= (0.5f + radius) && 
                            abs(localPoint.z) <= (0.5f + radius))
                        {
                            info.isGrounded = true;
                            info.standingOnIslandID = island->islandID;
                            info.groundNormal = face.normal;
                            info.groundVelocity = island->velocity;
                            
                            // Transform hit point from chunk-local to world space
                            Vec3 hitPointIslandLocal = hitPoint + chunkLocalOffset;
                            info.groundContactPoint = island->localToWorld(hitPointIslandLocal);
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
