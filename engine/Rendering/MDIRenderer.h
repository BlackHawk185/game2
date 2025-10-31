// MDIRenderer.h - Multi-Draw Indirect renderer for massive chunk batching
// Renders thousands of chunks with a single draw call
#pragma once

#include <vector>
#include <unordered_map>
#include <memory>
#include <glad/gl.h>
#include <glm/glm.hpp>
#include "../Math/Vec3.h"

// Forward declarations
class VoxelChunk;
class SimpleShader;

/**
 * Multi-Draw Indirect Command Structure
 * Matches OpenGL's DrawElementsIndirectCommand layout
 */
struct DrawElementsCommand
{
    GLuint count;         // Number of indices to draw
    GLuint instanceCount; // Number of instances (usually 1)
    GLuint firstIndex;    // Starting index in EBO
    GLuint baseVertex;    // Starting vertex in VBO
    GLuint baseInstance;  // Base instance for instance ID (chunk index)
};

/**
 * Chunk Draw Data
 * Tracks where each chunk's mesh data lives in the buffers
 */
struct ChunkDrawData
{
    uint32_t vertexOffset;  // Offset in shared VBO (in vertices)
    uint32_t indexOffset;   // Offset in shared EBO (in indices)
    uint32_t vertexCount;   // Number of vertices
    uint32_t indexCount;    // Number of indices
    glm::mat4 modelMatrix;  // Transform for this chunk
    bool dirty;             // Needs re-upload
};

/**
 * MDI Renderer
 * 
 * Batches all chunk meshes into large shared buffers and draws them with
 * a single glMultiDrawElementsIndirect() call. Massive performance improvement
 * over individual draw calls per chunk.
 * 
 * Architecture:
 * - Single large VBO for all chunk vertices
 * - Single large EBO for all chunk indices  
 * - Indirect command buffer with draw params for each chunk
 * - Transform buffer (UBO/SSBO) with model matrices for each chunk
 * - One draw call renders everything using instanced base vertex
 */
class MDIRenderer
{
public:
    MDIRenderer();
    ~MDIRenderer();
    
    // ================================
    // INITIALIZATION
    // ================================
    
    /**
     * Initialize MDI rendering system with dynamic allocation
     * @param maxChunks - Maximum number of chunks to support
     * @param initialBufferChunks - Initial buffer capacity in chunks (grows dynamically)
     */
    bool initialize(uint32_t maxChunks = 32768, uint32_t initialBufferChunks = 4096);
    
    /**
     * Cleanup GPU resources
     */
    void shutdown();
    
    // ================================
    // CHUNK MANAGEMENT
    // ================================
    
    /**
     * Register a chunk for MDI rendering
     * Uploads mesh data to shared buffers
     * @param chunk - Chunk to register
     * @param transform - Full transformation matrix (island transform * chunk local offset)
     * @return Chunk index in MDI system, or -1 on failure
     */
    int registerChunk(VoxelChunk* chunk, const glm::mat4& transform);
    
    /**
     * Update chunk transform (when island moves/rotates)
     */
    void updateChunkTransform(int chunkIndex, const glm::mat4& transform);
    
    /**
     * Update chunk mesh data (when voxels change)
     */
    void updateChunkMesh(int chunkIndex, VoxelChunk* chunk);
    
    /**
     * Queue chunk registration for next render frame (thread-safe)
     * Use this from game logic thread to avoid OpenGL cross-thread violations
     */
    void queueChunkRegistration(VoxelChunk* chunk, const glm::mat4& transform);
    
    /**
     * Queue chunk mesh update for next render frame (thread-safe)
     * Use this from game logic thread to avoid OpenGL cross-thread violations
     */
    void queueChunkMeshUpdate(int chunkIndex, VoxelChunk* chunk);
    
    /**
     * Process all pending updates (must be called from render thread)
     */
    void processPendingUpdates();
    
    /**
     * Remove chunk from MDI rendering
     */
    void unregisterChunk(int chunkIndex);
    
    // ================================
    // RENDERING
    // ================================
    
    /**
     * Render all registered chunks with single MDI call
     * @param viewMatrix - Camera view matrix
     * @param projectionMatrix - Camera projection matrix
     */
    void renderAll(const glm::mat4& viewMatrix,
                   const glm::mat4& projectionMatrix);
    
    /**
     * Render shadow depth pass for all chunks
     */
    void renderAllDepth(SimpleShader* depthShader,
                       const glm::mat4& lightVP);
    
    /**
     * Render cascaded shadow depth pass for all chunks
     */
    void beginDepthPass(const glm::mat4& lightVP, int cascadeIndex = 0);
    void endDepthPass(int screenWidth, int screenHeight);
    void renderDepth();
    
    // Set lighting/shadow parameters (call before renderAll)
    void setLightingData(const glm::mat4& lightVP, const glm::vec3& lightDir);
    
    // ================================
    // STATISTICS
    // ================================
    
    struct Statistics
    {
        uint32_t registeredChunks = 0;
        uint32_t activeChunks = 0;
        uint32_t totalVertices = 0;
        uint32_t totalIndices = 0;
        uint32_t drawCalls = 0;        // Should be 1!
        float lastFrameTimeMs = 0.0f;
    };
    
    const Statistics& getStatistics() const { return m_stats; }
    void resetStatistics();
    
private:
    // ================================
    // GPU RESOURCES
    // ================================
    
    GLuint m_vao = 0;                  // Vertex array object
    GLuint m_vertexBuffer = 0;         // Shared VBO for all chunks
    GLuint m_indexBuffer = 0;          // Shared EBO for all chunks
    GLuint m_indirectBuffer = 0;       // Indirect command buffer
    GLuint m_transformBuffer = 0;      // SSBO for chunk transforms
    
    // ================================
    // CHUNK TRACKING
    // ================================
    
    std::vector<ChunkDrawData> m_chunkData;           // Draw data per chunk
    std::vector<DrawElementsCommand> m_drawCommands;  // Indirect commands
    std::vector<glm::mat4> m_transforms;              // Transform matrices
    std::vector<int> m_freeSlots;                     // Recycled chunk indices
    
    // ================================
    // DEFERRED UPDATE QUEUE (THREAD-SAFE)
    // ================================
    
    struct PendingRegistration {
        VoxelChunk* chunk;
        glm::mat4 transform;
        int* outMDIIndex;  // Optional: where to store the result
    };
    
    struct PendingMeshUpdate {
        int chunkIndex;
        VoxelChunk* chunk;
    };
    
    std::mutex m_pendingMutex;
    std::vector<PendingRegistration> m_pendingRegistrations;
    std::vector<PendingMeshUpdate> m_pendingMeshUpdates;
    
    // ================================
    // BUFFER MANAGEMENT
    // ================================
    
    // Per-chunk allocation limits (not preallocated, just max size checks)
    static constexpr uint32_t MAX_VERTICES_PER_CHUNK = 8192;   // Reasonable limit for 16³ chunk
    static constexpr uint32_t MAX_INDICES_PER_CHUNK = 12288;   // 6 indices per quad
    
    uint32_t m_maxChunks = 0;
    uint32_t m_totalVertexCapacity = 0;     // Total vertices allocated so far
    uint32_t m_totalIndexCapacity = 0;      // Total indices allocated so far
    uint32_t m_currentVertexOffset = 0;     // Next free vertex position
    uint32_t m_currentIndexOffset = 0;      // Next free index position
    
    bool m_initialized = false;
    Statistics m_stats;
    
    // Lighting/shadow data
    glm::mat4 m_lightVP;
    glm::vec3 m_lightDir = glm::vec3(-0.3f, -1.0f, -0.2f);
    
    // Block textures (shared across all chunks)
    unsigned int m_dirtTextureID = 0;
    unsigned int m_stoneTextureID = 0;
    unsigned int m_grassTextureID = 0;
    unsigned int m_sandTextureID = 0;
    
    // ================================
    // INTERNAL METHODS
    // ================================
    
    /**
     * Upload vertex data to shared VBO
     */
    bool uploadVertices(uint32_t offset, const void* data, uint32_t sizeBytes);
    
    /**
     * Upload index data to shared EBO
     */
    bool uploadIndices(uint32_t offset, const void* data, uint32_t sizeBytes);
    
    /**
     * Update indirect command buffer
     */
    void updateIndirectBuffer();
    
    /**
     * Update transform buffer
     */
    void updateTransformBuffer();
    
    /**
     * Compact buffers when fragmentation is high
     */
    void defragment();
    
    // MDI shader (uses SSBO for transforms)
    SimpleShader* m_shader = nullptr;
    
    // Depth-only shader for shadow map pass
    GLuint m_depthProgram = 0;
    int m_depth_uLightVP = -1;
    
    // Helper to initialize depth shader
    bool initDepthShader();
};

// Global MDI renderer instance (owned pointer)
extern std::unique_ptr<MDIRenderer> g_mdiRenderer;
