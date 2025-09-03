#version 460 core

// Input from vertex shader
in vec2 v_TexCoord;

// G-Buffer inputs
uniform sampler2D u_GBufferAlbedo;
uniform sampler2D u_GBufferNormal;
uniform sampler2D u_GBufferDepth;

// Camera data
layout(std140, binding = 0) uniform CameraData {
    mat4 u_ViewMatrix;
    mat4 u_ProjectionMatrix;
    mat4 u_ViewProjectionMatrix;
    mat4 u_PrevViewProjectionMatrix;
    vec3 u_CameraPosition;
    float u_Time;
};

// Lighting data
layout(std140, binding = 1) uniform LightingData {
    vec3 u_SunDirection;
    vec3 u_SunColor;
    vec3 u_AmbientColor;
    float u_SunIntensity;
};

// Output
out vec4 FragColor;

// Utility functions
vec3 decodeNormal(vec3 encoded) {
    return encoded * 2.0 - 1.0;
}

vec3 worldPositionFromDepth(vec2 texCoord, float depth) {
    // Reconstruct world position from depth
    vec4 clipPos = vec4(texCoord * 2.0 - 1.0, depth * 2.0 - 1.0, 1.0);
    vec4 worldPos = inverse(u_ViewProjectionMatrix) * clipPos;
    return worldPos.xyz / worldPos.w;
}

float calculateLighting(vec3 worldPos, vec3 normal) {
    // Simple directional lighting for now
    vec3 lightDir = normalize(-u_SunDirection);
    float NdotL = max(dot(normal, lightDir), 0.0);
    
    // TODO: Add shadow mapping
    // TODO: Add ambient occlusion
    // TODO: Add volumetric lighting
    
    return NdotL;
}

vec3 calculatePBR(vec3 albedo, vec3 normal, float metallic, float roughness, vec3 worldPos) {
    // Simplified PBR lighting
    float lighting = calculateLighting(worldPos, normal);
    
    // Ambient contribution
    vec3 ambient = u_AmbientColor * albedo * 0.1;
    
    // Diffuse contribution
    vec3 diffuse = u_SunColor * albedo * lighting * u_SunIntensity;
    
    // TODO: Add specular/reflection contribution
    // TODO: Add IBL (Image Based Lighting)
    // TODO: Add subsurface scattering for specific materials
    
    return ambient + diffuse;
}

void main() {
    // Sample G-Buffer
    vec4 albedoMetallic = texture(u_GBufferAlbedo, v_TexCoord);
    vec4 normalRoughness = texture(u_GBufferNormal, v_TexCoord);
    float depth = texture(u_GBufferDepth, v_TexCoord).r;
    
    // Extract material properties
    vec3 albedo = albedoMetallic.rgb;
    float metallic = albedoMetallic.a;
    vec3 normal = decodeNormal(normalRoughness.rgb);
    float roughness = normalRoughness.a;
    
    // Reconstruct world position
    vec3 worldPos = worldPositionFromDepth(v_TexCoord, depth);
    
    // Calculate lighting
    vec3 color = calculatePBR(albedo, normal, metallic, roughness, worldPos);
    
    // Output final color (HDR)
    FragColor = vec4(color, 1.0);
}
