#pragma once

#include <glm/glm.hpp>
using GLuint = unsigned int;

class ShadowMap {
public:
    bool initialize(int size = 16384);
    void shutdown();

    int getSize() const { return m_size; }
    bool resize(int newSize);

    void begin();
    void end(int screenWidth, int screenHeight);

    GLuint getDepthTexture() const { return m_depthTex; }

private:
    int m_size = 0;
    GLuint m_fbo = 0;
    GLuint m_depthTex = 0;
};

extern ShadowMap g_shadowMap;
