// FrustumCuller.h - Fast view frustum culling for chunks
#pragma once
#include "../Math/Vec3.h"
#include <array>

class Camera;

// Frustum planes for efficient culling
struct Frustum {
    std::array<Vec3, 6> planes;  // Left, Right, Top, Bottom, Near, Far (normals)
    std::array<float, 6> distances; // Distance from origin
    
    // Update frustum from camera
    void updateFromCamera(const Camera& camera, float aspect, float fovDegrees = 75.0f);
    
    // AABB-Frustum intersection tests
    bool intersectsAABB(const Vec3& center, const Vec3& halfSize) const;
    bool intersectsSphere(const Vec3& center, float radius) const;
    
    // Chunk-specific tests (32x32x32 chunks)
    bool culls32x32Chunk(const Vec3& chunkWorldPos) const;
    
    // Debug visualization
    void debugPrint() const;
};

class FrustumCuller {
public:
    FrustumCuller();
    
    // Update culling frustum from camera
    void updateFromCamera(const Camera& camera, float aspect, float fovDegrees = 75.0f);
    
    // Culling tests
    bool shouldCullChunk(const Vec3& chunkCenter, float chunkRadius = 27.7f) const;
    bool shouldCullAABB(const Vec3& center, const Vec3& halfSize) const;
    bool shouldCullSphere(const Vec3& center, float radius) const;
    
    // Distance-based culling
    void setRenderDistance(float distance) { m_renderDistance = distance; }
    bool shouldCullByDistance(const Vec3& chunkCenter, const Vec3& cameraPos) const;
    
    // Statistics
    struct CullingStats {
        uint32_t chunksConsidered = 0;
        uint32_t chunksCulled = 0;
        uint32_t chunksRendered = 0;
        float cullPercentage = 0.0f;
        
        void reset() { 
            chunksConsidered = chunksCulled = chunksRendered = 0; 
            cullPercentage = 0.0f;
        }
        
        void update() {
            if (chunksConsidered > 0) {
                cullPercentage = (float)chunksCulled / chunksConsidered * 100.0f;
            }
        }
    };
    
    const CullingStats& getStats() const { return m_stats; }
    void resetStats() { m_stats.reset(); }
    
    // Configuration
    void setEnabled(bool enabled) { m_enabled = enabled; }
    bool isEnabled() const { return m_enabled; }
    
private:
    Frustum m_frustum;
    float m_renderDistance = 256.0f;  // Max render distance
    bool m_enabled = true;
    mutable CullingStats m_stats;
};

// Global frustum culler
extern FrustumCuller g_frustumCuller;
