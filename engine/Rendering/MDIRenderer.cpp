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
extern CascadedShadowMap g_csm;

MDIRenderer::MDIRenderer()
{
}

MDIRenderer::~MDIRenderer()
{
    shutdown();
}

bool MDIRenderer::initialize(uint32_t maxChunks, uint32_t maxVertices, uint32_t maxIndices)
{
    if (m_initialized)
        return true;
    
    std::cout << "🚀 Initializing MDI Renderer (capacity: " << maxChunks << " chunks)..." << std::endl;
    
    m_maxChunks = maxChunks;
    m_maxVertices = maxVertices;
    m_maxIndices = maxIndices;
    
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
    glBufferData(GL_ARRAY_BUFFER, maxVertices * sizeof(Vertex), nullptr, GL_DYNAMIC_DRAW);
    
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
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, maxIndices * sizeof(uint32_t), nullptr, GL_DYNAMIC_DRAW);
    
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
    
    // Initialize single shadow map (16K for 16 pixels per block at 1024 unit range)
    if (!g_csm.initialize(1, 16384))
    {
        std::cout << "❌ Failed to initialize shadow map!" << std::endl;
        shutdown();
        return false;
    }
    std::cout << "✅ Shadow map initialized (1 cascade: 16384x16384, 1GB)" << std::endl;
    
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
        
        if (m_dirtTextureID && m_stoneTextureID && m_grassTextureID) {
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

int MDIRenderer::registerChunk(VoxelChunk* chunk, const Vec3& worldOffset)
{
    if (!m_initialized || !chunk)
        return -1;
    
    PROFILE_SCOPE("MDIRenderer::registerChunk");
    
    std::lock_guard<std::mutex> lock(chunk->getMeshMutex());
    const VoxelMesh& mesh = chunk->getMesh();
    
    if (mesh.vertices.empty() || mesh.indices.empty())
        return -1;
    
    // Check if we have space
    if (m_currentVertexOffset + mesh.vertices.size() > m_maxVertices ||
        m_currentIndexOffset + mesh.indices.size() > m_maxIndices)
    {
        std::cout << "⚠️  MDI buffers full! Cannot register more chunks." << std::endl;
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
    
    // Safety check: ensure chunkIndex is within vector bounds
    if (chunkIndex < 0 || chunkIndex >= static_cast<int>(m_chunkData.size()))
    {
        std::cerr << "❌ Invalid chunk index " << chunkIndex << " (size: " << m_chunkData.size() << ")" << std::endl;
        return -1;
    }
    
    // Upload vertex data
    uploadVertices(m_currentVertexOffset, mesh.vertices.data(), 
                   static_cast<uint32_t>(mesh.vertices.size() * sizeof(Vertex)));
    
    // Upload index data
    uploadIndices(m_currentIndexOffset, mesh.indices.data(),
                  static_cast<uint32_t>(mesh.indices.size() * sizeof(uint32_t)));
    
    // Store chunk data
    ChunkDrawData& data = m_chunkData[chunkIndex];
    data.vertexOffset = m_currentVertexOffset;
    data.indexOffset = m_currentIndexOffset;
    data.vertexCount = static_cast<uint32_t>(mesh.vertices.size());
    data.indexCount = static_cast<uint32_t>(mesh.indices.size());
    data.modelMatrix = glm::translate(glm::mat4(1.0f), glm::vec3(worldOffset.x, worldOffset.y, worldOffset.z));
    data.dirty = false;
    
    // Create indirect command
    DrawElementsCommand& cmd = m_drawCommands[chunkIndex];
    cmd.count = data.indexCount;
    cmd.instanceCount = 1;
    cmd.firstIndex = data.indexOffset;
    cmd.baseVertex = data.vertexOffset;
    cmd.baseInstance = chunkIndex;  // Used for fetching transform in shader
    
    // Store transform
    m_transforms[chunkIndex] = data.modelMatrix;
    
    // Advance offsets
    m_currentVertexOffset += data.vertexCount;
    m_currentIndexOffset += data.indexCount;
    
    // Update stats
    m_stats.registeredChunks++;
    m_stats.activeChunks++;
    m_stats.totalVertices += data.vertexCount;
    m_stats.totalIndices += data.indexCount;
    
    return chunkIndex;
}

void MDIRenderer::updateChunkTransform(int chunkIndex, const Vec3& worldOffset)
{
    if (chunkIndex < 0 || chunkIndex >= static_cast<int>(m_chunkData.size()))
        return;
    
    ChunkDrawData& data = m_chunkData[chunkIndex];
    data.modelMatrix = glm::translate(glm::mat4(1.0f), glm::vec3(worldOffset.x, worldOffset.y, worldOffset.z));
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
        // Empty mesh - just mark as inactive
        m_drawCommands[chunkIndex].count = 0;
        return;
    }
    
    ChunkDrawData& data = m_chunkData[chunkIndex];
    
    // Check if mesh fits in existing allocation
    if (mesh.vertices.size() == data.vertexCount && mesh.indices.size() == data.indexCount)
    {
        // Perfect match - update in place (no reallocation needed!)
        uploadVertices(data.vertexOffset, mesh.vertices.data(), 
                      static_cast<uint32_t>(mesh.vertices.size() * sizeof(Vertex)));
        
        uploadIndices(data.indexOffset, mesh.indices.data(),
                     static_cast<uint32_t>(mesh.indices.size() * sizeof(uint32_t)));
        
        // Update draw command (counts should be same but refresh anyway)
        m_drawCommands[chunkIndex].count = data.indexCount;
    }
    else if (mesh.vertices.size() <= data.vertexCount && mesh.indices.size() <= data.indexCount)
    {
        // New mesh is smaller or equal - can reuse existing allocation
        uploadVertices(data.vertexOffset, mesh.vertices.data(), 
                      static_cast<uint32_t>(mesh.vertices.size() * sizeof(Vertex)));
        
        uploadIndices(data.indexOffset, mesh.indices.data(),
                     static_cast<uint32_t>(mesh.indices.size() * sizeof(uint32_t)));
        
        // Update stats BEFORE changing counts (need old values)
        m_stats.totalVertices = m_stats.totalVertices - data.vertexCount + mesh.vertices.size();
        m_stats.totalIndices = m_stats.totalIndices - data.indexCount + mesh.indices.size();
        
        // Update counts to reflect new (smaller) mesh
        data.vertexCount = static_cast<uint32_t>(mesh.vertices.size());
        data.indexCount = static_cast<uint32_t>(mesh.indices.size());
        
        // Update draw command with new index count
        DrawElementsCommand& cmd = m_drawCommands[chunkIndex];
        cmd.count = data.indexCount;
    }
    else
    {
        // New mesh is larger - need to allocate new space
        // Check if we have room for the larger mesh
        if (m_currentVertexOffset + mesh.vertices.size() > m_maxVertices ||
            m_currentIndexOffset + mesh.indices.size() > m_maxIndices)
        {
            // Buffer full - mark as inactive
            m_drawCommands[chunkIndex].count = 0;
            std::cerr << "⚠️  MDI buffers full! Cannot update chunk with larger mesh." << std::endl;
            return;
        }
        
        // Allocate new space at the end of the buffer (old space is orphaned)
        uploadVertices(m_currentVertexOffset, mesh.vertices.data(), 
                      static_cast<uint32_t>(mesh.vertices.size() * sizeof(Vertex)));
        
        uploadIndices(m_currentIndexOffset, mesh.indices.data(),
                     static_cast<uint32_t>(mesh.indices.size() * sizeof(uint32_t)));
        
        // Update chunk data with new allocation
        data.vertexOffset = m_currentVertexOffset;
        data.indexOffset = m_currentIndexOffset;
        data.vertexCount = static_cast<uint32_t>(mesh.vertices.size());
        data.indexCount = static_cast<uint32_t>(mesh.indices.size());
        
        // Update draw command
        DrawElementsCommand& cmd = m_drawCommands[chunkIndex];
        cmd.count = data.indexCount;
        cmd.firstIndex = data.indexOffset;
        cmd.baseVertex = data.vertexOffset;
        
        // Advance offsets
        m_currentVertexOffset += data.vertexCount;
        m_currentIndexOffset += data.indexCount;
        
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

void MDIRenderer::setLightingData(int cascadeCount, const glm::mat4* lightVPs,
                                  const float* cascadeSplits, const glm::vec3& lightDir)
{
    m_cascadeCount = cascadeCount;
    for (int i = 0; i < cascadeCount && i < 4; ++i) {
        m_lightVPs[i] = lightVPs[i];
        m_cascadeSplits[i] = cascadeSplits[i];
    }
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
    
    // Set cascaded shadow map lighting data
    m_shader->setInt("uCascadeCount", m_cascadeCount);
    for (int i = 0; i < m_cascadeCount && i < 4; ++i) {
        m_shader->setMatrix4(("uLightVP[" + std::to_string(i) + "]").c_str(), m_lightVPs[i]);
        float texel = 1.0f / float(g_csm.getSize(i) > 0 ? g_csm.getSize(i) : 8192);
        m_shader->setFloat(("uShadowTexel[" + std::to_string(i) + "]").c_str(), texel);
    }
    m_shader->setVector3("uLightDir", Vec3(m_lightDir.x, m_lightDir.y, m_lightDir.z));
    
    // Set material type for voxels
    m_shader->setInt("uMaterialType", 0);  // 0 = voxel
    
    // Bind block textures (dirt, stone, grass)
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, m_dirtTextureID);
    m_shader->setInt("uTexture", 0);
    
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, m_stoneTextureID);
    m_shader->setInt("uStoneTexture", 1);
    
    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, m_grassTextureID);
    m_shader->setInt("uGrassTexture", 2);
    
    // Bind single shadow map
    glActiveTexture(GL_TEXTURE7);
    glBindTexture(GL_TEXTURE_2D, g_csm.getDepthTexture(0));
    m_shader->setInt("uShadowMaps[0]", 7);
    
    // Bind VAO
    glBindVertexArray(m_vao);
    
    // Bind transform SSBO for shader access (binding 1, binding 0 is UBO)
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, m_transformBuffer);
    
    // Single MDI draw call for ALL chunks!
    // Use maxChunks as the draw count - chunks with count=0 will be skipped by GPU
    glBindBuffer(GL_DRAW_INDIRECT_BUFFER, m_indirectBuffer);
    glMultiDrawElementsIndirect(GL_TRIANGLES, GL_UNSIGNED_INT, nullptr, 
                                static_cast<GLsizei>(m_maxChunks), 0);
    
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
    
    // Single MDI draw call for shadow pass
    // Use maxChunks as the draw count - chunks with count=0 will be skipped by GPU
    glBindBuffer(GL_DRAW_INDIRECT_BUFFER, m_indirectBuffer);
    glMultiDrawElementsIndirect(GL_TRIANGLES, GL_UNSIGNED_INT, nullptr,
                                static_cast<GLsizei>(m_maxChunks), 0);
    
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
    glBindBuffer(GL_ARRAY_BUFFER, m_vertexBuffer);
    glBufferSubData(GL_ARRAY_BUFFER, offset * sizeof(Vertex), sizeBytes, data);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    
    GLenum error = glGetError();
    if (error != GL_NO_ERROR)
    {
        std::cout << "❌ Error uploading vertices: " << error << std::endl;
        return false;
    }
    return true;
}

bool MDIRenderer::uploadIndices(uint32_t offset, const void* data, uint32_t sizeBytes)
{
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_indexBuffer);
    glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, offset * sizeof(uint32_t), sizeBytes, data);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    
    GLenum error = glGetError();
    if (error != GL_NO_ERROR)
    {
        std::cout << "❌ Error uploading indices: " << error << std::endl;
        return false;
    }
    return true;
}

void MDIRenderer::updateIndirectBuffer()
{
    // Upload all draw commands
    glBindBuffer(GL_DRAW_INDIRECT_BUFFER, m_indirectBuffer);
    glBufferSubData(GL_DRAW_INDIRECT_BUFFER, 0, 
                   m_stats.registeredChunks * sizeof(DrawElementsCommand),
                   m_drawCommands.data());
    glBindBuffer(GL_DRAW_INDIRECT_BUFFER, 0);
}

void MDIRenderer::updateTransformBuffer()
{
    // Upload all transforms
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_transformBuffer);
    glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0,
                   m_stats.registeredChunks * sizeof(glm::mat4),
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

void MDIRenderer::beginDepthPassCascade(int cascadeIndex, const glm::mat4& lightVP)
{
    m_activeCascade = cascadeIndex;
    g_csm.beginCascade(cascadeIndex);
    
    glUseProgram(m_depthProgram);
    if (m_depth_uLightVP != -1) {
        glUniformMatrix4fv(m_depth_uLightVP, 1, GL_FALSE, glm::value_ptr(lightVP));
    }
}

void MDIRenderer::renderDepthCascade(int cascadeIndex)
{
    if (!m_initialized || m_stats.activeChunks == 0)
        return;
    
    PROFILE_SCOPE("MDIRenderer::renderDepthCascade");
    
    // Update buffers if needed
    updateIndirectBuffer();
    updateTransformBuffer();
    
    // Bind VAO
    glBindVertexArray(m_vao);
    
    // Bind transform SSBO for shader access (binding 1)
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, m_transformBuffer);
    
    // Single MDI draw call for shadow pass
    glBindBuffer(GL_DRAW_INDIRECT_BUFFER, m_indirectBuffer);
    glMultiDrawElementsIndirect(GL_TRIANGLES, GL_UNSIGNED_INT, nullptr,
                                static_cast<GLsizei>(m_stats.registeredChunks), 0);
    
    glBindVertexArray(0);
    glBindBuffer(GL_DRAW_INDIRECT_BUFFER, 0);
}

void MDIRenderer::endDepthPassCascade(int screenWidth, int screenHeight)
{
    g_csm.endCascade(screenWidth, screenHeight);
    m_activeCascade = -1;
}
