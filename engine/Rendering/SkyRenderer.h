// SkyRenderer.h - Procedural skybox rendering with day/night cycle
#pragma once

#include "../Math/Vec3.h"

typedef unsigned int GLuint;

class DayNightController;

/**
 * Renders a procedural skybox with atmospheric gradient, sun, and moon.
 * Replaces the solid clear color with a dynamic sky that changes with time of day.
 */
class SkyRenderer {
public:
    SkyRenderer();
    ~SkyRenderer();

    // Initialize OpenGL resources
    bool initialize();
    void shutdown();
    
    // Render the sky (call before rendering world geometry)
    // Requires view and projection matrices
    void render(const float* viewMatrix, const float* projectionMatrix, 
                const DayNightController& dayNight);
    
    // Configuration
    void setSunSize(float size) { m_sunSize = size; }
    void setMoonSize(float size) { m_moonSize = size; }
    void setStarIntensity(float intensity) { m_starIntensity = intensity; }

private:
    // OpenGL resources
    GLuint m_shaderProgram;
    GLuint m_vao;
    GLuint m_vbo;
    
    // Shader uniforms
    GLuint m_uniformView;
    GLuint m_uniformProjection;
    GLuint m_uniformSunDir;
    GLuint m_uniformMoonDir;
    GLuint m_uniformSunColor;
    GLuint m_uniformMoonColor;
    GLuint m_uniformZenithColor;
    GLuint m_uniformHorizonColor;
    GLuint m_uniformSunSize;
    GLuint m_uniformMoonSize;
    GLuint m_uniformStarIntensity;
    
    // Configuration
    float m_sunSize;
    float m_moonSize;
    float m_starIntensity;
    
    // Shader creation
    bool createShaders();
    GLuint compileShader(const char* source, unsigned int type);
    GLuint linkProgram(GLuint vertexShader, GLuint fragmentShader);
    
    // Fullscreen quad geometry
    void createSkyQuad();
};
