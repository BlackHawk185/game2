// BloomRenderer.cpp - Implementation of bloom post-processing
#include "BloomRenderer.h"
#include <glad/gl.h>
#include <iostream>

// Simple passthrough vertex shader for fullscreen quads
static const char* quadVertexShader = R"(
#version 460 core

layout(location = 0) in vec2 aPosition;
layout(location = 1) in vec2 aTexCoord;

out vec2 vTexCoord;

void main() {
    vTexCoord = aTexCoord;
    gl_Position = vec4(aPosition, 0.0, 1.0);
}
)";

// Extract bright pixels above threshold
static const char* brightnessFragmentShader = R"(
#version 460 core

in vec2 vTexCoord;

uniform sampler2D uSceneTexture;
uniform float uThreshold;

out vec4 FragColor;

void main() {
    vec3 color = texture(uSceneTexture, vTexCoord).rgb;
    
    // Calculate luminance
    float brightness = dot(color, vec3(0.2126, 0.7152, 0.0722));
    
    // Extract bright pixels
    if (brightness > uThreshold) {
        FragColor = vec4(color, 1.0);
    } else {
        FragColor = vec4(0.0, 0.0, 0.0, 1.0);
    }
}
)";

// Gaussian blur shader (separable filter for efficiency)
static const char* blurFragmentShader = R"(
#version 460 core

in vec2 vTexCoord;

uniform sampler2D uTexture;
uniform vec2 uBlurDirection; // (1, 0) for horizontal, (0, 1) for vertical

out vec4 FragColor;

// Gaussian weights for 9-tap blur
const float weights[5] = float[](0.227027, 0.1945946, 0.1216216, 0.054054, 0.016216);

void main() {
    vec2 texelSize = 1.0 / textureSize(uTexture, 0);
    vec3 result = texture(uTexture, vTexCoord).rgb * weights[0];
    
    for (int i = 1; i < 5; i++) {
        vec2 offset = uBlurDirection * texelSize * float(i);
        result += texture(uTexture, vTexCoord + offset).rgb * weights[i];
        result += texture(uTexture, vTexCoord - offset).rgb * weights[i];
    }
    
    FragColor = vec4(result, 1.0);
}
)";

// Composite bloom onto original scene
static const char* compositeFragmentShader = R"(
#version 460 core

in vec2 vTexCoord;

uniform sampler2D uSceneTexture;
uniform sampler2D uBloomTexture;
uniform float uBloomIntensity;

out vec4 FragColor;

void main() {
    vec3 sceneColor = texture(uSceneTexture, vTexCoord).rgb;
    vec3 bloomColor = texture(uBloomTexture, vTexCoord).rgb;
    
    // Additive blending with intensity control
    vec3 result = sceneColor + bloomColor * uBloomIntensity;
    
    FragColor = vec4(result, 1.0);
}
)";

BloomRenderer::BloomRenderer()
    : m_width(0)
    , m_height(0)
    , m_bloomIntensity(0.4f)
    , m_bloomThreshold(0.8f)
    , m_blurPasses(5)
    , m_brightnessFBO(0)
    , m_brightnessTexture(0)
    , m_compositeFBO(0)
    , m_compositeTexture(0)
    , m_brightnessShader(0)
    , m_blurShader(0)
    , m_compositeShader(0)
    , m_quadVAO(0)
    , m_quadVBO(0)
{
    m_blurFBO[0] = m_blurFBO[1] = 0;
    m_blurTexture[0] = m_blurTexture[1] = 0;
}

BloomRenderer::~BloomRenderer() {
    shutdown();
}

bool BloomRenderer::initialize(int width, int height) {
    m_width = width;
    m_height = height;
    
    createQuad();
    createShaders();
    createFramebuffers();
    
    std::cout << "BloomRenderer: Initialized (" << width << "x" << height << ")" << std::endl;
    return true;
}

void BloomRenderer::shutdown() {
    // Delete framebuffers
    if (m_brightnessFBO) glDeleteFramebuffers(1, &m_brightnessFBO);
    if (m_brightnessTexture) glDeleteTextures(1, &m_brightnessTexture);
    if (m_compositeFBO) glDeleteFramebuffers(1, &m_compositeFBO);
    if (m_compositeTexture) glDeleteTextures(1, &m_compositeTexture);
    
    for (int i = 0; i < 2; i++) {
        if (m_blurFBO[i]) glDeleteFramebuffers(1, &m_blurFBO[i]);
        if (m_blurTexture[i]) glDeleteTextures(1, &m_blurTexture[i]);
    }
    
    // Delete shaders
    if (m_brightnessShader) glDeleteProgram(m_brightnessShader);
    if (m_blurShader) glDeleteProgram(m_blurShader);
    if (m_compositeShader) glDeleteProgram(m_compositeShader);
    
    // Delete quad
    if (m_quadVBO) glDeleteBuffers(1, &m_quadVBO);
    if (m_quadVAO) glDeleteVertexArrays(1, &m_quadVAO);
}

void BloomRenderer::resize(int width, int height) {
    m_width = width;
    m_height = height;
    
    // Recreate framebuffers with new size
    if (m_brightnessTexture) glDeleteTextures(1, &m_brightnessTexture);
    if (m_compositeTexture) glDeleteTextures(1, &m_compositeTexture);
    for (int i = 0; i < 2; i++) {
        if (m_blurTexture[i]) glDeleteTextures(1, &m_blurTexture[i]);
    }
    
    createFramebuffers();
}

void BloomRenderer::createQuad() {
    // Fullscreen quad vertices
    float quadVertices[] = {
        // positions   // texCoords
        -1.0f,  1.0f,  0.0f, 1.0f,
        -1.0f, -1.0f,  0.0f, 0.0f,
         1.0f, -1.0f,  1.0f, 0.0f,
        
        -1.0f,  1.0f,  0.0f, 1.0f,
         1.0f, -1.0f,  1.0f, 0.0f,
         1.0f,  1.0f,  1.0f, 1.0f
    };
    
    glGenVertexArrays(1, &m_quadVAO);
    glGenBuffers(1, &m_quadVBO);
    
    glBindVertexArray(m_quadVAO);
    glBindBuffer(GL_ARRAY_BUFFER, m_quadVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), quadVertices, GL_STATIC_DRAW);
    
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
    
    glBindVertexArray(0);
}

void BloomRenderer::createShaders() {
    // Compile vertex shader (shared by all passes)
    GLuint vertShader = compileShader(quadVertexShader, GL_VERTEX_SHADER);
    
    // Brightness extraction shader
    GLuint brightnessFrag = compileShader(brightnessFragmentShader, GL_FRAGMENT_SHADER);
    m_brightnessShader = linkProgram(vertShader, brightnessFrag);
    glDeleteShader(brightnessFrag);
    
    // Blur shader
    GLuint blurFrag = compileShader(blurFragmentShader, GL_FRAGMENT_SHADER);
    m_blurShader = linkProgram(vertShader, blurFrag);
    glDeleteShader(blurFrag);
    
    // Composite shader
    GLuint compositeFrag = compileShader(compositeFragmentShader, GL_FRAGMENT_SHADER);
    m_compositeShader = linkProgram(vertShader, compositeFrag);
    glDeleteShader(compositeFrag);
    
    glDeleteShader(vertShader);
}

void BloomRenderer::createFramebuffers() {
    // Use half resolution for bloom to save performance
    int bloomWidth = m_width / 2;
    int bloomHeight = m_height / 2;
    
    // Brightness extraction FBO
    glGenFramebuffers(1, &m_brightnessFBO);
    glGenTextures(1, &m_brightnessTexture);
    
    glBindTexture(GL_TEXTURE_2D, m_brightnessTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, bloomWidth, bloomHeight, 0, GL_RGBA, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    
    glBindFramebuffer(GL_FRAMEBUFFER, m_brightnessFBO);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_brightnessTexture, 0);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    
    // Blur ping-pong FBOs
    for (int i = 0; i < 2; i++) {
        glGenFramebuffers(1, &m_blurFBO[i]);
        glGenTextures(1, &m_blurTexture[i]);
        
        glBindTexture(GL_TEXTURE_2D, m_blurTexture[i]);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, bloomWidth, bloomHeight, 0, GL_RGBA, GL_FLOAT, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        
        glBindFramebuffer(GL_FRAMEBUFFER, m_blurFBO[i]);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_blurTexture[i], 0);
    }
    
    // Composite FBO
    glGenFramebuffers(1, &m_compositeFBO);
    glGenTextures(1, &m_compositeTexture);
    
    glBindTexture(GL_TEXTURE_2D, m_compositeTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, m_width, m_height, 0, GL_RGBA, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    
    glBindFramebuffer(GL_FRAMEBUFFER, m_compositeFBO);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_compositeTexture, 0);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

GLuint BloomRenderer::applyBloom(GLuint sceneTexture) {
    // Step 1: Extract bright pixels
    extractBrightness(sceneTexture);
    
    // Step 2: Blur the bright pixels
    blurBrightness();
    
    // Step 3: Composite bloom back onto scene
    composite(sceneTexture);
    
    return m_compositeTexture;
}

void BloomRenderer::extractBrightness(GLuint sceneTexture) {
    glBindFramebuffer(GL_FRAMEBUFFER, m_brightnessFBO);
    glViewport(0, 0, m_width / 2, m_height / 2);
    glClear(GL_COLOR_BUFFER_BIT);
    
    glUseProgram(m_brightnessShader);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, sceneTexture);
    glUniform1i(glGetUniformLocation(m_brightnessShader, "uSceneTexture"), 0);
    glUniform1f(glGetUniformLocation(m_brightnessShader, "uThreshold"), m_bloomThreshold);
    
    glBindVertexArray(m_quadVAO);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);
    
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void BloomRenderer::blurBrightness() {
    glUseProgram(m_blurShader);
    
    bool horizontal = true;
    GLuint sourceTexture = m_brightnessTexture;
    
    for (int i = 0; i < m_blurPasses * 2; i++) {
        glBindFramebuffer(GL_FRAMEBUFFER, m_blurFBO[horizontal ? 0 : 1]);
        glViewport(0, 0, m_width / 2, m_height / 2);
        
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, sourceTexture);
        glUniform1i(glGetUniformLocation(m_blurShader, "uTexture"), 0);
        glUniform2f(glGetUniformLocation(m_blurShader, "uBlurDirection"),
                   horizontal ? 1.0f : 0.0f,
                   horizontal ? 0.0f : 1.0f);
        
        glBindVertexArray(m_quadVAO);
        glDrawArrays(GL_TRIANGLES, 0, 6);
        glBindVertexArray(0);
        
        sourceTexture = m_blurTexture[horizontal ? 0 : 1];
        horizontal = !horizontal;
    }
    
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void BloomRenderer::composite(GLuint sceneTexture) {
    glBindFramebuffer(GL_FRAMEBUFFER, m_compositeFBO);
    glViewport(0, 0, m_width, m_height);
    glClear(GL_COLOR_BUFFER_BIT);
    
    glUseProgram(m_compositeShader);
    
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, sceneTexture);
    glUniform1i(glGetUniformLocation(m_compositeShader, "uSceneTexture"), 0);
    
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, m_blurTexture[0]); // Final blurred result
    glUniform1i(glGetUniformLocation(m_compositeShader, "uBloomTexture"), 1);
    
    glUniform1f(glGetUniformLocation(m_compositeShader, "uBloomIntensity"), m_bloomIntensity);
    
    glBindVertexArray(m_quadVAO);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);
    
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

GLuint BloomRenderer::compileShader(const char* source, unsigned int type) {
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);
    
    int success;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        char infoLog[512];
        glGetShaderInfoLog(shader, 512, nullptr, infoLog);
        std::cerr << "BloomRenderer: Shader compilation failed: " << infoLog << std::endl;
        return 0;
    }
    
    return shader;
}

GLuint BloomRenderer::linkProgram(GLuint vertexShader, GLuint fragmentShader) {
    GLuint program = glCreateProgram();
    glAttachShader(program, vertexShader);
    glAttachShader(program, fragmentShader);
    glLinkProgram(program);
    
    int success;
    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if (!success) {
        char infoLog[512];
        glGetProgramInfoLog(program, 512, nullptr, infoLog);
        std::cerr << "BloomRenderer: Program linking failed: " << infoLog << std::endl;
        return 0;
    }
    
    return program;
}
