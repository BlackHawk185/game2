#include "ModelInstanceRenderer.h"
#include "../Assets/GLBLoader.h"
#include "CascadedShadowMap.h"
#include <glad/gl.h>
#include <glm/gtc/matrix_transform.hpp>
#include <iostream>
#include <cmath>
#include <cstdio>

ModelInstanceRenderer* g_modelRenderer = nullptr;

namespace {
    static const char* kVS = R"GLSL(
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
    // Simple wind sway: horizontal offset proportional to vertex local height (y)
    float height = max(aPos.y, 0.0);
    float swayAmp = 0.08 * clamp(height, 0.0, 1.0);
    float phi = uTime * 1.7 + aInstance.w + (aInstance.x * 0.2) + (aInstance.z * 0.2);
    vec3 windOffset = vec3(sin(phi), 0.0, cos(phi)) * swayAmp;

    vec4 world = uModel * vec4(aPos + aInstance.xyz + windOffset, 1.0);
    gl_Position = uProjection * uView * world;
    vUV = aUV;
    vNormalWS = mat3(uModel) * aNormal; // model has only translation; OK
    vWorldPos = world.xyz;
    for (int i=0;i<uCascadeCount;i++) {
        vLightSpacePos[i] = uLightVP[i] * world;
    }
    vViewZ = -(uView * world).z;
}
)GLSL";

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

out vec4 FragColor;

// Poisson disk (match voxel shader)
const vec2 POISSON[12] = vec2[12](
    vec2( -0.613,  0.354 ), vec2( 0.743,  0.106 ), vec2( 0.296, -0.682 ), vec2( -0.269, -0.402 ),
    vec2( -0.154,  0.692 ), vec2( 0.389,  0.463 ), vec2( 0.682, -0.321 ), vec2( -0.682,  0.228 ),
    vec2( -0.053, -0.934 ), vec2( 0.079,  0.934 ), vec2( -0.934, -0.079 ), vec2( 0.934,  0.053 )
);

float sampleCascadePCF(int idx, float bias)
{
    vec3 proj = vLightSpacePos[idx].xyz / vLightSpacePos[idx].w;
    proj = proj * 0.5 + 0.5;
    if (proj.x < 0.0 || proj.x > 1.0 || proj.y < 0.0 || proj.y > 1.0 || proj.z > 1.0)
        return 1.0;
    float current = proj.z - bias;
    float texel = uShadowTexel[idx];
    float radius = 2.5 * texel;
    float sum = 0.0;
    for (int i = 0; i < 12; ++i) {
        vec2 offset = POISSON[i] * radius;
        float d = texture(uShadowMaps[idx], proj.xy + offset).r;
        sum += current <= d ? 1.0 : 0.0;
    }
    return sum / 12.0;
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

    vec3 baseColor = vec3(0.35, 0.55, 0.25);
    vec3 lit = baseColor * (0.2 + 0.8 * NdotL * visibility);
    FragColor = vec4(lit, 1.0);
}
)GLSL";

    static const char* kDepthVS = R"GLSL(
#version 460 core
layout (location=0) in vec3 aPos;
layout (location=1) in vec3 aNormal;
layout (location=2) in vec2 aUV;
layout (location=3) in vec4 aInstance; // xyz=position offset, w=phase

uniform mat4 uModel; // chunk/world offset
uniform mat4 uLightVP;
uniform float uTime;

void main(){
    float height = max(aPos.y, 0.0);
    float swayAmp = 0.08 * clamp(height, 0.0, 1.0);
    float phi = uTime * 1.7 + aInstance.w + (aInstance.x * 0.2) + (aInstance.z * 0.2);
    vec3 windOffset = vec3(sin(phi), 0.0, cos(phi)) * swayAmp;
    vec4 world = uModel * vec4(aPos + aInstance.xyz + windOffset, 1.0);
    gl_Position = uLightVP * world;
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
    if (m_program != 0 && m_depthProgram != 0) return true;
    GLuint vs = Compile(GL_VERTEX_SHADER, kVS);
    GLuint fs = Compile(GL_FRAGMENT_SHADER, kFS);
    if (!vs || !fs) return false;
    m_program = Link(vs, fs);
    glDeleteShader(vs); glDeleteShader(fs);
    if (!m_program) return false;

    GLuint dvs = Compile(GL_VERTEX_SHADER, kDepthVS);
    if (!dvs) return false;
    m_depthProgram = Link(dvs, 0);
    glDeleteShader(dvs);
    if (!m_depthProgram) return false;

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

    d_uModel = glGetUniformLocation(m_depthProgram, "uModel");
    d_uLightVP = glGetUniformLocation(m_depthProgram, "uLightVP");
    d_uTime = glGetUniformLocation(m_depthProgram, "uTime");
    return true;
}

bool ModelInstanceRenderer::initialize() {
    return ensureShaders();
}

void ModelInstanceRenderer::shutdown() {
    for (auto& kv : m_chunkInstances) {
        if (kv.second.instanceVBO) glDeleteBuffers(1, &kv.second.instanceVBO);
    }
    m_chunkInstances.clear();
    for (auto& prim : m_grassModel.primitives) {
        if (prim.vao) glDeleteVertexArrays(1, &prim.vao);
        if (prim.vbo) glDeleteBuffers(1, &prim.vbo);
        if (prim.ebo) glDeleteBuffers(1, &prim.ebo);
    }
    m_grassModel.primitives.clear();
    if (m_program) { glDeleteProgram(m_program); m_program = 0; }
    if (m_depthProgram) { glDeleteProgram(m_depthProgram); m_depthProgram = 0; }
}

bool ModelInstanceRenderer::loadGrassModel(const std::string& path) {
    if (m_grassModel.valid && m_grassPath == path) return true;
    GLBModelCPU cpu;
    std::vector<std::string> candidates{ path,
        std::string("../") + path,
        std::string("../../") + path,
        std::string("../../../") + path,
        std::string("C:/Users/steve-17/Desktop/game2/") + path
    };
    bool ok=false;
    for (auto& p : candidates) {
        if (GLBLoader::loadGLB(p, cpu)) { ok=true; m_grassPath=p; break; }
    }
    if (!ok) return false;

    // Build GPU
    m_grassModel.primitives.clear();
    for (auto& cp : cpu.primitives) {
        ModelPrimitiveGPU gp;
        glGenVertexArrays(1, &gp.vao);
        glBindVertexArray(gp.vao);
        glGenBuffers(1, &gp.vbo);
        glBindBuffer(GL_ARRAY_BUFFER, gp.vbo);
        glBufferData(GL_ARRAY_BUFFER, cp.interleaved.size()*sizeof(float), cp.interleaved.data(), GL_STATIC_DRAW);
        glGenBuffers(1, &gp.ebo);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, gp.ebo);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, cp.indices.size()*sizeof(unsigned int), cp.indices.data(), GL_STATIC_DRAW);
        // Vertex attribs: pos(0), normal(1), uv(2)
        GLsizei stride = sizeof(float)*8;
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, (void*)0);
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, stride, (void*)(sizeof(float)*3));
        glEnableVertexAttribArray(2);
        glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, stride, (void*)(sizeof(float)*6));
        // Instance attrib at location 3 (vec4)
        GLuint dummyVBO = 0; // will be bound per-chunk
        glBindVertexArray(0);
        gp.indexCount = static_cast<GLsizei>(cp.indices.size());
        m_grassModel.primitives.emplace_back(gp);
    }
    m_grassModel.valid = !m_grassModel.primitives.empty();
    return m_grassModel.valid;
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

bool ModelInstanceRenderer::ensureChunkInstancesUploaded(VoxelChunk* chunk) {
    if (!chunk) return false;
    auto it = m_chunkInstances.find(chunk);
    const auto& instances = chunk->getGrassInstancePositions();
    GLsizei count = static_cast<GLsizei>(instances.size());
    if (count == 0) return false;

    if (it == m_chunkInstances.end()) {
        ChunkInstanceBuffer buf;
        glGenBuffers(1, &buf.instanceVBO);
        m_chunkInstances.emplace(chunk, buf);
        it = m_chunkInstances.find(chunk);
    }
    ChunkInstanceBuffer& buf = it->second;
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

    // Hook instance attrib to all model VAOs
    for (auto& prim : m_grassModel.primitives) {
        glBindVertexArray(prim.vao);
        glBindBuffer(GL_ARRAY_BUFFER, buf.instanceVBO);
        glEnableVertexAttribArray(3);
        glVertexAttribPointer(3, 4, GL_FLOAT, GL_FALSE, sizeof(float)*4, (void*)0);
        glVertexAttribDivisor(3, 1);
        glBindVertexArray(0);
    }
    return true;
}

void ModelInstanceRenderer::renderGrassChunk(VoxelChunk* chunk, const Vec3& worldOffset, const glm::mat4& view, const glm::mat4& proj) {
    if (!m_grassModel.valid || !ensureShaders()) return;
    if (!ensureChunkInstancesUploaded(chunk)) return;

    glUseProgram(m_program);

    // Matrices
    glUniformMatrix4fv(uView, 1, GL_FALSE, &view[0][0]);
    glUniformMatrix4fv(uProj, 1, GL_FALSE, &proj[0][0]);
    glm::mat4 model = glm::translate(glm::mat4(1.0f), glm::vec3(worldOffset.x, worldOffset.y, worldOffset.z));
    glUniformMatrix4fv(uModel, 1, GL_FALSE, &model[0][0]);
    glUniform1f(uTime, m_time);

    // Cascades
    glUniform1i(uCascadeCount, m_cascadeCount);
    // Upload as array of 4 mat4 starting at base location (std140 is not used; direct uniforms ok)
    for (int i=0;i<m_cascadeCount;i++) {
        char name[32]; snprintf(name, sizeof(name), "uLightVP[%d]", i);
        GLint loc = glGetUniformLocation(m_program, name);
        glUniformMatrix4fv(loc, 1, GL_FALSE, &m_lightVPs[i][0][0]);
    }
    glUniform3f(uLightDir, m_lightDir.x, m_lightDir.y, m_lightDir.z);
    // Splits & texel sizes
    GLint locSplits = uCascadeSplits;
    if (locSplits>=0) glUniform1fv(locSplits, m_cascadeCount, m_cascadeSplits);
    float texels[4] = {0};
    for (int i=0;i<m_cascadeCount;i++) {
        int sz = g_csm.getSize(i); texels[i] = sz>0? (1.0f/float(sz)) : (1.0f/2048.0f);
    }
    if (uShadowTexel>=0) glUniform1fv(uShadowTexel, m_cascadeCount, texels);
    // Bind depth textures at 7/8/9 like voxel shader
    if (m_cascadeCount>=1) { glActiveTexture(GL_TEXTURE7); glBindTexture(GL_TEXTURE_2D, g_csm.getDepthTexture(0)); }
    if (m_cascadeCount>=2) { glActiveTexture(GL_TEXTURE8); glBindTexture(GL_TEXTURE_2D, g_csm.getDepthTexture(1)); }
    if (m_cascadeCount>=3) { glActiveTexture(GL_TEXTURE9); glBindTexture(GL_TEXTURE_2D, g_csm.getDepthTexture(2)); }
    // Set uniform sampler indices
    GLint loc0 = glGetUniformLocation(m_program, "uShadowMaps[0]"); if (loc0>=0) glUniform1i(loc0, 7);
    GLint loc1 = glGetUniformLocation(m_program, "uShadowMaps[1]"); if (loc1>=0) glUniform1i(loc1, 8);
    GLint loc2 = glGetUniformLocation(m_program, "uShadowMaps[2]"); if (loc2>=0) glUniform1i(loc2, 9);

    // Draw instanced for each primitive
    ChunkInstanceBuffer& buf = m_chunkInstances[chunk];
    for (auto& prim : m_grassModel.primitives) {
        glBindVertexArray(prim.vao);
        glDrawElementsInstanced(GL_TRIANGLES, prim.indexCount, GL_UNSIGNED_INT, 0, buf.count);
        glBindVertexArray(0);
    }
}

void ModelInstanceRenderer::beginDepthPassCascade(int cascadeIndex, const glm::mat4& lightVP) {
    // We reuse g_csm FBO via VBORenderer; set light VP on our depth program
    (void)cascadeIndex;
    if (!ensureShaders()) return;
    glUseProgram(m_depthProgram);
    glUniformMatrix4fv(d_uLightVP, 1, GL_FALSE, &lightVP[0][0]);
}

void ModelInstanceRenderer::endDepthPassCascade(int screenWidth, int screenHeight) {
    (void)screenWidth; (void)screenHeight;
}

void ModelInstanceRenderer::renderDepthGrassChunk(VoxelChunk* chunk, const Vec3& worldOffset) {
    if (!m_grassModel.valid || !ensureShaders()) return;
    if (!ensureChunkInstancesUploaded(chunk)) return;
    glUseProgram(m_depthProgram);
    glm::mat4 model = glm::translate(glm::mat4(1.0f), glm::vec3(worldOffset.x, worldOffset.y, worldOffset.z));
    glUniformMatrix4fv(d_uModel, 1, GL_FALSE, &model[0][0]);
    glUniform1f(d_uTime, m_time);
    // The active light VP is provided by the caller via uniform each cascade
    // But to avoid querying per cascade, we expect the caller to set d_uLightVP before calls
    // For simplicity, set it from the first lightVP (caller should update per-cascade)
    // This is updated externally in GameClient per cascade.

    // Bind and draw
    ChunkInstanceBuffer& buf = m_chunkInstances[chunk];
    for (auto& prim : m_grassModel.primitives) {
        glBindVertexArray(prim.vao);
        glDrawElementsInstanced(GL_TRIANGLES, prim.indexCount, GL_UNSIGNED_INT, 0, buf.count);
        glBindVertexArray(0);
    }
}
