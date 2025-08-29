// VBORenderer.h - Simple VBO-based renderer for voxel chunks
// Modern OpenGL with GLAD loader
#pragma once

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <vector>
#include <unordered_map>

#include "../World/VoxelChunk.h"
#include "../Math/Vec3.h"

class VBORenderer
{
public:
    VBORenderer();
    ~VBORenderer();

    // Initialize OpenGL VBO extensions
    bool initialize();
    void shutdown();

    // Chunk VBO management
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

    // Helper methods
    void setupVertexAttributes();
};

// Global VBO renderer instance
extern VBORenderer* g_vboRenderer;
