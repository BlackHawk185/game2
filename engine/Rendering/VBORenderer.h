// VBORenderer.h - Modern VBO-based renderer with directional shadows
// Features: VAO/VBO mesh management, shader-based lighting, batch rendering
#pragma once

#include <vector>
#include <unordered_map>

#include "../World/VoxelChunk.h"
#include "../Math/Vec3.h"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include "SimpleShader.h"

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

    // Chunk VBO management with VAO support
    void uploadChunkMesh(VoxelChunk* chunk);
    void renderChunk(VoxelChunk* chunk, const Vec3& worldOffset);
    void deleteChunkVBO(VoxelChunk* chunk);

    // Batch rendering for multiple chunks
    void beginBatch();
    void renderChunkBatch(const std::vector<VoxelChunk*>& chunks, const std::vector<Vec3>& offsets);
    void endBatch();

    // Performance stats
    struct RenderStats {
        uint32_t chunksRendered = 0;
        uint32_t verticesRendered = 0;
        uint32_t drawCalls = 0;
        void reset() { chunksRendered = verticesRendered = drawCalls = 0; }
    };
    
    const RenderStats& getStats() const { return m_stats; }
    void resetStats() { m_stats.reset(); }

private:
    bool m_initialized;
    RenderStats m_stats;
    
    // Simple shader for basic rendering
    SimpleShader m_shader;
    
    // Modern OpenGL state
    glm::mat4 m_projectionMatrix;
    glm::mat4 m_viewMatrix;
    glm::mat4 m_modelMatrix;

    // Helper methods
    void setupVertexAttributes();
    void setupVAO(VoxelChunk* chunk);
};

// Global VBO renderer instance
extern VBORenderer* g_vboRenderer;
