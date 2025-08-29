// VBORenderer.h - Modern VBO-based renderer for voxel chunks with shader support
// Modern OpenGL with GLAD loader and VAO management
#pragma once

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <vector>
#include <unordered_map>

#include "../World/VoxelChunk.h"
#include "../Math/Vec3.h"
#include "../Math/Mat4.h"
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
    void setProjectionMatrix(const Mat4& projection);
    void setViewMatrix(const Mat4& view);
    void setModelMatrix(const Mat4& model);

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
    Mat4 m_projectionMatrix;
    Mat4 m_viewMatrix;
    Mat4 m_modelMatrix;

    // Helper methods
    void setupVertexAttributes();
    void setupVAO(VoxelChunk* chunk);
};

// Global VBO renderer instance
extern VBORenderer* g_vboRenderer;
