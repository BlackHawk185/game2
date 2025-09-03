// RaycastLighting.h - Reverse raycast lighting for voxel cube faces
// This calculates realistic lighting by shooting rays FROM cube faces TO light sources
#pragma once

#include "LightSource.h"
#include "Math/Vec3.h"
#include "World/VoxelRaycaster.h"
#include <vector>
#include <cstdint>

class IslandChunkSystem;

// **FACE LIGHTING DATA** - Lighting information for a voxel cube face
struct FaceLighting {
    Vec3 color;              // Final RGB color for this face
    float brightness;        // Overall brightness (0.0 to 1.0+)
    float shadowFactor;      // How much in shadow (0.0 = full shadow, 1.0 = full light)
    Vec3 reflectedColor;     // Color from light bouncing off nearby surfaces
    
    FaceLighting() : color(0, 0, 0), brightness(0.0f), shadowFactor(1.0f), reflectedColor(0, 0, 0) {}
};

// **CUBE FACE IDENTIFIERS** - Standard face orientations
enum class CubeFace : int {
    POSITIVE_Z = 0,  // Front face (+Z)
    NEGATIVE_Z = 1,  // Back face (-Z) 
    POSITIVE_Y = 2,  // Top face (+Y)
    NEGATIVE_Y = 3,  // Bottom face (-Y)
    POSITIVE_X = 4,  // Right face (+X)
    NEGATIVE_X = 5   // Left face (-X)
};

// **FACE LIGHTING CACHE** - Store calculated face lighting to avoid recalculation
// Uses SoA for cache efficiency when processing many faces
struct FaceLightingCacheSoA {
    std::vector<Vec3> facePositions;     // World positions of face centers
    std::vector<int> faceDirections;     // Which face (0-5)
    std::vector<Vec3> colors;            // Calculated light colors
    std::vector<float> brightnesses;     // Brightness values
    std::vector<uint32_t> timestamps;    // When each sample was calculated
    
    void clear() {
        facePositions.clear();
        faceDirections.clear();
        colors.clear();
        brightnesses.clear();
        timestamps.clear();
    }
    
    size_t size() const { return facePositions.size(); }
    
    void addFaceLighting(const Vec3& facePos, int faceDir, const Vec3& color, float brightness, uint32_t time) {
        facePositions.push_back(facePos);
        faceDirections.push_back(faceDir);
        colors.push_back(color);
        brightnesses.push_back(brightness);
        timestamps.push_back(time);
    }
};

// **REVERSE RAYCAST QUALITY** - Control quality vs performance for face-to-light casting
// **REVERSE RAYCAST QUALITY** - Control quality vs performance for face-to-light casting
struct ReverseRaycastQuality {
    int shadowSamples = 4;           // Samples around light for soft shadows
    int bounceRays = 8;              // Rays for calculating reflected light
    int maxBounces = 2;              // How many times light can bounce
    float shadowBias = 0.01f;        // Prevent shadow acne
    float maxLightDistance = 100.0f; // Don't calculate very distant lights
    bool enableReflectedLight = true; // Light bouncing off nearby surfaces
    bool enableSoftShadows = true;   // Multiple samples for soft shadow edges
    
    // **PERFORMANCE PRESETS** optimized for voxel face lighting
    static ReverseRaycastQuality fastest() {
        ReverseRaycastQuality q;
        q.shadowSamples = 1;         // Single ray per light
        q.bounceRays = 0;            // No reflected light
        q.maxBounces = 0;
        q.enableReflectedLight = false;
        q.enableSoftShadows = false;
        return q;
    }
    
    static ReverseRaycastQuality fast() {
        ReverseRaycastQuality q;
        q.shadowSamples = 1;         // Single ray per light
        q.bounceRays = 0;            // No reflected light
        q.maxBounces = 0;
        q.enableReflectedLight = false;
        q.enableSoftShadows = false;
        return q;
    }
    
    static ReverseRaycastQuality balanced() {
        ReverseRaycastQuality q; // Default values are already balanced
        return q;
    }
    
    static ReverseRaycastQuality beautiful() {
        ReverseRaycastQuality q;
        q.shadowSamples = 16;        // Many samples for very soft shadows
        q.bounceRays = 32;           // Lots of reflected light calculation
        q.maxBounces = 4;
        q.enableReflectedLight = true;
        q.enableSoftShadows = true;
        return q;
    }
};

// **REVERSE RAYCAST LIGHTING SYSTEM** - Optimized for voxel cube faces
class RaycastLighting {
public:
    RaycastLighting(LightingSystem* lightSystem, IslandChunkSystem* worldSystem);
    ~RaycastLighting() = default;
    
    // **CORE FACE LIGHTING** - Calculate lighting for a specific cube face
    // This shoots rays FROM the face center TO each light source
    FaceLighting calculateFaceLighting(const Vec3& faceCenter, const Vec3& faceNormal, 
                                     const ReverseRaycastQuality& quality = ReverseRaycastQuality::balanced());
    
    // **VOXEL FACE LIGHTING** - Calculate lighting for a voxel's face during mesh generation  
    // Now includes distance-based LOD culling for performance
    FaceLighting calculateVoxelFaceLighting(uint32_t islandID, const Vec3& voxelPos, CubeFace face, const Vec3& cameraPos);
    
    // **BULK FACE LIGHTING** - Process entire chunk's faces efficiently
    void calculateChunkFaceLighting(uint32_t islandID, const Vec3& chunkOffset,
                                  const std::vector<Vec3>& faceCenters, const std::vector<Vec3>& faceNormals,
                                  std::vector<FaceLighting>& results, 
                                  const ReverseRaycastQuality& quality = ReverseRaycastQuality::balanced());
    
    
    // **CACHE MANAGEMENT**
    void clearCache() { m_faceCache.clear(); }
    void updateCache(float deltaTime); // Remove old entries
    
    // **QUALITY CONTROL**
    void setQuality(const ReverseRaycastQuality& quality) { m_defaultQuality = quality; }
    const ReverseRaycastQuality& getQuality() const { return m_defaultQuality; }
    
    // **PERFORMANCE MONITORING**
    struct LightingStats {
        int facesProcessed = 0;
        int raysShot = 0;
        int shadowRays = 0;
        int bounceRays = 0;
        float totalTime = 0.0f;
        int cacheHits = 0;
        int cacheMisses = 0;
    };
    
    const LightingStats& getStats() const { return m_stats; }
    void resetStats() { m_stats = LightingStats{}; }
    
private:
    // **CORE SYSTEMS**
    LightingSystem* m_lightingSystem;     // Light source management
    IslandChunkSystem* m_worldSystem;     // Voxel world for ray intersections
    ReverseRaycastQuality m_defaultQuality; // Default quality settings
    
    // **CACHING SYSTEM**
    FaceLightingCacheSoA m_faceCache;     // Previously calculated face lighting
    uint32_t m_currentTimestamp;          // For cache aging
    
    // **PERFORMANCE TRACKING**
    mutable LightingStats m_stats;        // Performance statistics
    
    // **HELPER METHODS FOR REVERSE RAYCASTING**
    // Cast ray from face center to light source, check for shadows
    bool isInShadow(const Vec3& faceCenter, const Vec3& lightPosition, const LightSource& light);
    
    // Calculate direct lighting from face to single light source
    Vec3 calculateDirectLightToFace(const LightSource& light, const Vec3& faceCenter, const Vec3& faceNormal, const ReverseRaycastQuality& quality);
    
    // Calculate reflected light bouncing off nearby surfaces
    Vec3 calculateReflectedLight(const Vec3& faceCenter, const Vec3& faceNormal, const ReverseRaycastQuality& quality);
    
    // Get face normal vector from CubeFace enum
    Vec3 getFaceNormal(CubeFace face);
    
    // Get face center position from voxel position and face direction
    Vec3 getFaceCenter(const Vec3& voxelPos, CubeFace face);
    
    // Calculate light attenuation over distance
    float calculateAttenuation(float distance, float lightRange);
    
    // Get material color for light reflection calculations
    Vec3 getMaterialColor(uint32_t islandID, const Vec3& blockPos);
    
    // Cache lookup and storage for face lighting
    bool getCachedFaceLighting(const Vec3& faceCenter, CubeFace face, FaceLighting& lighting);
    void cacheFaceLighting(const Vec3& faceCenter, CubeFace face, const FaceLighting& lighting);
};
