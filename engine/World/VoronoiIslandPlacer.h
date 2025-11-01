// VoronoiIslandPlacer.h - Procedural island placement using Voronoi/Cellular noise
#pragma once

#include "../Math/Vec3.h"
#include <vector>
#include <cstdint>

struct IslandDefinition {
    Vec3 position;      // Island center position
    float radius;       // Island size/radius
    uint32_t seed;      // Unique seed for terrain generation
};

class VoronoiIslandPlacer {
public:
    VoronoiIslandPlacer();
    
    // Tunable parameters for advanced control
    float verticalSpreadMultiplier = 100.0f;    // Vertical Y-axis spread (±units)
    float heightNoiseFrequency = 0.005f;         // Frequency for vertical variation
    float cellCenterThreshold = 0.1f;            // Threshold for detecting cell centers (lower = stricter)
    
    // Generate island placements using Voronoi/Cellular noise
    // worldSeed: Master seed for reproducible world generation
    // regionSize: Size of the world region to fill with islands
    // islandDensity: Islands per 1000x1000 unit area (scales infinitely)
    // minRadius/maxRadius: Range for island sizes
    std::vector<IslandDefinition> generateIslands(
        uint32_t worldSeed,
        float regionSize,
        float islandDensity,
        float minRadius,
        float maxRadius
    );
    
private:
    // Generate a Voronoi cell point
    Vec3 generateVoronoiPoint(int cellX, int cellY, int cellZ, uint32_t seed);
    
    // Calculate distance to nearest Voronoi point
    float getVoronoiDistance(const Vec3& position, uint32_t seed, Vec3& nearestPoint);
};
