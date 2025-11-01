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

// SSBO for MDI rendering (thousands of transforms)
layout (std430, binding = 1) readonly buffer MDITransforms {
    mat4 uMDITransforms[];
};

uniform mat4 uModel;
uniform mat4 uView;
uniform mat4 uProjection;
uniform int uChunkIndex;  // Which chunk this vertex belongs to (-1=use uModel, -2=use MDI/gl_BaseInstance)
uniform mat4 uLightVP; // Light view-projection
uniform float uShadowTexel;

out vec2 TexCoord;
out vec3 Normal;
out vec3 WorldPos;
out vec4 LightSpacePos;
out float ViewZ;
out float BlockType;

void main()
{
    mat4 finalTransform;
    if (uChunkIndex == -2) {  // ShaderMode::USE_MDI_SSBO
        // MDI mode: fetch from SSBO using gl_BaseInstance
        finalTransform = uMDITransforms[gl_BaseInstance];
    } else if (uChunkIndex >= 0 && uChunkIndex < uNumChunks) {
        // UBO batch mode: fetch from chunk transforms array
        finalTransform = uChunkTransforms[uChunkIndex];
    } else {  // ShaderMode::USE_UNIFORM_TRANSFORM (-1 or other)
        // Single chunk mode: use uModel uniform
        finalTransform = uModel;
    }

    vec4 world = finalTransform * vec4(aPosition, 1.0);
    gl_Position = uProjection * uView * world;
    TexCoord = aTexCoord;
    Normal = aNormal;
    WorldPos = world.xyz;
    BlockType = aBlockType;
    LightSpacePos = uLightVP * world;
    // View-space depth (positive distance)
    ViewZ = -(uView * world).z;
}
)";

// Fragment shader: texture * shadow visibility (inverse lighting)
static const char* FRAGMENT_SHADER_SOURCE = R"(
#version 460 core

in vec2 TexCoord;
in vec3 Normal;
in vec3 WorldPos;
in vec4 LightSpacePos;
in float ViewZ;
in float BlockType;

uniform sampler2D uTexture;      // Default/dirt texture
uniform sampler2D uStoneTexture; // Stone texture
uniform sampler2D uGrassTexture; // Grass texture
uniform sampler2D uSandTexture;  // Sand texture
uniform sampler2DArrayShadow uShadowMap;  // Cascaded shadow map array
uniform float uShadowTexel;
uniform vec3 uLightDir;

// Cascade uniforms
uniform mat4 uCascadeVP[2];      // View-projection for each cascade
uniform float uCascadeSplits[2];  // Split distances for cascades
uniform int uNumCascades;         // Number of cascades (typically 2)

// Material uniforms for different object types
uniform vec4 uMaterialColor;       // Diffuse color with alpha (for fluid particles, UI, etc.)
uniform int uMaterialType;         // 0=voxel, 1=fluid, 2=ui
uniform float uMaterialRoughness;  // Surface roughness
uniform vec3 uMaterialEmissive;    // Emissive color

out vec4 FragColor;

// Poisson disk with 8 samples for fast soft shadows
const vec2 POISSON[8] = vec2[8](
    vec2(-0.94201624, -0.39906216), 
    vec2(0.94558609, -0.76890725),
    vec2(-0.09418410, -0.92938870), 
    vec2(0.34495938, 0.29387760),
    vec2(-0.91588581, 0.45771432), 
    vec2(-0.81544232, -0.87912464),
    vec2(-0.38277543, 0.27676845), 
    vec2(0.97484398, 0.75648379)
);

float sampleShadowPCF(float bias)
{
    // Select cascade based on view-space depth
    // Use far cascade (index 1) starting at 64 blocks for smooth transitions
    int cascadeIndex = 0;
    float viewDepth = abs(ViewZ);
    
    if (viewDepth > 64.0) {
        cascadeIndex = 1;  // Far cascade starts at 64 blocks
    }
    
    // Transform to light space for selected cascade
    vec4 lightSpacePos = uCascadeVP[cascadeIndex] * vec4(WorldPos, 1.0);
    vec3 proj = lightSpacePos.xyz / lightSpacePos.w;
    proj = proj * 0.5 + 0.5;
    
    // If outside light frustum, surface receives NO light (dark by default)
    if (proj.x < 0.0 || proj.x > 1.0 || proj.y < 0.0 || proj.y > 1.0 || proj.z > 1.0)
        return 0.0;
    
    float current = proj.z - bias;
    
    // Adjust PCF radius based on cascade to maintain consistent world-space blur
    // Near cascade (256 units): use 128 pixel radius
    // Far cascade (2048 units): scale down radius to maintain same world-space coverage
    float baseRadius = 128.0;
    float radiusScale = (cascadeIndex == 0) ? 1.0 : 0.125;  // 1/8 for far cascade (256/2048)
    float radius = baseRadius * radiusScale * uShadowTexel;
    
    // Sample center first using array shadow sampler
    float center = texture(uShadowMap, vec4(proj.xy, cascadeIndex, current));
    
    // Early exit if fully lit - prevents shadow bleeding
    if (center >= 1.0) {
        return 1.0;
    }
    
    // Poisson disk sampling - reduced to 8 samples for performance
    float sum = center;
    for (int i = 0; i < 8; ++i) {
        vec2 offset = POISSON[i] * radius;
        float d = texture(uShadowMap, vec4(proj.xy + offset, cascadeIndex, current));
        sum += d;
    }
    
    // Average and lighten-only
    return max(center, sum / 9.0);  // 9 = 8 samples + 1 center
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
        
        if (blockID == 13) {
            // Decorative grass placeholder: fully transparent (no voxel draw)
            discard;
        } else if (blockID == 1) {
            // Stone blocks
            texColor = texture(uStoneTexture, TexCoord);
        } else if (blockID == 2) {
            // Dirt blocks  
            texColor = texture(uTexture, TexCoord);
        } else if (blockID == 3) {
            // Grass blocks
            texColor = texture(uGrassTexture, TexCoord);
        }
        // Elemental/crafted blocks (20-29) - placeholder colors until textures are made
        else if (blockID == 20) {
            // Coal - dark gray/black
            texColor = vec4(0.2, 0.2, 0.2, 1.0);
        } else if (blockID == 21) {
            // Iron Block - metallic gray
            texColor = vec4(0.7, 0.7, 0.8, 1.0);
        } else if (blockID == 22) {
            // Gold Block - golden yellow
            texColor = vec4(1.0, 0.84, 0.0, 1.0);
        } else if (blockID == 23) {
            // Copper Block - copper/orange
            texColor = vec4(0.72, 0.45, 0.20, 1.0);
        } else if (blockID == 24) {
            // Water - blue transparent
            texColor = vec4(0.2, 0.5, 0.9, 0.6);
        } else if (blockID == 25) {
            // Sand - use sand texture
            texColor = texture(uSandTexture, TexCoord);
        } else if (blockID == 26) {
            // Salt Block - white/crystalline
            texColor = vec4(0.95, 0.95, 0.98, 1.0);
        } else if (blockID == 27) {
            // Limestone - light gray/beige
            texColor = vec4(0.83, 0.81, 0.75, 1.0);
        } else if (blockID == 28) {
            // Ice - light blue transparent
            texColor = vec4(0.7, 0.9, 1.0, 0.8);
        } else if (blockID == 29) {
            // Diamond Block - cyan/bright blue
            texColor = vec4(0.4, 0.9, 1.0, 1.0);
        } else {
            // Default fallback (air/unknown)
            texColor = texture(uTexture, TexCoord);
        }
        
        if (texColor.a < 0.1) { discard; }

        // Slope-scale bias based on surface angle to light
        vec3 N = normalize(Normal);
        vec3 L = normalize(-uLightDir);
        float ndotl = max(dot(N, L), 0.0);
        float bias = max(0.0, 0.0002 * (1.0 - ndotl));
        
        float shadow = sampleShadowPCF(bias);
        
        // Dark-by-default: shadow value represents LIGHT VISIBILITY (reverse shadow map)
        // Surfaces are unlit unless the light map says they receive light
        float ambient = 0.04;
        float lit = ambient + shadow;
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
