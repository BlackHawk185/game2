// PhysicsSystem.h - Basic collision detection system
#pragma once
#include "ECS/ECS.h"
#include "Math/Vec3.h"
#include <vector>

// Forward declarations
class IslandChunkSystem;
class VoxelChunk;
struct FloatingIsland;

// Ground detection information for player physics
struct GroundInfo
{
    bool isGrounded = false;              // Is the player standing on solid ground?
    uint32_t standingOnIslandID = 0;      // Which island is the player standing on?
    Vec3 groundNormal = Vec3(0, 1, 0);    // Surface normal of the ground
    Vec3 groundVelocity = Vec3(0, 0, 0);  // Velocity of the ground (for moving platforms)
    Vec3 groundContactPoint = Vec3(0, 0, 0); // Where exactly we're touching the ground
    float distanceToGround = 999.0f;      // Distance to ground (for coyote time, etc.)
};

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
    
    // Ground detection for player physics on moving platforms
    GroundInfo detectGround(const Vec3& playerPos, float playerRadius = 0.5f, float rayLength = 0.2f);
    
    // Find contact point along a movement vector
    // Returns distance traveled before collision (0.0 to 1.0, where 1.0 = full movement)
    // If no collision, returns 1.0
    float findContactPoint(const Vec3& fromPos, const Vec3& toPos, float entityRadius, Vec3* outContactNormal = nullptr);
    
    // Capsule collision detection (for humanoid characters)
    // Capsule is defined by center position, radius, and height
    // Height is total height including hemispherical caps
    bool checkCapsuleCollision(const Vec3& capsuleCenter, float radius, float height, Vec3& outNormal, const FloatingIsland** outIsland = nullptr);
    float findCapsuleContactPoint(const Vec3& fromPos, const Vec3& toPos, float radius, float height, Vec3* outContactNormal = nullptr);
    GroundInfo detectGroundCapsule(const Vec3& capsuleCenter, float radius, float height, float rayMargin = 0.1f);
    
    // NEW: Direct voxel grid collision (MUCH faster - no face iteration!)
    // Sweeps a capsule through voxel grid and returns first solid voxel hit
    bool sweepCapsuleVoxel(const Vec3& fromPos, const Vec3& toPos, float radius, float height, 
                           Vec3& outContactPoint, Vec3& outNormal, const FloatingIsland** outIsland = nullptr);
    
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
    bool checkChunkCapsuleCollision(const VoxelChunk* chunk, const Vec3& capsuleCenter, const Vec3& chunkWorldPos,
                                   Vec3& outNormal, float radius, float height);
    
    // NEW: Direct voxel grid sweep (AABB sweep through 3D grid - extremely fast!)
    bool sweepCapsuleThroughChunk(const VoxelChunk* chunk, const Vec3& chunkWorldPos,
                                  const Vec3& fromPos, const Vec3& toPos, 
                                  float radius, float height,
                                  Vec3& outContactPoint, Vec3& outNormal);
};

// Global physics system
extern PhysicsSystem g_physics;
