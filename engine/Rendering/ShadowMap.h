#pragma once

#include <glm/glm.hpp>
using GLuint = unsigned int;

class ShadowMap {
public:
    ShadowMap();
    ~ShadowMap();

    bool initialize(int width = 2048, int height = 2048);
    void shutdown();

    void beginRender();
    void endRender(int screenWidth, int screenHeight);

    void setLightVP(const glm::mat4& lightVP) { m_lightVP = lightVP; }
    const glm::mat4& getLightVP() const { return m_lightVP; }

    GLuint getDepthTexture() const { return m_depthTex; }
    int getWidth() const { return m_width; }
    int getHeight() const { return m_height; }

private:
    GLuint m_fbo = 0;
    GLuint m_depthTex = 0;
    int m_width = 0;
    int m_height = 0;
    glm::mat4 m_lightVP{1.0f};
};

extern ShadowMap g_shadowMap;

