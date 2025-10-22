// VoxelRaycaster.cpp - Integrated thin wrapper around IslandChunkSystem
#include "VoxelRaycaster.h"

#include <algorithm>
#include <cmath>
#include <iostream>

#include "VoxelChunk.h"
#include "World/IslandChunkSystem.h"

RayHit VoxelRaycaster::raycast(const Vec3& origin, const Vec3& direction, float maxDistance,
                               IslandChunkSystem* islandSystem)
{
    return performDDA(origin, direction, maxDistance, islandSystem);
}

RayHit VoxelRaycaster::raycast(const Vec3& origin, const Vec3& direction, float maxDistance,
                               VoxelChunk* voxelChunk)
{
    return performDDA(origin, direction, maxDistance, voxelChunk);
}

Vec3 VoxelRaycaster::getPlacementPosition(const RayHit& hit)
{
    if (!hit.hit)
        return Vec3(0.0f, 0.0f, 0.0f);

    // **INTEGRATED APPROACH**: Use island-relative coordinates with proper normal
    // Place block adjacent to hit face (in island-relative coordinates)
    return hit.localBlockPos + hit.normal;
}

RayHit VoxelRaycaster::performDDA(const Vec3& rayStart, const Vec3& rayDirection, float maxDistance,
                                  IslandChunkSystem* islandSystem)
{
    RayHit result;
    
    // Normalize direction
    Vec3 dir = rayDirection;
    float length = std::sqrt(dir.x * dir.x + dir.y * dir.y + dir.z * dir.z);
    if (length < 0.0001f) {
        return result; // Invalid direction
    }
    dir.x /= length;
    dir.y /= length;
    dir.z /= length;
    
    const auto& islands = islandSystem->getIslands();
    
    // Check each island for intersection (islands are spatially separate)
    for (const auto& [islandID, island] : islands)
    {
        // Quick AABB test - skip islands we can't possibly hit
        Vec3 islandMin = island.physicsCenter + Vec3(-64, -64, -64); // Assuming max 128x128x128 islands
        Vec3 islandMax = island.physicsCenter + Vec3(64, 64, 64);
        
        // Simple ray-AABB intersection test
        float tMin = 0.0f;
        float tMax = maxDistance;
        
        for (int i = 0; i < 3; ++i) {
            float invD = 1.0f / (i == 0 ? dir.x : (i == 1 ? dir.y : dir.z));
            float origin = (i == 0 ? rayStart.x : (i == 1 ? rayStart.y : rayStart.z));
            float boxMin = (i == 0 ? islandMin.x : (i == 1 ? islandMin.y : islandMin.z));
            float boxMax = (i == 0 ? islandMax.x : (i == 1 ? islandMax.y : islandMax.z));
            
            float t0 = (boxMin - origin) * invD;
            float t1 = (boxMax - origin) * invD;
            if (invD < 0.0f) {
                float tmp = t0;
                t0 = t1;
                t1 = tmp;
            }
            
            tMin = t0 > tMin ? t0 : tMin;
            tMax = t1 < tMax ? t1 : tMax;
            
            if (tMax < tMin) {
                goto next_island; // Ray misses this island's AABB
            }
        }
        
        // Ray hits island AABB - perform DDA in island-local space
        {
            Vec3 localStart = rayStart - island.physicsCenter;
            
            // DDA voxel traversal (Amanatides & Woo algorithm)
            int x = static_cast<int>(std::floor(localStart.x));
            int y = static_cast<int>(std::floor(localStart.y));
            int z = static_cast<int>(std::floor(localStart.z));
            
            int stepX = dir.x > 0 ? 1 : -1;
            int stepY = dir.y > 0 ? 1 : -1;
            int stepZ = dir.z > 0 ? 1 : -1;
            
            float tDeltaX = (dir.x != 0) ? std::abs(1.0f / dir.x) : 1e30f;
            float tDeltaY = (dir.y != 0) ? std::abs(1.0f / dir.y) : 1e30f;
            float tDeltaZ = (dir.z != 0) ? std::abs(1.0f / dir.z) : 1e30f;
            
            float tMaxX, tMaxY, tMaxZ;
            
            if (dir.x > 0) {
                tMaxX = (std::floor(localStart.x) + 1.0f - localStart.x) * tDeltaX;
            } else {
                tMaxX = (localStart.x - std::floor(localStart.x)) * tDeltaX;
            }
            
            if (dir.y > 0) {
                tMaxY = (std::floor(localStart.y) + 1.0f - localStart.y) * tDeltaY;
            } else {
                tMaxY = (localStart.y - std::floor(localStart.y)) * tDeltaY;
            }
            
            if (dir.z > 0) {
                tMaxZ = (std::floor(localStart.z) + 1.0f - localStart.z) * tDeltaZ;
            } else {
                tMaxZ = (localStart.z - std::floor(localStart.z)) * tDeltaZ;
            }
            
            Vec3 normal(0, 0, 0);
            float currentDistance = 0.0f;
            const int maxSteps = static_cast<int>(maxDistance * 2.0f);
            
            for (int step = 0; step < maxSteps; ++step)
            {
                // Check voxel at current position
                Vec3 checkPos(static_cast<float>(x), static_cast<float>(y), static_cast<float>(z));
                uint8_t blockID = islandSystem->getVoxelFromIsland(islandID, checkPos);
                
                if (blockID != 0) // Hit solid block
                {
                    result.hit = true;
                    result.islandID = islandID;
                    result.localBlockPos = checkPos;
                    result.normal = normal;
                    result.distance = currentDistance;
                    return result; // First hit wins
                }
                
                // Step to next voxel boundary
                if (tMaxX < tMaxY) {
                    if (tMaxX < tMaxZ) {
                        currentDistance = tMaxX;
                        x += stepX;
                        tMaxX += tDeltaX;
                        normal = Vec3(static_cast<float>(-stepX), 0, 0);
                    } else {
                        currentDistance = tMaxZ;
                        z += stepZ;
                        tMaxZ += tDeltaZ;
                        normal = Vec3(0, 0, static_cast<float>(-stepZ));
                    }
                } else {
                    if (tMaxY < tMaxZ) {
                        currentDistance = tMaxY;
                        y += stepY;
                        tMaxY += tDeltaY;
                        normal = Vec3(0, static_cast<float>(-stepY), 0);
                    } else {
                        currentDistance = tMaxZ;
                        z += stepZ;
                        tMaxZ += tDeltaZ;
                        normal = Vec3(0, 0, static_cast<float>(-stepZ));
                    }
                }
                
                if (currentDistance > maxDistance) {
                    break; // Exceeded max distance
                }
            }
        }
        
        next_island:;
    }
    
    return result; // No hit found
}

// **SIMPLIFIED COMPATIBILITY METHOD** - Just calls the main integrated method
RayHit VoxelRaycaster::performDDA(const Vec3& rayStart, const Vec3& rayDirection, float maxDistance,
                                  VoxelChunk* voxelChunk)
{
    // This compatibility method is deprecated - VoxelChunk-specific raycasting
    // should use the IslandChunkSystem version instead
    RayHit result;
    result.hit = false;
    return result;
}
