// VoxelRenderer.h - Optimized voxel rendering with proper culling
#pragma once
#include <array>
#include <vector>

#include "../Math/Vec3.h"

// Forward declarations
class Camera;
struct VoxelChunk;

enum class CullingMode
{
    NONE = 0,
    FACE_CULLING = 1,
    FRUSTUM_CULLING = 2,
    DISTANCE_CULLING = 4,
    OCCLUSION_CULLING = 8,
    ALL = FACE_CULLING | FRUSTUM_CULLING | DISTANCE_CULLING | OCCLUSION_CULLING
};

// Forward declare Frustum (defined in FrustumCuller.h)
struct Frustum;

// Rendering statistics
struct RenderStats
{
    uint32_t chunksConsidered = 0;
    uint32_t chunksRendered = 0;
    uint32_t facesConsidered = 0;
    uint32_t facesRendered = 0;
    uint32_t drawCalls = 0;
    float cullingTimeMs = 0.0f;
    float renderTimeMs = 0.0f;

    void reset()
    {
        chunksConsidered = chunksRendered = facesConsidered = facesRendered = drawCalls = 0;
        cullingTimeMs = renderTimeMs = 0.0f;
    }
};

class VoxelRenderer
{
   public:
    VoxelRenderer();
    ~VoxelRenderer();

    // Core rendering
    void renderChunks(const std::vector<VoxelChunk*>& chunks, const Camera& camera, float aspect);
    void renderChunk(const VoxelChunk& chunk, const Camera& camera);

    // Optimization settings
    void setCullingMode(CullingMode mode)
    {
        m_cullingMode = mode;
    }
    void setRenderDistance(float distance)
    {
        m_renderDistance = distance;
    }
    void setLODDistances(float znear, float mid, float zfar)
    {
        m_lodNear = znear;
        m_lodMid = mid;
        m_lodFar = zfar;
    }

    // Modern GPU preparation (for future shader integration)
    void prepareInstancedRendering();  // Batch voxels for GPU instancing
    void prepareCullingCompute();      // Setup compute shader culling

    // Statistics and debugging
    const RenderStats& getStats() const
    {
        return m_stats;
    }
    void setWireframe(bool enable)
    {
        m_wireframe = enable;
    }

   private:
    // Culling operations
    bool frustumCullChunk(const VoxelChunk& chunk, const Camera& camera, float aspect);
    bool distanceCullChunk(const VoxelChunk& chunk, const Camera& camera);

    // Face culling - the BIG win!
    bool shouldRenderFace(const VoxelChunk& chunk, int x, int y, int z, int faceIndex);
    void renderVoxelFaces(const VoxelChunk& chunk, int x, int y, int z, const Vec3& worldPos);

    // LOD selection
    int selectLOD(float distance);
    void renderVoxelLOD(const VoxelChunk& chunk, int x, int y, int z, const Vec3& worldPos,
                        int lod);

    // Modern GPU hooks
    void setupInstanceBuffer();
    void setupCullingShader();

   private:
    CullingMode m_cullingMode = CullingMode::ALL;
    float m_renderDistance = 256.0f;
    float m_lodNear = 64.0f, m_lodMid = 128.0f, m_lodFar = 256.0f;
    bool m_wireframe = false;

    // m_frustum removed - using global FrustumCuller instead
    RenderStats m_stats;

    // Future GPU resources (placeholders)
    uint32_t m_instanceVBO = 0;
    uint32_t m_cullingComputeShader = 0;
    bool m_modernGPUEnabled = false;
};

// Utility functions for modern GPU detection
namespace GPUCapabilities
{
bool hasRaytracing();      // RTX/RDNA2+ detection
bool hasTensorCores();     // AI acceleration
bool hasComputeShaders();  // Basic compute support
bool hasMeshShaders();     // Next-gen geometry pipeline

void logCapabilities();
}  // namespace GPUCapabilities
