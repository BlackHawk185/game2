#include "SimpleShader.h"
#include <glad/gl.h>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <iostream>
#include <unordered_map>

// Vertex shader for voxel rendering with inverse shadow map lighting (no lightmaps)
static const char* VERTEX_SHADER_SOURCE = R"(
#version 460 core

layout (location = 0) in vec3 aPosition;
layout (location = 1) in vec2 aTexCoord;
layout (location = 2) in vec3 aNormal;
layout (location = 3) in vec2 aLightMapCoord;
layout (location = 4) in float aAmbientOcclusion;
layout (location = 5) in float aFaceIndex;
layout (location = 6) in float aBlockType;

// Retain UBO signature for compatibility, though not used for lighting
layout (std140, binding = 0) uniform ChunkLightingData {
    mat4 uChunkTransforms[64];
    vec4 uChunkLightColors[64];
    vec4 uChunkAmbientData[64];
    vec2 uChunkLightMapOffsets[64];
    int uNumChunks;
};

uniform mat4 uModel;
uniform mat4 uView;
uniform mat4 uProjection;
uniform int uChunkIndex;  // Which chunk this vertex belongs to
uniform mat4 uLightVP[4]; // Cascaded light view-projections
uniform int uCascadeCount;
uniform float uShadowTexel[4];

out vec2 TexCoord;
out vec3 Normal;
out vec3 WorldPos;
out vec4 LightSpacePos[4];
out float ViewZ;
out float BlockType;

void main()
{
    mat4 finalTransform = (uChunkIndex >= 0 && uChunkIndex < uNumChunks) ?
                         uChunkTransforms[uChunkIndex] : uModel;

    vec4 world = finalTransform * vec4(aPosition, 1.0);
    gl_Position = uProjection * uView * world;
    TexCoord = aTexCoord;
    Normal = aNormal;
    WorldPos = world.xyz;
    BlockType = aBlockType;
    for (int i=0;i<uCascadeCount;i++) {
        LightSpacePos[i] = uLightVP[i] * world;
    }
    // View-space depth for cascade selection (positive distance)
    ViewZ = -(uView * world).z;
}
)";

// Fragment shader: texture * shadow visibility (inverse lighting)
static const char* FRAGMENT_SHADER_SOURCE = R"(
#version 460 core

in vec2 TexCoord;
in vec3 Normal;
in vec3 WorldPos;
in vec4 LightSpacePos[4];
in float ViewZ;
in float BlockType;

uniform sampler2D uTexture;      // Default/dirt texture
uniform sampler2D uStoneTexture; // Stone texture
uniform sampler2D uGrassTexture; // Grass texture
uniform sampler2D uShadowMaps[4];
uniform int uCascadeCount;
uniform float uCascadeSplits[4];
uniform float uShadowTexel[4];
uniform vec3 uLightDir;

// Material uniforms for different object types
uniform vec4 uMaterialColor;       // Diffuse color with alpha (for fluid particles, UI, etc.)
uniform int uMaterialType;         // 0=voxel, 1=fluid, 2=ui
uniform float uMaterialRoughness;  // Surface roughness
uniform vec3 uMaterialEmissive;    // Emissive color

out vec4 FragColor;

// Poisson disk
const vec2 POISSON[12] = vec2[12](
    vec2( -0.613,  0.354 ), vec2( 0.743,  0.106 ), vec2( 0.296, -0.682 ), vec2( -0.269, -0.402 ),
    vec2( -0.154,  0.692 ), vec2( 0.389,  0.463 ), vec2( 0.682, -0.321 ), vec2( -0.682,  0.228 ),
    vec2( -0.053, -0.934 ), vec2( 0.079,  0.934 ), vec2( -0.934, -0.079 ), vec2( 0.934,  0.053 )
);

float sampleCascadePCF(int idx, float bias)
{
    vec3 proj = LightSpacePos[idx].xyz / LightSpacePos[idx].w;
    proj = proj * 0.5 + 0.5;
    if (proj.x < 0.0 || proj.x > 1.0 || proj.y < 0.0 || proj.y > 1.0 || proj.z > 1.0)
        return 1.0;
    float current = proj.z - bias;
    float texel = uShadowTexel[idx];
    float radius = 2.5 * texel;
    float sum = 0.0;
    for (int i = 0; i < 12; ++i) {
        vec2 offset = POISSON[i] * radius;
        float d = texture(uShadowMaps[idx], proj.xy + offset).r;
        sum += current <= d ? 1.0 : 0.0;
    }
    return sum / 12.0;
}

void main()
{
    vec4 finalColor;

    if (uMaterialType == 1) {
        // Fluid Material - simple color with transparency, no directional lighting
        finalColor = uMaterialColor;
    } else if (uMaterialType == 2) {
        // UI Material - no lighting, just color/texture
        vec4 texColor = texture(uTexture, TexCoord);
        finalColor = texColor * uMaterialColor;
    } else {
        // Voxel Material - select texture based on block type
        vec4 texColor;
        int blockID = int(BlockType + 0.5); // Round to nearest integer
        
        if (blockID == 1) {
            // Stone blocks
            texColor = texture(uStoneTexture, TexCoord);
        } else if (blockID == 2) {
            // Dirt blocks  
            texColor = texture(uTexture, TexCoord);
        } else if (blockID == 3) {
            // Grass blocks
            texColor = texture(uGrassTexture, TexCoord);
        } else {
            // Default fallback (air/unknown)
            texColor = texture(uTexture, TexCoord);
        }
        
        if (texColor.a < 0.1) { discard; }

        // Transform to [0,1] shadow map coords
        // Select cascade based on view-space depth
        int ci = 0;
        for (int i=0;i<uCascadeCount-1;i++) {
            if (ViewZ > uCascadeSplits[i]) ci = i+1; else break;
        }
        // Compute blended factor across boundary (10% of cascade span)
        int prev = max(ci-1, 0);
        float start = (ci==0)? 0.0 : uCascadeSplits[ci-1];
        float endV = uCascadeSplits[ci];
        float span = max(endV - start, 1e-3);
    float band = 0.2 * span; // Increased to 20% for wider overlap
        float tBlend = 0.0;
        if (ViewZ > endV - band && ci < uCascadeCount-1) {
            tBlend = clamp((ViewZ - (endV - band)) / band, 0.0, 1.0);
        }

        float shadow = 1.0;
        // Slope-scale bias based on N.L to mitigate acne
        vec3 N = normalize(Normal);
        vec3 L = normalize(-uLightDir);
        float ndotl = max(dot(N, L), 0.0);
        float bias = max(0.0015, 0.0035 * (1.0 - ndotl));
        
        float s0 = sampleCascadePCF(ci, bias);
        if (tBlend > 0.0 && ci < uCascadeCount-1) {
            float s1 = sampleCascadePCF(ci+1, bias);
            shadow = mix(s0, s1, tBlend);
        } else {
            shadow = s0;
        }
        // Simple lambert + small ambient floor for readability
        float lambert = ndotl;
        float ambient = 0.04;
        float lit = clamp(ambient + shadow * lambert, 0.0, 1.0);
        finalColor = vec4(texColor.rgb * lit, texColor.a);
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
