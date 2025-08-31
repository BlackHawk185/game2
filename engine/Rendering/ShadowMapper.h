#pragma once

#include "../Math/Mat4.h"
#include "../Math/Vec3.h"
using GLuint = unsigned int;

class ShadowMapper {
public:
    ShadowMapper();
    ~ShadowMapper();

    bool initialize(int shadowMapSize = 2048);
    void shutdown();

    // Begin shadow map rendering (render from light's perspective)
    void beginShadowPass(const Vec3& lightDirection, const Vec3& sceneCenter, float sceneRadius);
    void beginShadowPass(const Vec3& lightDirection); // Simplified version
    void endShadowPass();

    // Bind shadow map for main rendering pass
    void bindShadowMap(int textureUnit = 1);
    
    // Get light space matrix for main shader
    const Mat4& getLightSpaceMatrix() const { return m_lightSpaceMatrix; }
    
    // Get shadow map depth texture
    GLuint getDepthTexture() const { return m_depthMap; }
    
    // Set model matrix for shadow rendering
    void setModelMatrix(const Mat4& model);
    
    // Check initialization status
    bool isInitialized() const { return m_initialized; }

    // Get shadow map size
    int getShadowMapSize() const { return m_shadowMapSize; }

private:
    GLuint m_depthMapFBO;
    GLuint m_depthMap;
    int m_shadowMapSize;
    
    Mat4 m_lightProjection;
    Mat4 m_lightView;
    Mat4 m_lightSpaceMatrix;
    
    // Store viewport for restoration
    int m_previousViewport[4];
    
    bool m_initialized;
};
