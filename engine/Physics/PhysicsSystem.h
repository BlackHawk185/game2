// PhysicsSystem.h - Jolt Physics integration for floating islands
#pragma once
#include "Math/Vec3.h"
#include "ECS/ECS.h"
#include <Jolt/Jolt.h>
#include <Jolt/RegisterTypes.h>
#include <Jolt/Core/Factory.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Core/JobSystemThreadPool.h>
#include <Jolt/Physics/PhysicsSettings.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Body/BodyActivationListener.h>

// Simple object layers for our physics world
namespace Layers {
    static constexpr JPH::ObjectLayer NON_MOVING = 0;
    static constexpr JPH::ObjectLayer MOVING = 1;
    static constexpr JPH::uint NUM_LAYERS = 2;
}

// Simple broad phase layers
namespace BroadPhaseLayers {
    static constexpr JPH::BroadPhaseLayer NON_MOVING(0);
    static constexpr JPH::BroadPhaseLayer MOVING(1);
    static constexpr JPH::uint NUM_LAYERS(2);
}

// Broad phase layer interface
class BPLayerInterfaceImpl final : public JPH::BroadPhaseLayerInterface {
public:
    BPLayerInterfaceImpl() {
        mObjectToBroadPhase[Layers::NON_MOVING] = BroadPhaseLayers::NON_MOVING;
        mObjectToBroadPhase[Layers::MOVING] = BroadPhaseLayers::MOVING;
    }

    virtual JPH::uint GetNumBroadPhaseLayers() const override {
        return BroadPhaseLayers::NUM_LAYERS;
    }

    virtual JPH::BroadPhaseLayer GetBroadPhaseLayer(JPH::ObjectLayer inLayer) const override {
        JPH_ASSERT(inLayer < Layers::NUM_LAYERS);
        return mObjectToBroadPhase[inLayer];
    }
    
    virtual const char* GetBroadPhaseLayerName(JPH::BroadPhaseLayer inLayer) const override {
        switch ((JPH::BroadPhaseLayer::Type)inLayer) {
            case (JPH::BroadPhaseLayer::Type)BroadPhaseLayers::NON_MOVING: return "NON_MOVING";
            case (JPH::BroadPhaseLayer::Type)BroadPhaseLayers::MOVING: return "MOVING";
            default: JPH_ASSERT(false); return "INVALID";
        }
    }

private:
    JPH::BroadPhaseLayer mObjectToBroadPhase[Layers::NUM_LAYERS];
};

// Object vs broad phase layer filter
class ObjectVsBroadPhaseLayerFilterImpl : public JPH::ObjectVsBroadPhaseLayerFilter {
public:
    virtual bool ShouldCollide(JPH::ObjectLayer inLayer1, JPH::BroadPhaseLayer inLayer2) const override {
        switch (inLayer1) {
        case Layers::NON_MOVING:
            return inLayer2 == BroadPhaseLayers::MOVING;
        case Layers::MOVING:
            return true;
        default:
            JPH_ASSERT(false);
            return false;
        }
    }
};

// Object vs object layer filter
class ObjectLayerPairFilterImpl : public JPH::ObjectLayerPairFilter {
public:
    virtual bool ShouldCollide(JPH::ObjectLayer inObject1, JPH::ObjectLayer inObject2) const override {
        switch (inObject1) {
        case Layers::NON_MOVING:
            return inObject2 == Layers::MOVING;
        case Layers::MOVING:
            return true;
        default:
            JPH_ASSERT(false);
            return false;
        }
    }
};

class PhysicsSystem {
public:
    PhysicsSystem();
    ~PhysicsSystem();
    
    bool initialize();
    void update(float deltaTime);
    void shutdown();
    
    // Body creation for islands
    JPH::BodyID createFloatingIslandBody(const Vec3& position, float mass = 1000.0f);
    JPH::BodyID createStaticBox(const Vec3& position, const Vec3& halfExtent);
    
    // Body manipulation
    void setBodyPosition(JPH::BodyID bodyID, const Vec3& position);
    Vec3 getBodyPosition(JPH::BodyID bodyID);
    void addForce(JPH::BodyID bodyID, const Vec3& force);
    void addBuoyancyForce(JPH::BodyID bodyID, float buoyancy = 500.0f);
    
    JPH::PhysicsSystem& getJoltSystem() { return *m_physicsSystem; }
    
private:
    // Jolt Physics components
    JPH::TempAllocatorImpl* m_tempAllocator = nullptr;
    JPH::JobSystemThreadPool* m_jobSystem = nullptr;
    BPLayerInterfaceImpl* m_broadPhaseLayerInterface = nullptr;
    ObjectVsBroadPhaseLayerFilterImpl* m_objectVsBroadPhaseLayerFilter = nullptr;
    ObjectLayerPairFilterImpl* m_objectLayerPairFilter = nullptr;
    JPH::PhysicsSystem* m_physicsSystem = nullptr;
};

// Global physics system
extern PhysicsSystem g_physics;
