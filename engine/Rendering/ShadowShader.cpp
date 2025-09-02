#include "ShadowShader.h"
#include <iostream>
#include <string>

// Simple vertex shader for shadow mapping (depth only)
static const char* SHADOW_VERTEX_SHADER_SOURCE = R"(
#version 460 core

layout (location = 0) in vec3 aPos;

uniform mat4 uLightSpaceMatrix;
uniform mat4 uModel;

void main()
{
    gl_Position = uLightSpaceMatrix * uModel * vec4(aPos, 1.0);
}
)";

// Simple fragment shader for shadow mapping (depth only)
static const char* SHADOW_FRAGMENT_SHADER_SOURCE = R"(
#version 460 core

void main()
{
    // Fragment depth is automatically written to depth buffer
    // No need to output anything
}
)";

ShadowShader::ShadowShader()
    : m_program(0)
    , m_vertexShader(0)
    , m_fragmentShader(0)
    , m_lightSpaceMatrixLoc(-1)
    , m_modelMatrixLoc(-1)
    , m_initialized(false)
{
}

ShadowShader::~ShadowShader()
{
    cleanup();
}

bool ShadowShader::initialize()
{
    if (m_initialized) {
        return true;
    }

    // Create vertex shader
    m_vertexShader = glCreateShader(GL_VERTEX_SHADER);
    if (!compileShader(m_vertexShader, SHADOW_VERTEX_SHADER_SOURCE)) {
        std::cerr << "Failed to compile shadow vertex shader" << std::endl;
        return false;
    }

    // Create fragment shader
    m_fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    if (!compileShader(m_fragmentShader, SHADOW_FRAGMENT_SHADER_SOURCE)) {
        std::cerr << "Failed to compile shadow fragment shader" << std::endl;
        return false;
    }

    // Create program
    m_program = glCreateProgram();
    glAttachShader(m_program, m_vertexShader);
    glAttachShader(m_program, m_fragmentShader);
    glLinkProgram(m_program);

    // Check linking
    GLint success;
    glGetProgramiv(m_program, GL_LINK_STATUS, &success);
    if (!success) {
        char infoLog[512];
        glGetProgramInfoLog(m_program, 512, NULL, infoLog);
        std::cerr << "Shadow shader program linking failed: " << infoLog << std::endl;
        cleanup();
        return false;
    }

    // Get uniform locations
    m_lightSpaceMatrixLoc = glGetUniformLocation(m_program, "uLightSpaceMatrix");
    m_modelMatrixLoc = glGetUniformLocation(m_program, "uModel");

    if (m_lightSpaceMatrixLoc == -1 || m_modelMatrixLoc == -1) {
        std::cerr << "Failed to get shadow shader uniform locations" << std::endl;
        cleanup();
        return false;
    }

    m_initialized = true;
    std::cout << "Shadow shader initialized successfully" << std::endl;
    return true;
}

void ShadowShader::cleanup()
{
    if (m_program != 0) {
        glDeleteProgram(m_program);
        m_program = 0;
    }
    
    if (m_vertexShader != 0) {
        glDeleteShader(m_vertexShader);
        m_vertexShader = 0;
    }
    
    if (m_fragmentShader != 0) {
        glDeleteShader(m_fragmentShader);
        m_fragmentShader = 0;
    }
    
    m_initialized = false;
}

void ShadowShader::use()
{
    if (m_initialized) {
        glUseProgram(m_program);
    }
}

void ShadowShader::setLightSpaceMatrix(const glm::mat4& lightSpaceMatrix)
{
    if (m_initialized && m_lightSpaceMatrixLoc != -1) {
        glUniformMatrix4fv(m_lightSpaceMatrixLoc, 1, GL_FALSE, glm::value_ptr(lightSpaceMatrix));
    }
}

void ShadowShader::setModelMatrix(const glm::mat4& model)
{
    if (m_initialized && m_modelMatrixLoc != -1) {
        glUniformMatrix4fv(m_modelMatrixLoc, 1, GL_FALSE, glm::value_ptr(model));
    }
}

bool ShadowShader::compileShader(GLuint shader, const char* source)
{
    glShaderSource(shader, 1, &source, NULL);
    glCompileShader(shader);

    GLint success;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        char infoLog[512];
        glGetShaderInfoLog(shader, 512, NULL, infoLog);
        std::cerr << "Shader compilation failed: " << infoLog << std::endl;
        return false;
    }
    
    return true;
}
