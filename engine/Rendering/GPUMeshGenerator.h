// GPUMeshGenerator.h - GPU-accelerated voxel mesh generation
#pragma once

#include "ComputeShader.h"
#include "../World/VoxelChunk.h"
#include <memory>
#include <vector>

struct GPUMeshData {
    ComputeBuffer vertexBuffer;   // GPU vertex data
    ComputeBuffer indexBuffer;    // GPU index data
    ComputeBuffer counterBuffer;  // GPU counters (vertex/index/face counts)
    
    uint32_t vertexCount = 0;
    uint32_t indexCount = 0;
    uint32_t faceCount = 0;
    bool isGenerated = false;
};

class GPUMeshGenerator {
public:
    GPUMeshGenerator();
    ~GPUMeshGenerator();

    // Initialization
    bool initialize();
    void cleanup();

    // GPU mesh generation
    bool generateChunkMesh(VoxelChunk* chunk, GPUMeshData& outMeshData);
    bool generateChunkMeshBatch(const std::vector<VoxelChunk*>& chunks, std::vector<GPUMeshData>& outMeshData);

    // Utility functions
    bool uploadVoxelData(const VoxelChunk* chunk, ComputeBuffer& voxelBuffer);
    bool downloadMeshCounts(const GPUMeshData& meshData, uint32_t& vertexCount, uint32_t& indexCount, uint32_t& faceCount);
    
    // Performance monitoring
    struct PerformanceMetrics {
        float lastGenerationTime = 0.0f;     // Last mesh generation time (ms)
        float averageGenerationTime = 0.0f;  // Average mesh generation time (ms)
        uint32_t totalChunksGenerated = 0;   // Total chunks processed
        uint32_t totalFacesGenerated = 0;    // Total faces generated
        float totalGPUTime = 0.0f;           // Total GPU processing time (ms)
    };
    
    const PerformanceMetrics& getMetrics() const { return m_metrics; }
    void resetMetrics();

    // Settings
    void setVoxelSize(float size) { m_voxelSize = size; }
    float getVoxelSize() const { return m_voxelSize; }

    // GPU capability checking
    static bool isGPUComputeSupported();
    static void printGPUCapabilities();

private:
    // Core components
    std::unique_ptr<ComputeShader> m_meshGenShader;
    
    // GPU buffers (reused across mesh generations)
    ComputeBuffer m_voxelDataBuffer;    // Input: voxel data
    ComputeBuffer m_vertexBuffer;       // Output: vertex data  
    ComputeBuffer m_indexBuffer;        // Output: index data
    ComputeBuffer m_counterBuffer;      // Output: counters
    
    // Configuration
    float m_voxelSize = 1.0f;
    uint32_t m_maxVerticesPerChunk = 65536;  // Max vertices per chunk (safety limit)
    uint32_t m_maxIndicesPerChunk = 98304;   // Max indices per chunk (1.5x vertices)
    
    // Performance tracking
    PerformanceMetrics m_metrics;
    std::vector<float> m_recentTimes;  // For moving average calculation
    
    // Helper functions
    bool loadComputeShader();
    bool allocateGPUBuffers();
    void updateMetrics(float generationTime, uint32_t faceCount);
    
    // GPU memory management
    bool ensureBufferCapacity(size_t requiredVertexBytes, size_t requiredIndexBytes);
    void clearCounters();
    
    // Debug and validation
    bool validateMeshData(const GPUMeshData& meshData);
    void logGPUMemoryUsage();
};