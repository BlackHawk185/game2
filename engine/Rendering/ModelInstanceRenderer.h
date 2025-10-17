// ModelInstanceRenderer.h - Instanced rendering for GLB-based models (e.g., decorative grass)
#pragma once

#include <vector>
#include <unordered_map>
#include <memory>
#include <string>
#include <glm/glm.hpp>

#include "../Math/Vec3.h"
#include "../World/VoxelChunk.h"

using GLuint = unsigned int;
using GLsizei = int;

// Minimal vertex/mesh structure for GLB
struct ModelPrimitiveGPU {
    GLuint vao = 0;
    GLuint vbo = 0;      // interleaved position, normal, uv
    GLuint ebo = 0;
    GLsizei indexCount = 0;
};

struct ModelGPU {
    std::vector<ModelPrimitiveGPU> primitives;
    bool valid = false;
};

// Per-chunk instance buffer
struct ChunkInstanceBuffer {
    GLuint instanceVBO = 0; // mat3x4 or per-instance vec4 data, kept minimal here
    GLsizei count = 0;
    bool isUploaded = false; // Track if data is already uploaded to GPU
};

class ModelInstanceRenderer {
public:
    ModelInstanceRenderer();
    ~ModelInstanceRenderer();

    bool initialize();
    void shutdown();

    // Load grass model from GLB path (cached)
    bool loadGrassModel(const std::string& path);

    // Update per-frame (for time-based wind)
    void update(float deltaTime);

    // Cascade and lighting data
    void setCascadeCount(int count);
    void setCascadeMatrix(int index, const glm::mat4& lightVP);
    void setCascadeSplits(const float* splits, int count);
    void setLightDir(const glm::vec3& lightDir);

    // Render passes for decorative grass per chunk
    void renderGrassChunk(VoxelChunk* chunk, const Vec3& worldOffset, const glm::mat4& view, const glm::mat4& proj);
    void beginDepthPassCascade(int cascadeIndex, const glm::mat4& lightVP);
    void endDepthPassCascade(int screenWidth, int screenHeight);

private:
    bool ensureChunkInstancesUploaded(VoxelChunk* chunk);
    bool ensureShaders();
    bool buildGPUFromModel();

    // Internal GL helpers
    GLuint m_program = 0;       // forward shader

    // Uniform locations (forward)
    int uProj = -1, uView = -1, uModel = -1;
    int uCascadeCount = -1, uLightVP = -1, uCascadeSplits = -1, uShadowMaps = -1;
    int uShadowTexel = -1, uLightDir = -1, uTime = -1;
    int uShadowMaps0 = -1, uShadowMaps1 = -1, uShadowMaps2 = -1; // Individual shadow map samplers
    int uGrassTexture = -1; // Grass texture sampler

    // Shadow/cascade
    int m_cascadeCount = 0;
    glm::mat4 m_lightVPs[4];
    float m_cascadeSplits[4] = {0,0,0,0};
    glm::vec3 m_lightDir{ -0.3f, -1.0f, -0.2f };

    // Time for wind
    float m_time = 0.0f;

    // GPU model
    ModelGPU m_grassModel;
    std::string m_grassPath;
    GLuint m_albedoTex = 0;       // GLB baseColor texture if available
    GLuint m_engineGrassTex = 0;  // Engine grass.png texture (preferred)

    // Instance buffers per chunk
    std::unordered_map<VoxelChunk*, ChunkInstanceBuffer> m_chunkInstances;
};

// Global instance (owned pointer)
extern std::unique_ptr<ModelInstanceRenderer> g_modelRenderer;
