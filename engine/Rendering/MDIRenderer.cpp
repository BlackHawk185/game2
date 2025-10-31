// MDIRenderer.cpp - Multi-Draw Indirect implementation
#include "MDIRenderer.h"
#include "../World/VoxelChunk.h"
#include "SimpleShader.h"
#include "CascadedShadowMap.h"
#include "TextureManager.h"
#include "../Profiling/Profiler.h"
#include <glm/gtc/type_ptr.hpp>
#include <iostream>
#include <chrono>
#include <cstring>

// Global instances
std::unique_ptr<MDIRenderer> g_mdiRenderer = nullptr;
extern ShadowMap g_shadowMap;

MDIRenderer::MDIRenderer()
{
}

MDIRenderer::~MDIRenderer()
{
    shutdown();
}

bool MDIRenderer::initialize(uint32_t maxChunks, uint32_t initialBufferChunks)
{
    if (m_initialized)
        return true;
    
    std::cout << "🚀 Initializing MDI Renderer with dynamic allocation..." << std::endl;
    std::cout << "   Max chunks: " << maxChunks << std::endl;
    std::cout << "   Initial buffer capacity: " << initialBufferChunks << " chunks" << std::endl;
    std::cout << "   Vertices per chunk: " << MAX_VERTICES_PER_CHUNK << std::endl;
    std::cout << "   Indices per chunk: " << MAX_INDICES_PER_CHUNK << std::endl;
    
    m_maxChunks = maxChunks;
    
    // Allocate buffer for initial chunk count (will grow dynamically if needed)
    m_totalVertexCapacity = initialBufferChunks * MAX_VERTICES_PER_CHUNK;
    m_totalIndexCapacity = initialBufferChunks * MAX_INDICES_PER_CHUNK;
    
    size_t vertexBufferMB = (m_totalVertexCapacity * sizeof(Vertex)) / (1024 * 1024);
    size_t indexBufferMB = (m_totalIndexCapacity * sizeof(uint32_t)) / (1024 * 1024);
    std::cout << "   Initial vertex buffer: " << vertexBufferMB << " MB" << std::endl;
    std::cout << "   Initial index buffer: " << indexBufferMB << " MB" << std::endl;
    
    // Allocate tracking arrays
    m_chunkData.resize(maxChunks);
    m_drawCommands.resize(maxChunks);
    m_transforms.resize(maxChunks);
    
    // Create VAO
    glGenVertexArrays(1, &m_vao);
    glBindVertexArray(m_vao);
    
    // Create shared VBO
    glGenBuffers(1, &m_vertexBuffer);
    glBindBuffer(GL_ARRAY_BUFFER, m_vertexBuffer);
    glBufferData(GL_ARRAY_BUFFER, m_totalVertexCapacity * sizeof(Vertex), nullptr, GL_DYNAMIC_DRAW);
    
    // Setup vertex attributes (MUST match Vertex struct in VoxelChunk.h)
    // Vertex struct: x,y,z, nx,ny,nz, u,v, lu,lv, ao, faceIndex, blockType (13 floats total)
    
    // location 0: aPosition (x, y, z)
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)(0 * sizeof(float)));
    
    // location 1: aTexCoord (u, v) - shader expects this at location 1
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)(6 * sizeof(float)));
    
    // location 2: aNormal (nx, ny, nz) - shader expects this at location 2
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)(3 * sizeof(float)));
    
    // location 3: aLightMapCoord (lu, lv)
    glEnableVertexAttribArray(3);
    glVertexAttribPointer(3, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)(8 * sizeof(float)));
    
    // location 4: aAmbientOcclusion (ao)
    glEnableVertexAttribArray(4);
    glVertexAttribPointer(4, 1, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)(10 * sizeof(float)));
    
    // location 5: aFaceIndex
    glEnableVertexAttribArray(5);
    glVertexAttribPointer(5, 1, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)(11 * sizeof(float)));
    
    // location 6: aBlockType
    glEnableVertexAttribArray(6);
    glVertexAttribPointer(6, 1, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)(12 * sizeof(float)));
    
    // Create shared EBO
    glGenBuffers(1, &m_indexBuffer);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_indexBuffer);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, m_totalIndexCapacity * sizeof(uint32_t), nullptr, GL_DYNAMIC_DRAW);
    
    // Create indirect command buffer
    glGenBuffers(1, &m_indirectBuffer);
    glBindBuffer(GL_DRAW_INDIRECT_BUFFER, m_indirectBuffer);
    glBufferData(GL_DRAW_INDIRECT_BUFFER, maxChunks * sizeof(DrawElementsCommand), nullptr, GL_DYNAMIC_DRAW);
    
    // Create transform SSBO (binding 1, since binding 0 is UBO)
    glGenBuffers(1, &m_transformBuffer);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_transformBuffer);
    glBufferData(GL_SHADER_STORAGE_BUFFER, maxChunks * sizeof(glm::mat4), nullptr, GL_DYNAMIC_DRAW);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, m_transformBuffer);
    
    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    glBindBuffer(GL_DRAW_INDIRECT_BUFFER, 0);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
    
    // Check for errors
    GLenum error = glGetError();
    if (error != GL_NO_ERROR)
    {
        std::cout << "❌ OpenGL error during MDI initialization: " << error << std::endl;
        shutdown();
        return false;
    }
    
    // Enable OpenGL state for 3D rendering
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glFrontFace(GL_CCW);  // Counter-clockwise winding is front-facing
    std::cout << "✅ Enabled depth testing and back-face culling" << std::endl;
    
    // Load MDI shader (uses SSBO for transforms instead of uniform)
    m_shader = new SimpleShader();
    if (!m_shader->initialize())
    {
        std::cout << "❌ Failed to initialize MDI shader!" << std::endl;
        shutdown();
        return false;
    }
    
    // Initialize depth-only shader for shadow pass
    if (!initDepthShader())
    {
        std::cout << "❌ Failed to initialize depth shader!" << std::endl;
        shutdown();
        return false;
    }
    
    // Query max texture size and use 50% for shadow map
    GLint maxTexSize = 0;
    glGetIntegerv(GL_MAX_TEXTURE_SIZE, &maxTexSize);
    
    // Hardcode shadow map resolution to 16384 for now
    const int shadowMapSize = 16384;
    
    const int NUM_CASCADES = 2;  // Near cascade + far cascade
    if (!g_shadowMap.initialize(shadowMapSize, NUM_CASCADES))
    {
        std::cout << "❌ Failed to initialize shadow map!" << std::endl;
        shutdown();
        return false;
    }
    
    // Load block textures (shared by all chunks)
    extern TextureManager* g_textureManager;
    if (g_textureManager)
    {
        // Try multiple path candidates (working directory can vary)
        const char* pathCandidates[] = {
            "assets/textures/",
            "../assets/textures/",
            "../../assets/textures/"
        };
        
        auto tryLoadTexture = [&](const char* filename) -> GLuint {
            for (const auto& dir : pathCandidates) {
                std::string path = std::string(dir) + filename;
                GLuint texID = g_textureManager->loadTexture(path.c_str(), false, true);
                if (texID != 0) {
                    return texID;
                }
            }
            std::cerr << "⚠️  Failed to load texture: " << filename << std::endl;
            return 0;
        };
        
        m_dirtTextureID = tryLoadTexture("dirt.png");
        m_stoneTextureID = tryLoadTexture("stone.png");
        m_grassTextureID = tryLoadTexture("grass.png");
        m_sandTextureID = tryLoadTexture("sand.png");
        
        if (m_dirtTextureID && m_stoneTextureID && m_grassTextureID && m_sandTextureID) {
            std::cout << "✅ Block textures loaded successfully" << std::endl;
        }
    }
    
    m_initialized = true;
    std::cout << "✅ MDI Renderer initialized successfully" << std::endl;
    
    return true;
}

void MDIRenderer::shutdown()
{
    if (!m_initialized)
        return;
    
    if (m_shader) {
        delete m_shader;
        m_shader = nullptr;
    }
    
    if (m_depthProgram != 0) {
        glDeleteProgram(m_depthProgram);
        m_depthProgram = 0;
    }
    
    if (m_vao != 0) glDeleteVertexArrays(1, &m_vao);
    if (m_vertexBuffer != 0) glDeleteBuffers(1, &m_vertexBuffer);
    if (m_indexBuffer != 0) glDeleteBuffers(1, &m_indexBuffer);
    if (m_indirectBuffer != 0) glDeleteBuffers(1, &m_indirectBuffer);
    if (m_transformBuffer != 0) glDeleteBuffers(1, &m_transformBuffer);
    
    m_vao = 0;
    m_vertexBuffer = 0;
    m_indexBuffer = 0;
    m_indirectBuffer = 0;
    m_transformBuffer = 0;
    
    m_chunkData.clear();
    m_drawCommands.clear();
    m_transforms.clear();
    m_freeSlots.clear();
    
    m_initialized = false;
}

int MDIRenderer::registerChunk(VoxelChunk* chunk, const glm::mat4& transform)
{
    if (!m_initialized || !chunk)
        return -1;
    
    PROFILE_SCOPE("MDIRenderer::registerChunk");
    
    std::lock_guard<std::mutex> lock(chunk->getMeshMutex());
    const VoxelMesh& mesh = chunk->getMesh();
    
    if (mesh.vertices.empty() || mesh.indices.empty())
        return -1;
    
    // Check if mesh exceeds reasonable limits
    if (mesh.vertices.size() > MAX_VERTICES_PER_CHUNK ||
        mesh.indices.size() > MAX_INDICES_PER_CHUNK)
    {
        std::cerr << "❌ Chunk mesh exceeds limits! vertices=" 
                  << mesh.vertices.size() << "/" << MAX_VERTICES_PER_CHUNK
                  << " indices=" << mesh.indices.size() << "/" << MAX_INDICES_PER_CHUNK << std::endl;
        return -1;
    }
    
    // Check if we have space in buffer (allocate only what we need)
    if (m_currentVertexOffset + mesh.vertices.size() > m_totalVertexCapacity ||
        m_currentIndexOffset + mesh.indices.size() > m_totalIndexCapacity)
    {
        std::cerr << "❌ Buffer capacity exceeded!" << std::endl;
        std::cerr << "   Current: " << m_currentVertexOffset << "/" << m_totalVertexCapacity << " verts" << std::endl;
        std::cerr << "   Current: " << m_currentIndexOffset << "/" << m_totalIndexCapacity << " indices" << std::endl;
        std::cerr << "   Needed: +" << mesh.vertices.size() << " verts, +" << mesh.indices.size() << " indices" << std::endl;
        return -1;
    }
    
    // Get chunk index (reuse free slot or allocate new)
    int chunkIndex;
    if (!m_freeSlots.empty())
    {
        chunkIndex = m_freeSlots.back();
        m_freeSlots.pop_back();
    }
    else
    {
        chunkIndex = static_cast<int>(m_stats.registeredChunks);
        if (chunkIndex >= static_cast<int>(m_maxChunks))
        {
            std::cout << "❌ Max chunks reached!" << std::endl;
            return -1;
        }
    }
    
    // Safety check
    if (chunkIndex < 0 || chunkIndex >= static_cast<int>(m_chunkData.size()))
    {
        std::cerr << "❌ Invalid chunk index " << chunkIndex << std::endl;
        return -1;
    }
    
    // Allocate space at current offset (variable size, grows dynamically)
    uint32_t vertexOffset = m_currentVertexOffset;
    uint32_t indexOffset = m_currentIndexOffset;
    
    // Upload mesh data
    uploadVertices(vertexOffset, mesh.vertices.data(), 
                   static_cast<uint32_t>(mesh.vertices.size() * sizeof(Vertex)));
    
    uploadIndices(indexOffset, mesh.indices.data(),
                  static_cast<uint32_t>(mesh.indices.size() * sizeof(uint32_t)));
    
    // Store chunk data
    ChunkDrawData& data = m_chunkData[chunkIndex];
    data.vertexOffset = vertexOffset;
    data.indexOffset = indexOffset;
    data.vertexCount = static_cast<uint32_t>(mesh.vertices.size());
    data.indexCount = static_cast<uint32_t>(mesh.indices.size());
    data.modelMatrix = transform;
    data.dirty = false;
    
    // Create indirect command
    DrawElementsCommand& cmd = m_drawCommands[chunkIndex];
    cmd.count = data.indexCount;
    cmd.instanceCount = 1;
    cmd.firstIndex = data.indexOffset;
    cmd.baseVertex = data.vertexOffset;
    cmd.baseInstance = chunkIndex;
    
    // Store transform
    m_transforms[chunkIndex] = data.modelMatrix;
    
    // Advance offsets by ACTUAL mesh size (packed allocation)
    m_currentVertexOffset += static_cast<uint32_t>(mesh.vertices.size());
    m_currentIndexOffset += static_cast<uint32_t>(mesh.indices.size());
    
    // Update stats
    m_stats.registeredChunks++;
    m_stats.activeChunks++;
    m_stats.totalVertices += data.vertexCount;
    m_stats.totalIndices += data.indexCount;
    
    return chunkIndex;
}

// ================================
// DEFERRED UPDATE QUEUE (THREAD-SAFE)
// ================================

void MDIRenderer::queueChunkRegistration(VoxelChunk* chunk, const glm::mat4& transform)
{
    std::lock_guard<std::mutex> lock(m_pendingMutex);
    m_pendingRegistrations.push_back({chunk, transform, nullptr});
}

void MDIRenderer::queueChunkMeshUpdate(int chunkIndex, VoxelChunk* chunk)
{
    std::lock_guard<std::mutex> lock(m_pendingMutex);
    m_pendingMeshUpdates.push_back({chunkIndex, chunk});
}

void MDIRenderer::processPendingUpdates()
{
    PROFILE_SCOPE("MDIRenderer::processPendingUpdates");
    
    // Process registrations
    std::vector<PendingRegistration> registrations;
    std::vector<PendingMeshUpdate> meshUpdates;
    
    {
        std::lock_guard<std::mutex> lock(m_pendingMutex);
        registrations.swap(m_pendingRegistrations);
        meshUpdates.swap(m_pendingMeshUpdates);
    }
    
    // Now execute on render thread
    for (const auto& pending : registrations)
    {
        int mdiIndex = registerChunk(pending.chunk, pending.transform);
        if (mdiIndex >= 0 && pending.chunk)
        {
            pending.chunk->setMDIIndex(mdiIndex);
        }
    }
    
    for (const auto& pending : meshUpdates)
    {
        updateChunkMesh(pending.chunkIndex, pending.chunk);
    }
}

void MDIRenderer::updateChunkTransform(int chunkIndex, const glm::mat4& transform)
{
    if (chunkIndex < 0 || chunkIndex >= static_cast<int>(m_chunkData.size()))
        return;
    
    ChunkDrawData& data = m_chunkData[chunkIndex];
    data.modelMatrix = transform;
    m_transforms[chunkIndex] = data.modelMatrix;
    data.dirty = true;
}

void MDIRenderer::updateChunkMesh(int chunkIndex, VoxelChunk* chunk)
{
    if (chunkIndex < 0 || chunkIndex >= static_cast<int>(m_chunkData.size()) || !chunk)
        return;
    
    PROFILE_SCOPE("MDIRenderer::updateChunkMesh");
    
    std::lock_guard<std::mutex> lock(chunk->getMeshMutex());
    const VoxelMesh& mesh = chunk->getMesh();
    
    if (mesh.vertices.empty() || mesh.indices.empty())
    {
        // Empty mesh - mark as inactive
        m_drawCommands[chunkIndex].count = 0;
        return;
    }
    
    // Check limits
    if (mesh.vertices.size() > MAX_VERTICES_PER_CHUNK ||
        mesh.indices.size() > MAX_INDICES_PER_CHUNK)
    {
        std::cerr << "❌ Chunk mesh exceeds limits during update!" << std::endl;
        m_drawCommands[chunkIndex].count = 0;
        return;
    }
    
    ChunkDrawData& data = m_chunkData[chunkIndex];
    
    // If new mesh fits in old allocation, update in-place
    if (mesh.vertices.size() <= data.vertexCount && mesh.indices.size() <= data.indexCount)
    {
        uploadVertices(data.vertexOffset, mesh.vertices.data(), 
                      static_cast<uint32_t>(mesh.vertices.size() * sizeof(Vertex)));
        
        uploadIndices(data.indexOffset, mesh.indices.data(),
                     static_cast<uint32_t>(mesh.indices.size() * sizeof(uint32_t)));
        
        // Update counts
        m_stats.totalVertices = m_stats.totalVertices - data.vertexCount + mesh.vertices.size();
        m_stats.totalIndices = m_stats.totalIndices - data.indexCount + mesh.indices.size();
        
        data.vertexCount = static_cast<uint32_t>(mesh.vertices.size());
        data.indexCount = static_cast<uint32_t>(mesh.indices.size());
        
        m_drawCommands[chunkIndex].count = data.indexCount;
    }
    else
    {
        // Mesh grew - need to allocate new space (old space becomes wasted)
        // This creates fragmentation but avoids expensive defrag
        
        if (m_currentVertexOffset + mesh.vertices.size() > m_totalVertexCapacity ||
            m_currentIndexOffset + mesh.indices.size() > m_totalIndexCapacity)
        {
            std::cerr << "⚠️ Buffer full! Cannot grow chunk mesh." << std::endl;
            m_drawCommands[chunkIndex].count = 0;
            return;
        }
        
        // Allocate new space
        uint32_t newVertexOffset = m_currentVertexOffset;
        uint32_t newIndexOffset = m_currentIndexOffset;
        
        uploadVertices(newVertexOffset, mesh.vertices.data(),
                      static_cast<uint32_t>(mesh.vertices.size() * sizeof(Vertex)));
        
        uploadIndices(newIndexOffset, mesh.indices.data(),
                     static_cast<uint32_t>(mesh.indices.size() * sizeof(uint32_t)));
        
        // Update to new allocation (old space is orphaned)
        data.vertexOffset = newVertexOffset;
        data.indexOffset = newIndexOffset;
        data.vertexCount = static_cast<uint32_t>(mesh.vertices.size());
        data.indexCount = static_cast<uint32_t>(mesh.indices.size());
        
        // Update draw command
        DrawElementsCommand& cmd = m_drawCommands[chunkIndex];
        cmd.count = data.indexCount;
        cmd.firstIndex = data.indexOffset;
        cmd.baseVertex = data.vertexOffset;
        
        // Advance offsets
        m_currentVertexOffset += static_cast<uint32_t>(mesh.vertices.size());
        m_currentIndexOffset += static_cast<uint32_t>(mesh.indices.size());
        
        // Update stats
        m_stats.totalVertices += data.vertexCount;
        m_stats.totalIndices += data.indexCount;
    }
}

void MDIRenderer::unregisterChunk(int chunkIndex)
{
    if (chunkIndex < 0 || chunkIndex >= static_cast<int>(m_chunkData.size()))
        return;
    
    ChunkDrawData& data = m_chunkData[chunkIndex];
    
    // Update stats
    m_stats.activeChunks--;
    m_stats.totalVertices -= data.vertexCount;
    m_stats.totalIndices -= data.indexCount;
    
    // Mark as free
    data.vertexCount = 0;
    data.indexCount = 0;
    m_drawCommands[chunkIndex].count = 0;  // Will be skipped in render
    
    m_freeSlots.push_back(chunkIndex);
}

void MDIRenderer::setLightingData(const glm::mat4& lightVP, const glm::vec3& lightDir)
{
    m_lightVP = lightVP;
    m_lightDir = lightDir;
}

void MDIRenderer::renderAll(const glm::mat4& viewMatrix,
                            const glm::mat4& projectionMatrix)
{
    if (!m_initialized || !m_shader || m_stats.activeChunks == 0)
        return;
    
    PROFILE_SCOPE("MDIRenderer::renderAll");
    
    auto startTime = std::chrono::high_resolution_clock::now();
    
    // Update buffers if needed
    updateIndirectBuffer();
    updateTransformBuffer();
    
    // Bind shader
    m_shader->use();
    m_shader->setMatrix4("uView", viewMatrix);
    m_shader->setMatrix4("uProjection", projectionMatrix);
    m_shader->setInt("uChunkIndex", ShaderMode::USE_MDI_SSBO);  // Use SSBO with gl_BaseInstance
    
    // Set cascade shadow map data
    int numCascades = g_shadowMap.getNumCascades();
    m_shader->setInt("uNumCascades", numCascades);
    
    for (int i = 0; i < numCascades; ++i) {
        const CascadeData& cascade = g_shadowMap.getCascade(i);
        std::string vpName = "uCascadeVP[" + std::to_string(i) + "]";
        std::string splitName = "uCascadeSplits[" + std::to_string(i) + "]";
        m_shader->setMatrix4(vpName.c_str(), cascade.viewProj);
        m_shader->setFloat(splitName.c_str(), cascade.splitDistance);
    }
    
    float texel = 1.0f / float(g_shadowMap.getSize() > 0 ? g_shadowMap.getSize() : 8192);
    m_shader->setFloat("uShadowTexel", texel);
    m_shader->setVector3("uLightDir", Vec3(m_lightDir.x, m_lightDir.y, m_lightDir.z));
    
    // Set material type for voxels
    m_shader->setInt("uMaterialType", 0);  // 0 = voxel
    
    // Bind block textures (dirt, stone, grass, sand)
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, m_dirtTextureID);
    m_shader->setInt("uTexture", 0);
    
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, m_stoneTextureID);
    m_shader->setInt("uStoneTexture", 1);
    
    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, m_grassTextureID);
    m_shader->setInt("uGrassTexture", 2);
    
    glActiveTexture(GL_TEXTURE3);
    glBindTexture(GL_TEXTURE_2D, m_sandTextureID);
    m_shader->setInt("uSandTexture", 3);
    
    // Bind cascaded shadow map array
    glActiveTexture(GL_TEXTURE7);
    glBindTexture(GL_TEXTURE_2D_ARRAY, g_shadowMap.getDepthTexture());
    m_shader->setInt("uShadowMap", 7);
    
    // Bind VAO
    glBindVertexArray(m_vao);
    
    // Bind transform SSBO for shader access (binding 1, binding 0 is UBO)
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, m_transformBuffer);
    
    // Single MDI draw call for ALL active chunks
    glBindBuffer(GL_DRAW_INDIRECT_BUFFER, m_indirectBuffer);
    glMultiDrawElementsIndirect(GL_TRIANGLES, GL_UNSIGNED_INT, nullptr, 
                                static_cast<GLsizei>(m_stats.activeChunks), 0);
    
    glBindVertexArray(0);
    glBindBuffer(GL_DRAW_INDIRECT_BUFFER, 0);
    
    auto endTime = std::chrono::high_resolution_clock::now();
    m_stats.lastFrameTimeMs = std::chrono::duration<float, std::milli>(endTime - startTime).count();
    m_stats.drawCalls = 1;  // Only 1 draw call!
}

void MDIRenderer::renderAllDepth(SimpleShader* depthShader, const glm::mat4& lightVP)
{
    if (!m_initialized || !depthShader || m_stats.activeChunks == 0)
        return;
    
    PROFILE_SCOPE("MDIRenderer::renderAllDepth");
    
    // Bind shader
    depthShader->use();
    depthShader->setMatrix4("uLightVP", lightVP);
    
    // Bind VAO
    glBindVertexArray(m_vao);
    
    // Bind transform SSBO
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, m_transformBuffer);
    
    // Single MDI draw call for shadow pass (only active chunks)
    glBindBuffer(GL_DRAW_INDIRECT_BUFFER, m_indirectBuffer);
    glMultiDrawElementsIndirect(GL_TRIANGLES, GL_UNSIGNED_INT, nullptr,
                                static_cast<GLsizei>(m_stats.activeChunks), 0);
    
    glBindVertexArray(0);
    glBindBuffer(GL_DRAW_INDIRECT_BUFFER, 0);
}

void MDIRenderer::resetStatistics()
{
    m_stats.drawCalls = 0;
    m_stats.lastFrameTimeMs = 0.0f;
}

bool MDIRenderer::uploadVertices(uint32_t offset, const void* data, uint32_t sizeBytes)
{
    // Calculate byte offset (offset is in vertex count)
    size_t byteOffset = static_cast<size_t>(offset) * sizeof(Vertex);
    size_t totalBufferSize = static_cast<size_t>(m_totalVertexCapacity) * sizeof(Vertex);
    
    // Bounds check
    if (byteOffset + sizeBytes > totalBufferSize)
    {
        std::cout << "❌ Vertex upload would overflow buffer!" << std::endl;
        std::cout << "   Offset: " << offset << " verts (" << byteOffset << " bytes)" << std::endl;
        std::cout << "   Size: " << sizeBytes << " bytes" << std::endl;
        std::cout << "   Total: " << (byteOffset + sizeBytes) << " bytes" << std::endl;
        std::cout << "   Buffer capacity: " << totalBufferSize << " bytes" << std::endl;
        return false;
    }
    
    glBindBuffer(GL_ARRAY_BUFFER, m_vertexBuffer);
    
    // Clear any prior errors
    while (glGetError() != GL_NO_ERROR) {}
    
    // Check if buffer is still valid
    GLint bufferSize = 0;
    glGetBufferParameteriv(GL_ARRAY_BUFFER, GL_BUFFER_SIZE, &bufferSize);
    
    if (bufferSize == 0)
    {
        std::cout << "❌ Vertex buffer is invalid or deleted!" << std::endl;
        glBindBuffer(GL_ARRAY_BUFFER, 0);
        return false;
    }
    
    if (byteOffset + sizeBytes > static_cast<size_t>(bufferSize))
    {
        std::cout << "❌ Upload exceeds actual buffer size!" << std::endl;
        std::cout << "   Actual buffer size: " << bufferSize << " bytes" << std::endl;
        std::cout << "   Expected buffer size: " << totalBufferSize << " bytes" << std::endl;
        glBindBuffer(GL_ARRAY_BUFFER, 0);
        return false;
    }
    
    glBufferSubData(GL_ARRAY_BUFFER, static_cast<GLintptr>(byteOffset), sizeBytes, data);
    
    GLenum error = glGetError();
    if (error != GL_NO_ERROR)
    {
        std::cout << "❌ Error uploading vertices: " << error << std::endl;
        std::cout << "   Offset: " << offset << " verts (" << byteOffset << " bytes)" << std::endl;
        std::cout << "   Size: " << sizeBytes << " bytes" << std::endl;
        std::cout << "   Actual buffer size: " << bufferSize << " bytes" << std::endl;
        std::cout << "   Buffer capacity: " << totalBufferSize << " bytes" << std::endl;
        std::cout << "   Buffer ID: " << m_vertexBuffer << std::endl;
        glBindBuffer(GL_ARRAY_BUFFER, 0);
        return false;
    }
    
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    return true;
}

bool MDIRenderer::uploadIndices(uint32_t offset, const void* data, uint32_t sizeBytes)
{
    // Calculate byte offset (offset is in index count)
    size_t byteOffset = static_cast<size_t>(offset) * sizeof(uint32_t);
    size_t totalBufferSize = static_cast<size_t>(m_totalIndexCapacity) * sizeof(uint32_t);
    
    // Bounds check
    if (byteOffset + sizeBytes > totalBufferSize)
    {
        std::cout << "❌ Index upload would overflow buffer!" << std::endl;
        std::cout << "   Offset: " << offset << " indices (" << byteOffset << " bytes)" << std::endl;
        std::cout << "   Size: " << sizeBytes << " bytes" << std::endl;
        std::cout << "   Total: " << (byteOffset + sizeBytes) << " bytes" << std::endl;
        std::cout << "   Buffer capacity: " << totalBufferSize << " bytes" << std::endl;
        return false;
    }
    
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_indexBuffer);
    
    // Check if buffer is still valid
    GLint bufferSize = 0;
    glGetBufferParameteriv(GL_ELEMENT_ARRAY_BUFFER, GL_BUFFER_SIZE, &bufferSize);
    
    if (bufferSize == 0)
    {
        std::cout << "❌ Index buffer is invalid or deleted!" << std::endl;
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
        return false;
    }
    
    if (byteOffset + sizeBytes > static_cast<size_t>(bufferSize))
    {
        std::cout << "❌ Upload exceeds actual buffer size!" << std::endl;
        std::cout << "   Actual buffer size: " << bufferSize << " bytes" << std::endl;
        std::cout << "   Expected buffer size: " << totalBufferSize << " bytes" << std::endl;
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
        return false;
    }
    
    glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, static_cast<GLintptr>(byteOffset), sizeBytes, data);
    
    GLenum error = glGetError();
    if (error != GL_NO_ERROR)
    {
        std::cout << "❌ Error uploading indices: " << error << std::endl;
        std::cout << "   Offset: " << offset << " indices (" << byteOffset << " bytes)" << std::endl;
        std::cout << "   Size: " << sizeBytes << " bytes" << std::endl;
        std::cout << "   Buffer capacity: " << totalBufferSize << " bytes" << std::endl;
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
        return false;
    }
    
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    return true;
}

void MDIRenderer::updateIndirectBuffer()
{
    // Upload all draw commands for active chunks
    glBindBuffer(GL_DRAW_INDIRECT_BUFFER, m_indirectBuffer);
    glBufferSubData(GL_DRAW_INDIRECT_BUFFER, 0, 
                   m_stats.activeChunks * sizeof(DrawElementsCommand),
                   m_drawCommands.data());
    glBindBuffer(GL_DRAW_INDIRECT_BUFFER, 0);
}

void MDIRenderer::updateTransformBuffer()
{
    // Upload all transforms for active chunks
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_transformBuffer);
    glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0,
                   m_stats.activeChunks * sizeof(glm::mat4),
                   m_transforms.data());
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
}

void MDIRenderer::defragment()
{
    // TODO: Implement defragmentation to reclaim unused buffer space
    // This would compact the vertex/index buffers after many chunk updates
}

// Helper to compile shader source
static unsigned int compileShaderSrc(const char* src, unsigned int type)
{
    unsigned int sh = glCreateShader(type);
    glShaderSource(sh, 1, &src, nullptr);
    glCompileShader(sh);
    int ok = 0;
    glGetShaderiv(sh, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[512];
        glGetShaderInfoLog(sh, 512, nullptr, log);
        std::cerr << "Shader compilation failed: " << log << std::endl;
    }
    return sh;
}

bool MDIRenderer::initDepthShader()
{
    // Depth-only vertex shader with SSBO support for MDI
    const char* vs = R"(
        #version 460 core
        layout(location = 0) in vec3 aPosition;
        
        uniform mat4 uLightVP;
        
        // SSBO for chunk transforms (binding 1)
        layout(std430, binding = 1) readonly buffer TransformBuffer {
            mat4 transforms[];
        };
        
        void main() {
            mat4 model = transforms[gl_BaseInstance];
            gl_Position = uLightVP * model * vec4(aPosition, 1.0);
        }
    )";
    
    const char* fs = R"(
        #version 460 core
        void main() {}
    )";
    
    unsigned int v = compileShaderSrc(vs, GL_VERTEX_SHADER);
    unsigned int f = compileShaderSrc(fs, GL_FRAGMENT_SHADER);
    
    m_depthProgram = glCreateProgram();
    glAttachShader(m_depthProgram, v);
    glAttachShader(m_depthProgram, f);
    glLinkProgram(m_depthProgram);
    
    glDeleteShader(v);
    glDeleteShader(f);
    
    int ok = 0;
    glGetProgramiv(m_depthProgram, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[512];
        glGetProgramInfoLog(m_depthProgram, 512, nullptr, log);
        std::cerr << "Depth shader link failed: " << log << std::endl;
        return false;
    }
    
    m_depth_uLightVP = glGetUniformLocation(m_depthProgram, "uLightVP");
    
    return true;
}

void MDIRenderer::beginDepthPass(const glm::mat4& lightVP, int cascadeIndex)
{
    g_shadowMap.begin(cascadeIndex);
    
    glUseProgram(m_depthProgram);
    if (m_depth_uLightVP != -1) {
        glUniformMatrix4fv(m_depth_uLightVP, 1, GL_FALSE, glm::value_ptr(lightVP));
    }
}

void MDIRenderer::renderDepth()
{
    if (!m_initialized || m_stats.activeChunks == 0)
        return;
    
    PROFILE_SCOPE("MDIRenderer::renderDepth");
    
    // Update buffers if needed
    updateIndirectBuffer();
    updateTransformBuffer();
    
    // Bind VAO
    glBindVertexArray(m_vao);
    
    // Bind transform SSBO for shader access (binding 1)
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, m_transformBuffer);
    
    // Single MDI draw call for shadow pass (only active chunks)
    glBindBuffer(GL_DRAW_INDIRECT_BUFFER, m_indirectBuffer);
    glMultiDrawElementsIndirect(GL_TRIANGLES, GL_UNSIGNED_INT, nullptr,
                                static_cast<GLsizei>(m_stats.activeChunks), 0);
    
    glBindVertexArray(0);
    glBindBuffer(GL_DRAW_INDIRECT_BUFFER, 0);
}

void MDIRenderer::endDepthPass(int screenWidth, int screenHeight)
{
    g_shadowMap.end(screenWidth, screenHeight);
}
