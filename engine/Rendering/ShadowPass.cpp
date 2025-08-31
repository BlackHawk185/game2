#include "ShadowPass.h"
#include "../World/VoxelChunk.h"
#include <glad/glad.h>
#include <iostream>

ShadowPass* g_shadowPass = nullptr;

ShadowPass::ShadowPass()
    : m_size(1024)
    , m_lightView(Mat4::identity())
    , m_lightProj(Mat4::identity())
    , m_initialized(false)
{
}

ShadowPass::~ShadowPass()
{
    shutdown();
}

bool ShadowPass::initialize(int size)
{
    m_size = size;
    m_initialized = true;

    // Create depth texture
    glGenTextures(1, &m_depthTex);
    glBindTexture(GL_TEXTURE_2D, m_depthTex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, m_size, m_size, 0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, 0x812F /*GL_CLAMP_TO_EDGE*/);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, 0x812F /*GL_CLAMP_TO_EDGE*/);
    glBindTexture(GL_TEXTURE_2D, 0);

    // Create framebuffer if functions are available
    {
        glGenFramebuffers(1, &m_fbo);
        glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, m_depthTex, 0);
        // No color buffer
        glDrawBuffer(GL_NONE);
        glReadBuffer(GL_NONE);
        GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
        if (status == GL_FRAMEBUFFER_COMPLETE)
        {
            m_hasFBO = true;
            std::cout << "ShadowPass: FBO ready (" << m_size << "x" << m_size << ")" << std::endl;
        }
        else
        {
            std::cout << "ShadowPass: FBO incomplete, status=0x" << std::hex << status << std::dec << std::endl;
        }
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }

    // Minimal depth-only shader
    const char* VS = R"(
        #version 330 core
        layout(location=0) in vec3 aPosition;
        uniform mat4 uLightVP;
        uniform mat4 uModel;
        void main(){ gl_Position = uLightVP * uModel * vec4(aPosition,1.0); }
    )";
    const char* FS = R"(
        #version 330 core
        void main() { }
    )";
    GLuint vs = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vs, 1, &VS, nullptr);
    glCompileShader(vs);
    GLint ok=0; glGetShaderiv(vs, GL_COMPILE_STATUS, &ok);
    if (!ok) { GLchar log[512]; glGetShaderInfoLog(vs,512,nullptr,log); std::cout<<"ShadowPass VS error: "<<log<<std::endl; }
    GLuint fs = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fs,1,&FS,nullptr);
    glCompileShader(fs);
    glGetShaderiv(fs, GL_COMPILE_STATUS, &ok);
    if (!ok) { GLchar log[512]; glGetShaderInfoLog(fs,512,nullptr,log); std::cout<<"ShadowPass FS error: "<<log<<std::endl; }
    m_depthProgram = glCreateProgram();
    glAttachShader(m_depthProgram, vs);
    glAttachShader(m_depthProgram, fs);
    glLinkProgram(m_depthProgram);
    glGetProgramiv(m_depthProgram, GL_LINK_STATUS, &ok);
    if (!ok) { GLchar log[512]; glGetProgramInfoLog(m_depthProgram,512,nullptr,log); std::cout<<"ShadowPass link error: "<<log<<std::endl; }
    glDeleteShader(vs);
    glDeleteShader(fs);
    m_uLightVP = glGetUniformLocation(m_depthProgram, "uLightVP");
    m_uModel   = glGetUniformLocation(m_depthProgram, "uModel");

    return true;
}

void ShadowPass::shutdown()
{
    if (!m_initialized) return;
    if (m_fbo)
    {
        glDeleteFramebuffers(1, &m_fbo);
        m_fbo = 0;
    }
    if (m_depthTex)
    {
        glDeleteTextures(1, &m_depthTex);
        m_depthTex = 0;
    }
    m_initialized = false;
}

void ShadowPass::computeLightMatrices(const Vec3& sunDir, const Vec3& focusPoint,
                                      float extent, float nearPlane, float farPlane)
{
    // Build a stable light view matrix: light looks at focus point from far along sun direction
    Vec3 lightDir = sunDir.normalized();
    Vec3 lightPos = focusPoint - lightDir * (farPlane * 0.5f + extent);
    Vec3 up(0.0f, 1.0f, 0.0f);
    if (std::abs(lightDir.y) > 0.9f) up = Vec3(0.0f, 0.0f, 1.0f);

    m_lightView = Mat4::lookAt(lightPos, focusPoint, up);
    m_lightProj = Mat4::ortho(-extent, extent, -extent, extent, nearPlane, farPlane);
}

void ShadowPass::renderDepth(const std::vector<std::pair<VoxelChunk*, Vec3>>& chunks)
{
    if (!m_initialized) return;
    if (!m_hasFBO) return;

    // Save state
    GLint prevFBO = 0; glGetIntegerv(GL_FRAMEBUFFER_BINDING, &prevFBO);
    GLint viewport[4]; glGetIntegerv(GL_VIEWPORT, viewport);
    GLboolean colorMask[4]; glGetBooleanv(GL_COLOR_WRITEMASK, colorMask);
    GLboolean depthMask; glGetBooleanv(GL_DEPTH_WRITEMASK, &depthMask);

    // Bind FBO and set viewport
    glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);
    glViewport(0, 0, m_size, m_size);
    glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
    glDepthMask(GL_TRUE);
    glEnable(GL_DEPTH_TEST);
    // Optional: polygon offset could be enabled here if needed; relying on shader bias for now
    glClearDepth(1.0);
    glClear(GL_DEPTH_BUFFER_BIT);

    // Use depth shader
    glUseProgram(m_depthProgram);
    Mat4 lightVP = m_lightProj * m_lightView;
    if (m_uLightVP != -1) glUniformMatrix4fv(m_uLightVP, 1, GL_FALSE, lightVP.data());

    // Draw each chunk VAO
    for (const auto& cp : chunks)
    {
        VoxelChunk* chunk = cp.first;
        const Vec3& worldOffset = cp.second;
        if (!chunk) continue;
        const VoxelMesh& mesh = chunk->getMesh();
        if (mesh.VAO == 0 || mesh.indices.empty()) continue;

        Mat4 model = Mat4::translate(worldOffset);
        if (m_uModel != -1) glUniformMatrix4fv(m_uModel, 1, GL_FALSE, model.data());

        glBindVertexArray(mesh.VAO);
        glDrawElements(GL_TRIANGLES, (GLsizei)mesh.indices.size(), GL_UNSIGNED_INT, 0);
    }

    // Restore state
    glBindVertexArray(0);
    glUseProgram(0);
    glColorMask(colorMask[0], colorMask[1], colorMask[2], colorMask[3]);
    glDepthMask(depthMask);
    glViewport(viewport[0], viewport[1], viewport[2], viewport[3]);
    glBindFramebuffer(GL_FRAMEBUFFER, prevFBO);
}
