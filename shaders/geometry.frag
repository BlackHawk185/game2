#version 460 core

// Input from vertex shader
in VS_OUT {
    vec3 worldPosition;
    vec3 worldNormal;
    vec2 texCoord;
    vec3 vertexColor;
    vec4 clipPosition;
    vec4 prevClipPosition;
} fs_in;

// G-Buffer outputs
layout(location = 0) out vec4 g_Albedo;      // RGB: albedo, A: metallic
layout(location = 1) out vec4 g_Normal;      // RGB: world normal, A: roughness
layout(location = 2) out vec2 g_Motion;      // RG: motion vectors

// Material properties
uniform sampler2D u_AlbedoTexture;
uniform float u_Metallic;
uniform float u_Roughness;

// Utility functions
vec2 calculateMotionVector() {
    // Convert to NDC
    vec2 currentNDC = (fs_in.clipPosition.xy / fs_in.clipPosition.w) * 0.5 + 0.5;
    vec2 prevNDC = (fs_in.prevClipPosition.xy / fs_in.prevClipPosition.w) * 0.5 + 0.5;
    
    // Motion vector is the difference
    return currentNDC - prevNDC;
}

vec3 encodeNormal(vec3 normal) {
    // Encode normal from [-1,1] to [0,1] range
    return normal * 0.5 + 0.5;
}

void main() {
    // Sample albedo texture and combine with vertex color
    vec4 textureColor = texture(u_AlbedoTexture, fs_in.texCoord);
    vec3 albedo = textureColor.rgb * fs_in.vertexColor;
    
    // Pack albedo and metallic
    g_Albedo = vec4(albedo, u_Metallic);
    
    // Normalize and encode normal, pack with roughness
    vec3 normal = normalize(fs_in.worldNormal);
    g_Normal = vec4(encodeNormal(normal), u_Roughness);
    
    // Calculate motion vectors for temporal effects
    g_Motion = calculateMotionVector();
}
