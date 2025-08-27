// Renderer.cpp - Minimal OpenGL renderer
#include "Renderer.h"
#define WIN32_LEAN_AND_MEAN
#include <GL/gl.h>
#include <windows.h>

#include <iostream>

unsigned int Renderer::voxelShaderProgram = 0;
unsigned int Renderer::viewMatrixLocation = 0;
unsigned int Renderer::projMatrixLocation = 0;

bool Renderer::initialize()
{
    // Removed verbose debug output
    glEnable(GL_DEPTH_TEST);
    return true;
}

void Renderer::clear()
{
    glClearColor(0.5f, 0.8f, 1.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

void Renderer::setViewMatrix(float* viewMatrix)
{
    // Simple implementation
}

void Renderer::setProjectionMatrix(float* projMatrix)
{
    // Simple implementation
}

unsigned int Renderer::getVoxelShader()
{
    return 0;
}

void Renderer::shutdown()
{
    std::cout << "Renderer shutdown" << std::endl;
}

unsigned int Renderer::createShader(const char* vertexSource, const char* fragmentSource)
{
    return 0;
}
