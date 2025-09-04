#pragma once

#include "../World/MeshGenerator.h"
#include "../Math/Vec3.h"
#include <vector>
#include <string>
#include <cstdint>

// Forward declare OpenGL types to avoid including OpenGL headers in header
typedef unsigned int GLuint;
typedef int GLint;

namespace Engine {
namespace Rendering {

/**
 * Represents an OpenGL texture
 */
struct Texture {
    GLuint textureId;
    int width;
    int height;
    int channels;
    std::string filePath;
    
    Texture() : textureId(0), width(0), height(0), channels(0) {}
};

/**
 * Represents a rendered chunk with OpenGL VBO/VAO data
 */
struct RenderedChunk {
    GLuint VAO;                // Vertex Array Object
    GLuint VBO_vertices;       // Vertex Buffer Object for positions
    GLuint VBO_normals;        // Vertex Buffer Object for normals
    GLuint VBO_textureUV;      // Vertex Buffer Object for texture coordinates
    // ...existing code...
    GLuint EBO;                // Element Buffer Object for indices
    
    uint32_t indexCount;       // Number of indices to render
    bool isGenerated;          // Whether OpenGL data has been generated
    
    Vec3 chunkPosition;        // World position of this chunk
    
    RenderedChunk() : VAO(0), VBO_vertices(0), VBO_normals(0), VBO_textureUV(0), 
                     /*VBO_lightmapUV(0),*/ EBO(0), indexCount(0), isGenerated(false),
                     chunkPosition(0, 0, 0) {}
};

/**
 * Modern OpenGL voxel renderer with VBO/VAO management
 * Handles texture loading, shader management, and efficient chunk rendering
 */
class VoxelRenderer {
public:
    VoxelRenderer();
    ~VoxelRenderer();
    
    /**
     * Initialize the renderer
     * @return true if initialization succeeded
     */
    bool initialize();
    
    /**
     * Shutdown and cleanup resources
     */
    void shutdown();
    
    /**
     * Load texture from file
     * @param filePath Path to texture file (e.g., "textures/dirt.png")
     * @return Texture object (textureId will be 0 if failed)
     */
    Texture loadTexture(const std::string& filePath);
    
    /**
     * Generate OpenGL rendering data from mesh
     * @param mesh The render mesh data
     * @param renderedChunk Output rendered chunk with VBO/VAO data
     */
    void generateChunkRenderData(const Engine::World::RenderMesh& mesh, RenderedChunk& renderedChunk);
    
    /**
     * Render a chunk
     * @param renderedChunk The chunk to render
     * @param texture The texture to use
     */
    void renderChunk(const RenderedChunk& renderedChunk, const Texture& texture);
    
    /**
     * Render a chunk with world position offset
     * @param renderedChunk The chunk to render
     * @param texture The texture to use
     * @param worldOffset World position offset (for moving islands)
     */
    void renderChunk(const RenderedChunk& renderedChunk, const Texture& texture, const Vec3& worldOffset);
    
    /**
     * Render multiple chunks efficiently
     */
    void renderChunks(const std::vector<RenderedChunk>& chunks, const Texture& texture);
    
    /**
     * Render multiple chunks with world position offset
     */
    void renderChunks(const std::vector<RenderedChunk>& chunks, const Texture& texture, const Vec3& worldOffset);
    
    /**
     * Update rendering for frame
     * Call once per frame before rendering chunks
     */
    void beginFrame();
    
    /**
     * Finish rendering for frame
     * Call once per frame after rendering all chunks
     */
    void endFrame();
    
    /**
     * Set view and projection matrices
     */
    void setViewMatrix(const float* viewMatrix);
    void setProjectionMatrix(const float* projectionMatrix);
    
    /**
     * Get the default dirt texture
     */
    const Texture& getDirtTexture() const { return m_dirtTexture; }
    
private:
    // Shader management
    bool loadShaders();
    GLuint compileShader(const std::string& source, unsigned int type);
    GLuint createShaderProgram(const std::string& vertexSource, const std::string& fragmentSource);
    
    // OpenGL state
    GLuint m_shaderProgram;
    GLint m_uniformView;
    GLint m_uniformProjection;
    GLint m_uniformModel;
    GLint m_uniformTexture;
    // ...existing code...
    
    // Default textures
    Texture m_dirtTexture;
    // ...existing code...
    
    // Statistics
    uint32_t m_chunksRendered;
    uint32_t m_trianglesRendered;
    
public:
    // Performance statistics
    uint32_t getChunksRendered() const { return m_chunksRendered; }
    uint32_t getTrianglesRendered() const { return m_trianglesRendered; }
    void resetStatistics();
    
    // Debug methods
    void printChunkInfo(const RenderedChunk& chunk) const;
    void printRenderStatistics() const;
};

} // namespace Rendering
} // namespace Engine
