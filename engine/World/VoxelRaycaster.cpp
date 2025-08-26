// VoxelRaycaster.cpp - Implementation of voxel ray casting using DDA algorithm
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

    // Place block adjacent to hit face (in local island coordinates)
    return hit.localBlockPos + hit.normal;
}

RayHit VoxelRaycaster::performDDA(const Vec3& rayStart, const Vec3& rayDirection, float maxDistance,
                                  IslandChunkSystem* islandSystem)
{
    RayHit result;

    // **ISLAND-CENTRIC ARCHITECTURE**: Find all islands and test ray against each one
    std::vector<VoxelChunk*> islandChunks;
    islandSystem->getAllChunks(islandChunks);

    float closestDistance = maxDistance;
    bool foundHit = false;

    // Test ray against each island
    for (VoxelChunk* chunk : islandChunks)
    {
        if (!chunk)
            continue;

        // Find which island this chunk belongs to
        uint32_t chunkIslandID = findIslandIDForChunk(islandSystem, chunk);
        if (chunkIslandID == 0)
            continue;

        FloatingIsland* island = islandSystem->getIsland(chunkIslandID);
        if (!island)
            continue;

        // Get island's physics center as world position
        Vec3 islandWorldPos = island->physicsCenter;

        // **CONVERT RAY TO ISLAND'S LOCAL COORDINATE SPACE**
        Vec3 localRayStart = rayStart - islandWorldPos;

        // Test if ray intersects this island's AABB (32x32x32 cube)
        if (!rayIntersectsAABB(localRayStart, rayDirection, Vec3(0.0f, 0.0f, 0.0f),
                               Vec3(32.0f, 32.0f, 32.0f), maxDistance))
        {
            continue;  // Ray doesn't hit this island at all
        }

        // Perform DDA in island's local coordinate system
        RayHit islandHit = performLocalDDA(localRayStart, rayDirection, maxDistance, chunk);

        if (islandHit.hit)
        {
            // Calculate world distance to this hit
            Vec3 worldHitPos = islandWorldPos + islandHit.localBlockPos;
            float distance = rayStart.distance(worldHitPos);

            if (distance < closestDistance)
            {
                closestDistance = distance;
                foundHit = true;

                result = islandHit;
                result.chunk = chunk;
                // Find the island ID by asking the system
                result.islandID = findIslandIDForChunk(islandSystem, chunk);
            }
        }
    }

    return result;
}

// VoxelChunk version of DDA raycast
RayHit VoxelRaycaster::performDDA(const Vec3& rayStart, const Vec3& rayDirection, float maxDistance,
                                  VoxelChunk* voxelChunk)
{
    RayHit result;

    // std::cout << "🔍 Starting DDA raycast from (" << rayStart.x << "," << rayStart.y << "," <<
    // rayStart.z << ")" << std::endl;

    // Normalize direction
    Vec3 dir = rayDirection.normalized();

    // Current voxel coordinates (integer)
    int mapX = (int) floor(rayStart.x);
    int mapY = (int) floor(rayStart.y);
    int mapZ = (int) floor(rayStart.z);

    // Length of ray from current position to x, y, z side
    float deltaDistX = std::abs(1.0f / dir.x);
    float deltaDistY = std::abs(1.0f / dir.y);
    float deltaDistZ = std::abs(1.0f / dir.z);

    // Calculate step and initial sideDist
    int stepX, stepY, stepZ;
    float sideDistX, sideDistY, sideDistZ;

    if (dir.x < 0)
    {
        stepX = -1;
        sideDistX = (rayStart.x - mapX) * deltaDistX;
    }
    else
    {
        stepX = 1;
        sideDistX = (mapX + 1.0f - rayStart.x) * deltaDistX;
    }

    if (dir.y < 0)
    {
        stepY = -1;
        sideDistY = (rayStart.y - mapY) * deltaDistY;
    }
    else
    {
        stepY = 1;
        sideDistY = (mapY + 1.0f - rayStart.y) * deltaDistY;
    }

    if (dir.z < 0)
    {
        stepZ = -1;
        sideDistZ = (rayStart.z - mapZ) * deltaDistZ;
    }
    else
    {
        stepZ = 1;
        sideDistZ = (mapZ + 1.0f - rayStart.z) * deltaDistZ;
    }

    // Perform DDA
    int side = 0;  // Was a NS, EW or Z wall hit?
    float distance = 0;
    int steps = 0;

    while (distance < maxDistance)
    {
        // Jump to next map square, either in x-direction, y-direction or z-direction
        if (sideDistX < sideDistY && sideDistX < sideDistZ)
        {
            sideDistX += deltaDistX;
            mapX += stepX;
            side = 0;
            distance = sideDistX;
        }
        else if (sideDistY < sideDistZ)
        {
            sideDistY += deltaDistY;
            mapY += stepY;
            side = 1;
            distance = sideDistY;
        }
        else
        {
            sideDistZ += deltaDistZ;
            mapZ += stepZ;
            side = 2;
            distance = sideDistZ;
        }

        // Check what was hit - Convert world coordinates to chunk coordinates
        // VoxelChunk is positioned in world space starting at (0,0,0), size 32x32x32
        int chunkX = mapX;  // Direct mapping
        int chunkY = mapY;  // Direct mapping
        int chunkZ = mapZ;  // Direct mapping

        if (++steps % 50 == 0)
        {  // Reduce debug frequency
            std::cout << "🔍 Step " << steps << ": Testing world(" << mapX << "," << mapY << ","
                      << mapZ << ")"
                      << " = chunk(" << chunkX << "," << chunkY << "," << chunkZ << ")";
        }

        // Check bounds and query voxel
        if (chunkX >= 0 && chunkX < 32 && chunkY >= 0 && chunkY < 32 && chunkZ >= 0 && chunkZ < 32)
        {
            uint8_t voxel = voxelChunk->getVoxel(chunkX, chunkY, chunkZ);
            if (steps % 50 == 0)
            {  // Reduce debug spam
                std::cout << " = " << (int) voxel << std::endl;
            }

            if (voxel > 0)
            {
                // Hit a solid block!
                result.hit = true;
                result.localBlockPos =
                    Vec3(static_cast<float>(mapX), static_cast<float>(mapY),
                         static_cast<float>(mapZ));  // For compatibility, use localBlockPos
                result.chunkX = mapX;
                result.chunkY = mapY;
                result.chunkZ = mapZ;
                if (side == 0)
                {
                    result.normal = Vec3(static_cast<float>(-stepX), 0.0f, 0.0f);
                }
                else if (side == 1)
                {
                    result.normal = Vec3(0.0f, static_cast<float>(-stepY), 0.0f);
                }
                else
                {
                    result.normal = Vec3(0.0f, 0.0f, static_cast<float>(-stepZ));
                }

                return result;
            }
        }
    }

    return result;
}

// **NEW ISLAND-CENTRIC HELPER FUNCTIONS**

RayHit VoxelRaycaster::performLocalDDA(const Vec3& localRayStart, const Vec3& rayDirection,
                                       float maxDistance, VoxelChunk* chunk)
{
    RayHit result;

    // Normalize direction
    Vec3 dir = rayDirection.normalized();

    // Current voxel coordinates (integer) - now in LOCAL island space
    int mapX = (int) floor(localRayStart.x);
    int mapY = (int) floor(localRayStart.y);
    int mapZ = (int) floor(localRayStart.z);

    // Length of ray from current position to x, y, z side
    float deltaDistX = std::abs(1.0f / dir.x);
    float deltaDistY = std::abs(1.0f / dir.y);
    float deltaDistZ = std::abs(1.0f / dir.z);

    // Calculate step and initial sideDist
    int stepX, stepY, stepZ;
    float sideDistX, sideDistY, sideDistZ;

    if (dir.x < 0)
    {
        stepX = -1;
        sideDistX = (localRayStart.x - mapX) * deltaDistX;
    }
    else
    {
        stepX = 1;
        sideDistX = (mapX + 1.0f - localRayStart.x) * deltaDistX;
    }

    if (dir.y < 0)
    {
        stepY = -1;
        sideDistY = (localRayStart.y - mapY) * deltaDistY;
    }
    else
    {
        stepY = 1;
        sideDistY = (mapY + 1.0f - localRayStart.y) * deltaDistY;
    }

    if (dir.z < 0)
    {
        stepZ = -1;
        sideDistZ = (localRayStart.z - mapZ) * deltaDistZ;
    }
    else
    {
        stepZ = 1;
        sideDistZ = (mapZ + 1.0f - localRayStart.z) * deltaDistZ;
    }

    // Perform DDA
    int side = 0;  // Which side was hit?
    float distance = 0;

    while (distance < maxDistance)
    {
        // Jump to next map square
        if (sideDistX < sideDistY && sideDistX < sideDistZ)
        {
            sideDistX += deltaDistX;
            mapX += stepX;
            side = 0;
            distance = sideDistX;
        }
        else if (sideDistY < sideDistZ)
        {
            sideDistY += deltaDistY;
            mapY += stepY;
            side = 1;
            distance = sideDistY;
        }
        else
        {
            sideDistZ += deltaDistZ;
            mapZ += stepZ;
            side = 2;
            distance = sideDistZ;
        }

        // Check bounds (32x32x32 chunk)
        if (mapX >= 0 && mapX < 32 && mapY >= 0 && mapY < 32 && mapZ >= 0 && mapZ < 32)
        {
            uint8_t voxel = chunk->getVoxel(mapX, mapY, mapZ);

            if (voxel > 0)
            {
                // Hit a solid block!
                result.hit = true;
                result.localBlockPos = Vec3(static_cast<float>(mapX), static_cast<float>(mapY),
                                            static_cast<float>(mapZ));  // LOCAL coordinates
                result.chunkX = mapX;
                result.chunkY = mapY;
                result.chunkZ = mapZ;

                // Calculate normal based on which side we hit
                if (side == 0)
                {
                    result.normal = Vec3(static_cast<float>(-stepX), 0.0f, 0.0f);
                }
                else if (side == 1)
                {
                    result.normal = Vec3(0.0f, static_cast<float>(-stepY), 0.0f);
                }
                else
                {
                    result.normal = Vec3(0.0f, 0.0f, static_cast<float>(-stepZ));
                }

                return result;
            }
        }
        else
        {
            // Out of bounds, stop searching this island
            break;
        }
    }

    return result;
}

bool VoxelRaycaster::rayIntersectsAABB(const Vec3& rayStart, const Vec3& rayDir,
                                       const Vec3& aabbMin, const Vec3& aabbMax, float maxDist)
{
    // Simple AABB ray intersection test
    Vec3 invDir(1.0f / rayDir.x, 1.0f / rayDir.y, 1.0f / rayDir.z);

    Vec3 t1((aabbMin.x - rayStart.x) * invDir.x, (aabbMin.y - rayStart.y) * invDir.y,
            (aabbMin.z - rayStart.z) * invDir.z);
    Vec3 t2((aabbMax.x - rayStart.x) * invDir.x, (aabbMax.y - rayStart.y) * invDir.y,
            (aabbMax.z - rayStart.z) * invDir.z);

    Vec3 tmin(std::min(t1.x, t2.x), std::min(t1.y, t2.y), std::min(t1.z, t2.z));
    Vec3 tmax(std::max(t1.x, t2.x), std::max(t1.y, t2.y), std::max(t1.z, t2.z));

    float znear = std::max(std::max(tmin.x, tmin.y), tmin.z);
    float zfar = std::min(std::min(tmax.x, tmax.y), tmax.z);

    return znear <= zfar && zfar >= 0 && znear <= maxDist;
}

uint32_t VoxelRaycaster::findIslandIDForChunk(IslandChunkSystem* islandSystem, VoxelChunk* chunk)
{
    // For now, assume main island is ID 1
    // TODO: Implement proper chunk->island lookup
    return 1;
}
