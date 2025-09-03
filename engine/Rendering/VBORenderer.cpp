// VBORenderer.cpp - Modern VBO implementation with GLAD (shader support disabled temporarily)
#include "VBORenderer.h"
#include <glad/gl.h>
#include "TextureManager.h"
#include "../Core/Profiler.h"
#include "../Physics/FluidSystem.h"
#include <iostream>
#include <filesystem>

// Define missing OpenGL constants that should be in GLAD
#ifndef GL_TEXTURE0
#define GL_TEXTURE0 0x84C0
#endif

// Global instances
VBORenderer* g_vboRenderer = nullptr;

VBORenderer::VBORenderer()
    : m_initialized(false)
    , m_projectionMatrix(1.0f)
    , m_viewMatrix(1.0f)
    , m_modelMatrix(1.0f)
{
    std::cout << "VBORenderer constructor" << std::endl;
}

VBORenderer::~VBORenderer()
{
    shutdown();
}

bool VBORenderer::initialize()
{
    if (m_initialized) {
        return true;
    }

    std::cout << "VBORenderer::initialize - using GLAD loader and simple shader..." << std::endl;
    
    // Initialize simple shader
    if (!m_shader.initialize()) {
        std::cout << "Failed to initialize simple shader" << std::endl;
        return false;
    }
    
    // Initialize UBO for batch rendering
    if (!m_shader.initializeUBO()) {
        std::cout << "Failed to initialize UBO" << std::endl;
        return false;
    }
    
    // Set basic OpenGL state
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    
    // Initialize texture manager if not already done
    if (!g_textureManager) {
        g_textureManager = new TextureManager();
    }
    
    // Locate and load dirt texture from common project-relative paths
    auto findTexturePath = [](const std::string& name) -> std::string {
        const char* candidates[] = {
            "textures/",            // run from project root
            "../textures/",         // run from build/bin
            "../../textures/",      // alternative build layout
            "../../../textures/"     // deeper nesting
        };
        for (const auto& dir : candidates) {
            std::filesystem::path p = std::filesystem::path(dir) / name;
            if (std::filesystem::exists(p)) {
                return p.string();
            }
        }
        // Fallback to original absolute path if present
        std::filesystem::path fallback("C:/Users/steve-17/Desktop/game2/textures/");
        fallback /= name;
        if (std::filesystem::exists(fallback)) {
            return fallback.string();
        }
        return {};
    };

    std::string dirtPath = findTexturePath("dirt.png");
    GLuint grassTexture = 0;
    if (!dirtPath.empty()) {
        grassTexture = g_textureManager->loadTexture(dirtPath, false, true);
    }
    if (grassTexture == 0) {
        std::cout << "ERROR: Could not load dirt.png texture!" << std::endl;
        return false;
    } else {
        std::cout << "Loaded dirt texture (ID: " << grassTexture << ") from '" << dirtPath << "'" << std::endl;
    }

    m_initialized = true;
    std::cout << "VBORenderer initialized successfully with simple shader support" << std::endl;
    return true;
}

void VBORenderer::shutdown()
{
    if (!m_initialized) {
        return;
    }
    
    std::cout << "VBORenderer::shutdown" << std::endl;
    
    // Cleanup shader
    m_shader.cleanup();
    
    m_initialized = false;
}

void VBORenderer::setProjectionMatrix(const glm::mat4& projection)
{
    m_projectionMatrix = projection;
}

void VBORenderer::setViewMatrix(const glm::mat4& view)
{
    m_viewMatrix = view;
}

void VBORenderer::setModelMatrix(const glm::mat4& model)
{
    m_modelMatrix = model;
}

void VBORenderer::setupVAO(VoxelChunk* chunk)
{
    VoxelMesh& mesh = chunk->getMesh();
    
    // Generate VAO if needed
    if (mesh.VAO == 0) {
        glGenVertexArrays(1, &mesh.VAO);
    }
    
    // Bind VAO
    glBindVertexArray(mesh.VAO);
    
    // Bind buffers
    glBindBuffer(GL_ARRAY_BUFFER, mesh.VBO);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh.EBO);
    
    // Setup vertex attributes for modern OpenGL with light mapping
    // Vertex layout: position(3) + normal(3) + texcoord(2) + lightMapUV(2) + ambientOcclusion(1) + faceIndex(1) = 12 floats = 48 bytes
    
    // Position attribute (location 0)
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)0);
    glEnableVertexAttribArray(0);
    
    // Texture coordinate attribute (location 1) - matches shader
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)(6 * sizeof(float)));
    glEnableVertexAttribArray(1);
    
    // Normal attribute (location 2) - matches shader
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(2);
    
    // Light map UV attribute (location 3) - NEW
    glVertexAttribPointer(3, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)(8 * sizeof(float)));
    glEnableVertexAttribArray(3);
    
    // Ambient occlusion attribute (location 4) - NEW
    glVertexAttribPointer(4, 1, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)(10 * sizeof(float)));
    glEnableVertexAttribArray(4);
    
    // Face index attribute (location 5) - NEW for per-face light maps
    glVertexAttribPointer(5, 1, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)(11 * sizeof(float)));
    glEnableVertexAttribArray(5);
    
    // Unbind VAO
    glBindVertexArray(0);
}

void VBORenderer::uploadChunkMesh(VoxelChunk* chunk)
{
    if (!chunk || !m_initialized) {
        return;
    }
    
    PROFILE_SCOPE("VBORenderer::uploadChunkMesh");
    
    VoxelMesh& mesh = chunk->getMesh();
    
    // Skip if no geometry
    if (mesh.vertices.empty() || mesh.indices.empty()) {
        return;
    }
    
    // Skip if mesh doesn't need update
    if (!mesh.needsUpdate) {
        return;
    }
    
    // Generate buffers if needed
    if (mesh.VBO == 0) {
        glGenBuffers(1, &mesh.VBO);
        glGenBuffers(1, &mesh.EBO);
        std::cout << "Generated VBO=" << mesh.VBO << " EBO=" << mesh.EBO << " for chunk with " 
                  << mesh.vertices.size() << " vertices" << std::endl;
    }
    
    // Upload vertex data
    glBindBuffer(GL_ARRAY_BUFFER, mesh.VBO);
    glBufferData(GL_ARRAY_BUFFER, 
                 mesh.vertices.size() * sizeof(Vertex), 
                 mesh.vertices.data(), 
                 GL_STATIC_DRAW);
    
    // Upload index data
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh.EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, 
                 mesh.indices.size() * sizeof(uint32_t), 
                 mesh.indices.data(), 
                 GL_STATIC_DRAW);
    
    // Setup VAO with vertex attributes
    setupVAO(chunk);
    
    // Unbind buffers
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    
    mesh.needsUpdate = false;
}

void VBORenderer::renderChunk(VoxelChunk* chunk, const Vec3& worldOffset)
{
    if (!chunk || !m_initialized || !m_shader.isValid()) {
        return;
    }
    
    PROFILE_SCOPE("VBORenderer::renderChunk");
    
    const VoxelMesh& mesh = chunk->getMesh();
    
    // Skip if no geometry or no VAO
    if (mesh.vertices.empty() || mesh.indices.empty() || mesh.VAO == 0) {
        return;
    }
    
    // Use shader and set uniforms
    m_shader.use();
    
    // Create model matrix with world offset
    glm::mat4 modelMatrix = glm::translate(glm::mat4(1.0f), glm::vec3(worldOffset.x, worldOffset.y, worldOffset.z));
    
    // For single chunk rendering, use chunk index 0 and update UBO
    Vec3 lightColor(1.0f, 0.9f, 0.8f);  // Warm sunlight
    Vec3 ambientColor(0.3f, 0.35f, 0.4f);  // Cool ambient
    m_shader.updateChunkLightingData(0, modelMatrix, lightColor, ambientColor, 0.8f);
    m_shader.setChunkIndex(0);
    
    // Set shader uniforms
    m_shader.setMatrix4("uModel", modelMatrix);
    m_shader.setMatrix4("uView", m_viewMatrix);
    m_shader.setMatrix4("uProjection", m_projectionMatrix);
    
    // Note: Using pure light map approach, no traditional lighting uniforms needed
    
    // Bind albedo texture (texture unit 0)
    GLuint grassTexture = g_textureManager->getTexture("dirt.png");
    if (grassTexture != 0) {
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, grassTexture);
        m_shader.setInt("uTexture", 0);
        // Texture found and bound successfully
    } else {
        std::cout << "WARNING: dirt.png texture not found in TextureManager!" << std::endl;
    }
    
    // Bind light map textures (texture units 1-6) - one for each face
    ChunkLightMaps& lightMaps = chunk->getLightMaps(); // Non-const to allow texture access
    
    // Check if lightmaps are ready - don't create them automatically
    // The GlobalLightingManager should have processed this chunk already
    if (!chunk->hasValidLightMaps()) {
        // Lightmaps not ready - this should not happen if our systems are properly coordinated
        static int skipCount = 0;
        if (skipCount < 5) {
            std::cout << "[VBORenderer] Warning: Lightmaps not ready for chunk - skipping! (count: " << skipCount << ")" << std::endl;
            skipCount++;
        }
        return; // Skip rendering this chunk until lightmaps are ready
    }
    
    // Bind all 6 face light map textures
    bool allTexturesValid = true;
    for (int face = 0; face < 6; ++face) {
        if (lightMaps.faceMaps[face].textureHandle != 0) {
            glActiveTexture(GL_TEXTURE1 + face);
            glBindTexture(GL_TEXTURE_2D, lightMaps.faceMaps[face].textureHandle);
        } else {
            allTexturesValid = false;
            std::cerr << "Warning: Light map texture for face " << face << " is not available" << std::endl;
        }
    }
    
    // Set shader uniforms for light map textures
    m_shader.setInt("uLightMapFace0", 1);  // GL_TEXTURE1
    m_shader.setInt("uLightMapFace1", 2);  // GL_TEXTURE2
    m_shader.setInt("uLightMapFace2", 3);  // GL_TEXTURE3
    m_shader.setInt("uLightMapFace3", 4);  // GL_TEXTURE4
    m_shader.setInt("uLightMapFace4", 5);  // GL_TEXTURE5
    m_shader.setInt("uLightMapFace5", 6);  // GL_TEXTURE6
    
    if (allTexturesValid) {
        // Enable pure light map rendering (1.0 = full light map, 0.0 = traditional lighting only)
        m_shader.setFloat("uLightMapStrength", 1.0f);
    } else {
        // Fallback: Use default bright lighting when no light map available
        m_shader.setFloat("uLightMapStrength", 0.0f);
    }
    
    // Bind VAO and render
    glBindVertexArray(mesh.VAO);
    glDrawElements(GL_TRIANGLES, mesh.indices.size(), GL_UNSIGNED_INT, 0);
    glBindVertexArray(0);
    
    // Clean up texture state - unbind all light map textures
    for (int face = 0; face < 6; ++face) {
        glActiveTexture(GL_TEXTURE1 + face);
        glBindTexture(GL_TEXTURE_2D, 0);
    }
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, 0);
    
    // Update stats
    m_stats.chunksRendered++;
    m_stats.verticesRendered += mesh.vertices.size();
    m_stats.drawCalls++;
}

void VBORenderer::beginBatch()
{
    PROFILE_SCOPE("VBORenderer::beginBatch");
    m_stats.reset();
    
    // Initialize UBO for this batch
    if (m_shader.isValid()) {
        m_shader.use();
        // Clear any previous chunk data
        // The UBO will be populated as chunks are rendered
    }
}

void VBORenderer::renderChunkBatch(const std::vector<VoxelChunk*>& chunks, const std::vector<Vec3>& offsets)
{
    PROFILE_SCOPE("VBORenderer::renderChunkBatch");
    
    if (!m_initialized || !m_shader.isValid() || chunks.size() != offsets.size()) {
        return;
    }
    
    // Use UBO for batch rendering up to 64 chunks
    size_t numChunks = std::min(chunks.size(), size_t(64));
    
    m_shader.use();
    
    // Update UBO with chunk data
    for (size_t i = 0; i < numChunks; ++i) {
        VoxelChunk* chunk = chunks[i];
        const Vec3& offset = offsets[i];
        
        if (!chunk) continue;
        
        // Create transform matrix for this chunk
        glm::mat4 chunkTransform = glm::translate(glm::mat4(1.0f), glm::vec3(offset.x, offset.y, offset.z));
        
        // Set chunk lighting data (for now, use simple default values)
        Vec3 lightColor(1.0f, 0.9f, 0.8f);  // Warm sunlight
        Vec3 ambientColor(0.3f, 0.35f, 0.4f);  // Cool ambient
        
        // Update UBO data for this chunk
        m_shader.updateChunkLightingData(static_cast<int>(i), chunkTransform, lightColor, ambientColor, 0.8f);
    }
    
    // Set common shader uniforms
    m_shader.setMatrix4("uView", m_viewMatrix);
    m_shader.setMatrix4("uProjection", m_projectionMatrix);
    
    // Render each chunk in the batch
    for (size_t i = 0; i < numChunks; ++i) {
        VoxelChunk* chunk = chunks[i];
        if (!chunk) continue;
        
        const VoxelMesh& mesh = chunk->getMesh();
        if (mesh.vertices.empty() || mesh.indices.empty() || mesh.VAO == 0) {
            continue;
        }
        
        // Set chunk index for UBO lookup
        m_shader.setChunkIndex(static_cast<int>(i));
        
        // Bind textures
        GLuint grassTexture = g_textureManager->getTexture("dirt.png");
        if (grassTexture != 0) {
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, grassTexture);
            m_shader.setInt("uTexture", 0);
        }
        
        // Bind light map textures (texture units 1-6) - one for each face
        ChunkLightMaps& lightMaps = chunk->getLightMaps();
        
        // Check if lightmaps are ready - don't create them automatically  
        // The GlobalLightingManager should have processed this chunk already
        if (!chunk->hasValidLightMaps()) {
            // Skip this chunk until lightmaps are ready
            continue; // Skip to next chunk in batch
        }
        
        // Bind all 6 face light map textures
        bool allTexturesValid = true;
        for (int face = 0; face < 6; ++face) {
            if (lightMaps.faceMaps[face].textureHandle != 0) {
                glActiveTexture(GL_TEXTURE1 + face);
                glBindTexture(GL_TEXTURE_2D, lightMaps.faceMaps[face].textureHandle);
            } else {
                allTexturesValid = false;
            }
        }
        
        // Set shader uniforms for light map textures (same for all chunks)
        if (i == 0) { // Only set once per batch
            m_shader.setInt("uLightMapFace0", 1);  // GL_TEXTURE1
            m_shader.setInt("uLightMapFace1", 2);  // GL_TEXTURE2
            m_shader.setInt("uLightMapFace2", 3);  // GL_TEXTURE3
            m_shader.setInt("uLightMapFace3", 4);  // GL_TEXTURE4
            m_shader.setInt("uLightMapFace4", 5);  // GL_TEXTURE5
            m_shader.setInt("uLightMapFace5", 6);  // GL_TEXTURE6
            m_shader.setFloat("uLightMapStrength", allTexturesValid ? 1.0f : 0.0f);
        }
        
        // Render chunk
        glBindVertexArray(mesh.VAO);
        glDrawElements(GL_TRIANGLES, mesh.indices.size(), GL_UNSIGNED_INT, 0);
        glBindVertexArray(0);
        
        // Update stats
        m_stats.chunksRendered++;
        m_stats.verticesRendered += mesh.vertices.size();
        m_stats.drawCalls++;
    }
    
    // Clean up texture state - unbind all light map textures
    for (int face = 0; face < 6; ++face) {
        glActiveTexture(GL_TEXTURE1 + face);
        glBindTexture(GL_TEXTURE_2D, 0);
    }
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, 0);
}

void VBORenderer::endBatch()
{
    PROFILE_SCOPE("VBORenderer::endBatch");
    // Could add batch optimizations here in the future
}

void VBORenderer::deleteChunkVBO(VoxelChunk* chunk)
{
    if (!chunk || !m_initialized) {
        return;
    }
    
    VoxelMesh& mesh = chunk->getMesh();
    
    if (mesh.VAO != 0) {
        glDeleteVertexArrays(1, &mesh.VAO);
        mesh.VAO = 0;
    }
    
    if (mesh.VBO != 0) {
        glDeleteBuffers(1, &mesh.VBO);
        mesh.VBO = 0;
    }
    
    if (mesh.EBO != 0) {
        glDeleteBuffers(1, &mesh.EBO);
        mesh.EBO = 0;
    }
}

void VBORenderer::renderFluidParticles(const std::vector<EntityID>& particles)
{
    PROFILE_SCOPE("VBORenderer::renderFluidParticles");
    
    if (!m_initialized || particles.empty()) {
        return;
    }
    
    // Get component storages
    auto* transformStorage = g_ecs.getStorage<TransformComponent>();
    auto* fluidStorage = g_ecs.getStorage<FluidParticleComponent>();
    auto* renderStorage = g_ecs.getStorage<FluidRenderComponent>();
    
    if (!transformStorage || !fluidStorage || !renderStorage) {
        return;
    }
    
    // Use shader for rendering
    m_shader.use();
    
    // Enable blending for water effect
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    
    // Simple cube vertices for each particle (could be optimized with instancing)
    static const float cubeVertices[] = {
        // Positions (centered around origin)
        -0.5f, -0.5f, -0.5f,
         0.5f, -0.5f, -0.5f,
         0.5f,  0.5f, -0.5f,
        -0.5f,  0.5f, -0.5f,
        -0.5f, -0.5f,  0.5f,
         0.5f, -0.5f,  0.5f,
         0.5f,  0.5f,  0.5f,
        -0.5f,  0.5f,  0.5f
    };
    
    static const unsigned int cubeIndices[] = {
        0, 1, 2, 2, 3, 0,  // Front face
        4, 5, 6, 6, 7, 4,  // Back face
        0, 1, 5, 5, 4, 0,  // Bottom face
        2, 3, 7, 7, 6, 2,  // Top face
        0, 3, 7, 7, 4, 0,  // Left face
        1, 2, 6, 6, 5, 1   // Right face
    };
    
    // Create VAO/VBO/EBO for particle rendering (could be cached)
    static GLuint particleVAO = 0, particleVBO = 0, particleEBO = 0;
    
    if (particleVAO == 0) {
        glGenVertexArrays(1, &particleVAO);
        glGenBuffers(1, &particleVBO);
        glGenBuffers(1, &particleEBO);
        
        glBindVertexArray(particleVAO);
        
        glBindBuffer(GL_ARRAY_BUFFER, particleVBO);
        glBufferData(GL_ARRAY_BUFFER, sizeof(cubeVertices), cubeVertices, GL_STATIC_DRAW);
        
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, particleEBO);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(cubeIndices), cubeIndices, GL_STATIC_DRAW);
        
        // Position attribute
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(0);
        
        glBindVertexArray(0);
    }
    
    glBindVertexArray(particleVAO);
    
    // Render each particle
    for (EntityID particleEntity : particles) {
        auto* transform = transformStorage->getComponent(particleEntity);
        auto* fluid = fluidStorage->getComponent(particleEntity);
        auto* render = renderStorage->getComponent(particleEntity);
        
        if (!transform || !fluid || !render) {
            continue;
        }
        
        // Create model matrix for this particle
        glm::mat4 model = glm::mat4(1.0f);
        model = glm::translate(model, glm::vec3(transform->position.x, transform->position.y, transform->position.z));
        model = glm::scale(model, glm::vec3(fluid->radius, fluid->radius, fluid->radius));
        
        // Set model matrix
        m_shader.setMatrix4("uModel", model);
        
        // Set fluid material properties
        glm::vec4 waterColor(0.2f, 0.6f, 1.0f, 0.7f);  // Blue water with transparency
        m_shader.setMaterialColor(waterColor);
        m_shader.setMaterialType(1);  // Fluid material type
        m_shader.setMaterialRoughness(0.1f);  // Smooth water surface
        m_shader.setMaterialEmissive(glm::vec3(0.0f));  // No emission
        
        // Render the cube
        glDrawElements(GL_TRIANGLES, 36, GL_UNSIGNED_INT, 0);
        
        m_stats.drawCalls++;
        m_stats.verticesRendered += 8;  // 8 vertices per cube
    }
    
    glBindVertexArray(0);
    glDisable(GL_BLEND);
    
    m_stats.chunksRendered += particles.size();  // Count particles as "chunks" for stats
}
