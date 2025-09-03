#include "SimpleShader.h"
#include <glad/gl.h>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <iostream>
#include <unordered_map>

// Enhanced vertex shader for voxel rendering with light mapping and UBO
static const char* VERTEX_SHADER_SOURCE = R"(
#version 460 core

layout (location = 0) in vec3 aPosition;
layout (location = 1) in vec2 aTexCoord;
layout (location = 2) in vec3 aNormal;
layout (location = 3) in vec2 aLightMapUV;  // Light map texture coordinates
layout (location = 4) in float aAmbientOcclusion;  // Per-vertex ambient occlusion
layout (location = 5) in float aFaceIndex;  // Face index for per-face light maps

// UBO for batch rendering multiple chunks with different lighting
layout (std140, binding = 0) uniform ChunkLightingData {
    mat4 uChunkTransforms[64];    // Transform matrices for up to 64 chunks
    vec4 uChunkLightColors[64];   // Per-chunk light tinting (rgb + intensity)
    vec4 uChunkAmbientData[64];   // Per-chunk ambient (rgb + occlusion strength)
    vec2 uChunkLightMapOffsets[64]; // UV offsets for light map atlasing
    int uNumChunks;               // Number of active chunks
};

uniform mat4 uModel;
uniform mat4 uView;
uniform mat4 uProjection;
uniform int uChunkIndex;  // Which chunk this vertex belongs to

out vec2 TexCoord;
out vec3 Normal;
out vec2 LightMapUV;
out float AmbientOcclusion;
out float FaceIndex;             // Pass face index for per-face light map selection
out vec4 ChunkLightColor;     // Pass chunk-specific lighting data
out vec4 ChunkAmbientData;    // Pass chunk-specific ambient data

void main()
{
    // Use chunk-specific transform if available, otherwise use uniform transform
    mat4 finalTransform = (uChunkIndex >= 0 && uChunkIndex < uNumChunks) ? 
                         uChunkTransforms[uChunkIndex] : uModel;
    
    gl_Position = uProjection * uView * finalTransform * vec4(aPosition, 1.0);
    TexCoord = aTexCoord;
    Normal = aNormal;
    
    // Apply chunk-specific light map UV offset for atlasing
    vec2 uvOffset = (uChunkIndex >= 0 && uChunkIndex < uNumChunks) ? 
                   uChunkLightMapOffsets[uChunkIndex] : vec2(0.0);
    LightMapUV = aLightMapUV + uvOffset;
    
    AmbientOcclusion = aAmbientOcclusion;
    FaceIndex = aFaceIndex;  // Pass face index to fragment shader
    
    // Pass chunk-specific lighting data to fragment shader
    ChunkLightColor = (uChunkIndex >= 0 && uChunkIndex < uNumChunks) ? 
                     uChunkLightColors[uChunkIndex] : vec4(1.0, 1.0, 1.0, 1.0);
    ChunkAmbientData = (uChunkIndex >= 0 && uChunkIndex < uNumChunks) ? 
                      uChunkAmbientData[uChunkIndex] : vec4(0.3, 0.3, 0.3, 1.0);
}
)";

// Enhanced fragment shader with per-face light maps and UBO support
static const char* FRAGMENT_SHADER_SOURCE = R"(
#version 460 core

in vec2 TexCoord;
in vec3 Normal;
in vec2 LightMapUV;
in float AmbientOcclusion;
in float FaceIndex;             // Face index for per-face light map selection
in vec4 ChunkLightColor;
in vec4 ChunkAmbientData;

uniform sampler2D uTexture;
uniform sampler2D uLightMapFace0;   // Light map texture for +X face
uniform sampler2D uLightMapFace1;   // Light map texture for -X face
uniform sampler2D uLightMapFace2;   // Light map texture for +Y face
uniform sampler2D uLightMapFace3;   // Light map texture for -Y face
uniform sampler2D uLightMapFace4;   // Light map texture for +Z face
uniform sampler2D uLightMapFace5;   // Light map texture for -Z face
uniform vec3 uSunDirection;         // Dynamic sun direction from day/night cycle
uniform float uTimeOfDay;          // 0.0 = midnight, 0.5 = noon, 1.0 = midnight
uniform float uLightMapStrength;   // Strength of light map influence [0.0-1.0]

// Material uniforms for different object types
uniform vec4 uMaterialColor;       // Diffuse color with alpha (for fluid particles, UI, etc.)
uniform int uMaterialType;         // 0=voxel, 1=fluid, 2=ui
uniform float uMaterialRoughness;  // Surface roughness
uniform vec3 uMaterialEmissive;    // Emissive color

out vec4 FragColor;

void main()
{
    vec4 finalColor;
    
    // Branch based on material type for optimal performance
    if (uMaterialType == 1) {
        // Fluid Material - simple color with transparency
        finalColor = uMaterialColor;
        
        // Add simple lighting for fluids
        float sunDot = max(dot(normalize(Normal), normalize(-uSunDirection)), 0.0);
        finalColor.rgb *= (0.3 + 0.7 * sunDot); // Ambient + directional
        
    } else if (uMaterialType == 2) {
        // UI Material - no lighting, just color/texture
        vec4 texColor = texture(uTexture, TexCoord);
        finalColor = texColor * uMaterialColor;
        
    } else {
        // Voxel Material (default) - full lightmap + texture system
        vec4 texColor = texture(uTexture, TexCoord);
        
        // Check if texture is valid (not all black/white)
        if (texColor.rgb == vec3(0.0, 0.0, 0.0) || texColor.a < 0.1) {
            // Fallback to bright magenta if texture is missing/invalid
            texColor = vec4(1.0, 0.0, 1.0, 1.0);
        }
        
        // Sample the appropriate light map based on face index
        vec3 lightMapColor;
        int face = int(FaceIndex + 0.5); // Round to nearest integer
        
        if (face == 0) {
            lightMapColor = texture(uLightMapFace0, LightMapUV).rgb;
        } else if (face == 1) {
            lightMapColor = texture(uLightMapFace1, LightMapUV).rgb;
        } else if (face == 2) {
            lightMapColor = texture(uLightMapFace2, LightMapUV).rgb;
        } else if (face == 3) {
            lightMapColor = texture(uLightMapFace3, LightMapUV).rgb;
        } else if (face == 4) {
            lightMapColor = texture(uLightMapFace4, LightMapUV).rgb;
        } else if (face == 5) {
            lightMapColor = texture(uLightMapFace5, LightMapUV).rgb;
        } else {
            // Fallback for invalid face index
            lightMapColor = vec3(1.0, 0.0, 1.0); // Bright magenta for debugging
        }
        
        // Calculate traditional directional lighting from sun (as backup/blend)
        float sunDot = dot(normalize(Normal), normalize(-uSunDirection));
        sunDot = max(sunDot, 0.0);
        float directionalLight = sunDot * 0.8;
        
        // Use chunk-specific ambient data
        float ambientStrength = mix(ChunkAmbientData.a * 0.2, ChunkAmbientData.a * 0.4, 
                                   sin(uTimeOfDay * 3.14159)); // Varies based on chunk data
        
        // Traditional lighting with chunk-specific color tinting
        float traditionalLight = ambientStrength + (0.6 * directionalLight);
        
        // Blend light map with traditional lighting
        float finalLightIntensity = mix(traditionalLight, length(lightMapColor), uLightMapStrength);
        
        // Apply ambient occlusion with chunk-specific strength
        finalLightIntensity *= mix(1.0, AmbientOcclusion, ChunkAmbientData.a);
        
        // Apply chunk-specific light color tinting
        vec3 chunkTintedLight = ChunkLightColor.rgb * ChunkLightColor.a * finalLightIntensity;
        
        // Apply lighting to texture
        vec3 baseColor = texColor.rgb * chunkTintedLight;
        
        // Add light map color tinting for colored lighting effects with chunk influence
        baseColor = mix(baseColor, baseColor * lightMapColor * ChunkLightColor.rgb, 
                        uLightMapStrength * 0.3);
        
        // Mix with chunk ambient color for inter-chunk light propagation
        finalColor = vec4(mix(baseColor, baseColor * ChunkAmbientData.rgb, 0.1), texColor.a);
    }
    
    FragColor = finalColor;
}
)";

SimpleShader::SimpleShader()
    : m_program(0), m_vertexShader(0), m_fragmentShader(0), m_uboHandle(0)
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
    
    // Initialize UBO for chunk lighting data
    if (!initializeUBO()) {
        std::cerr << "Failed to initialize UBO" << std::endl;
        return false;
    }
    
    std::cout << "Simple shader initialized successfully with UBO support" << std::endl;
    return true;
}

void SimpleShader::use()
{
    if (m_program != 0) {
        glUseProgram(m_program);
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
        glUniform1i(location, value);
    }
}

// Material system helper methods
void SimpleShader::setMaterialColor(const glm::vec4& color)
{
    GLint location = getUniformLocation("uMaterialColor");
    if (location != -1) {
        glUniform4f(location, color.r, color.g, color.b, color.a);
    }
}

void SimpleShader::setMaterialType(int type)
{
    GLint location = getUniformLocation("uMaterialType");
    if (location != -1) {
        glUniform1i(location, type);
    }
}

void SimpleShader::setMaterialRoughness(float roughness)
{
    GLint location = getUniformLocation("uMaterialRoughness");
    if (location != -1) {
        glUniform1f(location, roughness);
    }
}

void SimpleShader::setMaterialEmissive(const glm::vec3& emissive)
{
    GLint location = getUniformLocation("uMaterialEmissive");
    if (location != -1) {
        glUniform3f(location, emissive.r, emissive.g, emissive.b);
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

// UBO implementation
bool SimpleShader::initializeUBO()
{
    // Generate UBO
    glGenBuffers(1, &m_uboHandle);
    glBindBuffer(GL_UNIFORM_BUFFER, m_uboHandle);
    
    // Allocate memory for UBO (size of ChunkLightingData struct)
    glBufferData(GL_UNIFORM_BUFFER, sizeof(ChunkLightingData), nullptr, GL_DYNAMIC_DRAW);
    
    // Bind UBO to binding point 0 (matching the shader layout)
    glBindBufferBase(GL_UNIFORM_BUFFER, 0, m_uboHandle);
    
    // Get uniform block index and bind it
    GLuint blockIndex = glGetUniformBlockIndex(m_program, "ChunkLightingData");
    if (blockIndex == GL_INVALID_INDEX) {
        std::cerr << "Warning: ChunkLightingData uniform block not found in shader" << std::endl;
        return true; // Continue without UBO support
    }
    
    glUniformBlockBinding(m_program, blockIndex, 0);
    
    glBindBuffer(GL_UNIFORM_BUFFER, 0);
    return true;
}

void SimpleShader::updateChunkLightingData(const ChunkLightingData& data)
{
    if (m_uboHandle == 0) return;
    
    glBindBuffer(GL_UNIFORM_BUFFER, m_uboHandle);
    glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(ChunkLightingData), &data);
    glBindBuffer(GL_UNIFORM_BUFFER, 0);
}

void SimpleShader::updateChunkLightingData(int chunkIndex, const glm::mat4& transform, const Vec3& lightColor, const Vec3& ambientColor, float ambientStrength)
{
    // Create a temporary ChunkLightingData structure and update the specified chunk
    static ChunkLightingData data; // Static to persist between calls and avoid reallocation
    
    if (chunkIndex >= 0 && chunkIndex < 64) {
        // Set transform matrix
        data.transforms[chunkIndex] = transform;
        
        data.lightColors[chunkIndex] = glm::vec4(lightColor.x, lightColor.y, lightColor.z, 1.0f);
        data.ambientData[chunkIndex] = glm::vec4(ambientColor.x, ambientColor.y, ambientColor.z, ambientStrength);
        data.lightMapOffsets[chunkIndex] = glm::vec2(0.0f, 0.0f); // Default offset
        data.numChunks = std::max(data.numChunks, chunkIndex + 1);
        
        updateChunkLightingData(data);
    }
}

void SimpleShader::setChunkIndex(int chunkIndex)
{
    setInt("uChunkIndex", chunkIndex);
}

void SimpleShader::cleanup()
{
    if (m_uboHandle != 0) {
        glDeleteBuffers(1, &m_uboHandle);
        m_uboHandle = 0;
    }
    
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
