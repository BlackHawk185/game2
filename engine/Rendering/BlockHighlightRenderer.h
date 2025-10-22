// BlockHighlightRenderer.h - Wireframe cube for selected block
#pragma once

#include "../Math/Vec3.h"
#include <glad/gl.h>

// Renders a wireframe cube around the selected block
class BlockHighlightRenderer {
public:
    BlockHighlightRenderer();
    ~BlockHighlightRenderer();

    // Initialize GPU resources
    bool initialize();
    
    // Render wireframe cube at block position (world coordinates)
    void render(const Vec3& blockPos, const float* viewMatrix, const float* projectionMatrix);
    
    // Cleanup GPU resources
    void shutdown();

private:
    GLuint m_vao = 0;
    GLuint m_vbo = 0;
    GLuint m_ebo = 0;
    GLuint m_shader = 0;
    
    bool m_initialized = false;
    
    // Compile simple shader for wireframe rendering
    bool compileShader();
};
