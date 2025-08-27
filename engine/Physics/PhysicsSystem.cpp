// PhysicsSystem.cpp - Stub physics implementation (Jolt removed)
#include "PhysicsSystem.h"

#include <iostream>

PhysicsSystem g_physics;

PhysicsSystem::PhysicsSystem() {}

PhysicsSystem::~PhysicsSystem()
{
    shutdown();
}

bool PhysicsSystem::initialize()
{
    // Removed verbose debug output
    return true;
}

void PhysicsSystem::update(float deltaTime)
{
    // Stub - no physics simulation
}

uint32_t PhysicsSystem::createFloatingIslandBody(const Vec3& position, float mass)
{
    // Stub - return dummy ID
    static uint32_t nextID = 1;
    return nextID++;
}

uint32_t PhysicsSystem::createStaticBox(const Vec3& position, const Vec3& halfExtent)
{
    // Stub - return dummy ID
    static uint32_t nextID = 1000;
    return nextID++;
}

void PhysicsSystem::addBuoyancyForce(uint32_t bodyID, float buoyancy)
{
    // Stub - does nothing
}

Vec3 PhysicsSystem::getBodyPosition(uint32_t bodyID)
{
    // Stub - return zero position
    return Vec3(0, 0, 0);
}

void PhysicsSystem::setBodyPosition(uint32_t bodyID, const Vec3& position)
{
    // Stub - does nothing
}

void PhysicsSystem::addForce(uint32_t bodyID, const Vec3& force)
{
    // Stub - does nothing
}

void PhysicsSystem::shutdown()
{
    // Removed verbose debug output
}
