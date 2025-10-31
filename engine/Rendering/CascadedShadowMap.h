#pragma once

#include <glm/glm.hpp>
#include <vector>
using GLuint = unsigned int;

struct CascadeData {
    glm::mat4 viewProj;
    float splitDistance;  // Far plane of this cascade
    float orthoSize;      // Size of orthographic projection
};

class ShadowMap {
public:
    bool initialize(int size = 16384, int numCascades = 2);
    void shutdown();

    int getSize() const { return m_size; }
    int getNumCascades() const { return m_numCascades; }
    bool resize(int newSize);

    void begin(int cascadeIndex = 0);
    void end(int screenWidth, int screenHeight);

    GLuint getDepthTexture() const { return m_depthTex; }
    GLuint getDepthTexture(int cascadeIndex) const;
    
    // Get cascade data for rendering
    const CascadeData& getCascade(int index) const { return m_cascades[index]; }
    void setCascadeData(int index, const CascadeData& data) { m_cascades[index] = data; }

private:
    int m_size = 0;
    int m_numCascades = 2;
    GLuint m_fbo = 0;
    GLuint m_depthTex = 0;  // Array texture for all cascades
    std::vector<CascadeData> m_cascades;
};

extern ShadowMap g_shadowMap;
