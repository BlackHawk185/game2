#include "ShadowMapper.h"
#include <glad/glad.h>
#include <iostream>
#include <algorithm>
#include <cmath>

ShadowMapper::ShadowMapper()
    : m_depthMapFBO(0)
    , m_depthMap(0)
    , m_shadowMapSize(2048)
    , m_initialized(false)
{
}

ShadowMapper::~ShadowMapper()
{
    shutdown();
}

bool ShadowMapper::initialize(int shadowMapSize)
{
    if (m_initialized) {
        return true;
    }

    // Ensure GLAD is loaded (should already be done by VBORenderer)
    if (!gladLoadGL()) {
        std::cerr << "GLAD not loaded for shadow mapping!" << std::endl;
        return false;
    }

    m_shadowMapSize = shadowMapSize;

    // Generate framebuffer for shadow map
    glGenFramebuffers(1, &m_depthMapFBO);
    
    // Generate depth texture
    glGenTextures(1, &m_depthMap);
    glBindTexture(GL_TEXTURE_2D, m_depthMap);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, m_shadowMapSize, m_shadowMapSize, 
                 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);
    
    // Set texture parameters
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
    
    // Set border color to white (no shadow outside the light frustum)
    float borderColor[] = { 1.0f, 1.0f, 1.0f, 1.0f };
    glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, borderColor);

    // Attach depth texture to framebuffer
    glBindFramebuffer(GL_FRAMEBUFFER, m_depthMapFBO);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, m_depthMap, 0);
    
    // Tell OpenGL we don't want to draw to color buffer
    glDrawBuffer(GL_NONE);
    glReadBuffer(GL_NONE);
    
    // Check framebuffer completeness
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        std::cerr << "Shadow map framebuffer not complete!" << std::endl;
        shutdown();
        return false;
    }
    
    // Restore default framebuffer
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    
    m_initialized = true;
    std::cout << "Shadow mapper initialized with " << m_shadowMapSize << "x" << m_shadowMapSize << " shadow map" << std::endl;
    return true;
}

void ShadowMapper::shutdown()
{
    if (m_depthMap != 0) {
        glDeleteTextures(1, &m_depthMap);
        m_depthMap = 0;
    }
    
    if (m_depthMapFBO != 0) {
        glDeleteFramebuffers(1, &m_depthMapFBO);
        m_depthMapFBO = 0;
    }
    
    m_initialized = false;
}

void ShadowMapper::beginShadowPass(const Vec3& lightDirection, const Vec3& sceneCenter, float sceneRadius)
{
    if (!m_initialized) {
        return;
    }

    // Store current viewport
    glGetIntegerv(GL_VIEWPORT, m_previousViewport);
    
    // Create orthographic projection matrix for directional light
    float left = -sceneRadius;
    float right = sceneRadius;
    float bottom = -sceneRadius;
    float top = sceneRadius;
    float near_plane = 1.0f;
    float far_plane = sceneRadius * 2.0f;
    
    m_lightProjection = Mat4::ortho(left, right, bottom, top, near_plane, far_plane);
    
    // Create view matrix looking from light position towards scene center
    Vec3 lightPos = sceneCenter - (lightDirection * sceneRadius);
    Vec3 up = Vec3(0.0f, 1.0f, 0.0f);
    
    // If light direction is too close to up vector, use different up vector
    if (std::abs(lightDirection.dot(up)) > 0.95f) {
        up = Vec3(1.0f, 0.0f, 0.0f);
    }
    
    m_lightView = Mat4::lookAt(lightPos, sceneCenter, up);
    m_lightSpaceMatrix = m_lightProjection * m_lightView;
    
    // Bind shadow map framebuffer and set viewport
    glBindFramebuffer(GL_FRAMEBUFFER, m_depthMapFBO);
    glViewport(0, 0, m_shadowMapSize, m_shadowMapSize);
    
    // Clear depth buffer
    glClear(GL_DEPTH_BUFFER_BIT);
    
    // Enable front face culling for shadow acne reduction
    glCullFace(GL_FRONT);
}

void ShadowMapper::endShadowPass()
{
    if (!m_initialized) {
        return;
    }

    // Restore back face culling
    glCullFace(GL_BACK);
    
    // Restore default framebuffer and viewport
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(m_previousViewport[0], m_previousViewport[1], 
               m_previousViewport[2], m_previousViewport[3]);
}

void ShadowMapper::bindShadowMap(int textureUnit)
{
    if (!m_initialized) {
        return;
    }

    glActiveTexture(GL_TEXTURE0 + textureUnit);
    glBindTexture(GL_TEXTURE_2D, m_depthMap);
}

void ShadowMapper::beginShadowPass(const Vec3& lightDirection)
{
    // Simplified version with default scene parameters
    beginShadowPass(lightDirection, Vec3(0, 0, 0), 100.0f);
}

void ShadowMapper::setModelMatrix(const Mat4& model)
{
    // This would typically set a uniform in the depth shader
    // For now, we'll assume the caller handles model matrix setting
    (void)model; // Suppress unused parameter warning
}
