// VBORenderer.cpp - Modern VBO implementation with GLAD (shader support disabled temporarily)
#include "VBORenderer.h"
#include <glad/glad.h>
#include "TextureManager.h"
#include "ShadowPass.h"
#include "SSAO.h"
#include "../Core/Profiler.h"
#include <iostream>
#include <filesystem>

// Define missing OpenGL constants that should be in GLAD
#ifndef GL_TEXTURE0
#define GL_TEXTURE0 0x84C0
#endif

// Additional fallbacks for enums some headers might miss
#ifndef GL_RGBA8
#define GL_RGBA8 0x8058
#endif
#ifndef GL_CLAMP_TO_EDGE
#define GL_CLAMP_TO_EDGE 0x812F
#endif
#ifndef GL_COLOR_ATTACHMENT0
#define GL_COLOR_ATTACHMENT0 0x8CE0
#endif
#ifndef GL_READ_FRAMEBUFFER
#define GL_READ_FRAMEBUFFER 0x8CA8
#endif
#ifndef GL_DRAW_FRAMEBUFFER
#define GL_DRAW_FRAMEBUFFER 0x8CA9
#endif

// Forward declare a few functions if not present in headers (GLAD provides implementation)
#ifndef GL_VERSION_3_0
extern "C" GLAPI void APIENTRY glDrawBuffers(GLsizei n, const GLenum* bufs);
extern "C" GLAPI void APIENTRY glBlitFramebuffer(GLint srcX0, GLint srcY0, GLint srcX1, GLint srcY1,
                                                 GLint dstX0, GLint dstY0, GLint dstX1, GLint dstY1,
                                                 GLbitfield mask, GLenum filter);
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
    
    // Initialize lit shader
    if (!m_shader.initialize()) {
        std::cout << "Failed to initialize lit shader" << std::endl;
        return false;
    }
    // Initialize SSAO post-process
    if (!m_ssao.initialize()) {
        std::cout << "Failed to initialize SSAO" << std::endl;
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
    std::cout << "VBORenderer initialized successfully with lit shader support" << std::endl;
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
    m_ssao.shutdown();

    // Cleanup scene FBO resources
    if (m_sceneFBO) { glDeleteFramebuffers(1, &m_sceneFBO); m_sceneFBO = 0; }
    if (m_sceneColorTex) { glDeleteTextures(1, &m_sceneColorTex); m_sceneColorTex = 0; }
    if (m_sceneDepthTex) { glDeleteTextures(1, &m_sceneDepthTex); m_sceneDepthTex = 0; }
    
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
    
    // Create model matrix with world offset
    Mat4 modelMatrix = Mat4::translate(worldOffset);
    
    // Set shader uniforms
    m_shader.setMatrix4("uModel", modelMatrix);
    
    // Bind texture (default texture unit 0 is active by default)
    GLuint grassTexture = g_textureManager->getTexture("dirt.png");
    if (grassTexture != 0) {
        glBindTexture(GL_TEXTURE_2D, grassTexture);
        // Set texture sampler uniform to use texture unit 0
        m_shader.setInt("uTexture", 0);
        // Debug: print texture ID once to avoid log spam
        static bool s_loggedTextureOnce = false;
        if (!s_loggedTextureOnce) {
            std::cout << "Using texture ID: " << grassTexture << " for dirt.png" << std::endl;
            s_loggedTextureOnce = true;
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

    // Bind shader once and upload frame-constant uniforms
    if (!m_initialized || !m_shader.isValid()) return;

    // Ensure scene FBO matches viewport and bind it for rendering
    ensureSceneFramebuffer();
    glBindFramebuffer(GL_FRAMEBUFFER, m_sceneFBO);
    glViewport(0, 0, m_fbWidth, m_fbHeight);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    m_shader.use();
    m_shader.setMatrix4("uView", m_viewMatrix);
    m_shader.setMatrix4("uProjection", m_projectionMatrix);
    m_shader.setVector3("uSunDirection", Vec3(0.5f, -0.8f, 0.3f));
    m_shader.setVector3("uCameraPos", m_cameraPos);
    m_shader.setVector3("uAlbedoTint", Vec3(1.0f, 1.0f, 1.0f));
    m_shader.setFloat("uAmbient", 0.25f);
    m_shader.setFloat("uSpecularStrength", 0.25f);
    m_shader.setFloat("uShininess", 32.0f);
    m_shader.setFloat("uExposure", 1.5f);  // Exposure control for tone mapping

    // Sky and atmosphere settings
    // Dark blue at top/bottom, light blue at horizon (not white)
    m_shader.setVector3("uSkyColorTop", Vec3(0.06f, 0.16f, 0.42f));       // Deeper sky blue (less black)
    m_shader.setVector3("uSkyColorHorizon", Vec3(0.45f, 0.70f, 0.92f));    // Light sky blue (avoid white)
    m_shader.setVector3("uSunColor", Vec3(1.0f, 0.95f, 0.8f));        // Warm sunlight
    m_shader.setFloat("uFogDensity", 0.008f);                          // Very subtle fog for sky gradient
    m_shader.setFloat("uSkyMode", 0.0f);                               // Normal voxel rendering mode

    // Provide camera basis and projection params for world-space sky gradient
    // View matrix rows are (Right, Up, -Forward) in world space.
    Vec3 camRight(m_viewMatrix.m[0], m_viewMatrix.m[4], m_viewMatrix.m[8]);
    Vec3 camUp(m_viewMatrix.m[1], m_viewMatrix.m[5], m_viewMatrix.m[9]);
    Vec3 camForward(-m_viewMatrix.m[2], -m_viewMatrix.m[6], -m_viewMatrix.m[10]);
    m_shader.setVector3("uCameraRight", camRight);
    m_shader.setVector3("uCameraUp", camUp);
    m_shader.setVector3("uCameraForward", camForward);
    // Extract tan(fov/2) and aspect from projection matrix
    float tanHalfFov = 0.0f;
    float aspect = 1.0f;
    if (m_projectionMatrix.m[5] != 0.0f) {
        tanHalfFov = 1.0f / m_projectionMatrix.m[5];
    }
    if (m_projectionMatrix.m[0] != 0.0f) {
        aspect = m_projectionMatrix.m[5] / m_projectionMatrix.m[0];
    }
    m_shader.setFloat("uTanHalfFov", tanHalfFov);
    m_shader.setFloat("uAspect", aspect);

    if (g_shadowPass) {
        Mat4 lightVP = g_shadowPass->getLightProj() * g_shadowPass->getLightView();
        m_shader.setMatrix4("uLightVP", lightVP);
        if (g_shadowPass->hasFBO()) {
            glActiveTexture(GL_TEXTURE0 + 1);
            glBindTexture(GL_TEXTURE_2D, g_shadowPass->getDepthTexture());
            m_shader.setInt("uShadowMap", 1);
            glActiveTexture(GL_TEXTURE0);
            m_shader.setFloat("uShadowEnabled", 1.0f);
            // PCF texel size and bias
            float texel = 1.0f / static_cast<float>(g_shadowPass->getSize());
            m_shader.setFloat("uShadowTexelSize", texel);
            m_shader.setFloat("uShadowBiasConst", 0.0015f);
            m_shader.setFloat("uShadowBiasSlope", 0.02f);
        } else {
            m_shader.setFloat("uShadowEnabled", 0.0f);
        }
    } else {
        m_shader.setFloat("uShadowEnabled", 0.0f);
    }
}

void VBORenderer::endBatch()
{
    PROFILE_SCOPE("VBORenderer::endBatch");
    // Run SSAO and composite to the default framebuffer
    if (m_sceneDepthTex && m_sceneColorTex) {
        float tanHalfFov = (m_projectionMatrix.m[5] != 0.0f) ? 1.0f / m_projectionMatrix.m[5] : 1.0f;
        float aspect = (m_projectionMatrix.m[0] != 0.0f) ? (m_projectionMatrix.m[5] / m_projectionMatrix.m[0]) : 1.0f;
        float nearPlane = m_projectionMatrix.m[14] / (m_projectionMatrix.m[10] - 1.0f);
        float farPlane  = m_projectionMatrix.m[14] / (m_projectionMatrix.m[10] + 1.0f);
        m_ssao.computeAO(m_sceneDepthTex, m_fbWidth, m_fbHeight, tanHalfFov, aspect, nearPlane, farPlane);
        m_ssao.blurAO(m_fbWidth, m_fbHeight);

        // Unbind scene FBO to render to backbuffer
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        m_ssao.composite(m_sceneColorTex, m_fbWidth, m_fbHeight, 1.0f);
    }
}

void VBORenderer::renderSkyBackground()
{
    if (!m_initialized) return;
    
    // Disable depth test to ensure sky renders behind everything
    glDisable(GL_DEPTH_TEST);
    
    // Create a fullscreen quad that covers the far plane
    // We'll render this at the far clip distance to act as background
    float skyVertices[] = {
        // Positions (NDC coords)    // UV coordinates for sky sampling
        -1.0f, -1.0f, 0.999f,       0.0f, 0.0f,   // Bottom-left
         1.0f, -1.0f, 0.999f,       1.0f, 0.0f,   // Bottom-right  
         1.0f,  1.0f, 0.999f,       1.0f, 1.0f,   // Top-right
        -1.0f,  1.0f, 0.999f,       0.0f, 1.0f    // Top-left
    };
    
    unsigned int skyIndices[] = {
        0, 1, 2,   // First triangle
        2, 3, 0    // Second triangle
    };
    
    // Create temporary VAO/VBO for sky quad
    static unsigned int skyVAO = 0, skyVBO = 0, skyEBO = 0;
    if (skyVAO == 0) {
        glGenVertexArrays(1, &skyVAO);
        glGenBuffers(1, &skyVBO);
        glGenBuffers(1, &skyEBO);
        
        glBindVertexArray(skyVAO);
        
        glBindBuffer(GL_ARRAY_BUFFER, skyVBO);
        glBufferData(GL_ARRAY_BUFFER, sizeof(skyVertices), skyVertices, GL_STATIC_DRAW);
        
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, skyEBO);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(skyIndices), skyIndices, GL_STATIC_DRAW);
        
        // Position attribute
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(0);
        
        // UV attribute (we'll use this to calculate sky gradient)
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));
        glEnableVertexAttribArray(1);
    }
    
    // Use identity matrix to render in NDC space (bypass view/projection)
    Mat4 identity; // Constructor already makes identity matrix
    
    m_shader.setMatrix4("uView", identity);
    m_shader.setMatrix4("uProjection", identity);
    m_shader.setMatrix4("uModel", identity);
    
    // Enable sky rendering mode
    m_shader.setFloat("uSkyMode", 1.0f);
    
    // Bind sky quad and render
    glBindVertexArray(skyVAO);
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
    
    // Restore original matrices and mode
    m_shader.setMatrix4("uView", m_viewMatrix);
    m_shader.setMatrix4("uProjection", m_projectionMatrix);
    m_shader.setFloat("uSkyMode", 0.0f);  // Back to normal voxel mode
    
    // Re-enable depth test for normal rendering
    glEnable(GL_DEPTH_TEST);
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

void VBORenderer::ensureSceneFramebuffer()
{
    // Query current viewport
    GLint vp[4] = {0,0,0,0};
    glGetIntegerv(GL_VIEWPORT, vp);
    int w = vp[2];
    int h = vp[3];
    if (w <= 0 || h <= 0) { w = 1280; h = 720; }
    if (w == m_fbWidth && h == m_fbHeight && m_sceneFBO != 0) return;

    m_fbWidth = w; m_fbHeight = h;

    // Create or resize textures
    if (m_sceneColorTex == 0) glGenTextures(1, &m_sceneColorTex);
    glBindTexture(GL_TEXTURE_2D, m_sceneColorTex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, m_fbWidth, m_fbHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    if (m_sceneDepthTex == 0) glGenTextures(1, &m_sceneDepthTex);
    glBindTexture(GL_TEXTURE_2D, m_sceneDepthTex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, m_fbWidth, m_fbHeight, 0, GL_DEPTH_COMPONENT, GL_UNSIGNED_INT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    if (m_sceneFBO == 0) glGenFramebuffers(1, &m_sceneFBO);
    glBindFramebuffer(GL_FRAMEBUFFER, m_sceneFBO);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_sceneColorTex, 0);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, m_sceneDepthTex, 0);
    // Single color attachment; default draw buffer is GL_COLOR_ATTACHMENT0
    GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (status != GL_FRAMEBUFFER_COMPLETE) {
        std::cout << "Scene FBO incomplete: 0x" << std::hex << status << std::dec << std::endl;
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // Ensure AO textures match
    m_ssao.ensureResources(m_fbWidth, m_fbHeight);
}
