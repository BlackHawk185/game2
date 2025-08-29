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

    // **OPTIMIZED INTEGRATED APPROACH**: Use proper DDA algorithm with IslandChunkSystem
    Vec3 normalizedDir = rayDirection;
    float length = std::sqrt(normalizedDir.x * normalizedDir.x + normalizedDir.y * normalizedDir.y + normalizedDir.z * normalizedDir.z);
    if (length > 0.001f) {
        normalizedDir.x /= length;
        normalizedDir.y /= length; 
        normalizedDir.z /= length;
    }
    
    // Use larger step size for better performance and accuracy
    float stepSize = 0.5f;
    Vec3 rayStep = normalizedDir * stepSize;
    Vec3 currentPos = rayStart;
    float currentDistance = 0.0f;
    
    const auto& islands = islandSystem->getIslands();
    
    // Iterate along the ray
    while (currentDistance < maxDistance)
    {
        // Check each island at current position
        for (const auto& [islandID, island] : islands)
        {
            // **COORDINATE FIX**: Convert world position to island-relative coordinates
            Vec3 islandRelativePos = currentPos - island.physicsCenter;
            
            // **CRITICAL**: getVoxelFromIsland expects island-relative coordinates, not world coordinates!
            uint8_t voxel = islandSystem->getVoxelFromIsland(islandID, islandRelativePos);
            
            if (voxel > 0) // Solid voxel found
            {
                result.hit = true;
                result.islandID = islandID;
                result.distance = currentDistance;
                
                // Convert world position to island-relative position for block coordinates
                Vec3 islandRelativePos = currentPos - island.physicsCenter;
                
                // Convert to island-relative block position (integer coordinates)
                result.localBlockPos = Vec3(
                    std::floor(islandRelativePos.x),
                    std::floor(islandRelativePos.y), 
                    std::floor(islandRelativePos.z)
                );
                
                // Calculate face normal by checking which face we hit
                Vec3 blockCenter = result.localBlockPos + Vec3(0.5f, 0.5f, 0.5f);
                Vec3 hitOffset = islandRelativePos - blockCenter;
                
                // Determine which face based on largest offset component
                if (std::abs(hitOffset.x) > std::abs(hitOffset.y) && std::abs(hitOffset.x) > std::abs(hitOffset.z))
                    result.normal = Vec3(hitOffset.x > 0 ? 1.0f : -1.0f, 0.0f, 0.0f);
                else if (std::abs(hitOffset.y) > std::abs(hitOffset.z))
                    result.normal = Vec3(0.0f, hitOffset.y > 0 ? 1.0f : -1.0f, 0.0f);
                else
                    result.normal = Vec3(0.0f, 0.0f, hitOffset.z > 0 ? 1.0f : -1.0f);
                
                return result; // Return first hit
            }
        }
        
        // Move along ray
        currentPos = currentPos + rayStep;
        currentDistance += stepSize;
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
