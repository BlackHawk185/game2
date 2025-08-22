#include "VoxelRenderer.h"
#include "../Input/Camera.h"
#include "../World/VoxelChunk.h"

// Windows OpenGL fix
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <GL/gl.h>

#include <iostream>
#include <chrono>
#include <cmath>

VoxelRenderer::VoxelRenderer() {
    std::cout << "🎨 VoxelRenderer initialized with modern optimizations!" << std::endl;
    
    // Enable basic OpenGL optimizations immediately
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glFrontFace(GL_CCW);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    
    std::cout << "✅ Basic GPU culling enabled (backface culling)" << std::endl;
}

VoxelRenderer::~VoxelRenderer() = default;

void VoxelRenderer::renderChunks(const std::vector<VoxelChunk*>& chunks, const Camera& camera, float aspect) {
    auto startTime = std::chrono::high_resolution_clock::now();
    m_stats.reset();
    
    // Frustum culling is now handled by global FrustumCuller
    // (Remove local frustum update)
    
    m_stats.chunksConsidered = static_cast<uint32_t>(chunks.size());
    
    for (const auto* chunk : chunks) {
        if (!chunk) continue;
        
        // Distance culling
        if (static_cast<int>(m_cullingMode) & static_cast<int>(CullingMode::DISTANCE_CULLING)) {
            if (distanceCullChunk(*chunk, camera)) {
                continue;
            }
        }
        
        // Frustum culling  
        if (static_cast<int>(m_cullingMode) & static_cast<int>(CullingMode::FRUSTUM_CULLING)) {
            if (frustumCullChunk(*chunk, camera, aspect)) {
                continue;
            }
        }
        
        renderChunk(*chunk, camera);
        m_stats.chunksRendered++;
    }
    
    auto endTime = std::chrono::high_resolution_clock::now();
    m_stats.renderTimeMs = std::chrono::duration<float, std::milli>(endTime - startTime).count();
}

void VoxelRenderer::renderChunk(const VoxelChunk& chunk, const Camera& camera) {
    // For now, let's just render the chunk using its existing render method
    // TODO: Implement optimized rendering here
    const_cast<VoxelChunk&>(chunk).render();
    m_stats.facesRendered += 36; // Estimate for stats
    m_stats.facesConsidered += 36;
    m_stats.drawCalls++;
}

bool VoxelRenderer::shouldRenderFace(const VoxelChunk& chunk, int x, int y, int z, int faceIndex) {
    // **CRITICAL OPTIMIZATION: Face culling**
    // Only render faces that are exposed (adjacent voxel is air)
    
    if (!(static_cast<int>(m_cullingMode) & static_cast<int>(CullingMode::FACE_CULLING))) {
        return true; // No culling - render all faces
    }
    
    // Check adjacent voxel based on face direction
    int adjX = x, adjY = y, adjZ = z;
    
    switch (faceIndex) {
        case 0: adjX--; break; // Left face
        case 1: adjX++; break; // Right face  
        case 2: adjY--; break; // Bottom face
        case 3: adjY++; break; // Top face
        case 4: adjZ--; break; // Back face
        case 5: adjZ++; break; // Front face
    }
    
    // Boundary faces are always visible
    if (adjX < 0 || adjX >= 32 || adjY < 0 || adjY >= 32 || adjZ < 0 || adjZ >= 32) {
        return true;
    }
    
    // Face is hidden if adjacent voxel exists
    return chunk.getVoxel(adjX, adjY, adjZ) == 0;
}

void VoxelRenderer::renderVoxelFaces(const VoxelChunk& chunk, int x, int y, int z, const Vec3& worldPos) {
    // **FACE CULLING OPTIMIZATION**
    // Only render visible faces - MASSIVE performance gain!
    
    for (int face = 0; face < 6; face++) {
        if (!shouldRenderFace(chunk, x, y, z, face)) {
            continue; // Skip hidden face
        }
        
        m_stats.facesRendered++;
        
        // Render the face (immediate mode for now - will optimize later)
        glBegin(GL_QUADS);
        
        // Set color based on face direction for visual debugging
        switch (face) {
            case 0: glColor3f(0.8f, 0.4f, 0.4f); break; // Left - red
            case 1: glColor3f(0.4f, 0.8f, 0.4f); break; // Right - green  
            case 2: glColor3f(0.4f, 0.4f, 0.8f); break; // Bottom - blue
            case 3: glColor3f(0.9f, 0.9f, 0.4f); break; // Top - yellow
            case 4: glColor3f(0.8f, 0.4f, 0.8f); break; // Back - magenta
            case 5: glColor3f(0.4f, 0.8f, 0.8f); break; // Front - cyan
        }
        
        // Generate face vertices based on direction
        float x1 = worldPos.x, y1 = worldPos.y, z1 = worldPos.z;
        float x2 = x1 + 1.0f, y2 = y1 + 1.0f, z2 = z1 + 1.0f;
        
        switch (face) {
            case 0: // Left face (-X)
                glVertex3f(x1, y1, z1); glVertex3f(x1, y2, z1); 
                glVertex3f(x1, y2, z2); glVertex3f(x1, y1, z2);
                break;
            case 1: // Right face (+X)
                glVertex3f(x2, y1, z2); glVertex3f(x2, y2, z2);
                glVertex3f(x2, y2, z1); glVertex3f(x2, y1, z1);
                break;
            case 2: // Bottom face (-Y)
                glVertex3f(x1, y1, z1); glVertex3f(x1, y1, z2);
                glVertex3f(x2, y1, z2); glVertex3f(x2, y1, z1);
                break;
            case 3: // Top face (+Y)
                glVertex3f(x1, y2, z2); glVertex3f(x1, y2, z1);
                glVertex3f(x2, y2, z1); glVertex3f(x2, y2, z2);
                break;
            case 4: // Back face (-Z)
                glVertex3f(x2, y1, z1); glVertex3f(x2, y2, z1);
                glVertex3f(x1, y2, z1); glVertex3f(x1, y1, z1);
                break;
            case 5: // Front face (+Z)
                glVertex3f(x1, y1, z2); glVertex3f(x1, y2, z2);
                glVertex3f(x2, y2, z2); glVertex3f(x2, y1, z2);
                break;
        }
        
        glEnd();
    }
}

bool VoxelRenderer::distanceCullChunk(const VoxelChunk& chunk, const Camera& camera) {
    // TODO: Get actual world position from chunk - for now assume Vec3(0,0,0)
    Vec3 chunkWorldPos(0, 0, 0); // Placeholder
    float distance = (camera.position - chunkWorldPos).length();
    return distance > m_renderDistance;
}

bool VoxelRenderer::frustumCullChunk(const VoxelChunk& chunk, const Camera& camera, float aspect) {
    // TODO: Get actual world position from chunk - for now assume Vec3(0,0,0) 
    Vec3 chunkWorldPos(0, 0, 0); // Placeholder
    Vec3 chunkCenter = chunkWorldPos + Vec3(16.0f, 16.0f, 16.0f);
    float chunkRadius = 27.7f; // sqrt(16²+16²+16²) for 32³ chunk
    
    // Very basic frustum test - distance from camera
    Vec3 toChunk = chunkCenter - camera.position;
    float distance = toChunk.length();
    
    // Simple field-of-view test
    Vec3 forward = camera.front.normalized();
    float dot = toChunk.normalized().dot(forward);
    
    // If chunk is behind camera or too far to the side, cull it
    return dot < -0.2f; // Rough 120° FOV
}

int VoxelRenderer::selectLOD(float distance) {
    if (distance < m_lodNear) return 0;      // Full detail
    if (distance < m_lodMid) return 1;       // Half detail  
    if (distance < m_lodFar) return 2;       // Quarter detail
    return 3; // Minimal detail
}

void VoxelRenderer::renderVoxelLOD(const VoxelChunk& chunk, int x, int y, int z, const Vec3& worldPos, int lod) {
    // LOD optimization - skip some voxels at distance
    if (lod > 0 && ((x + y + z) % (lod + 1)) != 0) {
        return; // Skip this voxel for LOD
    }
    
    // Render simplified version
    renderVoxelFaces(chunk, x, y, z, worldPos);
}

// Frustum culling is now handled by FrustumCuller.h/cpp - no local implementation needed

// GPU Capabilities detection (future-proofing)
namespace GPUCapabilities {
    bool hasRaytracing() {
        // TODO: Query for RTX/RDNA2+ support
        return false;
    }
    
    bool hasTensorCores() {
        // TODO: Query for AI acceleration support  
        return false;
    }
    
    bool hasComputeShaders() {
        // TODO: Query OpenGL 4.3+ or Vulkan compute
        return true; // Assume modern GPU
    }
    
    bool hasMeshShaders() {
        // TODO: Query for mesh shader support
        return false;
    }
    
    void logCapabilities() {
        std::cout << "🔍 GPU Capabilities:" << std::endl;
        std::cout << "  Raytracing: " << (hasRaytracing() ? "✅" : "❌") << std::endl;
        std::cout << "  Tensor Cores: " << (hasTensorCores() ? "✅" : "❌") << std::endl;
        std::cout << "  Compute Shaders: " << (hasComputeShaders() ? "✅" : "❌") << std::endl;
        std::cout << "  Mesh Shaders: " << (hasMeshShaders() ? "✅" : "❌") << std::endl;
    }
}
