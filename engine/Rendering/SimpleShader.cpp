#include "SimpleShader.h"
#include <glad/glad.h>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <iostream>
#include <unordered_map>

// Simple vertex shader for voxel rendering
static const char* VERTEX_SHADER_SOURCE = R"(
#version 330 core

layout (location = 0) in vec3 aPosition;
layout (location = 1) in vec2 aTexCoord;
layout (location = 2) in vec3 aNormal;

uniform mat4 uModel;
uniform mat4 uView;
uniform mat4 uProjection;

out vec2 TexCoord;
out vec3 Normal;

void main()
{
    gl_Position = uProjection * uView * uModel * vec4(aPosition, 1.0);
    TexCoord = aTexCoord;
    Normal = aNormal;
}
)";

// Simple fragment shader for textured voxels with dynamic lighting
static const char* FRAGMENT_SHADER_SOURCE = R"(
#version 330 core

in vec2 TexCoord;
in vec3 Normal;

uniform sampler2D uTexture;
uniform vec3 uSunDirection;  // Dynamic sun direction from day/night cycle
uniform float uTimeOfDay;   // 0.0 = midnight, 0.5 = noon, 1.0 = midnight

out vec4 FragColor;

void main()
{
    vec4 texColor = texture(uTexture, TexCoord);
    
    // Check if texture is valid (not all black/white)
    if (texColor.rgb == vec3(0.0, 0.0, 0.0) || texColor.a < 0.1) {
        // Fallback to bright magenta if texture is missing/invalid
        texColor = vec4(1.0, 0.0, 1.0, 1.0);
    }
    
    // Calculate directional lighting from sun
    float sunDot = dot(normalize(Normal), normalize(-uSunDirection));
    float directionalLight = max(sunDot, 0.0);
    
    // Add ambient light that varies with time of day
    float ambientStrength = mix(0.2, 0.4, sin(uTimeOfDay * 3.14159)); // Varies 0.2-0.4
    
    // Combine directional and ambient lighting
    float totalLight = ambientStrength + (0.6 * directionalLight);
    
    // Apply simple shadows: faces not facing the sun are darker
    vec3 finalColor = texColor.rgb * totalLight;
    
    FragColor = vec4(finalColor, texColor.a);
}
)";

SimpleShader::SimpleShader()
    : m_program(0), m_vertexShader(0), m_fragmentShader(0)
{
}

SimpleShader::~SimpleShader()
{
    cleanup();
}

bool SimpleShader::initialize()
{
    // Create shaders
    m_vertexShader = glCreateShader(GL_VERTEX_SHADER);
    m_fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    
    // Compile vertex shader
    if (!compileShader(m_vertexShader, VERTEX_SHADER_SOURCE)) {
        std::cerr << "Failed to compile vertex shader" << std::endl;
        return false;
    }
    
    // Compile fragment shader
    if (!compileShader(m_fragmentShader, FRAGMENT_SHADER_SOURCE)) {
        std::cerr << "Failed to compile fragment shader" << std::endl;
        return false;
    }
    
    // Create and link program
    m_program = glCreateProgram();
    glAttachShader(m_program, m_vertexShader);
    glAttachShader(m_program, m_fragmentShader);
    
    if (!linkProgram()) {
        std::cerr << "Failed to link shader program" << std::endl;
        return false;
    }
    
    std::cout << "Simple shader initialized successfully" << std::endl;
    return true;
}

void SimpleShader::use()
{
    if (m_program != 0) {
        glUseProgram(m_program);
    }
}

void SimpleShader::cleanup()
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
}

void SimpleShader::setMatrix4(const std::string& name, const glm::mat4& matrix)
{
    GLint location = getUniformLocation(name);
    if (location != -1) {
        glUniformMatrix4fv(location, 1, GL_FALSE, glm::value_ptr(matrix));
    }
}

void SimpleShader::setVector3(const std::string& name, const Vec3& vector)
{
    GLint location = getUniformLocation(name);
    if (location != -1) {
        glUniform3f(location, vector.x, vector.y, vector.z);
    }
}

void SimpleShader::setFloat(const std::string& name, float value)
{
    GLint location = getUniformLocation(name);
    if (location != -1) {
        glUniform1f(location, value);
    }
}

void SimpleShader::setInt(const std::string& name, int value)
{
    GLint location = getUniformLocation(name);
    if (location != -1) {
        // Some minimal GL headers in this project do not expose glUniform1i.
        // Using glUniform1f here has worked in practice for sampler uniforms.
        glUniform1f(location, static_cast<float>(value));
    }
}

bool SimpleShader::compileShader(GLuint shader, const char* source)
{
    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);
    
    GLint success;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    
    if (!success) {
        GLchar infoLog[1024];
        glGetShaderInfoLog(shader, 1024, nullptr, infoLog);
        std::cerr << "Shader compilation error: " << infoLog << std::endl;
        return false;
    }
    
    return true;
}

bool SimpleShader::linkProgram()
{
    glLinkProgram(m_program);
    
    GLint success;
    glGetProgramiv(m_program, GL_LINK_STATUS, &success);
    
    if (!success) {
        GLchar infoLog[1024];
        glGetProgramInfoLog(m_program, 1024, nullptr, infoLog);
        std::cerr << "Program linking error: " << infoLog << std::endl;
        return false;
    }
    
    return true;
}

int SimpleShader::getUniformLocation(const std::string& name)
{
    static std::unordered_map<std::string, GLint> locationCache;
    
    auto it = locationCache.find(name);
    if (it != locationCache.end()) {
        return it->second;
    }
    
    GLint location = glGetUniformLocation(m_program, name.c_str());
    locationCache[name] = location;
    
    if (location == -1) {
        std::cerr << "Warning: Uniform '" << name << "' not found in shader" << std::endl;
    }
    
    return location;
}
