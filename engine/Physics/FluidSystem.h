// FluidSystem.h - Particle-based fluid physics system
#pragma once
#include "ECS/ECS.h"
#include "Math/Vec3.h"
#include <vector>
#include <unordered_map>

// Fluid particle component
struct FluidParticleComponent
{
    float mass = 1.0f;
    float radius = 0.5f;        // Particle size (half a block)
    float density = 1000.0f;    // Water density
    bool inContainer = false;   // Is particle in a container/pool
    float lifeTime = 0.0f;      // For evaporation
    EntityID parentEntity = INVALID_ENTITY; // Which island/entity spawned this
};

// Container/Pool tracking component
struct FluidContainerComponent
{
    std::vector<EntityID> containedParticles;
    Vec3 minBounds{0, 0, 0};
    Vec3 maxBounds{0, 0, 0};
    float fillLevel = 0.0f;     // 0.0 to 1.0
    bool needsUpdate = true;    // Recalculate fill level
};

// Fluid rendering component (for visual effects)
struct FluidRenderComponent
{
    bool isConnectedToOthers = false;
    std::vector<EntityID> connectedParticles;
    float renderRadius = 0.6f;  // Slightly larger than physics radius for visual blending
};

class FluidSystem
{
public:
    FluidSystem();
    ~FluidSystem();

    bool initialize();
    void update(float deltaTime);
    void handleCollisions(float deltaTime);
    void evaporateParticles(float deltaTime);
    void shutdown();

    // Particle creation/destruction
    EntityID spawnFluidParticle(const Vec3& position, const Vec3& velocity = Vec3(0, 0, 0), EntityID parentEntity = INVALID_ENTITY);
    void destroyFluidParticle(EntityID particleEntity);

    // Container management
    void updateContainers(float deltaTime);
    void checkParticleContainerStatus(EntityID particleEntity);
    
    // Rendering helpers
    void updateParticleConnections();
    std::vector<EntityID> getVisibleParticles(const Vec3& cameraPos, float maxDistance = 100.0f);

    // Configuration
    void setEvaporationTime(float time) { m_evaporationTime = time; }
    void setMaxParticles(size_t max) { m_maxParticles = max; }

private:
    float m_evaporationTime = 30.0f;  // Particles evaporate after 30 seconds if not in container
    size_t m_maxParticles = 1000;
    
    // Spatial partitioning for efficient neighbor queries
    struct SpatialCell
    {
        std::vector<EntityID> particles;
    };
    
    static constexpr float CELL_SIZE = 2.0f;  // 2 block cells
    std::unordered_map<int64_t, SpatialCell> m_spatialGrid;
    
    // Helper methods
    int64_t getCellKey(const Vec3& position);
    std::vector<EntityID> getNeighborParticles(const Vec3& position, float radius = 2.0f);
    bool checkTerrainCollision(const Vec3& position, Vec3& normal);
    void updateSpatialGrid();
};

// Global fluid system
extern FluidSystem g_fluidSystem;
