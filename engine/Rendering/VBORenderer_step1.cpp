// VBORenderer.cpp - Step-by-step VBO implementation
#include "VBORenderer.h"
#include "../Core/Profiler.h"
#include <iostream>

// Global instance
VBORenderer* g_vboRenderer = nullptr;

VBORenderer::VBORenderer() 
    : m_initialized(false)
    // Initialize all OpenGL function pointers to nullptr
    , glGenBuffers(nullptr)
    , glDeleteBuffers(nullptr)
    , glBindBuffer(nullptr)
    , glBufferData(nullptr)
    , glBufferSubData(nullptr)
    , glGenVertexArrays(nullptr)
    , glDeleteVertexArrays(nullptr)
    , glBindVertexArray(nullptr)
    , glEnableVertexAttribArray(nullptr)
    , glVertexAttribPointer(nullptr)
{
    std::cout << "VBORenderer constructor" << std::endl;
}

VBORenderer::~VBORenderer()
{
    shutdown();
}

bool VBORenderer::initialize()
{
    if (m_initialized) {
        return true;
    }

    std::cout << "VBORenderer::initialize - loading extensions..." << std::endl;
    
    if (!loadVBOExtensions()) {
        std::cout << "Failed to load VBO extensions" << std::endl;
        return false;
    }

    m_initialized = true;
    std::cout << "VBORenderer initialized successfully" << std::endl;
    return true;
}

void VBORenderer::shutdown()
{
    if (!m_initialized) {
        return;
    }
    
    std::cout << "VBORenderer::shutdown" << std::endl;
    m_initialized = false;
}

bool VBORenderer::loadVBOExtensions()
{
    // Load essential VBO functions
    glGenBuffers = (PFNGLGENBUFFERSPROC)glfwGetProcAddress("glGenBuffers");
    glDeleteBuffers = (PFNGLDELETEBUFFERSPROC)glfwGetProcAddress("glDeleteBuffers");
    glBindBuffer = (PFNGLBINDBUFFERPROC)glfwGetProcAddress("glBindBuffer");
    glBufferData = (PFNGLBUFFERDATAPROC)glfwGetProcAddress("glBufferData");
    glBufferSubData = (PFNGLBUFFERSUBDATAPROC)glfwGetProcAddress("glBufferSubData");

    // Check if essential functions loaded
    if (!glGenBuffers || !glDeleteBuffers || !glBindBuffer || !glBufferData) {
        std::cout << "Failed to load essential VBO functions" << std::endl;
        return false;
    }

    std::cout << "Essential VBO functions loaded successfully" << std::endl;
    return true;
}

void VBORenderer::uploadChunkMesh(VoxelChunk* chunk)
{
    if (!chunk || !m_initialized) {
        return;
    }
    
    // For now, just a stub that doesn't use OpenGL
    // We'll implement actual VBO upload later
}

void VBORenderer::renderChunk(VoxelChunk* chunk, const Vec3& worldOffset)
{
    if (!chunk || !m_initialized) {
        return;
    }
    
    // NO IMMEDIATE MODE FALLBACK - VBO only!
    // Get the mesh data from chunk
    const auto& mesh = chunk->getMesh();
    if (mesh.vertices.empty()) {
        return; // No geometry to render
    }
    
    // Use VBO rendering only
    // For now, skip actual rendering since we need to debug the original issue
    // This ensures we don't fall back to immediate mode
    (void)worldOffset; // Suppress unused warning
}

void VBORenderer::beginBatch()
{
    // Stub for batch rendering
}

void VBORenderer::endBatch()
{
    // Stub for batch rendering
}
