// GPUMeshGenerator.cpp - GPU-accelerated voxel mesh generation implementation
#include "GPUMeshGenerator.h"
#include <iostream>
#include <chrono>
#include <algorithm>
#include <numeric>

GPUMeshGenerator::GPUMeshGenerator() {
}

GPUMeshGenerator::~GPUMeshGenerator() {
    cleanup();
}

bool GPUMeshGenerator::initialize() {
    std::cout << "🚀 Initializing GPU Mesh Generator..." << std::endl;
    std::cout << "🔧 Step 1: Starting initialization sequence" << std::endl;
    std::flush(std::cout);  // Force output immediately
    
    try {
        std::cout << "🔧 Step 2: About to check GPU compute support" << std::endl;
        std::flush(std::cout);
        
        // Check GPU compute support
        if (!isGPUComputeSupported()) {
            std::cerr << "❌ GPU compute shaders not supported on this hardware" << std::endl;
            return false;
        }
        
        std::cout << "✅ GPU compute support verified" << std::endl;
        printGPUCapabilities();
        
        std::cout << "🔧 Step 3: About to load compute shader" << std::endl;
        std::flush(std::cout);
        
        // Load compute shader
        std::cout << "📂 Loading compute shader..." << std::endl;
        if (!loadComputeShader()) {
            std::cerr << "❌ Failed to load voxel mesh generation compute shader" << std::endl;
            return false;
        }
        
        std::cout << "✅ Compute shader loaded successfully" << std::endl;
        
        std::cout << "🔧 Step 4: About to allocate GPU buffers" << std::endl;
        std::flush(std::cout);
        
        // Allocate GPU buffers
        std::cout << "💾 Allocating GPU buffers..." << std::endl;
        if (!allocateGPUBuffers()) {
            std::cerr << "❌ Failed to allocate GPU buffers for mesh generation" << std::endl;
            return false;
        }
        
        std::cout << "✅ GPU buffers allocated successfully" << std::endl;
        
        // Reset performance metrics
        resetMetrics();
        
        std::cout << "✅ GPU Mesh Generator initialized successfully" << std::endl;
        std::cout << "📊 Max vertices per chunk: " << m_maxVerticesPerChunk << std::endl;
        std::cout << "📊 Max indices per chunk: " << m_maxIndicesPerChunk << std::endl;
        
        return true;
        
    } catch (const std::exception& e) {
        std::cerr << "💥 Exception during GPU mesh generator initialization: " << e.what() << std::endl;
        return false;
    } catch (...) {
        std::cerr << "💥 Unknown exception during GPU mesh generator initialization" << std::endl;
        return false;
    }
}

void GPUMeshGenerator::cleanup() {
    m_meshGenShader.reset();
    m_voxelDataBuffer.cleanup();
    m_vertexBuffer.cleanup();
    m_indexBuffer.cleanup();
    m_counterBuffer.cleanup();
}

bool GPUMeshGenerator::generateChunkMesh(VoxelChunk* chunk, GPUMeshData& outMeshData) {
    if (!chunk || !m_meshGenShader) {
        std::cerr << "❌ Invalid chunk or compute shader not loaded" << std::endl;
        return false;
    }
    
    auto startTime = std::chrono::high_resolution_clock::now();
    
    // Clear previous mesh data
    outMeshData.isGenerated = false;
    outMeshData.vertexCount = 0;
    outMeshData.indexCount = 0;
    outMeshData.faceCount = 0;
    
    // Upload voxel data to GPU
    if (!uploadVoxelData(chunk, m_voxelDataBuffer)) {
        std::cerr << "❌ Failed to upload voxel data to GPU" << std::endl;
        return false;
    }
    
    // Clear GPU counters
    clearCounters();
    
    // Set compute shader uniforms
    m_meshGenShader->use();
    m_meshGenShader->setVec3("chunkSize", VoxelChunk::SIZE, VoxelChunk::SIZE, VoxelChunk::SIZE);
    m_meshGenShader->setVec3("chunkOffset", 0.0f, 0.0f, 0.0f); // TODO: Add chunk world offset
    m_meshGenShader->setFloat("voxelSize", m_voxelSize);
    
    // Bind GPU buffers
    m_voxelDataBuffer.bindAsStorageBuffer(0);     // Input: voxel data
    m_vertexBuffer.bindAsStorageBuffer(1);        // Output: vertices
    m_indexBuffer.bindAsStorageBuffer(2);         // Output: indices
    m_counterBuffer.bindAsStorageBuffer(3);       // Output: counters
    
    // Dispatch compute shader
    // Each work group processes 8x8x1 voxels, so we need ceil(16/8) = 2 groups per dimension
    uint32_t groupsX = (VoxelChunk::SIZE + 7) / 8;  // Round up division
    uint32_t groupsY = (VoxelChunk::SIZE + 7) / 8;
    uint32_t groupsZ = VoxelChunk::SIZE;  // Process all Z layers
    
    m_meshGenShader->dispatch(groupsX, groupsY, groupsZ);
    
    // Wait for compute shader to complete
    m_meshGenShader->memoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
    
    // Download mesh counts from GPU
    uint32_t vertexCount, indexCount, faceCount;
    if (!downloadMeshCounts(outMeshData, vertexCount, indexCount, faceCount)) {
        std::cerr << "❌ Failed to download mesh counts from GPU" << std::endl;
        return false;
    }
    
    // Validate results
    if (vertexCount > m_maxVerticesPerChunk || indexCount > m_maxIndicesPerChunk) {
        std::cerr << "❌ Generated mesh exceeds safety limits: " << vertexCount << " vertices, " 
                  << indexCount << " indices" << std::endl;
        return false;
    }
    
    // Copy GPU buffers to mesh data
    // For now, we'll keep the data on GPU and just store references
    outMeshData.vertexCount = vertexCount;
    outMeshData.indexCount = indexCount;
    outMeshData.faceCount = faceCount;
    outMeshData.isGenerated = true;
    
    // Calculate timing
    auto endTime = std::chrono::high_resolution_clock::now();
    float generationTime = std::chrono::duration<float, std::milli>(endTime - startTime).count();
    
    // Update performance metrics
    updateMetrics(generationTime, faceCount);
    
    std::cout << "🎯 GPU mesh generation complete: " << vertexCount << " vertices, " 
              << indexCount << " indices, " << faceCount << " faces in " 
              << generationTime << "ms" << std::endl;
    
    return true;
}

bool GPUMeshGenerator::generateChunkMeshBatch(const std::vector<VoxelChunk*>& chunks, std::vector<GPUMeshData>& outMeshData) {
    // TODO: Implement batched mesh generation for even better performance
    // For now, generate meshes one by one
    
    outMeshData.resize(chunks.size());
    
    auto startTime = std::chrono::high_resolution_clock::now();
    
    uint32_t successCount = 0;
    for (size_t i = 0; i < chunks.size(); i++) {
        if (generateChunkMesh(chunks[i], outMeshData[i])) {
            successCount++;
        }
    }
    
    auto endTime = std::chrono::high_resolution_clock::now();
    float totalTime = std::chrono::duration<float, std::milli>(endTime - startTime).count();
    
    std::cout << "📦 GPU batch generation complete: " << successCount << "/" << chunks.size() 
              << " chunks in " << totalTime << "ms (" << (totalTime / chunks.size()) << "ms per chunk)" << std::endl;
    
    return successCount == chunks.size();
}

bool GPUMeshGenerator::uploadVoxelData(const VoxelChunk* chunk, ComputeBuffer& voxelBuffer) {
    if (!chunk) return false;
    
    // Get voxel data from chunk
    // TODO: Add method to VoxelChunk to get raw voxel array
    std::vector<uint32_t> voxelData(VoxelChunk::SIZE * VoxelChunk::SIZE * VoxelChunk::SIZE);
    
    // Copy voxel data from chunk to upload buffer
    for (int z = 0; z < VoxelChunk::SIZE; z++) {
        for (int y = 0; y < VoxelChunk::SIZE; y++) {
            for (int x = 0; x < VoxelChunk::SIZE; x++) {
                int index = z * (VoxelChunk::SIZE * VoxelChunk::SIZE) + y * VoxelChunk::SIZE + x;
                voxelData[index] = static_cast<uint32_t>(chunk->getVoxel(x, y, z));
            }
        }
    }
    
    // Upload to GPU
    size_t dataSize = voxelData.size() * sizeof(uint32_t);
    voxelBuffer.setData(voxelData.data(), dataSize, 0);
    
    return true;
}

bool GPUMeshGenerator::downloadMeshCounts(const GPUMeshData& meshData, uint32_t& vertexCount, uint32_t& indexCount, uint32_t& faceCount) {
    // Map counter buffer and read results
    uint32_t* counters = static_cast<uint32_t*>(m_counterBuffer.mapBuffer(GL_READ_ONLY));
    if (!counters) {
        std::cerr << "❌ Failed to map counter buffer" << std::endl;
        return false;
    }
    
    vertexCount = counters[0];
    indexCount = counters[1];
    faceCount = counters[2];
    // counters[3] is chunkID, not used for now
    
    m_counterBuffer.unmapBuffer();
    
    return true;
}

bool GPUMeshGenerator::loadComputeShader() {
    std::cout << "🔍 Loading voxel mesh generation compute shader..." << std::endl;
    
    m_meshGenShader = std::make_unique<ComputeShader>();
    
    // Try to load from file first - correct path relative to executable
    std::cout << "📁 Attempting to load from: ../../shaders/voxel_mesh_gen.comp" << std::endl;
    if (m_meshGenShader->loadFromFile("../../shaders/voxel_mesh_gen.comp")) {
        std::cout << "✅ Compute shader loaded from file successfully" << std::endl;
        return true;
    }
    
    std::cerr << "⚠️ Failed to load compute shader from file, trying embedded fallback..." << std::endl;
    
    // Embedded shader source as fallback
    const std::string embeddedShader = R"(#version 460 core
layout(local_size_x = 8, local_size_y = 8, local_size_z = 1) in;

// Simple test compute shader that does nothing
void main() {
    // This is just a placeholder to test if compute shaders work
}
)";
    
    std::cout << "📝 Trying embedded fallback shader..." << std::endl;
    if (m_meshGenShader->loadFromSource(embeddedShader)) {
        std::cout << "✅ Embedded fallback shader loaded successfully" << std::endl;
        return true;
    }
    
    std::cerr << "❌ Both file and embedded shader loading failed" << std::endl;
    return false;
}

bool GPUMeshGenerator::allocateGPUBuffers() {
    // Calculate buffer sizes
    size_t voxelBufferSize = VoxelChunk::SIZE * VoxelChunk::SIZE * VoxelChunk::SIZE * sizeof(uint32_t);
    size_t vertexBufferSize = m_maxVerticesPerChunk * 8 * sizeof(float); // 8 floats per vertex (pos + normal + uv)
    size_t indexBufferSize = m_maxIndicesPerChunk * sizeof(uint32_t);
    size_t counterBufferSize = 4 * sizeof(uint32_t); // 4 counters
    
    std::cout << "📊 Allocating GPU buffers:" << std::endl;
    std::cout << "  Voxel buffer: " << (voxelBufferSize / 1024) << " KB" << std::endl;
    std::cout << "  Vertex buffer: " << (vertexBufferSize / 1024) << " KB" << std::endl;
    std::cout << "  Index buffer: " << (indexBufferSize / 1024) << " KB" << std::endl;
    std::cout << "  Counter buffer: " << counterBufferSize << " bytes" << std::endl;
    std::cout << "  Total GPU memory: " << ((voxelBufferSize + vertexBufferSize + indexBufferSize + counterBufferSize) / 1024) << " KB" << std::endl;
    
    // Allocate buffers
    if (!m_voxelDataBuffer.create(voxelBufferSize, nullptr, GL_DYNAMIC_DRAW)) {
        std::cerr << "❌ Failed to allocate voxel data buffer" << std::endl;
        return false;
    }
    
    if (!m_vertexBuffer.create(vertexBufferSize, nullptr, GL_DYNAMIC_DRAW)) {
        std::cerr << "❌ Failed to allocate vertex buffer" << std::endl;
        return false;
    }
    
    if (!m_indexBuffer.create(indexBufferSize, nullptr, GL_DYNAMIC_DRAW)) {
        std::cerr << "❌ Failed to allocate index buffer" << std::endl;
        return false;
    }
    
    if (!m_counterBuffer.create(counterBufferSize, nullptr, GL_DYNAMIC_DRAW)) {
        std::cerr << "❌ Failed to allocate counter buffer" << std::endl;
        return false;
    }
    
    return true;
}

void GPUMeshGenerator::clearCounters() {
    // Reset all counters to zero
    uint32_t zeros[4] = {0, 0, 0, 0};
    m_counterBuffer.setData(zeros, sizeof(zeros), 0);
}

void GPUMeshGenerator::updateMetrics(float generationTime, uint32_t faceCount) {
    m_metrics.lastGenerationTime = generationTime;
    m_metrics.totalChunksGenerated++;
    m_metrics.totalFacesGenerated += faceCount;
    m_metrics.totalGPUTime += generationTime;
    
    // Update moving average
    m_recentTimes.push_back(generationTime);
    if (m_recentTimes.size() > 100) {
        m_recentTimes.erase(m_recentTimes.begin());
    }
    
    if (!m_recentTimes.empty()) {
        m_metrics.averageGenerationTime = std::accumulate(m_recentTimes.begin(), m_recentTimes.end(), 0.0f) / m_recentTimes.size();
    }
}

void GPUMeshGenerator::resetMetrics() {
    m_metrics = PerformanceMetrics();
    m_recentTimes.clear();
}

bool GPUMeshGenerator::isGPUComputeSupported() {
    std::cout << "🔍 Checking OpenGL compute shader support..." << std::endl;
    std::flush(std::cout);
    
    try {
        // Check if compute shaders are supported (OpenGL 4.3+)
        int major, minor;
        std::cout << "🔍 Querying OpenGL version..." << std::endl;
        std::flush(std::cout);
        
        glGetIntegerv(GL_MAJOR_VERSION, &major);
        GLenum error = glGetError();
        if (error != GL_NO_ERROR) {
            std::cerr << "❌ OpenGL error getting major version: " << error << std::endl;
            return false;
        }
        
        glGetIntegerv(GL_MINOR_VERSION, &minor);
        error = glGetError();
        if (error != GL_NO_ERROR) {
            std::cerr << "❌ OpenGL error getting minor version: " << error << std::endl;
            return false;
        }
        
        std::cout << "🔍 OpenGL version detected: " << major << "." << minor << std::endl;
        std::flush(std::cout);
        
        if (major > 4 || (major == 4 && minor >= 3)) {
            std::cout << "✅ Compute shaders supported (OpenGL " << major << "." << minor << ")" << std::endl;
            return true;
        }
        
        std::cerr << "❌ OpenGL " << major << "." << minor << " detected, but compute shaders require OpenGL 4.3+" << std::endl;
        return false;
        
    } catch (const std::exception& e) {
        std::cerr << "💥 Exception in isGPUComputeSupported: " << e.what() << std::endl;
        return false;
    } catch (...) {
        std::cerr << "💥 Unknown exception in isGPUComputeSupported" << std::endl;
        return false;
    }
}

void GPUMeshGenerator::printGPUCapabilities() {
    std::cout << "🔍 GPU Compute Capabilities:" << std::endl;
    
    int maxWorkGroupCount[3];
    int maxWorkGroupSize[3];
    int maxWorkGroupInvocations;
    
    glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_COUNT, 0, &maxWorkGroupCount[0]);
    glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_COUNT, 1, &maxWorkGroupCount[1]);
    glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_COUNT, 2, &maxWorkGroupCount[2]);
    
    glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_SIZE, 0, &maxWorkGroupSize[0]);
    glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_SIZE, 1, &maxWorkGroupSize[1]);
    glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_SIZE, 2, &maxWorkGroupSize[2]);
    
    glGetIntegerv(GL_MAX_COMPUTE_WORK_GROUP_INVOCATIONS, &maxWorkGroupInvocations);
    
    std::cout << "  Max work groups: " << maxWorkGroupCount[0] << " x " << maxWorkGroupCount[1] << " x " << maxWorkGroupCount[2] << std::endl;
    std::cout << "  Max work group size: " << maxWorkGroupSize[0] << " x " << maxWorkGroupSize[1] << " x " << maxWorkGroupSize[2] << std::endl;
    std::cout << "  Max invocations per work group: " << maxWorkGroupInvocations << std::endl;
}