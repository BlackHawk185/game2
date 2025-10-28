// ModelInstanceRenderer.h - Instanced rendering for GLB-based models (generic multi-model support)
#pragma once

#include <vector>
#include <unordered_map>
#include <memory>
#include <string>
#include <utility>  // for std::pair
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

// Per-chunk, per-block-type instance buffer
struct ChunkInstanceBuffer {
    GLuint instanceVBO = 0; // mat3x4 or per-instance vec4 data, kept minimal here
    GLsizei count = 0;
    bool isUploaded = false; // Track if data is already uploaded to GPU
};

// Hash function for std::pair<VoxelChunk*, uint8_t> (for instance buffer map)
struct ChunkBlockPairHash {
    std::size_t operator()(const std::pair<VoxelChunk*, uint8_t>& pair) const {
        return std::hash<void*>()(pair.first) ^ (std::hash<uint8_t>()(pair.second) << 1);
    }
};

class ModelInstanceRenderer {
public:
    ModelInstanceRenderer();
    ~ModelInstanceRenderer();

    bool initialize();
    void shutdown();

    // Generic model loading (NEW)
    bool loadModel(uint8_t blockID, const std::string& glbPath);

    // Update per-frame (for time-based animations)
    void update(float deltaTime);

    // Cascade and lighting data
    void setCascadeCount(int count);
    void setCascadeMatrix(int index, const glm::mat4& lightVP);
    void setCascadeSplits(const float* splits, int count);
    void setLightDir(const glm::vec3& lightDir);

    // Generic model chunk rendering (NEW)
    void renderModelChunk(uint8_t blockID, VoxelChunk* chunk, const Vec3& chunkLocalPos, const glm::mat4& islandTransform, const glm::mat4& view, const glm::mat4& proj);
    
    void beginDepthPassCascade(int cascadeIndex, const glm::mat4& lightVP);
    void endDepthPassCascade(int screenWidth, int screenHeight);

private:
    bool ensureChunkInstancesUploaded(uint8_t blockID, VoxelChunk* chunk);
    bool ensureShaders();
    bool buildGPUFromModel(uint8_t blockID);
    GLuint compileShaderForBlock(uint8_t blockID);  // NEW: Per-model shader compilation

    // Internal GL helpers
    GLuint m_program = 0;       // DEPRECATED: forward shader (grass-specific with wind) - use m_shaders instead

    // NEW: Per-model shaders
    std::unordered_map<uint8_t, GLuint> m_shaders;
    
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

    // Time for animations
    float m_time = 0.0f;

    // NEW: Multiple GPU models by BlockID
    std::unordered_map<uint8_t, ModelGPU> m_models;
    std::unordered_map<uint8_t, std::string> m_modelPaths;  // Track loaded paths
    std::unordered_map<uint8_t, GLuint> m_albedoTextures;   // Per-model textures
    GLuint m_engineGrassTex = 0;  // Engine grass.png texture (for grass model)

    // NEW: Instance buffers per (chunk, blockID) pair
    std::unordered_map<std::pair<VoxelChunk*, uint8_t>, ChunkInstanceBuffer, ChunkBlockPairHash> m_chunkInstances;
};

// Global instance (owned pointer)
extern std::unique_ptr<ModelInstanceRenderer> g_modelRenderer;
