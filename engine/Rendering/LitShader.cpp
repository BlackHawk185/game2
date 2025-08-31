#include "LitShader.h"
#include <glad/glad.h>
#include <iostream>
#include <unordered_map>
#include <cctype>

// Vertex shader: outputs world position and normal for lighting
static const char* LIT_VERT = R"(
#version 330 core
layout (location = 0) in vec3 aPosition;
layout (location = 1) in vec2 aTexCoord;
layout (location = 2) in vec3 aNormal;

uniform mat4 uModel;
uniform mat4 uView;
uniform mat4 uProjection;

out vec2 vTexCoord;
out vec3 vNormal;
out vec3 vWorldPos;
out vec2 vNdcPos; // NDC position for sky rendering

void main()
{
    vec4 worldPos4 = uModel * vec4(aPosition, 1.0);
    vWorldPos = worldPos4.xyz;
    // Approximate normal transform (assumes uniform scale)
    vNormal = mat3(uModel) * aNormal;
    vTexCoord = aTexCoord;
    // Pass through NDC position for sky background rendering (when using fullscreen quad)
    // When rendering the sky, uModel, uView, uProjection are identity and aPosition is already NDC
    vNdcPos = aPosition.xy;
    gl_Position = uProjection * uView * worldPos4;
}
)";

// Fragment shader: textured Blinn-Phong with gamma correction
static const char* LIT_FRAG = R"(
#version 330 core
in vec2 vTexCoord;
in vec3 vNormal;
in vec3 vWorldPos;
in vec2 vNdcPos;

uniform sampler2D uTexture;
uniform vec3 uSunDirection;   // points FROM light (e.g., (0.5, -0.8, 0.3))
uniform vec3 uCameraPos;
uniform vec3 uAlbedoTint;     // multiply with texture
uniform float uAmbient;       // 0..1
uniform float uSpecularStrength; // 0..1
uniform float uShininess;     // e.g., 16..64
uniform mat4 uLightVP;        // light view-projection
uniform sampler2D uShadowMap; // depth texture from light
uniform float uShadowEnabled; // 0 or 1
uniform float uShadowTexelSize; // 1.0 / shadowMapSize (assumed square)
uniform float uShadowBiasConst; // constant bias
uniform float uShadowBiasSlope; // slope-scaled bias factor
uniform float uExposure;        // exposure control for tone mapping
uniform vec3 uSkyColorTop;      // sky color at zenith
uniform vec3 uSkyColorHorizon;  // sky color at horizon
uniform vec3 uSunColor;         // sun light color
uniform float uFogDensity;      // fog density (0 = no fog, higher = more fog)
uniform float uSkyMode;         // 0 = normal voxel rendering, 1 = sky background rendering
// Camera basis and projection info for world-space sky gradient
uniform vec3 uCameraRight;
uniform vec3 uCameraUp;
uniform vec3 uCameraForward;
uniform float uTanHalfFov;
uniform float uAspect;

out vec4 FragColor;

vec3 toLinear(vec3 c) { return pow(c, vec3(2.2)); }
vec3 toGamma(vec3 c) { return pow(c, vec3(1.0/2.2)); }

// ACES tone mapping function - makes colors look cinematic
vec3 acesToneMapping(vec3 color) {
    const float a = 2.51;
    const float b = 0.03;
    const float c = 2.43;
    const float d = 0.59;
    const float e = 0.14;
    return clamp((color * (a * color + b)) / (color * (c * color + d) + e), 0.0, 1.0);
}

// Calculate sky color based on view direction
vec3 getSkyColor(vec3 viewDir) {
    float skyGradient = max(0.0, viewDir.y); // 0 at horizon, 1 at zenith
    return mix(uSkyColorHorizon, uSkyColorTop, skyGradient);
}

// Calculate distance-based fog factor
float getFogFactor(float distance) {
    return 1.0 - exp(-uFogDensity * distance * distance);
}

float shadowFactor(vec3 worldPos, float bias)
{
    // Transform to light clip space
    vec4 lightClip = uLightVP * vec4(worldPos, 1.0);
    vec3 ndc = lightClip.xyz / lightClip.w;
    vec3 uvw = ndc * 0.5 + 0.5; // to [0,1]
    if (uvw.x < 0.0 || uvw.x > 1.0 || uvw.y < 0.0 || uvw.y > 1.0) return 1.0; // outside map
    
    // 5x5 PCF for much softer shadows
    float result = 0.0;
    int sampleCount = 0;
    for (int y = -2; y <= 2; ++y) {
        for (int x = -2; x <= 2; ++x) {
            vec2 offset = vec2(float(x), float(y)) * uShadowTexelSize;
            float closest = texture(uShadowMap, uvw.xy + offset).r;
            result += (uvw.z - bias) <= closest ? 1.0 : 0.0;
            sampleCount++;
        }
    }
    result /= float(sampleCount); // Divide by 25 for 5x5 grid
    
    // Softer shadow transition (less harsh black shadows)
    return mix(0.5, 1.0, result);
}

void main()
{
    // Check if we're rendering sky background
    if (uSkyMode > 0.5) {
        // Reconstruct view ray in world space from NDC and camera basis
        // vNdcPos is in [-1,1] for x and y. Map to view plane using tan(fov/2) and aspect.
        float x = vNdcPos.x * uAspect * uTanHalfFov;
        float y = vNdcPos.y * uTanHalfFov;
        vec3 viewDir = normalize(uCameraForward + x * uCameraRight + y * uCameraUp);

        // World-space gradient: light at horizon (|viewDir.y| ~ 0), dark at poles (|viewDir.y| ~ 1)
        float t = 1.0 - clamp(abs(viewDir.y), 0.0, 1.0);
        // Shape the band and cap intensity so center doesn't reach pure horizon color
        t = pow(t, 1.6);
        t = min(t, 0.85);
        vec3 skyColor = mix(uSkyColorTop, uSkyColorHorizon, t);
        
        // No exposure or tone mapping for the sky; use gamma only to preserve intended hue
        FragColor = vec4(toGamma(skyColor), 1.0);
        return;
    }
    
    // Normal voxel rendering
    vec4 tex = texture(uTexture, vTexCoord);
    if (tex.a < 0.1) discard;

    // Linearize texture color (approx, since not using sRGB textures yet)
    vec3 albedo = toLinear(tex.rgb) * uAlbedoTint;

    // Lighting vectors
    vec3 N = normalize(vNormal);
    vec3 L = normalize(-uSunDirection); // direction TO light
    vec3 V = normalize(uCameraPos - vWorldPos);
    vec3 H = normalize(L + V);

    // Terms
    float diff = max(dot(N, L), 0.0);
    float spec = 0.0;
    if (diff > 0.0) {
        spec = pow(max(dot(N, H), 0.0), uShininess) * uSpecularStrength;
    }

    float ambient = clamp(uAmbient, 0.0, 1.0);
    float slopeBias = uShadowBiasSlope * (1.0 - max(dot(N, L), 0.0));
    float bias = max(uShadowBiasConst, slopeBias);
    float sf = (uShadowEnabled > 0.5) ? shadowFactor(vWorldPos, bias) : 1.0;
    
    // Calculate distance from camera for fog
    float distance = length(uCameraPos - vWorldPos);
    
    // Sky-driven ambient color (mix sky colors based on lighting)
    vec3 ambientColor = mix(uSkyColorHorizon, uSkyColorTop, 0.5) * uSunColor;
    
    // Standard lighting with sky-tinted ambient
    vec3 color = albedo * (ambient * ambientColor + sf * (0.8 * diff * uSunColor)) + vec3(spec) * sf * uSunColor;
    
    // Apply fog
    float fogFactor = getFogFactor(distance * 0.005); // Much lighter fog scaling (was 0.01)
    vec3 skyColor = getSkyColor(normalize(vWorldPos - uCameraPos));
    color = mix(color, skyColor, fogFactor);

    // Apply exposure control
    color *= uExposure;
    
    // Apply ACES tone mapping for cinematic look
    color = acesToneMapping(color);

    // Convert back to gamma space for output
    FragColor = vec4(toGamma(color), tex.a);
}
)";

LitShader::LitShader() : m_program(0), m_vertexShader(0), m_fragmentShader(0) {}
LitShader::~LitShader() { cleanup(); }

bool LitShader::initialize()
{
    m_vertexShader = glCreateShader(GL_VERTEX_SHADER);
    m_fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);

    if (!compileShader(m_vertexShader, LIT_VERT)) {
        std::cerr << "Failed to compile lit vertex shader" << std::endl;
        return false;
    }
    if (!compileShader(m_fragmentShader, LIT_FRAG)) {
        std::cerr << "Failed to compile lit fragment shader" << std::endl;
        return false;
    }
    m_program = glCreateProgram();
    glAttachShader(m_program, m_vertexShader);
    glAttachShader(m_program, m_fragmentShader);
    if (!linkProgram()) {
        std::cerr << "Failed to link lit shader program" << std::endl;
        return false;
    }
    return true;
}

void LitShader::use()
{
    if (m_program) glUseProgram(m_program);
}

void LitShader::cleanup()
{
    if (m_program) { glDeleteProgram(m_program); m_program = 0; }
    if (m_vertexShader) { glDeleteShader(m_vertexShader); m_vertexShader = 0; }
    if (m_fragmentShader) { glDeleteShader(m_fragmentShader); m_fragmentShader = 0; }
}

void LitShader::setMatrix4(const std::string& name, const Mat4& matrix)
{
    int loc = getUniformLocation(name);
    if (loc != -1) glUniformMatrix4fv(loc, 1, GL_FALSE, matrix.data());
}
void LitShader::setVector3(const std::string& name, const Vec3& v)
{
    int loc = getUniformLocation(name);
    if (loc != -1) glUniform3f(loc, v.x, v.y, v.z);
}
void LitShader::setFloat(const std::string& name, float value)
{
    int loc = getUniformLocation(name);
    if (loc != -1) glUniform1f(loc, value);
}
void LitShader::setInt(const std::string& name, int value)
{
    int loc = getUniformLocation(name);
    if (loc != -1) {
        // Some GL headers in this setup do not expose glUniform1i; fallback to float
        glUniform1f(loc, static_cast<float>(value));
    }
}

bool LitShader::compileShader(GLuint shader, const char* source)
{
    // Ensure #version is on the very first line by trimming leading whitespace/newlines
    const char* src = source;
    while (*src == ' ' || *src == '\t' || *src == '\r' || *src == '\n') {
        ++src;
    }
    glShaderSource(shader, 1, &src, nullptr);
    glCompileShader(shader);
    GLint ok = 0; glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        GLchar log[1024]; glGetShaderInfoLog(shader, 1024, nullptr, log);
        std::cerr << "Shader compilation error: " << log << std::endl;
        return false;
    }
    return true;
}

bool LitShader::linkProgram()
{
    glLinkProgram(m_program);
    GLint ok = 0; glGetProgramiv(m_program, GL_LINK_STATUS, &ok);
    if (!ok) {
        GLchar log[1024]; glGetProgramInfoLog(m_program, 1024, nullptr, log);
        std::cerr << "Program linking error: " << log << std::endl;
        return false;
    }
    return true;
}

int LitShader::getUniformLocation(const std::string& name)
{
    static std::unordered_map<std::string, GLint> cache;
    auto it = cache.find(name);
    if (it != cache.end()) return it->second;
    GLint loc = glGetUniformLocation(m_program, name.c_str());
    cache[name] = loc;
    if (loc == -1) {
        std::cerr << "Warning: Uniform '" << name << "' not found in lit shader" << std::endl;
    }
    return loc;
}
