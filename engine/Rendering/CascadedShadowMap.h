#pragma once

#include <glm/glm.hpp>
using GLuint = unsigned int;

class CascadedShadowMap {
public:
    bool initialize(int cascades = 1, int size = 16384);
    void shutdown();

    int getCascadeCount() const { return m_cascadeCount; }
    int getSize(int index) const { return (index>=0 && index<m_cascadeCount) ? m_size[index] : 0; }
    bool resizeCascade(int index, int newSize);

    void beginCascade(int index);
    void endCascade(int screenWidth, int screenHeight);

    GLuint getDepthTexture(int index) const { return m_depthTex[index]; }

private:
    int m_cascadeCount = 0;
    int m_size[4] = {0,0,0,0};
    GLuint m_fbo[4] = {0,0,0,0};
    GLuint m_depthTex[4] = {0,0,0,0};
};

extern CascadedShadowMap g_csm;
