// PhysicsSystem.h - Basic collision detection system
#pragma once
#include "ECS/ECS.h"
#include "Math/Vec3.h"
#include <vector>

// Forward declarations
class IslandChunkSystem;
class VoxelChunk;

// Simple collision detection system using voxel face culling
class PhysicsSystem
{
   public:
    PhysicsSystem();
    ~PhysicsSystem();

    bool initialize();
    void update(float deltaTime);
    void shutdown();

    // Collision detection methods
    bool checkPlayerCollision(const Vec3& playerPos, Vec3& outNormal, float playerRadius = 0.5f);
    bool checkRayCollision(const Vec3& rayOrigin, const Vec3& rayDirection, float maxDistance, Vec3& hitPoint, Vec3& hitNormal);
    
    // Island system integration
    void setIslandSystem(IslandChunkSystem* islandSystem) { m_islandSystem = islandSystem; }

    // Stub body creation - returns dummy IDs (for future physics engine)
    uint32_t createFloatingIslandBody(const Vec3& position, float mass = 1000.0f);
    uint32_t createStaticBox(const Vec3& position, const Vec3& halfExtent);

    // Stub body manipulation - does nothing (for future physics engine)
    void setBodyPosition(uint32_t bodyID, const Vec3& position);
    Vec3 getBodyPosition(uint32_t bodyID);
    void addForce(uint32_t bodyID, const Vec3& force);
    void addBuoyancyForce(uint32_t bodyID, float buoyancy = 500.0f);

   private:
    IslandChunkSystem* m_islandSystem = nullptr;
    
    // Helper methods
    bool checkChunkCollision(const VoxelChunk* chunk, const Vec3& playerPos, const Vec3& chunkWorldPos, Vec3& outNormal, float playerRadius);
};

// Global physics system
extern PhysicsSystem g_physics;
