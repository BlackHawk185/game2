#include "CascadedShadowMap.h"
#include "../Input/Camera.h"
#include <glad/gl.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <iostream>
#include <algorithm>
#include <cmath>
#include <limits>

CascadedShadowMap g_csm;

bool CascadedShadowMap::initialize()
{
    shutdown();
    
    // Initialize cascade 0 (near) - 4K resolution to match texture detail
    m_cascades[0].resolution = 4096;  // 4K resolution
    m_cascades[0].nearPlane = 0.1f;
    m_cascades[0].farPlane = 40.0f;   // Range 0-40
    
    // Initialize cascade 1 (far) - Lower resolution for performance
    m_cascades[1].resolution = 4096;  // 4K resolution for distant shadows
    m_cascades[1].nearPlane = 30.0f;  // Overlap: 30-40
    m_cascades[1].farPlane = 600.0f;  // Extended far range

    for (int i = 0; i < MAX_CASCADES; ++i) {
        // Create depth texture
        glGenTextures(1, &m_depthTex[i]);
        glBindTexture(GL_TEXTURE_2D, m_depthTex[i]);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, 
                     m_cascades[i].resolution, m_cascades[i].resolution, 
                     0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
        
        // Use linear filtering for better shadow quality
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
        
        // Set border color to white (no shadow outside shadow map)
        float borderColor[] = { 1.0f, 1.0f, 1.0f, 1.0f };
        glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, borderColor);

        // Create framebuffer
        glGenFramebuffers(1, &m_fbo[i]);
        glBindFramebuffer(GL_FRAMEBUFFER, m_fbo[i]);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, m_depthTex[i], 0);
        glDrawBuffer(GL_NONE);
        glReadBuffer(GL_NONE);
        
        // Check framebuffer completeness
        if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
            std::cerr << "ERROR: Shadow map framebuffer " << i << " not complete!" << std::endl;
            return false;
        }
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glBindTexture(GL_TEXTURE_2D, 0);
    
    std::cout << "Cascaded shadow map initialized: 2 cascades (" 
              << m_cascades[0].resolution << "x" << m_cascades[0].resolution << " near, "
              << m_cascades[1].resolution << "x" << m_cascades[1].resolution << " far)" << std::endl;
    return true;
}

void CascadedShadowMap::shutdown()
{
    for (int i = 0; i < MAX_CASCADES; ++i) {
        if (m_fbo[i]) { 
            glDeleteFramebuffers(1, &m_fbo[i]); 
            m_fbo[i] = 0; 
        }
        if (m_depthTex[i]) { 
            glDeleteTextures(1, &m_depthTex[i]); 
            m_depthTex[i] = 0; 
        }
    }
}

void CascadedShadowMap::updateCascades(const Camera& camera, const Vec3& lightDirection, float maxShadowDistance)
{
    // Calculate optimal split distances
    calculateOptimalSplits(camera.getNearPlane(), camera.getFarPlane(), maxShadowDistance);
    
    // Generate light matrices for each cascade
    generateLightMatrices(lightDirection, camera);
}

void CascadedShadowMap::calculateOptimalSplits(float nearPlane, float farPlane, float maxDistance)
{
    // Clamp max distance to camera far plane
    maxDistance = std::min(maxDistance, farPlane);
    
    // Overlapping ranges: 0-40 (near), 10-1000 (far)
    m_splitDistance = 40.0f;  // Split at 40 units (still used for shader split)
    
    // Set overlapping cascade ranges
    m_cascades[0].nearPlane = nearPlane;      // Typically 0.1
    m_cascades[0].farPlane = 40.0f;           // Near: 0-40
    
    m_cascades[1].nearPlane = 30.0f;         // Far: 30-1000 (10 unit overlap)
    m_cascades[1].farPlane = 1000.0f;         // Far end at 1000
}

void CascadedShadowMap::generateLightMatrices(const Vec3& lightDir, const Camera& camera)
{
    const glm::vec3 lightDirection = glm::vec3(-lightDir.x, -lightDir.y, -lightDir.z);  // Towards light
    
    for (int i = 0; i < MAX_CASCADES; ++i) {
        // Get frustum corners for this cascade
        std::vector<glm::vec3> frustumCorners;
        
        const float nearPlane = m_cascades[i].nearPlane;
        const float farPlane = m_cascades[i].farPlane;
        const float aspect = camera.getAspectRatio();
        const float fov = camera.getFOV();
        
        const float halfVSide = farPlane * tanf(fov * 0.5f);
        const float halfHSide = halfVSide * aspect;
        const glm::vec3 frontMultFar = farPlane * camera.getFront();
        
        // Calculate 8 frustum corners
        const glm::vec3 camPos = camera.getPosition();
        const glm::vec3 right = camera.getRight();
        const glm::vec3 up = camera.getUp();
        
        // Far plane corners
        frustumCorners.push_back(camPos + frontMultFar - up * halfVSide - right * halfHSide);
        frustumCorners.push_back(camPos + frontMultFar + up * halfVSide - right * halfHSide);
        frustumCorners.push_back(camPos + frontMultFar + up * halfVSide + right * halfHSide);
        frustumCorners.push_back(camPos + frontMultFar - up * halfVSide + right * halfHSide);
        
        // Near plane corners
        const float halfVSideNear = nearPlane * tanf(fov * 0.5f);
        const float halfHSideNear = halfVSideNear * aspect;
        const glm::vec3 frontMultNear = nearPlane * camera.getFront();
        
        frustumCorners.push_back(camPos + frontMultNear - up * halfVSideNear - right * halfHSideNear);
        frustumCorners.push_back(camPos + frontMultNear + up * halfVSideNear - right * halfHSideNear);
        frustumCorners.push_back(camPos + frontMultNear + up * halfVSideNear + right * halfHSideNear);
        frustumCorners.push_back(camPos + frontMultNear - up * halfVSideNear + right * halfHSideNear);
        
        // Calculate center of frustum
        glm::vec3 center = glm::vec3(0.0f);
        for (const auto& corner : frustumCorners) {
            center += corner;
        }
        center /= frustumCorners.size();
        
        // Create light view matrix
        const glm::mat4 lightView = createLightViewMatrix(lightDir, center);
        
        // Transform frustum corners to light space
        std::vector<glm::vec3> lightSpaceCorners;
        for (const auto& corner : frustumCorners) {
            glm::vec4 transformed = lightView * glm::vec4(corner, 1.0f);
            lightSpaceCorners.push_back(glm::vec3(transformed));
        }
        
        // Create tight orthographic projection
        const glm::mat4 lightProjection = createLightProjectionMatrix(lightSpaceCorners);
        
        // Store the combined matrix
        m_cascades[i].viewProjection = lightProjection * lightView;
        
        // Calculate bounding sphere for culling optimization
        float maxDistance = 0.0f;
        for (const auto& corner : frustumCorners) {
            float dist = glm::length(corner - center);
            maxDistance = std::max(maxDistance, dist);
        }
        m_cascades[i].boundingSphere = glm::vec4(center, maxDistance);
    }
}

glm::mat4 CascadedShadowMap::createLightViewMatrix(const Vec3& lightDir, const glm::vec3& sceneCenter)
{
    const glm::vec3 lightDirection = glm::vec3(-lightDir.x, -lightDir.y, -lightDir.z);
    const glm::vec3 lightPos = sceneCenter - lightDirection * 100.0f;  // Position light far from scene
    
    // Create up vector (avoid parallel to light direction)
    glm::vec3 up = glm::vec3(0.0f, 1.0f, 0.0f);
    if (std::abs(glm::dot(lightDirection, up)) > 0.9f) {
        up = glm::vec3(1.0f, 0.0f, 0.0f);  // Use X axis if light is too vertical
    }
    
    return glm::lookAt(lightPos, sceneCenter, up);
}

glm::mat4 CascadedShadowMap::createLightProjectionMatrix(const std::vector<glm::vec3>& frustumCorners)
{
    // Find AABB in light space
    float minX = std::numeric_limits<float>::max();
    float maxX = std::numeric_limits<float>::lowest();
    float minY = std::numeric_limits<float>::max();
    float maxY = std::numeric_limits<float>::lowest();
    float minZ = std::numeric_limits<float>::max();
    float maxZ = std::numeric_limits<float>::lowest();
    
    for (const auto& corner : frustumCorners) {
        minX = std::min(minX, corner.x);
        maxX = std::max(maxX, corner.x);
        minY = std::min(minY, corner.y);
        maxY = std::max(maxY, corner.y);
        minZ = std::min(minZ, corner.z);
        maxZ = std::max(maxZ, corner.z);
    }
    
    // Add some padding to reduce edge artifacts
    const float padding = 10.0f;
    minX -= padding;
    maxX += padding;
    minY -= padding;
    maxY += padding;
    
    // Extend Z range to capture more geometry
    minZ -= 50.0f;  // Extend behind to catch shadow casters
    
    return glm::ortho(minX, maxX, minY, maxY, minZ, maxZ);
}

void CascadedShadowMap::setCascadeInfo(int index, const glm::mat4& lightVP, float nearPlane, float farPlane)
{
    if (index >= 0 && index < MAX_CASCADES) {
        m_cascades[index].viewProjection = lightVP;
        m_cascades[index].nearPlane = nearPlane;
        m_cascades[index].farPlane = farPlane;
    }
}

void CascadedShadowMap::beginCascade(int index)
{
    if (index < 0 || index >= MAX_CASCADES) return;
    
    glBindFramebuffer(GL_FRAMEBUFFER, m_fbo[index]);
    glViewport(0, 0, m_cascades[index].resolution, m_cascades[index].resolution);
    glClear(GL_DEPTH_BUFFER_BIT);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_POLYGON_OFFSET_FILL);
    glPolygonOffset(2.0f, 4.0f);  // Bias to reduce shadow acne
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
