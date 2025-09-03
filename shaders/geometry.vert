#version 460 core

// Vertex attributes
layout(location = 0) in vec3 a_Position;
layout(location = 1) in vec3 a_Normal;
layout(location = 2) in vec2 a_TexCoord;
layout(location = 3) in vec3 a_Color;

// Uniform buffer objects
layout(std140, binding = 0) uniform CameraData {
    mat4 u_ViewMatrix;
    mat4 u_ProjectionMatrix;
    mat4 u_ViewProjectionMatrix;
    mat4 u_PrevViewProjectionMatrix;
    vec3 u_CameraPosition;
    float u_Time;
};

// Instance data (for instanced rendering)
uniform mat4 u_ModelMatrix;

// Outputs to fragment shader
out VS_OUT {
    vec3 worldPosition;
    vec3 worldNormal;
    vec2 texCoord;
    vec3 vertexColor;
    vec4 clipPosition;
    vec4 prevClipPosition;
} vs_out;

void main() {
    // Transform to world space
    vec4 worldPos = u_ModelMatrix * vec4(a_Position, 1.0);
    vs_out.worldPosition = worldPos.xyz;
    
    // Transform normal to world space (assuming uniform scaling)
    vs_out.worldNormal = normalize(mat3(u_ModelMatrix) * a_Normal);
    
    // Pass through texture coordinates and vertex color
    vs_out.texCoord = a_TexCoord;
    vs_out.vertexColor = a_Color;
    
    // Transform to clip space
    vs_out.clipPosition = u_ViewProjectionMatrix * worldPos;
    
    // Previous frame clip position for motion vectors
    vs_out.prevClipPosition = u_PrevViewProjectionMatrix * worldPos;
    
    gl_Position = vs_out.clipPosition;
}
