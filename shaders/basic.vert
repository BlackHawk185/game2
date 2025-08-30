#version 330 core

// Input vertex attributes
layout (location = 0) in vec3 aPosition;    // Vertex position
layout (location = 1) in vec3 aNormal;      // Vertex normal
layout (location = 2) in vec2 aTexCoord;    // Texture coordinates

// Uniforms (passed from CPU)
uniform mat4 uModel;        // Model matrix
uniform mat4 uView;         // View matrix  
uniform mat4 uProjection;   // Projection matrix

// Output to fragment shader
out vec3 FragPos;     // Fragment position in world space
out vec3 Normal;      // Normal vector
out vec2 TexCoord;    // Texture coordinates

void main() {
    // Transform vertex position to world space
    FragPos = vec3(uModel * vec4(aPosition, 1.0));
    
    // Transform normal to world space (should use normal matrix for non-uniform scaling)
    Normal = mat3(transpose(inverse(uModel))) * aNormal;
    
    // Pass texture coordinates through
    TexCoord = aTexCoord;
    
    // Transform vertex to clip space
    gl_Position = uProjection * uView * vec4(FragPos, 1.0);
}
