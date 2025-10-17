// MDI Vertex Shader - Fetches transform from SSBO using gl_BaseInstance
#version 460 core

layout (location = 0) in vec3 aPosition;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec2 aTexCoord;
layout (location = 3) in vec2 aLightMapCoord;
layout (location = 4) in float aFaceIndex;
layout (location = 5) in float aBlockType;

// SSBO containing all chunk transforms (indexed by gl_BaseInstance)
layout (std430, binding = 0) readonly buffer ChunkTransforms {
    mat4 uTransforms[];
};

uniform mat4 uView;
uniform mat4 uProjection;
uniform mat4 uLightVP[4];
uniform int uCascadeCount;

out vec2 TexCoord;
out vec3 Normal;
out vec3 WorldPos;
out vec4 LightSpacePos[4];
out float ViewZ;
out float BlockType;

void main()
{
    // Fetch this chunk's transform from SSBO using gl_BaseInstance
    mat4 modelMatrix = uTransforms[gl_BaseInstance];
    
    // Transform to world space
    vec4 worldPos = modelMatrix * vec4(aPosition, 1.0);
    WorldPos = worldPos.xyz;
    
    // Transform normal
    Normal = mat3(modelMatrix) * aNormal;
    
    // Clip space position
    gl_Position = uProjection * uView * worldPos;
    
    // Pass through data
    TexCoord = aTexCoord;
    BlockType = aBlockType;
    
    // Shadow space positions for cascaded shadow maps
    for (int i = 0; i < uCascadeCount; i++) {
        LightSpacePos[i] = uLightVP[i] * worldPos;
    }
    
    // View-space depth for cascade selection
    ViewZ = -(uView * worldPos).z;
}
