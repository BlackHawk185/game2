// Renderer.cpp - Minimal OpenGL renderer (lightweight facade)
#include "Renderer.h"

#include <glad/glad.h>
#include <iostream>

unsigned int Renderer::voxelShaderProgram = 0;
unsigned int Renderer::viewMatrixLocation = 0;
unsigned int Renderer::projMatrixLocation = 0;

bool Renderer::initialize()
{
    // Keep this minimal; low-level GL state is configured by the active renderer
    return true;
}

void Renderer::clear()
{
    // Use deep sky color for background (dramatic blue sky)
    glClearColor(0.1f, 0.3f, 0.9f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

void Renderer::setViewMatrix(float* /*viewMatrix*/)
{
    // No-op in facade; modern path uses VBORenderer matrices
}

void Renderer::setProjectionMatrix(float* /*projMatrix*/)
{
    // No-op in facade; modern path uses VBORenderer matrices
}

unsigned int Renderer::getVoxelShader()
{
    return 0;
}

void Renderer::shutdown()
{
    // Nothing to do currently
}

unsigned int Renderer::createShader(const char* /*vertexSource*/, const char* /*fragmentSource*/)
{
    return 0;
}
