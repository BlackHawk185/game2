// VBORenderer.h - Modern VBO-based renderer with directional shadows
// Features: VAO/VBO mesh management, shader-based lighting, batch rendering
#pragma once

#include <vector>
#include <unordered_map>
#include <memory>

#include "../World/VoxelChunk.h"
#include "../Math/Vec3.h"
#include "../ECS/ECS.h"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include "SimpleShader.h"
#include "CascadedShadowMap.h"

class VBORenderer
{
public:
    VBORenderer();
    ~VBORenderer();

    // Initialize OpenGL VBO extensions and shader system
    bool initialize();
    void shutdown();

    // Matrix management for modern OpenGL
    void setProjectionMatrix(const glm::mat4& projection);
    void setViewMatrix(const glm::mat4& view);
    void setModelMatrix(const glm::mat4& model);
    void setLightVP(const glm::mat4& lightVP);
    void setLightDir(const glm::vec3& lightDir);
    void setCascadeMatrix(int index, const glm::mat4& lightVP);
    void setCascadeCount(int count);
    void setCascadeSplits(const float* splits, int count);

    // Chunk VBO management with VAO support
    void uploadChunkMesh(VoxelChunk* chunk);
    void renderChunk(VoxelChunk* chunk, const Vec3& worldOffset);
    void deleteChunkVBO(VoxelChunk* chunk);
    
    // Shadow depth pass helpers
    void beginDepthPass(const glm::mat4& lightVP);
    void renderDepthChunk(VoxelChunk* chunk, const Vec3& worldOffset);
    void endDepthPass(int screenWidth, int screenHeight);
    // Cascaded
    void beginDepthPassCascade(int cascadeIndex, const glm::mat4& lightVP);
    void endDepthPassCascade(int screenWidth, int screenHeight);

    // Batch rendering for multiple chunks
    void beginBatch();
    void renderChunkBatch(const std::vector<VoxelChunk*>& chunks, const std::vector<Vec3>& offsets);
    void endBatch();

    // Fluid particle rendering
    void renderFluidParticles(const std::vector<EntityID>& particles);
    void renderSphere();

    // Performance stats
    struct RenderStats {
        uint32_t chunksRendered = 0;
        uint32_t verticesRendered = 0;
        uint32_t drawCalls = 0;
        void reset() { chunksRendered = verticesRendered = drawCalls = 0; }
    };
    
    const RenderStats& getStats() const { return m_stats; }
    void resetStats() { m_stats.reset(); }
    
    // Public access to shader for MDI renderer
    SimpleShader* getShader() { return &m_shader; }

private:
    bool m_initialized;
    RenderStats m_stats;
    
    // Simple shader for basic rendering
    SimpleShader m_shader;
    
    // Modern OpenGL state
    glm::mat4 m_projectionMatrix;
    glm::mat4 m_viewMatrix;
    glm::mat4 m_modelMatrix;
    glm::mat4 m_lightVP{1.0f};
    glm::vec3 m_lightDir{ -0.3f, -1.0f, -0.2f };
    int m_cascadeCount = 1;
    glm::mat4 m_lightVPs[4];
    float m_cascadeSplits[4] = { 333.0f, 666.0f, 1000.0f, 1000.0f };

    // Texture IDs for different block types
    unsigned int m_dirtTextureID = 0;
    unsigned int m_stoneTextureID = 0;
    unsigned int m_grassTextureID = 0;

    // Depth-only shader for shadow map pass
    unsigned int m_depthProgram = 0;
    int m_depth_uLightVP = -1;
    int m_depth_uModel = -1;
    int m_activeCascade = -1;

    // Helper methods
    void setupVertexAttributes();
    void setupVAO(VoxelChunk* chunk);
    bool initDepthShader();
};

// Global VBO renderer instance (owned pointer)
extern std::unique_ptr<VBORenderer> g_vboRenderer;
