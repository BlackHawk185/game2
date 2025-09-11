// ModelInstanceRenderer.h - Instanced rendering for GLB-based models (e.g., decorative grass)
#pragma once

#include <vector>
#include <unordered_map>
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
    void renderDepthGrassChunk(VoxelChunk* chunk, const Vec3& worldOffset);

private:
    bool ensureChunkInstancesUploaded(VoxelChunk* chunk);
    bool ensureShaders();
    bool buildGPUFromModel();

    // Internal GL helpers
    GLuint m_program = 0;       // forward shader
    GLuint m_depthProgram = 0;  // depth-only shader

    // Uniform locations (forward)
    int uProj = -1, uView = -1, uModel = -1;
    int uCascadeCount = -1, uLightVP = -1, uCascadeSplits = -1, uShadowMaps = -1;
    int uShadowTexel = -1, uLightDir = -1, uTime = -1;

    // Uniform locations (depth)
    int d_uModel = -1, d_uLightVP = -1, d_uTime = -1;

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

    // Instance buffers per chunk
    std::unordered_map<VoxelChunk*, ChunkInstanceBuffer> m_chunkInstances;
};

// Global instance
extern ModelInstanceRenderer* g_modelRenderer;
