#pragma once

#include "../Math/Vec3.h"

/**
 * Simple procedural sun renderer - draws a yellow circle in the sky
 */
class SkyRenderer {
public:
    SkyRenderer();
    ~SkyRenderer() = default;

    // Initialize the sky renderer
    bool initialize();
    
    // Render the sky background and sun
    void renderSky(const Vec3& cameraPos, const Vec3& cameraForward, float fov, float aspect);
    
    // Sun configuration
    void setSunPosition(const Vec3& position) { m_sunPosition = position; }
    void setSunSize(float size) { m_sunSize = size; }
    void setSunColor(float r, float g, float b) { m_sunColor = Vec3(r, g, b); }
    
    // Sky configuration  
    void setSkyColor(float r, float g, float b) { m_skyColor = Vec3(r, g, b); }
    
    void cleanup();

private:
    // Sun properties
    Vec3 m_sunPosition{0.0f, 1000.0f, 0.0f};  // Sun position in world space
    float m_sunSize = 50.0f;                   // Visual size of the sun
    Vec3 m_sunColor{1.0f, 1.0f, 0.0f};        // Yellow sun
    
    // Sky properties
    Vec3 m_skyColor{0.5f, 0.8f, 1.0f};        // Light blue sky
    
    // Rendering helpers
    void drawSun(const Vec3& sunScreenPos, float screenSize);
    Vec3 worldToScreen(const Vec3& worldPos, const Vec3& cameraPos, const Vec3& cameraForward, float fov, float aspect);
    bool isSunVisible(const Vec3& sunDirection, const Vec3& cameraForward);
};
