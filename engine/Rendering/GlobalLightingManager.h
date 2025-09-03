// GlobalLightingManager.h - Frustum-culled global lighting system
#pragma once

#include <vector>
#include <unordered_map>
#include "../Math/Vec3.h"

class Camera;
class VoxelChunk;
class IslandChunkSystem;

// Manages global lighting for all visible chunks using frustum culling
class GlobalLightingManager {
public:
    GlobalLightingManager();
    ~GlobalLightingManager();

    // Main update function - call once per frame
    void updateGlobalLighting(const Camera& camera, IslandChunkSystem* islandSystem, float aspect = 16.0f/9.0f);

    // Configuration
    void setEnabled(bool enabled) { m_enabled = enabled; }
    bool isEnabled() const { return m_enabled; }
    
    void setAmbientIntensity(float intensity) { m_ambientIntensity = intensity; }
    void setSunIntensity(float intensity) { m_sunIntensity = intensity; }
    void setSunDirection(const Vec3& direction) { m_sunDirection = direction.normalized(); }
    
    // Performance controls
    void setUpdateFrequency(float hz) { m_updateIntervalMs = 1000.0f / hz; }
    void setOcclusionEnabled(bool enabled) { m_occlusionEnabled = enabled; }
    
    // Force immediate update (useful after major world changes)
    void forceUpdate() { m_lastUpdateTime = 0.0f; }

    // Visible chunk tracking - public for system coordination
    struct VisibleChunk {
        VoxelChunk* chunk;
        Vec3 worldPosition;
        uint32_t islandID;
    };

    // Statistics
    struct Stats {
        int chunksConsidered = 0;
        int chunksLit = 0;
        int chunksCulled = 0;
        float updateTimeMs = 0.0f;
    };
    
    const Stats& getStats() const { return m_stats; }
    
    // Access to visible chunks for coordinated rendering
    const std::vector<VisibleChunk>& getVisibleChunks() const { return m_visibleChunks; }

private:
    // Core lighting functions
    void gatherVisibleChunks(const Camera& camera, IslandChunkSystem* islandSystem, float aspect);
    void generateUnifiedLighting();
    void processChunkLighting(VoxelChunk* chunk, const Vec3& chunkWorldPos);
    
    // Optimized lighting functions - spatial partitioning approach
    void gatherVisibleChunksEfficient(const Camera& camera, IslandChunkSystem* islandSystem, float aspect);
    void generateOptimizedLighting();
    void processChunkLightingOptimized(VoxelChunk* chunk, const Vec3& chunkWorldPos);
    bool performFastOcclusionCheck(const Vec3& worldPos, const Vec3& faceNormal) const;
    uint8_t sampleVoxelAtWorldPosOptimized(const Vec3& worldPos) const;
    
    // Cross-chunk raycast for unified lighting
    bool performGlobalSunRaycast(const Vec3& rayStart, const Vec3& sunDirection, float maxDistance) const;
    bool performFastSunRaycast(const Vec3& rayStart, const Vec3& sunDirection, float maxDistance) const;
    uint8_t sampleVoxelAtWorldPos(const Vec3& worldPos) const;

    // Configuration
    bool m_enabled = true;
    float m_ambientIntensity = 0.0f;  // DISABLED for shadow testing - was 0.4f
    float m_sunIntensity = 1.0f;
    Vec3 m_sunDirection{0.3f, -0.8f, 0.5f};  // Angled sun for interesting shadows
    
    // Performance controls
    float m_updateIntervalMs = 100.0f;  // Update lighting every 100ms (10 FPS) instead of every frame
    float m_lastUpdateTime = 0.0f;
    bool m_occlusionEnabled = true;     // Enable proper occlusion checking

    // Visible chunk tracking - moved to private since we have public accessor
    std::vector<VisibleChunk> m_visibleChunks;
    mutable std::unordered_map<uint32_t, const void*> m_islandChunkMap;  // For fast lookups
    
    // Island system reference (set during updateGlobalLighting)
    mutable IslandChunkSystem* m_currentIslandSystem = nullptr;
    
    // Statistics
    mutable Stats m_stats;
};

// Global instance
extern GlobalLightingManager g_globalLighting;
