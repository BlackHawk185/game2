#include "ModelInstanceRenderer.h"
#include "../Assets/GLBLoader.h"
#include "CascadedShadowMap.h"
#include <glad/gl.h>
#include "TextureManager.h"
#include <glm/gtc/matrix_transform.hpp>
#include <filesystem>
#include <iostream>
#include <cmath>
#include <cstdio>
#include <tiny_gltf.h>

// Global instance
std::unique_ptr<ModelInstanceRenderer> g_modelRenderer = nullptr;

namespace {
    // Wind-animated shader for grass/foliage
    static const char* kVS_Wind = R"GLSL(
#version 460 core
layout (location=0) in vec3 aPos;
layout (location=1) in vec3 aNormal;
layout (location=2) in vec2 aUV;
layout (location=3) in vec4 aInstance; // xyz=position offset (voxel center), w=phase

uniform mat4 uView;
uniform mat4 uProjection;
uniform mat4 uModel;       // chunk/world offset
uniform mat4 uLightVP[4];
uniform int  uCascadeCount;
uniform float uTime;

out vec2 vUV;   
out vec3 vNormalWS;
out vec3 vWorldPos;
out vec4 vLightSpacePos[4];
out float vViewZ;

void main(){
    // Wind sway: affect vertices based on their height within the grass model
    // Higher vertices (larger Y) sway more, creating natural grass movement
    float windStrength = 0.15;
    float heightFactor = max(0.0, aPos.y * 0.8); // Scale with vertex height
    vec3 windOffset = vec3(
        sin(uTime * 1.8 + aInstance.w * 2.0) * windStrength * heightFactor,
        0.0,
        cos(uTime * 1.4 + aInstance.w * 1.7) * windStrength * heightFactor * 0.7
    );
    
    vec4 world = uModel * vec4(aPos + windOffset + aInstance.xyz, 1.0);
    gl_Position = uProjection * uView * world;
    vUV = aUV;
    vNormalWS = mat3(uModel) * aNormal;
    vWorldPos = world.xyz;
    for (int i=0;i<uCascadeCount;i++) {
        vLightSpacePos[i] = uLightVP[i] * world;
    }
    vViewZ = -(uView * world).z;
}
)GLSL";
    
    // Static shader for non-animated models (QFG, rocks, etc.)
    static const char* kVS_Static = R"GLSL(
#version 460 core
layout (location=0) in vec3 aPos;
layout (location=1) in vec3 aNormal;
layout (location=2) in vec2 aUV;
layout (location=3) in vec4 aInstance; // xyz=position offset, w=unused

uniform mat4 uView;
uniform mat4 uProjection;
uniform mat4 uModel;       // chunk/world offset
uniform mat4 uLightVP[4];
uniform int  uCascadeCount;
uniform float uTime;

out vec2 vUV;   
out vec3 vNormalWS;
out vec3 vWorldPos;
out vec4 vLightSpacePos[4];
out float vViewZ;

void main(){
    // No wind animation - static model
    vec4 world = uModel * vec4(aPos + aInstance.xyz, 1.0);
    gl_Position = uProjection * uView * world;
    vUV = aUV;
    vNormalWS = mat3(uModel) * aNormal;
    vWorldPos = world.xyz;
    for (int i=0;i<uCascadeCount;i++) {
        vLightSpacePos[i] = uLightVP[i] * world;
    }
    vViewZ = -(uView * world).z;
}
)GLSL";

    static const char* kVS = kVS_Wind;  // Keep legacy for compatibility

    static const char* kFS = R"GLSL(
#version 460 core
in vec2 vUV;
in vec3 vNormalWS;
in vec3 vWorldPos;
in vec4 vLightSpacePos[4];
in float vViewZ;

uniform sampler2D uShadowMaps[4];
uniform int uCascadeCount;
uniform float uCascadeSplits[4];
uniform float uShadowTexel[4];
uniform vec3 uLightDir;
uniform sampler2D uGrassTexture; // engine grass texture with alpha

out vec4 FragColor;

// Poisson disk (match voxel shader)
const vec2 POISSON[12] = vec2[12](
    vec2(-0.35, -0.35), vec2(-0.35, 0.35), vec2(0.35, -0.35), vec2(0.35, 0.35),
    vec2(-0.25, 0.0), vec2(0.25, 0.0), vec2(0.0, -0.25), vec2(0.0, 0.25),
    vec2(-0.15, -0.15), vec2(-0.15, 0.15), vec2(0.15, -0.15), vec2(0.15, 0.15)
);

float sampleCascadePCF(int idx, float bias)
{
    vec3 proj = vLightSpacePos[idx].xyz / vLightSpacePos[idx].w;
    proj = proj * 0.5 + 0.5;
    if (proj.x < 0.0 || proj.x > 1.0 || proj.y < 0.0 || proj.y > 1.0 || proj.z > 1.0)
        return 1.0;
    float current = proj.z - bias;
    float radius = 0.2;  // 5x5 grid covers a wide area
    
    // Sample center first
    float center = texture(uShadowMaps[idx], proj.xy).r;
    float baseShadow = current <= center ? 1.0 : 0.0;
    
    // Early exit if fully lit - prevents shadow bleeding
    if (baseShadow >= 1.0) {
        return 1.0;
    }
    
    // Sample neighbors in 5x5 grid
    int grid = 5;
    float step = radius / float(grid - 1);
    float sum = 0.0;
    int count = 0;
    for (int x = 0; x < grid; ++x) {
        for (int y = 0; y < grid; ++y) {
            vec2 offset = vec2(float(x) - 2.0, float(y) - 2.0) * step;
            float d = texture(uShadowMaps[idx], proj.xy + offset).r;
            sum += current <= d ? 1.0 : 0.0;
            count++;
        }
    }
    return max(baseShadow, sum / float(count));  // Lighten-only: prevents shadow bleeding
}

void main(){
    // Simple lambert with shadow and a desaturated green tint for grass
    vec3 N = normalize(vNormalWS);
    vec3 L = normalize(-uLightDir);
    float NdotL = max(dot(N, L), 0.0);

    // Select cascade by comparing view-space Z with splits
    int idx = 0;
    for (int i=0;i<uCascadeCount-1;i++) {
        if (vViewZ > uCascadeSplits[i]) idx = i+1;
    }
    float bias = 0.0015;
    float visibility = sampleCascadePCF(idx, bias);

    vec4 albedo = texture(uGrassTexture, vUV);
    // Alpha cutout
    if (albedo.a < 0.3) discard;
    vec3 lit = albedo.rgb * (0.2 + 0.8 * NdotL * visibility);
    FragColor = vec4(lit, 1.0);
}
)GLSL";

}

static GLuint Compile(GLuint type, const char* src) {
    GLuint s = glCreateShader(type);
    glShaderSource(s, 1, &src, nullptr);
    glCompileShader(s);
    GLint ok = 0; glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[1024]; glGetShaderInfoLog(s, 1024, nullptr, log);
        std::cerr << "Shader compile error: " << log << std::endl;
        glDeleteShader(s); return 0;
    }
    return s;
}

static GLuint Link(GLuint vs, GLuint fs = 0) {
    GLuint p = glCreateProgram();
    glAttachShader(p, vs);
    if (fs) glAttachShader(p, fs);
    glLinkProgram(p);
    GLint ok = 0; glGetProgramiv(p, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[1024]; glGetProgramInfoLog(p, 1024, nullptr, log);
        std::cerr << "Program link error: " << log << std::endl;
        glDeleteProgram(p); return 0;
    }
    return p;
}

ModelInstanceRenderer::ModelInstanceRenderer() {}
ModelInstanceRenderer::~ModelInstanceRenderer() { shutdown(); }

bool ModelInstanceRenderer::ensureShaders() {
    if (m_program != 0) return true;
    GLuint vs = Compile(GL_VERTEX_SHADER, kVS);
    GLuint fs = Compile(GL_FRAGMENT_SHADER, kFS);
    if (!vs || !fs) return false;
    m_program = Link(vs, fs);
    glDeleteShader(vs); glDeleteShader(fs);
    if (!m_program) return false;

    // Cache uniforms
    uProj = glGetUniformLocation(m_program, "uProjection");
    uView = glGetUniformLocation(m_program, "uView");
    uModel = glGetUniformLocation(m_program, "uModel");
    uCascadeCount = glGetUniformLocation(m_program, "uCascadeCount");
    uLightVP = glGetUniformLocation(m_program, "uLightVP"); // array base
    uCascadeSplits = glGetUniformLocation(m_program, "uCascadeSplits");
    uShadowTexel = glGetUniformLocation(m_program, "uShadowTexel");
    uLightDir = glGetUniformLocation(m_program, "uLightDir");
    uTime = glGetUniformLocation(m_program, "uTime");
    
    // Cache shadow map sampler locations (these are queried every frame currently)
    uShadowMaps0 = glGetUniformLocation(m_program, "uShadowMaps[0]");
    uShadowMaps1 = glGetUniformLocation(m_program, "uShadowMaps[1]");
    uShadowMaps2 = glGetUniformLocation(m_program, "uShadowMaps[2]");
    uGrassTexture = glGetUniformLocation(m_program, "uGrassTexture");

    return true;
}

// Compile shader for specific block type (NEW)
GLuint ModelInstanceRenderer::compileShaderForBlock(uint8_t blockID) {
    // Determine which vertex shader to use based on block type
    const char* vertexShader = kVS_Static;  // Default to static (no wind)
    
    // Wind animation for grass and foliage
    if (blockID == 13) {  // BlockID::DECOR_GRASS
        vertexShader = kVS_Wind;
    }
    // TODO: Add other wind-animated blocks (leaves, reeds, etc.)
    
    // Compile and link
    GLuint vs = Compile(GL_VERTEX_SHADER, vertexShader);
    GLuint fs = Compile(GL_FRAGMENT_SHADER, kFS);  // Same fragment shader for all
    if (!vs || !fs) return 0;
    
    GLuint program = Link(vs, fs);
    glDeleteShader(vs);
    glDeleteShader(fs);
    
    return program;
}

bool ModelInstanceRenderer::initialize() {
    return ensureShaders();
}

void ModelInstanceRenderer::shutdown() {
    // Clean up all instance buffers
    for (auto& kv : m_chunkInstances) {
        if (kv.second.instanceVBO) glDeleteBuffers(1, &kv.second.instanceVBO);
    }
    m_chunkInstances.clear();
    
    // Clean up all loaded models
    for (auto& modelPair : m_models) {
        for (auto& prim : modelPair.second.primitives) {
            if (prim.vao) glDeleteVertexArrays(1, &prim.vao);
            if (prim.vbo) glDeleteBuffers(1, &prim.vbo);
            if (prim.ebo) glDeleteBuffers(1, &prim.ebo);
        }
    }
    m_models.clear();
    
    // Clean up textures
    for (auto& texPair : m_albedoTextures) {
        if (texPair.second) glDeleteTextures(1, &texPair.second);
    }
    m_albedoTextures.clear();
    if (m_engineGrassTex) { glDeleteTextures(1, &m_engineGrassTex); m_engineGrassTex = 0; }
    
    // Clean up shaders
    if (m_program) { glDeleteProgram(m_program); m_program = 0; }
}

bool ModelInstanceRenderer::loadModel(uint8_t blockID, const std::string& path) {
    // Check if already loaded
    if (m_models.find(blockID) != m_models.end() && m_modelPaths[blockID] == path) {
        return m_models[blockID].valid;
    }
    
    // Load GLB file from disk
    GLBModelCPU cpu;
    std::vector<std::string> candidates{ path,
        std::string("../") + path,
        std::string("../../") + path,
        std::string("../../../") + path,
        std::string("C:/Users/steve-17/Desktop/game2/") + path
    };
    bool ok = false;
    std::string resolvedPath;
    for (auto& p : candidates) {
        if (GLBLoader::loadGLB(p, cpu)) { 
            ok = true; 
            resolvedPath = p;
            break;
        }
    }
    if (!ok) return false;

    // Clear any existing model for this blockID
    auto it = m_models.find(blockID);
    if (it != m_models.end()) {
        for (auto& prim : it->second.primitives) {
            if (prim.vao) glDeleteVertexArrays(1, &prim.vao);
            if (prim.vbo) glDeleteBuffers(1, &prim.vbo);
            if (prim.ebo) glDeleteBuffers(1, &prim.ebo);
        }
    }
    
    // Build GPU model from CPU data
    ModelGPU gpuModel;
    gpuModel.primitives.clear();
    for (auto& cpuPrim : cpu.primitives) {
        ModelPrimitiveGPU gp;
        glGenVertexArrays(1, &gp.vao);
        glBindVertexArray(gp.vao);
        glGenBuffers(1, &gp.vbo);
        glBindBuffer(GL_ARRAY_BUFFER, gp.vbo);
        glBufferData(GL_ARRAY_BUFFER, cpuPrim.interleaved.size() * sizeof(float), cpuPrim.interleaved.data(), GL_STATIC_DRAW);
        if (!cpuPrim.indices.empty()) {
            glGenBuffers(1, &gp.ebo);
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, gp.ebo);
            glBufferData(GL_ELEMENT_ARRAY_BUFFER, cpuPrim.indices.size() * sizeof(unsigned int), cpuPrim.indices.data(), GL_STATIC_DRAW);
        }
        GLsizei stride = sizeof(float) * 8; // pos(3) + normal(3) + uv(2)
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, (void*)0);
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, stride, (void*)(sizeof(float)*3));
        glEnableVertexAttribArray(2);
        glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, stride, (void*)(sizeof(float)*6));
        glBindVertexArray(0);
        gp.indexCount = (int)cpuPrim.indices.size();
        gpuModel.primitives.emplace_back(gp);
    }
    gpuModel.valid = !gpuModel.primitives.empty();
    
    // Store the model
    m_models[blockID] = gpuModel;
    m_modelPaths[blockID] = path;

    // Load base color texture from GLB (first material's baseColorTexture)
    GLuint albedoTex = 0;
    try {
        tinygltf::TinyGLTF gltf;
        tinygltf::Model model;
        std::string err, warn;
        if (gltf.LoadBinaryFromFile(&model, &err, &warn, resolvedPath)) {
            int texIndex = -1;
            if (!model.materials.empty()) {
                const auto& mat = model.materials[0];
                if (mat.pbrMetallicRoughness.baseColorTexture.index >= 0) {
                    texIndex = mat.pbrMetallicRoughness.baseColorTexture.index;
                }
            }
            if (texIndex >= 0 && texIndex < (int)model.textures.size()) {
                int imgIndex = model.textures[texIndex].source;
                if (imgIndex >= 0 && imgIndex < (int)model.images.size()) {
                    const auto& img = model.images[imgIndex];
                    GLenum fmt = (img.component == 4) ? GL_RGBA : (img.component == 3) ? GL_RGB : GL_RED;
                    glGenTextures(1, &albedoTex);
                    glBindTexture(GL_TEXTURE_2D, albedoTex);
                    glTexImage2D(GL_TEXTURE_2D, 0, fmt, img.width, img.height, 0, fmt, GL_UNSIGNED_BYTE, img.image.data());
                    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
                     glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
                    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
                    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
                    glGenerateMipmap(GL_TEXTURE_2D);
                    glBindTexture(GL_TEXTURE_2D, 0);
                }
            }
        }
    } catch (...) {
        // ignore texture loading failures
    }
    m_albedoTextures[blockID] = albedoTex;
    
    // Special case: Load engine grass.png texture for grass block (BlockID::DECOR_GRASS = 13)
    if (blockID == 13) {
        if (!g_textureManager) g_textureManager = new TextureManager();
        m_engineGrassTex = g_textureManager->getTexture("grass.png");
        if (m_engineGrassTex == 0) {
            const char* candidates[] = {
                "assets/textures/",
                "../assets/textures/",
                "../../assets/textures/",
                "../../../assets/textures/"
            };
            for (const auto& dir : candidates) {
                std::filesystem::path p = std::filesystem::path(dir) / "grass.png";
                if (std::filesystem::exists(p)) { 
                    m_engineGrassTex = g_textureManager->loadTexture(p.string(), false, true); 
                    break;
                }
            }
            if (m_engineGrassTex == 0) {
                std::filesystem::path fallback("C:/Users/steve-17\Desktop\game2/assets/textures/grass.png");
                if (std::filesystem::exists(fallback)) {
                    m_engineGrassTex = g_textureManager->loadTexture(fallback.string(), false, true);
                }
            }
        }
    }

    // NEW: Compile shader for this block type if not already compiled
    if (m_shaders.find(blockID) == m_shaders.end()) {
        GLuint shader = compileShaderForBlock(blockID);
        if (shader == 0) {
            std::cerr << "Failed to compile shader for block " << (int)blockID << std::endl;
            return false;
        }
        m_shaders[blockID] = shader;
    }

    return gpuModel.valid;
}

void ModelInstanceRenderer::update(float deltaTime) {
    m_time += deltaTime;
}

void ModelInstanceRenderer::setCascadeCount(int count) { m_cascadeCount = count; }
void ModelInstanceRenderer::setCascadeMatrix(int index, const glm::mat4& lightVP) { if (index>=0 && index<4) m_lightVPs[index] = lightVP; }
void ModelInstanceRenderer::setCascadeSplits(const float* splits, int count) {
    for (int i=0;i<count && i<4;i++) m_cascadeSplits[i] = splits[i];
}
void ModelInstanceRenderer::setLightDir(const glm::vec3& lightDir) { m_lightDir = lightDir; }

bool ModelInstanceRenderer::ensureChunkInstancesUploaded(uint8_t blockID, VoxelChunk* chunk) {
    if (!chunk) return false;
    
    // Check if model is loaded
    auto modelIt = m_models.find(blockID);
    if (modelIt == m_models.end() || !modelIt->second.valid) return false;
    
    // Get instances for this block type
    const auto& instances = chunk->getModelInstances(blockID);
    GLsizei count = static_cast<GLsizei>(instances.size());
    if (count == 0) return false;

    // Find or create instance buffer for (chunk, blockID) pair
    auto key = std::make_pair(chunk, blockID);
    auto it = m_chunkInstances.find(key);
    
    if (it == m_chunkInstances.end()) {
        ChunkInstanceBuffer buf;
        glGenBuffers(1, &buf.instanceVBO);
        m_chunkInstances.emplace(key, buf);
        it = m_chunkInstances.find(key);
    }
    
    ChunkInstanceBuffer& buf = it->second;
    
    // Only upload if data hasn't been uploaded yet or chunk mesh needs update
    if (buf.isUploaded && !chunk->getMesh().needsUpdate && buf.count == count) {
        return true; // Already up to date
    }
    
    // Build per-instance data vec4(x,y,z,phase). Phase derived from position for determinism
    std::vector<float> data;
    data.resize(count * 4);
    for (size_t i=0;i<instances.size();i++) {
        data[i*4+0] = instances[i].x;
        data[i*4+1] = instances[i].y;
        data[i*4+2] = instances[i].z;
        // Simple hash-based phase
        float phase = fmodf((instances[i].x*12.9898f + instances[i].z*78.233f) * 43758.5453f, 6.28318f);
        data[i*4+3] = phase;
    }
    glBindBuffer(GL_ARRAY_BUFFER, buf.instanceVBO);
    glBufferData(GL_ARRAY_BUFFER, data.size()*sizeof(float), data.data(), GL_STATIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    buf.count = count;
    buf.isUploaded = true;

    // Hook instance attrib to all model VAOs for this blockID
    for (auto& prim : modelIt->second.primitives) {
        glBindVertexArray(prim.vao);
        glBindBuffer(GL_ARRAY_BUFFER, buf.instanceVBO);
        glEnableVertexAttribArray(3);
        glVertexAttribPointer(3, 4, GL_FLOAT, GL_FALSE, sizeof(float)*4, (void*)0);
        glVertexAttribDivisor(3, 1);
        glBindVertexArray(0);
    }
    return true;
}

void ModelInstanceRenderer::renderModelChunk(uint8_t blockID, VoxelChunk* chunk, const Vec3& chunkLocalPos, const glm::mat4& islandTransform, const glm::mat4& view, const glm::mat4& proj) {
    // Check if model is loaded
    auto modelIt = m_models.find(blockID);
    if (modelIt == m_models.end() || !modelIt->second.valid || !ensureShaders()) return;
    if (!ensureChunkInstancesUploaded(blockID, chunk)) return;

    // Get shader for this block type
    auto shaderIt = m_shaders.find(blockID);
    if (shaderIt == m_shaders.end()) return;
    GLuint shader = shaderIt->second;

    // Ensure sane fixed-function state before color draw
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glDrawBuffer(GL_BACK);
    glReadBuffer(GL_BACK);
    glEnable(GL_DEPTH_TEST);
    glDisable(GL_POLYGON_OFFSET_FILL);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);

    glUseProgram(shader);

    // Get uniform locations for this shader
    int loc_View = glGetUniformLocation(shader, "uView");
    int loc_Proj = glGetUniformLocation(shader, "uProjection");
    int loc_Model = glGetUniformLocation(shader, "uModel");
    int loc_Time = glGetUniformLocation(shader, "uTime");
    int loc_CascadeCount = glGetUniformLocation(shader, "uCascadeCount");
    int loc_LightDir = glGetUniformLocation(shader, "uLightDir");
    int loc_CascadeSplits = glGetUniformLocation(shader, "uCascadeSplits");
    int loc_ShadowTexel = glGetUniformLocation(shader, "uShadowTexel");
    int loc_ShadowMap0 = glGetUniformLocation(shader, "uShadowMaps[0]");
    int loc_Texture = glGetUniformLocation(shader, "uGrassTexture");

    // Matrices - apply island transform (rotation + translation), then chunk offset
    glUniformMatrix4fv(loc_View, 1, GL_FALSE, &view[0][0]);
    glUniformMatrix4fv(loc_Proj, 1, GL_FALSE, &proj[0][0]);
    
    // Model matrix: island transform * chunk local offset
    glm::mat4 chunkOffset = glm::translate(glm::mat4(1.0f), glm::vec3(chunkLocalPos.x, chunkLocalPos.y, chunkLocalPos.z));
    glm::mat4 model = islandTransform * chunkOffset;
    glUniformMatrix4fv(loc_Model, 1, GL_FALSE, &model[0][0]);
    glUniform1f(loc_Time, m_time);

    // Single cascade shadow
    glUniform1i(loc_CascadeCount, 1);
    GLint loc_LightVP = glGetUniformLocation(shader, "uLightVP[0]");
    glUniformMatrix4fv(loc_LightVP, 1, GL_FALSE, &m_lightVPs[0][0][0]);
    glUniform3f(loc_LightDir, m_lightDir.x, m_lightDir.y, m_lightDir.z);
    
    // Split distance & texel size
    if (loc_CascadeSplits>=0) glUniform1fv(loc_CascadeSplits, 1, m_cascadeSplits);
    int sz = g_csm.getSize(0);
    float texel = sz > 0 ? (1.0f / float(sz)) : (1.0f / 8192.0f);
    if (loc_ShadowTexel>=0) glUniform1fv(loc_ShadowTexel, 1, &texel);
    
    // Bind shadow map texture
    glActiveTexture(GL_TEXTURE7);
    glBindTexture(GL_TEXTURE_2D, g_csm.getDepthTexture(0));
    if (loc_ShadowMap0>=0) glUniform1i(loc_ShadowMap0, 7);
    
    // Bind texture (engine grass.png for grass, GLB albedo for others)
    if (loc_Texture>=0) {
        glActiveTexture(GL_TEXTURE5);
        GLuint tex = 0;
        if (blockID == 13 && m_engineGrassTex) {  // 13 = BlockID::DECOR_GRASS
            tex = m_engineGrassTex;
        } else {
            auto texIt = m_albedoTextures.find(blockID);
            if (texIt != m_albedoTextures.end()) {
                tex = texIt->second;
            }
        }
        if (tex) {
            glBindTexture(GL_TEXTURE_2D, tex);
            glUniform1i(loc_Texture, 5);
        }
    }

    // Get instance buffer for this (chunk, blockID) pair
    auto key = std::make_pair(chunk, blockID);
    auto bufIt = m_chunkInstances.find(key);
    if (bufIt == m_chunkInstances.end()) return;  // Should never happen after ensureChunkInstancesUploaded
    
    ChunkInstanceBuffer& buf = bufIt->second;
    
    // Render two-sided foliage (disable culling for grass-like models)
    GLboolean wasCull = glIsEnabled(GL_CULL_FACE);
    if (wasCull) glDisable(GL_CULL_FACE);
    
    // Render instanced models for each primitive
    for (auto& prim : modelIt->second.primitives) {
        glBindVertexArray(prim.vao);
        glDrawElementsInstanced(GL_TRIANGLES, prim.indexCount, GL_UNSIGNED_INT, 0, buf.count);
        glBindVertexArray(0);
    }
    if (wasCull) glEnable(GL_CULL_FACE);
}

void ModelInstanceRenderer::beginDepthPassCascade(int cascadeIndex, const glm::mat4& lightVP) {
    // Depth pass disabled - was causing blue speckling artifacts
    (void)cascadeIndex;
    (void)lightVP;
}

void ModelInstanceRenderer::endDepthPassCascade(int screenWidth, int screenHeight) {
    (void)screenWidth; (void)screenHeight;
}
