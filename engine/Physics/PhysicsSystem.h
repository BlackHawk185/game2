// PhysicsSystem.h - Basic collision detection system
#pragma once
#include "ECS/ECS.h"
#include "Math/Vec3.h"
#include <vector>

// Forward declarations
class IslandChunkSystem;
class VoxelChunk;
struct FloatingIsland;

// Simple collision detection system using voxel face culling
class PhysicsSystem
{
   public:
    PhysicsSystem();
    ~PhysicsSystem();

    bool initialize();
    void update(float deltaTime);
    void updateEntities(float deltaTime);
    bool checkEntityCollision(const Vec3& entityPos, Vec3& outNormal, float entityRadius, const FloatingIsland** outIsland = nullptr);
    bool checkEntityCollisionWithPenetration(const Vec3& entityPos, Vec3& outNormal, float entityRadius,
                                           const FloatingIsland** outIsland = nullptr, float* outPenetrationDepth = nullptr);
    void shutdown();

    // Collision detection methods
    bool checkPlayerCollision(const Vec3& playerPos, Vec3& outNormal, float playerRadius = 0.5f);
    bool checkRayCollision(const Vec3& rayOrigin, const Vec3& rayDirection, float maxDistance, Vec3& hitPoint, Vec3& hitNormal);
    
    // Island system integration
    void setIslandSystem(IslandChunkSystem* islandSystem) { m_islandSystem = islandSystem; }

    // Debug and testing methods
    void debugCollisionInfo(const Vec3& playerPos, float playerRadius = 0.5f);
    int getTotalCollisionFaces() const;

   private:
    IslandChunkSystem* m_islandSystem = nullptr;
    
    // Helper methods
    bool checkChunkCollision(const VoxelChunk* chunk, const Vec3& playerPos, const Vec3& chunkWorldPos, Vec3& outNormal, float playerRadius);
    bool checkChunkCollisionWithPenetration(const VoxelChunk* chunk, const Vec3& playerPos, const Vec3& chunkWorldPos,
                                          Vec3& outNormal, float playerRadius, float* outPenetrationDepth = nullptr);
};

// Global physics system
extern PhysicsSystem g_physics;
