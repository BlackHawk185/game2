#include "CascadedShadowMap.h"
#include <glad/gl.h>

CascadedShadowMap g_csm;

bool CascadedShadowMap::initialize(int cascades, int size)
{
    shutdown();
    m_cascadeCount = cascades;
    for (int i = 0; i < m_cascadeCount; ++i) m_size[i] = size;

    for (int i = 0; i < m_cascadeCount; ++i) {
        glGenTextures(1, &m_depthTex[i]);
        glBindTexture(GL_TEXTURE_2D, m_depthTex[i]);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, m_size[i], m_size[i], 0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

        glGenFramebuffers(1, &m_fbo[i]);
        glBindFramebuffer(GL_FRAMEBUFFER, m_fbo[i]);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, m_depthTex[i], 0);
        glDrawBuffer(GL_NONE);
        glReadBuffer(GL_NONE);
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glBindTexture(GL_TEXTURE_2D, 0);
    return true;
}

void CascadedShadowMap::shutdown()
{
    for (int i = 0; i < 4; ++i) {
        if (m_fbo[i]) { glDeleteFramebuffers(1, &m_fbo[i]); m_fbo[i] = 0; }
        if (m_depthTex[i]) { glDeleteTextures(1, &m_depthTex[i]); m_depthTex[i] = 0; }
    }
    m_cascadeCount = 0;
    for (int i=0;i<4;++i) m_size[i] = 0;
}

void CascadedShadowMap::beginCascade(int index)
{
    glBindFramebuffer(GL_FRAMEBUFFER, m_fbo[index]);
    glViewport(0, 0, m_size[index], m_size[index]);
    glClear(GL_DEPTH_BUFFER_BIT);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_POLYGON_OFFSET_FILL);
    glPolygonOffset(2.0f, 4.0f);
}

bool CascadedShadowMap::resizeCascade(int index, int newSize)
{
    if (index < 0 || index >= m_cascadeCount) return false;
    // Delete existing
    if (m_fbo[index]) { glDeleteFramebuffers(1, &m_fbo[index]); m_fbo[index] = 0; }
    if (m_depthTex[index]) { glDeleteTextures(1, &m_depthTex[index]); m_depthTex[index] = 0; }

    m_size[index] = newSize;
    glGenTextures(1, &m_depthTex[index]);
    glBindTexture(GL_TEXTURE_2D, m_depthTex[index]);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, m_size[index], m_size[index], 0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glGenFramebuffers(1, &m_fbo[index]);
    glBindFramebuffer(GL_FRAMEBUFFER, m_fbo[index]);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, m_depthTex[index], 0);
    glDrawBuffer(GL_NONE);
    glReadBuffer(GL_NONE);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glBindTexture(GL_TEXTURE_2D, 0);
    return true;
}

void CascadedShadowMap::endCascade(int screenWidth, int screenHeight)
{
    glDisable(GL_POLYGON_OFFSET_FILL);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    // Restore default framebuffer draw/read buffers to avoid accidental color suppression
    glDrawBuffer(GL_BACK);
    glReadBuffer(GL_BACK);
    glViewport(0, 0, screenWidth, screenHeight);
}
