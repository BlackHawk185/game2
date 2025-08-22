// PhysicsSystem.cpp - Jolt Physics implementation
#include "PhysicsSystem.h"
#include <iostream>
#include <Jolt/Physics/Body/BodyInterface.h>

PhysicsSystem g_physics;

PhysicsSystem::PhysicsSystem() {
}

PhysicsSystem::~PhysicsSystem() {
    shutdown();
}

bool PhysicsSystem::initialize() {
    // Register allocation hook
    JPH::RegisterDefaultAllocator();
    
    // Create factory
    JPH::Factory::sInstance = new JPH::Factory();
    
    // Register physics types
    JPH::RegisterTypes();
    
    // Create temp allocator
    m_tempAllocator = new JPH::TempAllocatorImpl(10 * 1024 * 1024); // 10 MB
    
    // Create job system
    m_jobSystem = new JPH::JobSystemThreadPool(JPH::cMaxPhysicsJobs, JPH::cMaxPhysicsBarriers, std::thread::hardware_concurrency() - 1);
    
    // Create layer interfaces
    m_broadPhaseLayerInterface = new BPLayerInterfaceImpl();
    m_objectVsBroadPhaseLayerFilter = new ObjectVsBroadPhaseLayerFilterImpl();
    m_objectLayerPairFilter = new ObjectLayerPairFilterImpl();
    
    // Create physics system
    m_physicsSystem = new JPH::PhysicsSystem();
    m_physicsSystem->Init(1024, 0, 1024, 1024, *m_broadPhaseLayerInterface, *m_objectVsBroadPhaseLayerFilter, *m_objectLayerPairFilter);
    
    // Set gravity (reduced for floating effect)
    m_physicsSystem->SetGravity(JPH::Vec3(0, -5.0f, 0)); // Lighter gravity for floating islands
    
    return true;
}

void PhysicsSystem::update(float deltaTime) {
    if (!m_physicsSystem) return;
    
    // Step the physics simulation
    m_physicsSystem->Update(deltaTime, 1, m_tempAllocator, m_jobSystem);
}

JPH::BodyID PhysicsSystem::createFloatingIslandBody(const Vec3& position, float mass) {
    if (!m_physicsSystem) return JPH::BodyID();
    
    // Create a sphere shape for the island
    JPH::RefConst<JPH::Shape> sphereShape = new JPH::SphereShape(16.0f); // 16 meter radius
    
    // Create body creation settings
    JPH::BodyCreationSettings bodySettings(sphereShape, 
        JPH::Vec3(position.x, position.y, position.z), 
        JPH::Quat::sIdentity(), 
        JPH::EMotionType::Dynamic, 
        Layers::MOVING);
    
    bodySettings.mMassPropertiesOverride.mMass = mass;
    bodySettings.mLinearDamping = 0.1f;  // Some air resistance
    bodySettings.mAngularDamping = 0.2f; // Rotational damping
    
    // Create the body
    JPH::Body* body = m_physicsSystem->GetBodyInterface().CreateBody(bodySettings);
    if (!body) return JPH::BodyID();
    
    JPH::BodyID bodyID = body->GetID();
    
    // Add to physics world
    m_physicsSystem->GetBodyInterface().AddBody(bodyID, JPH::EActivation::Activate);
    
    return bodyID;
}

void PhysicsSystem::addBuoyancyForce(JPH::BodyID bodyID, float buoyancy) {
    if (!m_physicsSystem) return;
    
    // Add upward buoyancy force to counter gravity
    JPH::Vec3 buoyancyForce(0, buoyancy, 0);
    m_physicsSystem->GetBodyInterface().AddForce(bodyID, buoyancyForce);
}

Vec3 PhysicsSystem::getBodyPosition(JPH::BodyID bodyID) {
    if (!m_physicsSystem) return Vec3();
    
    JPH::Vec3 pos = m_physicsSystem->GetBodyInterface().GetPosition(bodyID);
    return Vec3(pos.GetX(), pos.GetY(), pos.GetZ());
}

void PhysicsSystem::setBodyPosition(JPH::BodyID bodyID, const Vec3& position) {
    if (!m_physicsSystem) return;
    
    m_physicsSystem->GetBodyInterface().SetPosition(bodyID, 
        JPH::Vec3(position.x, position.y, position.z), 
        JPH::EActivation::Activate);
}

void PhysicsSystem::addForce(JPH::BodyID bodyID, const Vec3& force) {
    if (!m_physicsSystem) return;
    
    m_physicsSystem->GetBodyInterface().AddForce(bodyID, 
        JPH::Vec3(force.x, force.y, force.z));
}

void PhysicsSystem::shutdown() {
    std::cout << "Shutting down Jolt Physics..." << std::endl;
    
    if (m_physicsSystem) {
        delete m_physicsSystem;
        m_physicsSystem = nullptr;
    }
    
    delete m_objectLayerPairFilter;
    delete m_objectVsBroadPhaseLayerFilter;
    delete m_broadPhaseLayerInterface;
    delete m_jobSystem;
    delete m_tempAllocator;
    
    // Unregister types
    JPH::UnregisterTypes();
    
    // Destroy factory
    delete JPH::Factory::sInstance;
    JPH::Factory::sInstance = nullptr;
}
