// VBORenderer.cpp - MINIMAL VERSION FOR DEBUGGING
#include "VBORenderer.h"
#include <iostream>

// Global instance
VBORenderer* g_vboRenderer = nullptr;

VBORenderer::VBORenderer() 
    : m_initialized(false)
{
    std::cout << "VBORenderer constructor called" << std::endl;
}

VBORenderer::~VBORenderer()
{
    shutdown();
}

bool VBORenderer::initialize()
{
    std::cout << "VBORenderer::initialize called" << std::endl;
    m_initialized = true;
    return true;
}

void VBORenderer::shutdown()
{
    std::cout << "VBORenderer::shutdown called" << std::endl;
    m_initialized = false;
}

void VBORenderer::uploadChunkMesh(VoxelChunk* chunk)
{
    (void)chunk; // Suppress unused parameter warning
    // Stub implementation
}

void VBORenderer::renderChunk(VoxelChunk* chunk, const Vec3& worldOffset)
{
    (void)chunk;
    (void)worldOffset;
    // Stub implementation
}

void VBORenderer::beginBatch()
{
    // Stub implementation
}

void VBORenderer::endBatch()
{
    // Stub implementation
}

bool VBORenderer::loadVBOExtensions()
{
    return true;
}
