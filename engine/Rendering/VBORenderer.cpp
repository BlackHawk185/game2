// VBORenderer.cpp - Modern VBO implementation with GLAD (shader support disabled temporarily)
#include "VBORenderer.h"
#include "ShadowMap.h"
#include "CascadedShadowMap.h"
#include <glad/gl.h>
#include "TextureManager.h"
#include "../Profiling/Profiler.h"
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
    
    // Initialize depth-only shader for shadow pass
    if (!initDepthShader()) {
        std::cout << "Failed to initialize depth shader" << std::endl;
        return false;
    }

    // Initialize cascaded shadow map (3 cascades)
    if (!g_csm.initialize(3, 2048)) {
        std::cout << "Failed to initialize cascaded shadow map" << std::endl;
        return false;
    }
    // Double resolution for near cascade
    g_csm.resizeCascade(0, 8192);

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
            "assets/textures/",            // run from project root
            "../assets/textures/",         // run from build/bin
            "../../assets/textures/",      // alternative build layout
            "../../../assets/textures/"    // deeper nesting
        };
        for (const auto& dir : candidates) {
            std::filesystem::path p = std::filesystem::path(dir) / name;
            if (std::filesystem::exists(p)) {
                return p.string();
            }
        }
        // Fallback to original absolute path if present
        std::filesystem::path fallback("C:/Users/steve-17/Desktop/game2/assets/textures/");
        fallback /= name;
        if (std::filesystem::exists(fallback)) {
            return fallback.string();
        }
        return {};
    };

    std::string dirtPath = findTexturePath("dirt.png");
    m_dirtTextureID = 0;
    if (!dirtPath.empty()) {
        m_dirtTextureID = g_textureManager->loadTexture(dirtPath, false, true);
    }
    if (m_dirtTextureID == 0) {
        std::cout << "ERROR: Could not load dirt.png texture!" << std::endl;
        return false;
    } else {
        std::cout << "Loaded dirt texture (ID: " << m_dirtTextureID << ") from '" << dirtPath << "'" << std::endl;
    }

    std::string stonePath = findTexturePath("stone.png");
    m_stoneTextureID = 0;
    if (!stonePath.empty()) {
        m_stoneTextureID = g_textureManager->loadTexture(stonePath, false, true);
    }
    if (m_stoneTextureID == 0) {
        std::cout << "ERROR: Could not load stone.png texture!" << std::endl;
        return false;
    } else {
        std::cout << "Loaded stone texture (ID: " << m_stoneTextureID << ") from '" << stonePath << "'" << std::endl;
    }

    std::string grassPath = findTexturePath("grass.png");
    m_grassTextureID = 0;
    if (!grassPath.empty()) {
        m_grassTextureID = g_textureManager->loadTexture(grassPath, false, true);
    }
    if (m_grassTextureID == 0) {
        std::cout << "WARNING: Could not load grass.png texture, using dirt as fallback" << std::endl;
        m_grassTextureID = m_dirtTextureID; // Fallback to dirt texture
    } else {
        std::cout << "Loaded grass texture (ID: " << m_grassTextureID << ") from '" << grassPath << "'" << std::endl;
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

void VBORenderer::setLightDir(const glm::vec3& lightDir)
{
    m_lightDir = lightDir;
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
    
    // Setup vertex attributes: position(3) + normal(3) + texcoord(2) + lightmap(2) + ao(1) + faceIndex(1) + blockType(1)

    // Position attribute (location 0)
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)0);
    glEnableVertexAttribArray(0);
    
    // Texture coordinate attribute (location 1) - matches shader
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)(6 * sizeof(float)));
    glEnableVertexAttribArray(1);
    
    // Normal attribute (location 2) - matches shader
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(2);
    
    // Light map coordinates (location 3)
    glVertexAttribPointer(3, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)(8 * sizeof(float)));
    glEnableVertexAttribArray(3);
    
    // Ambient occlusion (location 4)
    glVertexAttribPointer(4, 1, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)(10 * sizeof(float)));
    glEnableVertexAttribArray(4);
    
    // Face index (location 5)
    glVertexAttribPointer(5, 1, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)(11 * sizeof(float)));
    glEnableVertexAttribArray(5);
    
    // Block type (location 6)
    glVertexAttribPointer(6, 1, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)(12 * sizeof(float)));
    glEnableVertexAttribArray(6);

    // Important: unbind VAO so subsequent buffer unbinds don't alter VAO state
    glBindVertexArray(0);
}

// Shadow depth pass initialization and helpers
static unsigned int compileShaderSrc(const char* src, unsigned int type)
{
    unsigned int sh = glCreateShader(type);
    glShaderSource(sh,1,&src,nullptr);
    glCompileShader(sh);
    int ok=0; glGetShaderiv(sh, GL_COMPILE_STATUS, &ok);
    return sh;
}

bool VBORenderer::initDepthShader()
{
    const char* vs = "#version 460 core\nlayout(location=0) in vec3 aPosition;uniform mat4 uModel;uniform mat4 uLightVP;void main(){gl_Position=uLightVP*uModel*vec4(aPosition,1.0);}";
    const char* fs = "#version 460 core\nvoid main(){}";
    unsigned int v = compileShaderSrc(vs, GL_VERTEX_SHADER);
    unsigned int f = compileShaderSrc(fs, GL_FRAGMENT_SHADER);
    m_depthProgram = glCreateProgram();
    glAttachShader(m_depthProgram, v);
    glAttachShader(m_depthProgram, f);
    glLinkProgram(m_depthProgram);
    glDeleteShader(v); glDeleteShader(f);
    int ok=0; glGetProgramiv(m_depthProgram, GL_LINK_STATUS, &ok);
    if(!ok) return false;
    m_depth_uLightVP = glGetUniformLocation(m_depthProgram, "uLightVP");
    m_depth_uModel   = glGetUniformLocation(m_depthProgram, "uModel");
    return true;
}

void VBORenderer::setLightVP(const glm::mat4& lightVP)
{
    m_lightVP = lightVP;
}

void VBORenderer::setCascadeMatrix(int index, const glm::mat4& lightVP)
{
    if (index >= 0 && index < 4) m_lightVPs[index] = lightVP;
}

void VBORenderer::setCascadeCount(int count)
{
    m_cascadeCount = (count < 1) ? 1 : (count > 4 ? 4 : count);
}

void VBORenderer::setCascadeSplits(const float* splits, int count)
{
    int n = (count < 1) ? 1 : (count > 4 ? 4 : count);
    for (int i = 0; i < n; ++i) m_cascadeSplits[i] = splits[i];
}

void VBORenderer::beginDepthPass(const glm::mat4& lightVP)
{
    setLightVP(lightVP);
    // Kept for compatibility if single shadow path is used
    glUseProgram(m_depthProgram);
    if (m_depth_uLightVP != -1) glUniformMatrix4fv(m_depth_uLightVP,1,GL_FALSE,glm::value_ptr(lightVP));
}

void VBORenderer::renderDepthChunk(VoxelChunk* chunk, const Vec3& worldOffset)
{
    if (!chunk) return;
    std::lock_guard<std::mutex> lock(chunk->getMeshMutex());
    const VoxelMesh& mesh = chunk->getMesh();
    if (mesh.vertices.empty() || mesh.indices.empty() || mesh.VAO == 0) return;
    glm::mat4 model = glm::translate(glm::mat4(1.0f), glm::vec3(worldOffset.x, worldOffset.y, worldOffset.z));
    if (m_depth_uModel != -1) glUniformMatrix4fv(m_depth_uModel,1,GL_FALSE,glm::value_ptr(model));
    glBindVertexArray(mesh.VAO);
    glDrawElements(GL_TRIANGLES, (GLsizei)mesh.indices.size(), GL_UNSIGNED_INT, 0);
    glBindVertexArray(0);
}

void VBORenderer::endDepthPass(int screenWidth, int screenHeight)
{
    // No-op in CSM path
}

void VBORenderer::beginDepthPassCascade(int cascadeIndex, const glm::mat4& lightVP)
{
    m_activeCascade = cascadeIndex;
    setLightVP(lightVP);
    g_csm.beginCascade(cascadeIndex);
    glUseProgram(m_depthProgram);
    if (m_depth_uLightVP != -1) glUniformMatrix4fv(m_depth_uLightVP,1,GL_FALSE,glm::value_ptr(lightVP));
}

void VBORenderer::endDepthPassCascade(int screenWidth, int screenHeight)
{
    g_csm.endCascade(screenWidth, screenHeight);
    m_activeCascade = -1;
}

void VBORenderer::uploadChunkMesh(VoxelChunk* chunk)
{
    if (!chunk || !m_initialized) return;
    PROFILE_SCOPE("VBORenderer::uploadChunkMesh");
    std::lock_guard<std::mutex> lock(chunk->getMeshMutex());
    VoxelMesh& mesh = chunk->getMesh();
    if (mesh.vertices.empty() || mesh.indices.empty()) return;
    if (mesh.VBO == 0) { glGenBuffers(1, &mesh.VBO); glGenBuffers(1, &mesh.EBO); }
    glBindBuffer(GL_ARRAY_BUFFER, mesh.VBO);
    glBufferData(GL_ARRAY_BUFFER, mesh.vertices.size()*sizeof(Vertex), mesh.vertices.data(), GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh.EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, mesh.indices.size()*sizeof(uint32_t), mesh.indices.data(), GL_STATIC_DRAW);
    setupVAO(chunk);
    // Safe to unbind ARRAY buffer; leave ELEMENT buffer bound to VAO state
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    mesh.needsUpdate = false;
}

void VBORenderer::beginBatch()
{
    PROFILE_SCOPE("VBORenderer::beginBatch");
    m_stats.reset();
}

void VBORenderer::endBatch()
{
}

void VBORenderer::renderChunk(VoxelChunk* chunk, const Vec3& worldOffset)
{
    if (!chunk || !m_initialized || !m_shader.isValid()) return;
    PROFILE_SCOPE("VBORenderer::renderChunk");
    std::lock_guard<std::mutex> lock(chunk->getMeshMutex());
    const VoxelMesh& mesh = chunk->getMesh();
    if (mesh.vertices.empty() || mesh.indices.empty() || mesh.VAO == 0) return;

    // Ensure sane fixed-function state for color rendering
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glDrawBuffer(GL_BACK);
    glReadBuffer(GL_BACK);
    glEnable(GL_DEPTH_TEST);
    glDisable(GL_POLYGON_OFFSET_FILL);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);

    m_shader.use();
    // Ensure vertex shader uses uModel path (not uninitialized UBO transforms)
    m_shader.setChunkIndex(-1);

    glm::mat4 modelMatrix = glm::translate(glm::mat4(1.0f), glm::vec3(worldOffset.x, worldOffset.y, worldOffset.z));
    m_shader.setMatrix4("uModel", modelMatrix);
    m_shader.setMatrix4("uView", m_viewMatrix);
    m_shader.setMatrix4("uProjection", m_projectionMatrix);
    // Cascades
    m_shader.setInt("uCascadeCount", m_cascadeCount);
    for (int i = 0; i < m_cascadeCount; ++i) {
        char name[32];
        snprintf(name, sizeof(name), "uLightVP[%d]", i);
        m_shader.setMatrix4(name, m_lightVPs[i]);
        snprintf(name, sizeof(name), "uCascadeSplits[%d]", i);
        m_shader.setFloat(name, m_cascadeSplits[i]);
        snprintf(name, sizeof(name), "uShadowTexel[%d]", i);
        float texel = 1.0f / float(g_csm.getSize(i) > 0 ? g_csm.getSize(i) : 2048);
        m_shader.setFloat(name, texel);
    }
    // Set light direction for slope-bias + lambert
    m_shader.setVector3("uLightDir", Vec3(m_lightDir.x, m_lightDir.y, m_lightDir.z));

    // Bind cascaded shadow maps
    if (g_csm.getCascadeCount() >= 3) {
        glActiveTexture(GL_TEXTURE7);
        glBindTexture(GL_TEXTURE_2D, g_csm.getDepthTexture(0));
        m_shader.setInt("uShadowMaps[0]", 7);
        glActiveTexture(GL_TEXTURE8);
        glBindTexture(GL_TEXTURE_2D, g_csm.getDepthTexture(1));
        m_shader.setInt("uShadowMaps[1]", 8);
        glActiveTexture(GL_TEXTURE9);
        glBindTexture(GL_TEXTURE_2D, g_csm.getDepthTexture(2));
        m_shader.setInt("uShadowMaps[2]", 9);
    }

    // Bind all block textures to different texture units
    // Dirt texture (texture unit 0)
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, m_dirtTextureID);
    m_shader.setInt("uTexture", 0);
    
    // Stone texture (texture unit 1)
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, m_stoneTextureID);
    m_shader.setInt("uStoneTexture", 1);
    
    // Grass texture (texture unit 2)
    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, m_grassTextureID);
    m_shader.setInt("uGrassTexture", 2);

    glBindVertexArray(mesh.VAO);
    glDrawElements(GL_TRIANGLES, (GLsizei)mesh.indices.size(), GL_UNSIGNED_INT, 0);
    glBindVertexArray(0);

    m_stats.chunksRendered++;
    m_stats.verticesRendered += (uint32_t)mesh.vertices.size();
    m_stats.drawCalls++;
}

void VBORenderer::deleteChunkVBO(VoxelChunk* chunk)
{
    if (!chunk) return;
    std::lock_guard<std::mutex> lock(chunk->getMeshMutex());
    VoxelMesh& mesh = chunk->getMesh();
    if (mesh.VAO) { glDeleteVertexArrays(1, &mesh.VAO); mesh.VAO = 0; }
    if (mesh.VBO) { glDeleteBuffers(1, &mesh.VBO); mesh.VBO = 0; }
    if (mesh.EBO) { glDeleteBuffers(1, &mesh.EBO); mesh.EBO = 0; }
}

// **FLUID PARTICLE RENDERING IMPLEMENTATION**

void VBORenderer::renderFluidParticles(const std::vector<EntityID>& particles)
{
    if (particles.empty() || !m_initialized) return;

    // Use simple shader for fluid particles
    m_shader.use();

    // Set fluid material properties
    m_shader.setMaterialType(1); // Fluid material type
    m_shader.setMaterialColor(glm::vec4(0.2f, 0.4f, 0.8f, 0.8f)); // Blue semi-transparent

    // Set matrices
    m_shader.setMatrix4("uProjection", m_projectionMatrix);
    m_shader.setMatrix4("uView", m_viewMatrix);

    // Disable depth writing for transparency
    glDepthMask(GL_FALSE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // Render each particle as a sphere
    for (EntityID particleID : particles)
    {
        // Get particle components
        auto* transform = g_ecs.getComponent<TransformComponent>(particleID);
        auto* fluidComp = g_ecs.getComponent<FluidParticleComponent>(particleID);
        auto* renderComp = g_ecs.getComponent<FluidRenderComponent>(particleID);

        if (!transform || !fluidComp || !renderComp) continue;

        // Set model matrix for this particle
        glm::mat4 model = glm::translate(glm::mat4(1.0f),
            glm::vec3(transform->position.x, transform->position.y, transform->position.z));

        // Scale based on particle radius
        float scale = renderComp->renderRadius;
        model = glm::scale(model, glm::vec3(scale, scale, scale));

        m_shader.setMatrix4("uModel", model);

        // Render a simple sphere (using cube approximation for now)
        renderSphere();
    }

    // Restore depth writing and blending
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
    
    // Reset material settings back to voxel defaults
    m_shader.setMaterialType(0); // Voxel material type
    m_shader.setMaterialColor(glm::vec4(1.0f, 1.0f, 1.0f, 1.0f)); // White opaque
}

void VBORenderer::renderSphere()
{
    // Simple sphere approximation using a scaled cube
    // TODO: Replace with proper icosphere geometry for better sphere appearance

    static const float vertices[] = {
        // Front face (scaled to create sphere-like appearance)
        -0.8f, -0.8f,  0.8f,    0.8f, -0.8f,  0.8f,    0.8f,  0.8f,  0.8f,   -0.8f,  0.8f,  0.8f,
        // Back face
        -0.8f, -0.8f, -0.8f,   -0.8f,  0.8f, -0.8f,    0.8f,  0.8f, -0.8f,    0.8f, -0.8f, -0.8f,
        // Left face
        -0.8f, -0.8f, -0.8f,   -0.8f, -0.8f,  0.8f,   -0.8f,  0.8f,  0.8f,   -0.8f,  0.8f, -0.8f,
        // Right face
         0.8f, -0.8f, -0.8f,    0.8f,  0.8f, -0.8f,    0.8f,  0.8f,  0.8f,    0.8f, -0.8f,  0.8f,
        // Top face
        -0.8f,  0.8f, -0.8f,   -0.8f,  0.8f,  0.8f,    0.8f,  0.8f,  0.8f,    0.8f,  0.8f, -0.8f,
        // Bottom face
        -0.8f, -0.8f, -0.8f,    0.8f, -0.8f, -0.8f,    0.8f, -0.8f,  0.8f,   -0.8f, -0.8f,  0.8f
    };

    static const unsigned int indices[] = {
        // Front
        0, 1, 2, 2, 3, 0,
        // Back
        4, 5, 6, 6, 7, 4,
        // Left
        8, 9, 10, 10, 11, 8,
        // Right
        12, 13, 14, 14, 15, 12,
        // Top
        16, 17, 18, 18, 19, 16,
        // Bottom
        20, 21, 22, 22, 23, 20
    };

    // Create temporary VAO/VBO for sphere
    static unsigned int sphereVAO = 0;
    static unsigned int sphereVBO = 0;
    static unsigned int sphereEBO = 0;
    static bool sphereInitialized = false;

    if (!sphereInitialized)
    {
        glGenVertexArrays(1, &sphereVAO);
        glGenBuffers(1, &sphereVBO);
        glGenBuffers(1, &sphereEBO);

        glBindVertexArray(sphereVAO);

        glBindBuffer(GL_ARRAY_BUFFER, sphereVBO);
        glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, sphereEBO);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(0);

        sphereInitialized = true;
    }

    glBindVertexArray(sphereVAO);
    glDrawElements(GL_TRIANGLES, 36, GL_UNSIGNED_INT, 0);
    glBindVertexArray(0);
}

