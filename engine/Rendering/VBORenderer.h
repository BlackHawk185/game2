// VBORenderer.h - Modern VBO-based renderer with directional shadows
// Features: VAO/VBO mesh management, shader-based lighting, batch rendering
#pragma once

#include <vector>
#include <unordered_map>

#include "../World/VoxelChunk.h"
#include "../Math/Vec3.h"
#include "../Math/Mat4.h"
#include "LitShader.h"
#include "SSAO.h"

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
    void renderSkyBackground();  // Render fullscreen sky gradient
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
    
    // Lit shader for basic rendering
    LitShader m_shader;
    SSAO m_ssao;
    
    // Modern OpenGL state
    Mat4 m_projectionMatrix;
    Mat4 m_viewMatrix;
    Mat4 m_modelMatrix;
    Vec3 m_cameraPos;

    // Scene framebuffer (color + depth) for post-process like SSAO
    GLuint m_sceneFBO = 0;
    GLuint m_sceneColorTex = 0;
    GLuint m_sceneDepthTex = 0;
    int m_fbWidth = 0;
    int m_fbHeight = 0;

    // Helper methods
    void setupVertexAttributes();
    void setupVAO(VoxelChunk* chunk);
    void ensureSceneFramebuffer();
public:
    void setCameraPosition(const Vec3& camera) { m_cameraPos = camera; }
};

// Global VBO renderer instance
extern VBORenderer* g_vboRenderer;
