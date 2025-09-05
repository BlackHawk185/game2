// VBORenderer.cpp - Modern VBO implementation with GLAD (shader support disabled temporarily)
#include "VBORenderer.h"
#include "ShadowMap.h"
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

    // Initialize shadow map target
    if (!g_shadowMap.initialize(2048, 2048)) {
        std::cout << "Failed to initialize shadow map" << std::endl;
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
    
    // Setup vertex attributes: position(3) + normal(3) + texcoord(2)

    // Position attribute (location 0)
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)0);
    glEnableVertexAttribArray(0);
    
    // Texture coordinate attribute (location 1) - matches shader
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)(6 * sizeof(float)));
    glEnableVertexAttribArray(1);
    
    // Normal attribute (location 2) - matches shader
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(2);
    // Disable unused attributes (3,4,5)
    glDisableVertexAttribArray(3);
    glDisableVertexAttribArray(4);
    glDisableVertexAttribArray(5);

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

void VBORenderer::beginDepthPass(const glm::mat4& lightVP)
{
    setLightVP(lightVP);
    g_shadowMap.setLightVP(lightVP);
    g_shadowMap.beginRender();
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
    g_shadowMap.endRender(screenWidth, screenHeight);
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

    m_shader.use();

    glm::mat4 modelMatrix = glm::translate(glm::mat4(1.0f), glm::vec3(worldOffset.x, worldOffset.y, worldOffset.z));
    m_shader.setMatrix4("uModel", modelMatrix);
    m_shader.setMatrix4("uView", m_viewMatrix);
    m_shader.setMatrix4("uProjection", m_projectionMatrix);
    m_shader.setMatrix4("uLightVP", m_lightVP);
    // Set light direction for slope-bias + lambert
    m_shader.setVector3("uLightDir", Vec3(m_lightDir.x, m_lightDir.y, m_lightDir.z));

    // Shadow map
    glActiveTexture(GL_TEXTURE7);
    glBindTexture(GL_TEXTURE_2D, g_shadowMap.getDepthTexture());
    m_shader.setInt("uShadowMap", 7);

    // Albedo texture
    GLuint tex = g_textureManager->getTexture("dirt.png");
    if (tex != 0) {
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, tex);
        m_shader.setInt("uTexture", 0);
    }

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

