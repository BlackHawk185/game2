// FluidSystem.cpp - Particle-based fluid physics implementation
#include "FluidSystem.h"
#include "PhysicsSystem.h"
#include "Math/Vec3.h"
#include <iostream>
#include <algorithm>
#include <cmath>

// Global fluid system instance
FluidSystem g_fluidSystem;

FluidSystem::FluidSystem()
{
}

FluidSystem::~FluidSystem()
{
    shutdown();
}

bool FluidSystem::initialize()
{
    std::cout << "FluidSystem: Initializing particle-based fluid system..." << std::endl;
    
    // Clear spatial grid
    m_spatialGrid.clear();
    
    std::cout << "FluidSystem: Initialized successfully" << std::endl;
    return true;
}

void FluidSystem::update(float deltaTime)
{
    // Update spatial partitioning every frame (needed for collisions)
    updateSpatialGrid();
    
    // Apply physics every frame (needed for smooth movement)
    applyGravity(deltaTime);
    handleCollisions(deltaTime);
    
    // Throttle expensive operations
    static float containerUpdateTimer = 0.0f;
    static float connectionUpdateTimer = 0.0f;
    static float evaporationTimer = 0.0f;
    
    containerUpdateTimer += deltaTime;
    connectionUpdateTimer += deltaTime;
    evaporationTimer += deltaTime;
    
    // Update containers less frequently (every 0.5 seconds)
    if (containerUpdateTimer >= 0.5f) {
        updateContainers(deltaTime);
        containerUpdateTimer = 0.0f;
    }
    
    // Update visual connections less frequently (every 0.1 seconds)
    if (connectionUpdateTimer >= 0.1f) {
        updateParticleConnections();
        connectionUpdateTimer = 0.0f;
    }
    
    // Handle evaporation even less frequently (every 2 seconds)
    if (evaporationTimer >= 2.0f) {
        evaporateParticles(deltaTime);
        evaporationTimer = 0.0f;
    }
}

void FluidSystem::shutdown()
{
    std::cout << "FluidSystem: Shutting down..." << std::endl;
    m_spatialGrid.clear();
}

EntityID FluidSystem::spawnFluidParticle(const Vec3& position, const Vec3& velocity, EntityID parentEntity)
{
    // Check particle limit
    auto* fluidStorage = g_ecs.getStorage<FluidParticleComponent>();
    if (fluidStorage && fluidStorage->size() >= m_maxParticles)
    {
        std::cout << "FluidSystem: Max particles reached (" << m_maxParticles << "), cannot spawn more" << std::endl;
        return INVALID_ENTITY;
    }
    
    // Create entity
    EntityID particleEntity = g_ecs.createEntity();
    
    // Add transform component
    TransformComponent transform;
    transform.position = position;
    g_ecs.addComponent(particleEntity, transform);
    
    // Add velocity component
    VelocityComponent vel;
    vel.velocity = velocity;
    g_ecs.addComponent(particleEntity, vel);
    
    // Add fluid particle component
    FluidParticleComponent fluid;
    fluid.velocity = velocity;
    fluid.parentEntity = parentEntity;
    g_ecs.addComponent(particleEntity, fluid);
    
    // Add fluid render component
    FluidRenderComponent render;
    g_ecs.addComponent(particleEntity, render);
    
    return particleEntity;
}

void FluidSystem::destroyFluidParticle(EntityID particleEntity)
{
    if (particleEntity == INVALID_ENTITY)
        return;
        
    // Remove from any containers
    auto* containerStorage = g_ecs.getStorage<FluidContainerComponent>();
    if (containerStorage)
    {
        for (size_t i = 0; i < containerStorage->entities.size(); ++i)
        {
            auto& container = containerStorage->components[i];
            auto it = std::find(container.containedParticles.begin(), 
                               container.containedParticles.end(), 
                               particleEntity);
            if (it != container.containedParticles.end())
            {
                container.containedParticles.erase(it);
                container.needsUpdate = true;
            }
        }
    }
    
    // Destroy the entity
    g_ecs.destroyEntity(particleEntity);
}

void FluidSystem::applyGravity(float deltaTime)
{
    auto* fluidStorage = g_ecs.getStorage<FluidParticleComponent>();
    auto* velocityStorage = g_ecs.getStorage<VelocityComponent>();
    auto* transformStorage = g_ecs.getStorage<TransformComponent>();
    
    if (!fluidStorage || !velocityStorage || !transformStorage)
        return;
    
    // Apply gravity to all fluid particles
    for (size_t i = 0; i < fluidStorage->entities.size(); ++i)
    {
        EntityID entity = fluidStorage->entities[i];
        auto* velocity = velocityStorage->getComponent(entity);
        auto* transform = transformStorage->getComponent(entity);
        auto* fluid = &fluidStorage->components[i];
        
        if (!velocity || !transform)
            continue;
            
        // Apply gravity
        velocity->acceleration = m_gravity;
        velocity->velocity = velocity->velocity + velocity->acceleration * deltaTime;
        
        // Update position
        Vec3 newPosition = transform->position + velocity->velocity * deltaTime;
        
        // Simple collision with ground (y = 0)
        if (newPosition.y <= fluid->radius)
        {
            newPosition.y = fluid->radius;
            velocity->velocity.y = 0.0f;  // Stop falling
            velocity->velocity = velocity->velocity * 0.8f;  // Apply some friction
        }
        
        transform->position = newPosition;
        fluid->velocity = velocity->velocity;
        fluid->lifeTime += deltaTime;
    }
}

void FluidSystem::handleCollisions(float deltaTime)
{
    // For now, just handle basic terrain collision
    auto* fluidStorage = g_ecs.getStorage<FluidParticleComponent>();
    auto* transformStorage = g_ecs.getStorage<TransformComponent>();
    auto* velocityStorage = g_ecs.getStorage<VelocityComponent>();
    
    if (!fluidStorage || !transformStorage || !velocityStorage)
        return;
        
    for (size_t i = 0; i < fluidStorage->entities.size(); ++i)
    {
        EntityID entity = fluidStorage->entities[i];
        auto* transform = transformStorage->getComponent(entity);
        auto* velocity = velocityStorage->getComponent(entity);
        auto* fluid = &fluidStorage->components[i];
        
        if (!transform || !velocity)
            continue;
            
        // Check collision with terrain using physics system
        Vec3 normal;
        if (g_physics.checkPlayerCollision(transform->position, normal, fluid->radius))
        {
            // Bounce off surface
            float restitution = 0.3f;  // Bounciness
            Vec3 reflection = velocity->velocity - normal * (2.0f * velocity->velocity.dot(normal));
            velocity->velocity = reflection * restitution;
            
            // Move particle slightly away from surface to prevent penetration
            transform->position = transform->position + normal * 0.1f;
        }
    }
}

void FluidSystem::updateContainers(float deltaTime)
{
    // This will be implemented when we add container/pool logic
    // For now, just check if particles are in "containers" (holes in terrain)
    auto* fluidStorage = g_ecs.getStorage<FluidParticleComponent>();
    if (!fluidStorage)
        return;
        
    for (size_t i = 0; i < fluidStorage->entities.size(); ++i)
    {
        EntityID entity = fluidStorage->entities[i];
        checkParticleContainerStatus(entity);
    }
}

void FluidSystem::checkParticleContainerStatus(EntityID particleEntity)
{
    auto* fluid = g_ecs.getComponent<FluidParticleComponent>(particleEntity);
    auto* transform = g_ecs.getComponent<TransformComponent>(particleEntity);
    auto* velocity = g_ecs.getComponent<VelocityComponent>(particleEntity);
    
    if (!fluid || !transform || !velocity)
        return;
        
    // Simple heuristic: if particle is moving slowly and near ground, it's "contained"
    float speed = velocity->velocity.length();
    bool wasInContainer = fluid->inContainer;
    
    if (speed < 0.5f && transform->position.y < 2.0f)
    {
        fluid->inContainer = true;
    }
    else
    {
        fluid->inContainer = false;
    }
}

void FluidSystem::updateParticleConnections()
{
    auto* fluidStorage = g_ecs.getStorage<FluidParticleComponent>();
    auto* renderStorage = g_ecs.getStorage<FluidRenderComponent>();
    auto* transformStorage = g_ecs.getStorage<TransformComponent>();
    
    if (!fluidStorage || !renderStorage || !transformStorage)
        return;
        
    // For each particle, find nearby particles to connect to
    for (size_t i = 0; i < fluidStorage->entities.size(); ++i)
    {
        EntityID entity = fluidStorage->entities[i];
        auto* transform = transformStorage->getComponent(entity);
        auto* render = renderStorage->getComponent(entity);
        
        if (!transform || !render)
            continue;
            
        render->connectedParticles.clear();
        
        // Find nearby particles within connection range
        std::vector<EntityID> neighbors = getNeighborParticles(transform->position, 1.5f);
        for (EntityID neighborEntity : neighbors)
        {
            if (neighborEntity != entity)
            {
                render->connectedParticles.push_back(neighborEntity);
            }
        }
        
        render->isConnectedToOthers = !render->connectedParticles.empty();
    }
}

void FluidSystem::evaporateParticles(float deltaTime)
{
    auto* fluidStorage = g_ecs.getStorage<FluidParticleComponent>();
    if (!fluidStorage)
        return;
        
    std::vector<EntityID> toDestroy;
    
    for (size_t i = 0; i < fluidStorage->entities.size(); ++i)
    {
        EntityID entity = fluidStorage->entities[i];
        auto* fluid = &fluidStorage->components[i];
        
        // Only evaporate particles not in containers
        if (!fluid->inContainer && fluid->lifeTime > m_evaporationTime)
        {
            toDestroy.push_back(entity);
        }
    }
    
    // Destroy evaporated particles
    for (EntityID entity : toDestroy)
    {
        destroyFluidParticle(entity);
    }
}

std::vector<EntityID> FluidSystem::getVisibleParticles(const Vec3& cameraPos, float maxDistance)
{
    std::vector<EntityID> visibleParticles;
    
    auto* fluidStorage = g_ecs.getStorage<FluidParticleComponent>();
    auto* transformStorage = g_ecs.getStorage<TransformComponent>();
    
    if (!fluidStorage || !transformStorage)
        return visibleParticles;
        
    for (size_t i = 0; i < fluidStorage->entities.size(); ++i)
    {
        EntityID entity = fluidStorage->entities[i];
        auto* transform = transformStorage->getComponent(entity);
        
        if (!transform)
            continue;
            
        float distance = (transform->position - cameraPos).length();
        if (distance <= maxDistance)
        {
            visibleParticles.push_back(entity);
        }
    }
    
    return visibleParticles;
}

void FluidSystem::updateSpatialGrid()
{
    m_spatialGrid.clear();
    
    auto* fluidStorage = g_ecs.getStorage<FluidParticleComponent>();
    auto* transformStorage = g_ecs.getStorage<TransformComponent>();
    
    if (!fluidStorage || !transformStorage)
        return;
        
    for (size_t i = 0; i < fluidStorage->entities.size(); ++i)
    {
        EntityID entity = fluidStorage->entities[i];
        auto* transform = transformStorage->getComponent(entity);
        
        if (!transform)
            continue;
            
        int64_t cellKey = getCellKey(transform->position);
        m_spatialGrid[cellKey].particles.push_back(entity);
    }
}

int64_t FluidSystem::getCellKey(const Vec3& position)
{
    int32_t x = static_cast<int32_t>(std::floor(position.x / CELL_SIZE));
    int32_t y = static_cast<int32_t>(std::floor(position.y / CELL_SIZE));
    int32_t z = static_cast<int32_t>(std::floor(position.z / CELL_SIZE));
    
    // Pack 3D coordinates into 64-bit key
    return (static_cast<int64_t>(x) << 42) | 
           (static_cast<int64_t>(y) << 21) | 
           static_cast<int64_t>(z);
}

std::vector<EntityID> FluidSystem::getNeighborParticles(const Vec3& position, float radius)
{
    std::vector<EntityID> neighbors;
    
    int32_t cellRadius = static_cast<int32_t>(std::ceil(radius / CELL_SIZE));
    int32_t centerX = static_cast<int32_t>(std::floor(position.x / CELL_SIZE));
    int32_t centerY = static_cast<int32_t>(std::floor(position.y / CELL_SIZE));
    int32_t centerZ = static_cast<int32_t>(std::floor(position.z / CELL_SIZE));
    
    auto* transformStorage = g_ecs.getStorage<TransformComponent>();
    if (!transformStorage)
        return neighbors;
    
    // Check neighboring cells
    for (int32_t dx = -cellRadius; dx <= cellRadius; ++dx)
    {
        for (int32_t dy = -cellRadius; dy <= cellRadius; ++dy)
        {
            for (int32_t dz = -cellRadius; dz <= cellRadius; ++dz)
            {
                int64_t cellKey = (static_cast<int64_t>(centerX + dx) << 42) | 
                                 (static_cast<int64_t>(centerY + dy) << 21) | 
                                 static_cast<int64_t>(centerZ + dz);
                
                auto it = m_spatialGrid.find(cellKey);
                if (it == m_spatialGrid.end())
                    continue;
                    
                for (EntityID entity : it->second.particles)
                {
                    auto* transform = transformStorage->getComponent(entity);
                    if (transform && (transform->position - position).length() <= radius)
                    {
                        neighbors.push_back(entity);
                    }
                }
            }
        }
    }
    
    return neighbors;
}

bool FluidSystem::checkTerrainCollision(const Vec3& position, Vec3& normal)
{
    // Use existing physics system for terrain collision
    return g_physics.checkPlayerCollision(position, normal, 0.5f);
}
