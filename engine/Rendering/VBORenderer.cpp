// VBORenderer.cpp - Modern VBO implementation with GLAD (shader support disabled temporarily)
#include "VBORenderer.h"
#include <glad/glad.h>
#include "TextureManager.h"
#include "../Core/Profiler.h"
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
    , m_projectionMatrix(Mat4::identity())
    , m_viewMatrix(Mat4::identity())
    , m_modelMatrix(Mat4::identity())
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
    
    // Initialize GLAD
    if (!gladLoadGL()) {
        std::cout << "Failed to initialize GLAD" << std::endl;
        return false;
    }
    
    // Initialize simple shader
    if (!m_shader.initialize()) {
        std::cout << "Failed to initialize simple shader" << std::endl;
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

void VBORenderer::setProjectionMatrix(const Mat4& projection)
{
    m_projectionMatrix = projection;
}

void VBORenderer::setViewMatrix(const Mat4& view)
{
    m_viewMatrix = view;
}

void VBORenderer::setModelMatrix(const Mat4& model)
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
    
    // Setup vertex attributes for modern OpenGL
    // Vertex layout: position(3) + normal(3) + texcoord(2) = 8 floats = 32 bytes
    
    // Position attribute (location 0)
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)0);
    glEnableVertexAttribArray(0);
    
    // Texture coordinate attribute (location 1) - matches shader
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)(6 * sizeof(float)));
    glEnableVertexAttribArray(1);
    
    // Normal attribute (location 2) - matches shader
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(2);
    
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
    Mat4 modelMatrix = Mat4::translate(worldOffset);
    
    // Set shader uniforms
    m_shader.setMatrix4("uModel", modelMatrix);
    m_shader.setMatrix4("uView", m_viewMatrix);
    m_shader.setMatrix4("uProjection", m_projectionMatrix);
    
    // Set fixed directional lighting (Phase 1: Basic shadows working)
    // Sun direction: slightly from above and to the side for good shadow contrast
    m_shader.setVector3("uSunDirection", Vec3(0.5f, -0.8f, 0.3f)); 
    m_shader.setFloat("uTimeOfDay", 0.5f); // Noon lighting
    
    // Bind texture (default texture unit 0 is active by default)
    GLuint grassTexture = g_textureManager->getTexture("dirt.png");
    if (grassTexture != 0) {
        glBindTexture(GL_TEXTURE_2D, grassTexture);
        // Set texture sampler uniform to use texture unit 0
        m_shader.setInt("uTexture", 0);
        // Debug: print texture ID occasionally
        static int debugCounter = 0;
        if (debugCounter++ % 10000 == 0) {
            std::cout << "Using texture ID: " << grassTexture << " for dirt.png" << std::endl;
        }
    } else {
        std::cout << "WARNING: dirt.png texture not found in TextureManager!" << std::endl;
    }
    
    // Bind VAO and render
    glBindVertexArray(mesh.VAO);
    glDrawElements(GL_TRIANGLES, mesh.indices.size(), GL_UNSIGNED_INT, 0);
    glBindVertexArray(0);
    
    // Clean up texture state
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
