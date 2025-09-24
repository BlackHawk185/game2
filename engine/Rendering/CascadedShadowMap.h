#pragma once

#include <glm/glm.hpp>
#include "../Math/Vec3.h"

using GLuint = unsigned int;

// Forward declarations
class Camera;

struct CascadeInfo {
    glm::mat4 viewProjection;
    float nearPlane, farPlane;
    glm::vec4 boundingSphere;  // xyz = center, w = radius (for culling optimization)
    int resolution;
};

class CascadedShadowMap {
public:
    static constexpr int MAX_CASCADES = 2;  // Near and far cascades only
    
    bool initialize();
    void shutdown();

    // Automatically calculate cascades based on camera and light direction
    void updateCascades(const Camera& camera, const Vec3& lightDirection, float maxShadowDistance = 600.0f);
    
    // Manual override if needed
    void setCascadeInfo(int index, const glm::mat4& lightVP, float nearPlane, float farPlane);
    
    // Rendering interface
    void beginCascade(int index);
    void endCascade(int screenWidth, int screenHeight);
    
    // Getters
    int getCascadeCount() const { return MAX_CASCADES; }
    int getSize(int index) const { return (index >= 0 && index < MAX_CASCADES) ? m_cascades[index].resolution : 0; }
    GLuint getDepthTexture(int index) const { return (index >= 0 && index < MAX_CASCADES) ? m_depthTex[index] : 0; }
    const CascadeInfo& getCascade(int index) const { return m_cascades[index]; }
    float getSplitDistance() const { return m_splitDistance; }

private:
    void calculateOptimalSplits(float nearPlane, float farPlane, float maxDistance);
    void generateLightMatrices(const Vec3& lightDir, const Camera& camera);
    glm::mat4 createLightViewMatrix(const Vec3& lightDir, const glm::vec3& sceneCenter);
    glm::mat4 createLightProjectionMatrix(const std::vector<glm::vec3>& frustumCorners);
    
    CascadeInfo m_cascades[MAX_CASCADES];
    float m_splitDistance = 50.0f;  // Distance where near cascade ends and far begins
    
    GLuint m_fbo[MAX_CASCADES] = {0, 0};
    GLuint m_depthTex[MAX_CASCADES] = {0, 0};
};

extern CascadedShadowMap g_csm;
