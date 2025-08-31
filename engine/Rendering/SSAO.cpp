// SSAO.cpp - Screen Space Ambient Occlusion post-process
#include "SSAO.h"
#include <cmath>
#include <cstdlib>
#include <cstdio>
#include <iostream>

// Fallback GL constants and prototypes for older headers
#ifndef GL_RGBA16F
#define GL_RGBA16F 0x881A
#endif
#ifndef GL_R8
#define GL_R8 0x8229
#endif
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

#ifndef GLAPI
#define GLAPI extern
#endif
#ifndef APIENTRY
#define APIENTRY
#endif
// Forward declare functions if not provided by headers (implemented by GLAD object library)
#ifndef GL_VERSION_3_0
extern "C" GLAPI void APIENTRY glDrawBuffers(GLsizei n, const GLenum* bufs);
#endif
#ifndef GL_VERSION_3_0
extern "C" GLAPI void APIENTRY glBlitFramebuffer(GLint srcX0, GLint srcY0, GLint srcX1, GLint srcY1,
                                                 GLint dstX0, GLint dstY0, GLint dstX1, GLint dstY1,
                                                 GLbitfield mask, GLenum filter);
#endif

static const char* AO_VERT = R"(#version 330 core
layout (location = 0) in vec2 aPos;
layout (location = 1) in vec2 aUV;
out vec2 vUV;
void main(){ vUV = aUV; gl_Position = vec4(aPos, 0.0, 1.0); }
)";

// Reconstruct view-space position from depth using near/far/tanHalfFov/aspect
// Then evaluate SSAO by sampling around the point in view space.
static const char* AO_FRAG = R"(#version 330 core
in vec2 vUV;
layout (location = 0) out float FragAO;

uniform sampler2D uDepth;
uniform sampler2D uNoise;
uniform vec3 uSamples[64];
uniform int uKernelCount;
uniform float uNoiseScaleX;
uniform float uNoiseScaleY;
uniform float uNear;
uniform float uFar;
uniform float uTanHalfFov;
uniform float uAspect;

// Convert depth buffer value to view-space Z (negative forward)
float depthToViewZ(float depth)
{
    float z = depth * 2.0 - 1.0; // NDC
    // z_view = -(2*n*f) / (z*(f-n) - (f+n))
    float n = uNear; float f = uFar;
    return -(2.0 * n * f) / (z * (f - n) - (f + n));
}

vec3 reconstructViewPos(vec2 uv, float depth)
{
    float zView = depthToViewZ(depth); // negative
    // Reconstruct X,Y in view space using projection params
    float xNdc = uv.x * 2.0 - 1.0;
    float yNdc = uv.y * 2.0 - 1.0;
    float xView = -zView * xNdc * uAspect * uTanHalfFov;
    float yView = -zView * yNdc * uTanHalfFov;
    return vec3(xView, yView, zView);
}

void main()
{
    // Fetch depth and reconstruct position and normal
    float depth = texture(uDepth, vUV).r;
    if (depth >= 1.0) { FragAO = 1.0; return; } // sky/background -> no occlusion

    vec3 p = reconstructViewPos(vUV, depth);

    // Approximate normal from depth neighbors
    vec2 texel = 1.0 / vec2(textureSize(uDepth, 0));
    float depthR = texture(uDepth, vUV + vec2(texel.x, 0)).r;
    float depthU = texture(uDepth, vUV + vec2(0, texel.y)).r;
    vec3 px = reconstructViewPos(vUV + vec2(texel.x, 0), depthR);
    vec3 py = reconstructViewPos(vUV + vec2(0, texel.y), depthU);
    vec3 n = normalize(cross(py - p, px - p));

    // Random rotation from noise texture
    vec2 noiseScale = vec2(uNoiseScaleX, uNoiseScaleY);
    vec3 randVec = texture(uNoise, vUV * noiseScale).xyz * 2.0 - 1.0;
    vec3 tangent = normalize(randVec - n * dot(randVec, n));
    vec3 bitangent = cross(n, tangent);
    mat3 TBN = mat3(tangent, bitangent, n);

    float radius = 0.6; // scene-scale dependent
    float bias = 0.02;

    float occlusion = 0.0;
    for (int i = 0; i < uKernelCount; ++i)
    {
        vec3 samp = TBN * uSamples[i];
        samp = p + samp * radius;

        // Project sample to screen UV to fetch depth
        float xNdc = -(samp.x) / (samp.z * uAspect * uTanHalfFov);
        float yNdc = -(samp.y) / (samp.z * uTanHalfFov);
        vec2 uv = vec2(xNdc, yNdc) * 0.5 + 0.5;
        if (uv.x < 0.0 || uv.x > 1.0 || uv.y < 0.0 || uv.y > 1.0) { continue; }

        float sampDepth = texture(uDepth, uv).r;
        float sampViewZ = depthToViewZ(sampDepth);

        float rangeCheck = smoothstep(0.0, 1.0, radius / abs(p.z - sampViewZ));
        occlusion += (sampViewZ >= (samp.z + bias) ? 1.0 : 0.0) * rangeCheck;
    }

    occlusion = 1.0 - (occlusion / float(uKernelCount));
    FragAO = clamp(occlusion, 0.0, 1.0);
}
)";

static const char* BLUR_FRAG = R"(#version 330 core
in vec2 vUV;
layout (location = 0) out float FragAO;
uniform sampler2D uAO;
void main(){
    vec2 texel = 1.0 / vec2(textureSize(uAO, 0));
    float sum = 0.0;
    sum += texture(uAO, vUV + vec2(-texel.x, 0)).r * 0.25;
    sum += texture(uAO, vUV).r * 0.5;
    sum += texture(uAO, vUV + vec2(texel.x, 0)).r * 0.25;
    FragAO = sum;
}
)";

static const char* COMPOSITE_FRAG = R"(#version 330 core
in vec2 vUV;
layout (location = 0) out vec4 FragColor;
uniform sampler2D uScene;
uniform sampler2D uAO;
uniform float uIntensity;
void main(){
    vec3 color = texture(uScene, vUV).rgb;
    float ao = texture(uAO, vUV).r;
    float aoFactor = mix(1.0, ao, uIntensity);
    FragColor = vec4(color * aoFactor, 1.0);
}
)";

SSAO::SSAO() {}
SSAO::~SSAO() { shutdown(); }

bool SSAO::compileProgram(const char* vs, const char* fs, GLuint& programOut)
{
    GLuint vsId = glCreateShader(GL_VERTEX_SHADER);
    GLuint fsId = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(vsId, 1, &vs, nullptr);
    glCompileShader(vsId);
    GLint ok=0; glGetShaderiv(vsId, GL_COMPILE_STATUS, &ok);
    if (!ok) { char log[1024]; glGetShaderInfoLog(vsId, 1024, nullptr, log); std::cerr << "SSAO VS error: " << log << std::endl; return false; }
    glShaderSource(fsId, 1, &fs, nullptr);
    glCompileShader(fsId);
    glGetShaderiv(fsId, GL_COMPILE_STATUS, &ok);
    if (!ok) { char log[1024]; glGetShaderInfoLog(fsId, 1024, nullptr, log); std::cerr << "SSAO FS error: " << log << std::endl; return false; }
    programOut = glCreateProgram();
    glAttachShader(programOut, vsId);
    glAttachShader(programOut, fsId);
    glLinkProgram(programOut);
    glGetProgramiv(programOut, GL_LINK_STATUS, &ok);
    if (!ok) { char log[1024]; glGetProgramInfoLog(programOut, 1024, nullptr, log); std::cerr << "SSAO program link error: " << log << std::endl; return false; }
    glDeleteShader(vsId);
    glDeleteShader(fsId);
    return true;
}

void SSAO::createFullscreenQuad()
{
    if (m_fullscreenVAO != 0) return;
    float verts[] = {
        // pos      // uv
        -1.0f, -1.0f,  0.0f, 0.0f,
         1.0f, -1.0f,  1.0f, 0.0f,
         1.0f,  1.0f,  1.0f, 1.0f,
        -1.0f,  1.0f,  0.0f, 1.0f,
    };
    unsigned int idx[] = { 0,1,2, 2,3,0 };
    GLuint ebo;
    glGenVertexArrays(1, &m_fullscreenVAO);
    glGenBuffers(1, &m_fullscreenVBO);
    glGenBuffers(1, &ebo);
    glBindVertexArray(m_fullscreenVAO);
    glBindBuffer(GL_ARRAY_BUFFER, m_fullscreenVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(verts), verts, GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(idx), idx, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4*sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4*sizeof(float), (void*)(2*sizeof(float)));
    glEnableVertexAttribArray(1);
    glBindVertexArray(0);
}

void SSAO::createNoiseTexture()
{
    if (m_noiseTex != 0) return;
    std::vector<float> noise(4 * 16);
    for (int i=0;i<16;i++){
        float rx = (float(rand())/RAND_MAX)*2.0f - 1.0f;
        float ry = (float(rand())/RAND_MAX)*2.0f - 1.0f;
        float rz = 0.0f;
        noise[i*4+0]=rx; noise[i*4+1]=ry; noise[i*4+2]=rz; noise[i*4+3]=0.0f;
    }
    glGenTextures(1, &m_noiseTex);
    glBindTexture(GL_TEXTURE_2D, m_noiseTex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, 4, 4, 0, GL_RGBA, GL_FLOAT, noise.data());
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glBindTexture(GL_TEXTURE_2D, 0);
}

void SSAO::createOrResizeAOTextures(int width, int height)
{
    // AO
    if (m_aoTex == 0) glGenTextures(1, &m_aoTex);
    glBindTexture(GL_TEXTURE_2D, m_aoTex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, width, height, 0, GL_RED, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    if (m_aoFBO == 0) glGenFramebuffers(1, &m_aoFBO);
    glBindFramebuffer(GL_FRAMEBUFFER, m_aoFBO);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_aoTex, 0);
    // Single color attachment; default draw buffer is GL_COLOR_ATTACHMENT0

    // AO Blur
    if (m_aoBlurTex == 0) glGenTextures(1, &m_aoBlurTex);
    glBindTexture(GL_TEXTURE_2D, m_aoBlurTex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, width, height, 0, GL_RED, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    if (m_aoBlurFBO == 0) glGenFramebuffers(1, &m_aoBlurFBO);
    glBindFramebuffer(GL_FRAMEBUFFER, m_aoBlurFBO);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_aoBlurTex, 0);
    // Single color attachment; default draw buffer is GL_COLOR_ATTACHMENT0

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

bool SSAO::initialize()
{
    if (!compileProgram(AO_VERT, AO_FRAG, m_aoProgram)) return false;
    if (!compileProgram(AO_VERT, BLUR_FRAG, m_blurProgram)) return false;
    if (!compileProgram(AO_VERT, COMPOSITE_FRAG, m_compositeProgram)) return false;
    createFullscreenQuad();

    // Build kernel
    m_kernel.resize(m_kernelCount * 3);
    for (int i=0;i<m_kernelCount;i++){
        float x = (float(rand())/RAND_MAX)*2.0f - 1.0f;
        float y = (float(rand())/RAND_MAX)*2.0f - 1.0f;
        float z = (float(rand())/RAND_MAX);
        // Bias samples towards origin
        float scale = float(i)/float(m_kernelCount);
        scale = 0.1f + 0.9f*scale*scale;
        m_kernel[i*3+0] = x * scale;
        m_kernel[i*3+1] = y * scale;
        m_kernel[i*3+2] = z * scale;
    }

    createNoiseTexture();
    return true;
}

void SSAO::shutdown()
{
    if (m_aoProgram) { glDeleteProgram(m_aoProgram); m_aoProgram = 0; }
    if (m_blurProgram) { glDeleteProgram(m_blurProgram); m_blurProgram = 0; }
    if (m_compositeProgram) { glDeleteProgram(m_compositeProgram); m_compositeProgram = 0; }
    if (m_fullscreenVBO) { glDeleteBuffers(1, &m_fullscreenVBO); m_fullscreenVBO = 0; }
    if (m_fullscreenVAO) { glDeleteVertexArrays(1, &m_fullscreenVAO); m_fullscreenVAO = 0; }
    if (m_noiseTex) { glDeleteTextures(1, &m_noiseTex); m_noiseTex = 0; }
    if (m_aoTex) { glDeleteTextures(1, &m_aoTex); m_aoTex = 0; }
    if (m_aoBlurTex) { glDeleteTextures(1, &m_aoBlurTex); m_aoBlurTex = 0; }
    if (m_aoFBO) { glDeleteFramebuffers(1, &m_aoFBO); m_aoFBO = 0; }
    if (m_aoBlurFBO) { glDeleteFramebuffers(1, &m_aoBlurFBO); m_aoBlurFBO = 0; }
}

void SSAO::ensureResources(int width, int height)
{
    createOrResizeAOTextures(width, height);
}

void SSAO::computeAO(GLuint depthTex, int width, int height, float tanHalfFov, float aspect, float nearPlane, float farPlane)
{
    ensureResources(width, height);
    glBindFramebuffer(GL_FRAMEBUFFER, m_aoFBO);
    glViewport(0, 0, width, height);
    glUseProgram(m_aoProgram);
    // Set uniforms
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, depthTex);
    GLint loc = glGetUniformLocation(m_aoProgram, "uDepth"); if (loc!=-1) glUniform1i(loc, 0);

    glActiveTexture(GL_TEXTURE0 + 1);
    glBindTexture(GL_TEXTURE_2D, m_noiseTex);
    loc = glGetUniformLocation(m_aoProgram, "uNoise"); if (loc!=-1) glUniform1i(loc, 1);

    loc = glGetUniformLocation(m_aoProgram, "uKernelCount"); if (loc!=-1) glUniform1i(loc, m_kernelCount);
    // Upload samples
    for (int i=0;i<m_kernelCount;i++){
        char name[32]; snprintf(name, sizeof(name), "uSamples[%d]", i);
        GLint sLoc = glGetUniformLocation(m_aoProgram, name);
        if (sLoc != -1) glUniform3f(sLoc, m_kernel[i*3+0], m_kernel[i*3+1], m_kernel[i*3+2]);
    }
    // Noise scale (split to avoid potential missing glUniform2f symbol)
    loc = glGetUniformLocation(m_aoProgram, "uNoiseScaleX"); if (loc!=-1) glUniform1f(loc, (float)width/4.0f);
    loc = glGetUniformLocation(m_aoProgram, "uNoiseScaleY"); if (loc!=-1) glUniform1f(loc, (float)height/4.0f);
    // Camera params
    loc = glGetUniformLocation(m_aoProgram, "uNear"); if (loc!=-1) glUniform1f(loc, nearPlane);
    loc = glGetUniformLocation(m_aoProgram, "uFar"); if (loc!=-1) glUniform1f(loc, farPlane);
    loc = glGetUniformLocation(m_aoProgram, "uTanHalfFov"); if (loc!=-1) glUniform1f(loc, tanHalfFov);
    loc = glGetUniformLocation(m_aoProgram, "uAspect"); if (loc!=-1) glUniform1f(loc, aspect);

    glBindVertexArray(m_fullscreenVAO);
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
    glBindVertexArray(0);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void SSAO::blurAO(int width, int height)
{
    glBindFramebuffer(GL_FRAMEBUFFER, m_aoBlurFBO);
    glViewport(0, 0, width, height);
    glUseProgram(m_blurProgram);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, m_aoTex);
    GLint loc = glGetUniformLocation(m_blurProgram, "uAO"); if (loc!=-1) glUniform1i(loc, 0);
    glBindVertexArray(m_fullscreenVAO);
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
    glBindVertexArray(0);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void SSAO::composite(GLuint sceneColorTex, int width, int height, float intensity)
{
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, width, height);
    glUseProgram(m_compositeProgram);
    glDisable(GL_DEPTH_TEST);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, sceneColorTex);
    GLint loc = glGetUniformLocation(m_compositeProgram, "uScene"); if (loc!=-1) glUniform1i(loc, 0);
    glActiveTexture(GL_TEXTURE0 + 1);
    glBindTexture(GL_TEXTURE_2D, getAOTexture());
    loc = glGetUniformLocation(m_compositeProgram, "uAO"); if (loc!=-1) glUniform1i(loc, 1);
    loc = glGetUniformLocation(m_compositeProgram, "uIntensity"); if (loc!=-1) glUniform1f(loc, intensity);
    glBindVertexArray(m_fullscreenVAO);
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
    glBindVertexArray(0);
    glEnable(GL_DEPTH_TEST);
}
