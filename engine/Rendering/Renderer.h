// Renderer.h - Simple OpenGL rendering system
#pragma once

class Renderer {
public:
    static bool initialize();
    static void shutdown();
    static void clear();
    static void setViewMatrix(float* viewMatrix);
    static void setProjectionMatrix(float* projMatrix);
    
    // Simple shader for voxel rendering
    static unsigned int getVoxelShader();
    
private:
    static unsigned int createShader(const char* vertexSource, const char* fragmentSource);
    static unsigned int voxelShaderProgram;
    static unsigned int viewMatrixLocation;
    static unsigned int projMatrixLocation;
};
