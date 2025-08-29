// VBORenderer.h - Simple VBO-based renderer for voxel chunks
// Step 1: Replace immediate mode with VBOs while keeping fixed-function pipeline
#pragma once

#include <GL/gl.h>
#include <GLFW/glfw3.h>
#include <vector>
#include <unordered_map>

#include "../World/VoxelChunk.h"
#include "../Math/Vec3.h"

// OpenGL extension function types
#ifndef GL_ARRAY_BUFFER
#define GL_ARRAY_BUFFER 0x8892
#endif
#ifndef GL_ELEMENT_ARRAY_BUFFER
#define GL_ELEMENT_ARRAY_BUFFER 0x8893
#endif
#ifndef GL_STATIC_DRAW
#define GL_STATIC_DRAW 0x88E4
#endif

// Define missing OpenGL types
#ifndef GLsizeiptr
typedef ptrdiff_t GLsizeiptr;
#endif
#ifndef GLintptr
typedef ptrdiff_t GLintptr;
#endif

typedef void (APIENTRY *PFNGLGENBUFFERSPROC)(GLsizei n, GLuint *buffers);
typedef void (APIENTRY *PFNGLDELETEBUFFERSPROC)(GLsizei n, const GLuint *buffers);
typedef void (APIENTRY *PFNGLBINDBUFFERPROC)(GLenum target, GLuint buffer);
typedef void (APIENTRY *PFNGLBUFFERDATAPROC)(GLenum target, GLsizeiptr size, const void *data, GLenum usage);
typedef void (APIENTRY *PFNGLBUFFERSUBDATAPROC)(GLenum target, GLintptr offset, GLsizeiptr size, const void *data);
typedef void (APIENTRY *PFNGLGENVERTEXARRAYSPROC)(GLsizei n, GLuint *arrays);
typedef void (APIENTRY *PFNGLDELETEVERTEXARRAYSPROC)(GLsizei n, const GLuint *arrays);
typedef void (APIENTRY *PFNGLBINDVERTEXARRAYPROC)(GLuint array);
typedef void (APIENTRY *PFNGLENABLEVERTEXATTRIBARRAYPROC)(GLuint index);
typedef void (APIENTRY *PFNGLVERTEXATTRIBPOINTERPROC)(GLuint index, GLint size, GLenum type, GLboolean normalized, GLsizei stride, const void *pointer);

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
    // OpenGL VBO function pointers (for older OpenGL compatibility)
    PFNGLGENBUFFERSPROC glGenBuffers;
    PFNGLDELETEBUFFERSPROC glDeleteBuffers;
    PFNGLBINDBUFFERPROC glBindBuffer;
    PFNGLBUFFERDATAPROC glBufferData;
    PFNGLBUFFERSUBDATAPROC glBufferSubData;
    
    PFNGLGENVERTEXARRAYSPROC glGenVertexArrays;
    PFNGLDELETEVERTEXARRAYSPROC glDeleteVertexArrays;
    PFNGLBINDVERTEXARRAYPROC glBindVertexArray;
    
    PFNGLENABLEVERTEXATTRIBARRAYPROC glEnableVertexAttribArray;
    PFNGLVERTEXATTRIBPOINTERPROC glVertexAttribPointer;

    bool m_initialized;
    RenderStats m_stats;

    // Helper methods
    bool loadVBOExtensions();
    void setupVertexAttributes();
};

// Global VBO renderer instance
extern VBORenderer* g_vboRenderer;
