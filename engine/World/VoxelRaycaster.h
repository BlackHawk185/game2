// VoxelRaycaster.h - Ray casting system for block selection and placement
#pragma once
#include "Math/Vec3.h"
#include <cstdint>

class IslandChunkSystem;
class VoxelChunk;

struct RayHit {
    bool hit = false;
    uint32_t islandID = 0;       // Which island was hit
    Vec3 localBlockPos;          // Block coordinates within the island (NOT world coordinates) 
    Vec3 normal;                 // Face normal for placement
    VoxelChunk* chunk = nullptr; // The chunk that was hit
    int chunkX = 0, chunkY = 0, chunkZ = 0;  // Block coordinates within chunk (same as localBlockPos for single-chunk islands)
    float distance = 0.0f;       // Distance from ray start to hit point
    
    // Constructor to initialize Vec3 members
    RayHit() : localBlockPos(0,0,0), normal(0,0,0) {}
};

class VoxelRaycaster {
public:
    // Cast ray from camera to find block intersections
    static RayHit raycast(const Vec3& origin, const Vec3& direction, float maxDistance, IslandChunkSystem* islandSystem);
    
    // Cast ray using VoxelChunk (temporary compatibility method)
    static RayHit raycast(const Vec3& origin, const Vec3& direction, float maxDistance, VoxelChunk* voxelChunk);
    
    // Helper to get placement position (adjacent to hit face)
    static Vec3 getPlacementPosition(const RayHit& hit);
    
private:
    // DDA (Digital Differential Analyzer) voxel traversal
    static RayHit performDDA(const Vec3& rayStart, const Vec3& rayDirection, float maxDistance, IslandChunkSystem* islandSystem);
    
    // DDA for VoxelChunk
    static RayHit performDDA(const Vec3& rayStart, const Vec3& rayDirection, float maxDistance, VoxelChunk* voxelChunk);
    
    // **NEW ISLAND-CENTRIC HELPERS**
    static RayHit performLocalDDA(const Vec3& localRayStart, const Vec3& rayDirection, float maxDistance, VoxelChunk* chunk);
    static bool rayIntersectsAABB(const Vec3& rayStart, const Vec3& rayDir, const Vec3& aabbMin, const Vec3& aabbMax, float maxDist);
    static uint32_t findIslandIDForChunk(IslandChunkSystem* islandSystem, VoxelChunk* chunk);
};
