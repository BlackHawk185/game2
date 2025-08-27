// PhysicsSystem.h - Stub physics system (Jolt removed)
#pragma once
#include "ECS/ECS.h"
#include "Math/Vec3.h"

// Simple stub physics system - no actual physics simulation
class PhysicsSystem
{
   public:
    PhysicsSystem();
    ~PhysicsSystem();

    bool initialize();
    void update(float deltaTime);
    void shutdown();

    // Stub body creation - returns dummy IDs
    uint32_t createFloatingIslandBody(const Vec3& position, float mass = 1000.0f);
    uint32_t createStaticBox(const Vec3& position, const Vec3& halfExtent);

    // Stub body manipulation - does nothing
    void setBodyPosition(uint32_t bodyID, const Vec3& position);
    Vec3 getBodyPosition(uint32_t bodyID);
    void addForce(uint32_t bodyID, const Vec3& force);
    void addBuoyancyForce(uint32_t bodyID, float buoyancy = 500.0f);

   private:
    // No physics components needed for stub
};

// Global physics system
extern PhysicsSystem g_physics;
