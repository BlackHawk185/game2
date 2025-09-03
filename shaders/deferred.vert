#version 460 core

// Fullscreen triangle vertex shader for deferred lighting
layout(location = 0) in vec2 a_Position;

out vec2 v_TexCoord;

void main() {
    // Generate texture coordinates from vertex position
    v_TexCoord = a_Position * 0.5 + 0.5;
    
    // Output position directly (fullscreen triangle)
    gl_Position = vec4(a_Position, 0.0, 1.0);
}
