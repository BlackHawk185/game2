// SkyRenderer.cpp - Implementation of procedural skybox
#include "SkyRenderer.h"
#include "../Time/DayNightController.h"
#include <glad/gl.h>
#include <iostream>
#include <cmath>

// Skybox vertex shader - renders a fullscreen quad
static const char* skyVertexShader = R"(
#version 460 core

layout(location = 0) in vec3 aPosition;

uniform mat4 uView;
uniform mat4 uProjection;

out vec3 vViewDir;
out vec3 vPosition;

void main() {
    // Remove translation from view matrix (keep only rotation)
    mat4 rotView = mat4(mat3(uView));
    
    vec4 pos = uProjection * rotView * vec4(aPosition, 1.0);
    gl_Position = pos.xyww; // Set z = w for maximum depth
    
    vViewDir = aPosition;
    vPosition = aPosition;
}
)";

// Skybox fragment shader - procedural sky with sun and moon
static const char* skyFragmentShader = R"(
#version 460 core

in vec3 vViewDir;
in vec3 vPosition;

uniform vec3 uSunDir;
uniform vec3 uMoonDir;
uniform vec3 uSunColor;
uniform vec3 uMoonColor;
uniform vec3 uZenithColor;
uniform vec3 uHorizonColor;
uniform float uSunSize;
uniform float uMoonSize;
uniform float uStarIntensity;

out vec4 FragColor;

// Simple hash function for pseudo-random stars
float hash(vec2 p) {
    return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453123);
}

void main() {
    vec3 viewDir = normalize(vViewDir);
    
    // Sky gradient from horizon to zenith
    float heightFactor = max(0.0, viewDir.y); // 0 at horizon, 1 at zenith
    heightFactor = pow(heightFactor, 0.6); // Adjust gradient curve
    
    vec3 skyColor = mix(uHorizonColor, uZenithColor, heightFactor);
    
    // Add sun
    float sunDot = dot(viewDir, normalize(-uSunDir));
    float sunDisc = smoothstep(1.0 - uSunSize * 0.01, 1.0 - uSunSize * 0.005, sunDot);
    float sunGlow = pow(max(0.0, sunDot), 8.0) * 0.3;
    
    // Blend sun into sky
    skyColor = mix(skyColor, uSunColor, sunDisc);
    skyColor += uSunColor * sunGlow;
    
    // Add moon
    float moonDot = dot(viewDir, normalize(-uMoonDir));
    float moonDisc = smoothstep(1.0 - uMoonSize * 0.008, 1.0 - uMoonSize * 0.004, moonDot);
    skyColor = mix(skyColor, uMoonColor, moonDisc);
    
    // Add stars (visible at night)
    if (uStarIntensity > 0.01) {
        // Create star field using hash function
        vec2 starCoord = vPosition.xz / abs(vPosition.y + 0.1) * 10.0;
        float starField = hash(floor(starCoord));
        
        // Only show bright stars
        if (starField > 0.995) {
            float starBrightness = (starField - 0.995) / 0.005;
            starBrightness *= uStarIntensity;
            skyColor += vec3(starBrightness);
        }
    }
    
    FragColor = vec4(skyColor, 1.0);
}
)";

SkyRenderer::SkyRenderer()
    : m_shaderProgram(0)
    , m_vao(0)
    , m_vbo(0)
    , m_sunSize(2.0f)
    , m_moonSize(1.5f)
    , m_starIntensity(0.0f)
{
}

SkyRenderer::~SkyRenderer() {
    shutdown();
}

bool SkyRenderer::initialize() {
    // Create shader program
    if (!createShaders()) {
        std::cerr << "SkyRenderer: Failed to create shaders" << std::endl;
        return false;
    }
    
    // Create sky quad geometry
    createSkyQuad();
    
    std::cout << "SkyRenderer: Initialized successfully" << std::endl;
    return true;
}

void SkyRenderer::shutdown() {
    if (m_vbo != 0) {
        glDeleteBuffers(1, &m_vbo);
        m_vbo = 0;
    }
    if (m_vao != 0) {
        glDeleteVertexArrays(1, &m_vao);
        m_vao = 0;
    }
    if (m_shaderProgram != 0) {
        glDeleteProgram(m_shaderProgram);
        m_shaderProgram = 0;
    }
}

void SkyRenderer::createSkyQuad() {
    // Create a large cube that will be rendered as a skybox
    float vertices[] = {
        // Positions
        -1.0f,  1.0f, -1.0f,
        -1.0f, -1.0f, -1.0f,
         1.0f, -1.0f, -1.0f,
         1.0f, -1.0f, -1.0f,
         1.0f,  1.0f, -1.0f,
        -1.0f,  1.0f, -1.0f,
        
        -1.0f, -1.0f,  1.0f,
        -1.0f, -1.0f, -1.0f,
        -1.0f,  1.0f, -1.0f,
        -1.0f,  1.0f, -1.0f,
        -1.0f,  1.0f,  1.0f,
        -1.0f, -1.0f,  1.0f,
        
         1.0f, -1.0f, -1.0f,
         1.0f, -1.0f,  1.0f,
         1.0f,  1.0f,  1.0f,
         1.0f,  1.0f,  1.0f,
         1.0f,  1.0f, -1.0f,
         1.0f, -1.0f, -1.0f,
        
        -1.0f, -1.0f,  1.0f,
        -1.0f,  1.0f,  1.0f,
         1.0f,  1.0f,  1.0f,
         1.0f,  1.0f,  1.0f,
         1.0f, -1.0f,  1.0f,
        -1.0f, -1.0f,  1.0f,
        
        -1.0f,  1.0f, -1.0f,
         1.0f,  1.0f, -1.0f,
         1.0f,  1.0f,  1.0f,
         1.0f,  1.0f,  1.0f,
        -1.0f,  1.0f,  1.0f,
        -1.0f,  1.0f, -1.0f,
        
        -1.0f, -1.0f, -1.0f,
        -1.0f, -1.0f,  1.0f,
         1.0f, -1.0f, -1.0f,
         1.0f, -1.0f, -1.0f,
        -1.0f, -1.0f,  1.0f,
         1.0f, -1.0f,  1.0f
    };
    
    glGenVertexArrays(1, &m_vao);
    glGenBuffers(1, &m_vbo);
    
    glBindVertexArray(m_vao);
    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    
    // Position attribute
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    
    glBindVertexArray(0);
}

bool SkyRenderer::createShaders() {
    GLuint vertShader = compileShader(skyVertexShader, GL_VERTEX_SHADER);
    if (vertShader == 0) return false;
    
    GLuint fragShader = compileShader(skyFragmentShader, GL_FRAGMENT_SHADER);
    if (fragShader == 0) {
        glDeleteShader(vertShader);
        return false;
    }
    
    m_shaderProgram = linkProgram(vertShader, fragShader);
    
    glDeleteShader(vertShader);
    glDeleteShader(fragShader);
    
    if (m_shaderProgram == 0) return false;
    
    // Get uniform locations
    m_uniformView = glGetUniformLocation(m_shaderProgram, "uView");
    m_uniformProjection = glGetUniformLocation(m_shaderProgram, "uProjection");
    m_uniformSunDir = glGetUniformLocation(m_shaderProgram, "uSunDir");
    m_uniformMoonDir = glGetUniformLocation(m_shaderProgram, "uMoonDir");
    m_uniformSunColor = glGetUniformLocation(m_shaderProgram, "uSunColor");
    m_uniformMoonColor = glGetUniformLocation(m_shaderProgram, "uMoonColor");
    m_uniformZenithColor = glGetUniformLocation(m_shaderProgram, "uZenithColor");
    m_uniformHorizonColor = glGetUniformLocation(m_shaderProgram, "uHorizonColor");
    m_uniformSunSize = glGetUniformLocation(m_shaderProgram, "uSunSize");
    m_uniformMoonSize = glGetUniformLocation(m_shaderProgram, "uMoonSize");
    m_uniformStarIntensity = glGetUniformLocation(m_shaderProgram, "uStarIntensity");
    
    return true;
}

GLuint SkyRenderer::compileShader(const char* source, unsigned int type) {
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);
    
    int success;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        char infoLog[512];
        glGetShaderInfoLog(shader, 512, nullptr, infoLog);
        std::cerr << "SkyRenderer: Shader compilation failed: " << infoLog << std::endl;
        glDeleteShader(shader);
        return 0;
    }
    
    return shader;
}

GLuint SkyRenderer::linkProgram(GLuint vertexShader, GLuint fragmentShader) {
    GLuint program = glCreateProgram();
    glAttachShader(program, vertexShader);
    glAttachShader(program, fragmentShader);
    glLinkProgram(program);
    
    int success;
    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if (!success) {
        char infoLog[512];
        glGetProgramInfoLog(program, 512, nullptr, infoLog);
        std::cerr << "SkyRenderer: Program linking failed: " << infoLog << std::endl;
        glDeleteProgram(program);
        return 0;
    }
    
    return program;
}

void SkyRenderer::render(const float* viewMatrix, const float* projectionMatrix,
                        const DayNightController& dayNight) {
    // Disable depth writing (sky is always at max depth)
    glDepthMask(GL_FALSE);
    glDepthFunc(GL_LEQUAL);
    
    glUseProgram(m_shaderProgram);
    
    // Set matrices
    glUniformMatrix4fv(m_uniformView, 1, GL_FALSE, viewMatrix);
    glUniformMatrix4fv(m_uniformProjection, 1, GL_FALSE, projectionMatrix);
    
    // Get sky colors from day/night controller
    auto skyColors = dayNight.getSkyColors();
    
    // Set sun/moon directions
    Vec3 sunDir = dayNight.getSunDirection();
    Vec3 moonDir = dayNight.getMoonDirection();
    glUniform3f(m_uniformSunDir, sunDir.x, sunDir.y, sunDir.z);
    glUniform3f(m_uniformMoonDir, moonDir.x, moonDir.y, moonDir.z);
    
    // Set colors
    glUniform3f(m_uniformSunColor, skyColors.sunColor.x, skyColors.sunColor.y, skyColors.sunColor.z);
    glUniform3f(m_uniformMoonColor, skyColors.moonColor.x, skyColors.moonColor.y, skyColors.moonColor.z);
    glUniform3f(m_uniformZenithColor, skyColors.zenith.x, skyColors.zenith.y, skyColors.zenith.z);
    glUniform3f(m_uniformHorizonColor, skyColors.horizon.x, skyColors.horizon.y, skyColors.horizon.z);
    
    // Calculate star intensity (visible at night)
    float sunIntensity = dayNight.getSunIntensity();
    m_starIntensity = std::max(0.0f, 1.0f - sunIntensity * 4.0f);
    
    // Set configuration
    glUniform1f(m_uniformSunSize, m_sunSize);
    glUniform1f(m_uniformMoonSize, m_moonSize);
    glUniform1f(m_uniformStarIntensity, m_starIntensity);
    
    // Render skybox
    glBindVertexArray(m_vao);
    glDrawArrays(GL_TRIANGLES, 0, 36);
    glBindVertexArray(0);
    
    glUseProgram(0);
    
    // Restore depth state
    glDepthMask(GL_TRUE);
    glDepthFunc(GL_LESS);
}
