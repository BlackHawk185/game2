#include "MeshGenerator.h"
#include "Chunk.h"
#include <iostream>
#include <unordered_set>

namespace Engine {
namespace World {

MeshGenerator::MeshGenerator() 
    : m_totalFacesConsidered(0)
    , m_facesCulled(0)
    , m_quadsGenerated(0)
    , m_greedyMerges(0)
{
}

MeshGenerator::~MeshGenerator() {
}

void MeshGenerator::resetStatistics() {
    m_totalFacesConsidered = 0;
    m_facesCulled = 0;
    m_quadsGenerated = 0;
    m_greedyMerges = 0;
}

void MeshGenerator::generateMesh(const Chunk& chunk, 
                                bool generateRenderData,
                                PhysicsMesh& physicsMesh, 
                                RenderMesh& renderMesh) {
    // Clear output meshes
    physicsMesh.clear();
    renderMesh.clear();
    
    resetStatistics();
    
    std::cout << "MeshGenerator: Generating mesh for chunk (" 
              << chunk.getChunkX() << ", " << chunk.getChunkY() << ", " << chunk.getChunkZ() 
              << ") - RenderData: " << (generateRenderData ? "YES" : "NO") << std::endl;
    
    // Track processed faces to avoid duplicates in greedy meshing
    std::unordered_set<uint64_t> processedFaces;
    
    // Iterate through all voxels in the chunk
    for (int x = 0; x < 16; x++) {
        for (int y = 0; y < 16; y++) {
            for (int z = 0; z < 16; z++) {
                VoxelMaterial material = getVoxelMaterial(chunk, x, y, z);
                
                // Skip air voxels
                if (material == VoxelMaterial::AIR) {
                    continue;
                }
                
                // Check all 6 faces of this voxel
                for (int faceDir = 0; faceDir < 6; faceDir++) {
                    FaceDirection direction = static_cast<FaceDirection>(faceDir);
                    m_totalFacesConsidered++;
                    
                    // Face culling - skip faces that are against solid voxels
                    if (shouldCullFace(chunk, x, y, z, direction)) {
                        m_facesCulled++;
                        continue;
                    }
                    
                    // Create unique ID for this face to avoid double-processing
                    uint64_t faceId = ((uint64_t)x << 32) | ((uint64_t)y << 16) | ((uint64_t)z << 8) | (uint64_t)faceDir;
                    if (processedFaces.find(faceId) != processedFaces.end()) {
                        continue;
                    }
                    
                    // Generate greedy mesh quad
                    MeshQuad quad = expandFaceGreedily(chunk, x, y, z, direction, material);
                    
                    // Mark all faces covered by this quad as processed
                    // TODO: Implement proper greedy meshing face marking
                    processedFaces.insert(faceId);
                    
                    // Add quad to physics mesh (always)
                    addQuadToPhysics(quad, physicsMesh);
                    m_quadsGenerated++;
                    
                    // Add quad to render mesh (client only)
                    if (generateRenderData) {
                        addQuadToRender(quad, renderMesh);
                    }
                }
            }
        }
    }
    
    std::cout << "MeshGenerator: Statistics - Total faces: " << m_totalFacesConsidered 
              << ", Culled: " << m_facesCulled << " (" << getCullPercentage() << "%)"
              << ", Quads: " << m_quadsGenerated << std::endl;
    
    std::cout << "MeshGenerator: Output - Physics: " << physicsMesh.getVertexCount() << " vertices, "
              << physicsMesh.getTriangleCount() << " triangles";
    if (generateRenderData) {
        std::cout << " | Render: " << renderMesh.getVertexCount() << " vertices, "
                  << renderMesh.getTriangleCount() << " triangles";
    }
    std::cout << std::endl;
}

void MeshGenerator::generateMeshMultiChunk(const std::vector<const Chunk*>& chunks,
                                          bool generateRenderData,
                                          std::vector<PhysicsMesh>& physicsMeshes,
                                          std::vector<RenderMesh>& renderMeshes) {
    // TODO: Implement multi-chunk optimization
    // For now, just generate each chunk individually
    physicsMeshes.resize(chunks.size());
    renderMeshes.resize(chunks.size());
    
    for (size_t i = 0; i < chunks.size(); i++) {
        generateMesh(*chunks[i], generateRenderData, physicsMeshes[i], renderMeshes[i]);
    }
}

bool MeshGenerator::shouldCullFace(const Chunk& chunk, int x, int y, int z, FaceDirection direction) const {
    // Calculate neighbor position
    int neighborX = x;
    int neighborY = y;
    int neighborZ = z;
    
    switch (direction) {
        case FaceDirection::FRONT:  neighborZ += 1; break;  // +Z
        case FaceDirection::BACK:   neighborZ -= 1; break;  // -Z
        case FaceDirection::RIGHT:  neighborX += 1; break;  // +X
        case FaceDirection::LEFT:   neighborX -= 1; break;  // -X
        case FaceDirection::TOP:    neighborY += 1; break;  // +Y
        case FaceDirection::BOTTOM: neighborY -= 1; break;  // -Y
    }
    
    // If neighbor is outside chunk bounds, don't cull (expose face)
    if (neighborX < 0 || neighborX >= 16 || 
        neighborY < 0 || neighborY >= 16 || 
        neighborZ < 0 || neighborZ >= 16) {
        return false;
    }
    
    // Cull face if neighbor is solid (not air)
    VoxelMaterial neighborMaterial = getVoxelMaterial(chunk, neighborX, neighborY, neighborZ);
    return neighborMaterial != VoxelMaterial::AIR;
}

VoxelMaterial MeshGenerator::getVoxelMaterial(const Chunk& chunk, int x, int y, int z) const {
    // Get voxel data from chunk
    uint8_t voxelData = chunk.getBlock(x, y, z);
    
    // Convert to material enum
    if (voxelData == 0) return VoxelMaterial::AIR;
    if (voxelData == 1) return VoxelMaterial::DIRT;
    return VoxelMaterial::DIRT; // Default to dirt for now
}

MeshQuad MeshGenerator::expandFaceGreedily(const Chunk& chunk, int startX, int startY, int startZ, 
                                          FaceDirection direction, VoxelMaterial material) {
    MeshQuad quad;
    quad.material = material;
    quad.normal = getFaceNormal(direction);
    
    // For now, just create a single-voxel face (no greedy expansion yet)
    // TODO: Implement actual greedy meshing algorithm
    getFaceVertices(startX, startY, startZ, direction, quad.vertices);
    getFaceTextureUV(direction, material, quad.textureUV);
    getFaceLightmapUV(startX, startY, startZ, direction, quad.lightmapUV);
    
    return quad;
}

bool MeshGenerator::canExpandQuad(const Chunk& chunk, const MeshQuad& quad, int deltaX, int deltaY, int deltaZ,
                                 FaceDirection direction, VoxelMaterial material) {
    // TODO: Implement greedy meshing expansion logic
    return false;
}

void MeshGenerator::addQuadToPhysics(const MeshQuad& quad, PhysicsMesh& mesh) {
    uint32_t baseIndex = static_cast<uint32_t>(mesh.vertices.size());
    
    // Add vertices (4 corners of quad)
    for (int i = 0; i < 4; i++) {
        mesh.vertices.push_back(quad.vertices[i]);
        mesh.normals.push_back(quad.normal);
    }
    
    // Add indices for two triangles (quad = 2 triangles)
    // Triangle 1: 0, 1, 2
    mesh.indices.push_back(baseIndex + 0);
    mesh.indices.push_back(baseIndex + 1);
    mesh.indices.push_back(baseIndex + 2);
    
    // Triangle 2: 0, 2, 3
    mesh.indices.push_back(baseIndex + 0);
    mesh.indices.push_back(baseIndex + 2);
    mesh.indices.push_back(baseIndex + 3);
}

void MeshGenerator::addQuadToRender(const MeshQuad& quad, RenderMesh& mesh) {
    uint32_t baseIndex = static_cast<uint32_t>(mesh.vertices.size());
    
    // Add vertices and all rendering data
    for (int i = 0; i < 4; i++) {
        mesh.vertices.push_back(quad.vertices[i]);
        mesh.normals.push_back(quad.normal);
        mesh.textureUV.push_back(quad.textureUV[i]);
        mesh.lightmapUV.push_back(quad.lightmapUV[i]);
    }
    
    // Add indices for two triangles (same as physics)
    mesh.indices.push_back(baseIndex + 0);
    mesh.indices.push_back(baseIndex + 1);
    mesh.indices.push_back(baseIndex + 2);
    
    mesh.indices.push_back(baseIndex + 0);
    mesh.indices.push_back(baseIndex + 2);
    mesh.indices.push_back(baseIndex + 3);
}

Vec3 MeshGenerator::getFaceNormal(FaceDirection direction) const {
    switch (direction) {
        case FaceDirection::FRONT:  return Vec3(0, 0, 1);   // +Z
        case FaceDirection::BACK:   return Vec3(0, 0, -1);  // -Z
        case FaceDirection::RIGHT:  return Vec3(1, 0, 0);   // +X
        case FaceDirection::LEFT:   return Vec3(-1, 0, 0);  // -X
        case FaceDirection::TOP:    return Vec3(0, 1, 0);   // +Y
        case FaceDirection::BOTTOM: return Vec3(0, -1, 0);  // -Y
        default: return Vec3(0, 0, 0);
    }
}

void MeshGenerator::getFaceVertices(int x, int y, int z, FaceDirection direction, Vec3 vertices[4]) const {
    // Convert chunk-local coordinates to world coordinates
    float worldX = static_cast<float>(x);
    float worldY = static_cast<float>(y);
    float worldZ = static_cast<float>(z);
    
    switch (direction) {
        case FaceDirection::FRONT: // +Z face
            vertices[0] = Vec3(worldX,     worldY,     worldZ + 1); // Bottom-left
            vertices[1] = Vec3(worldX + 1, worldY,     worldZ + 1); // Bottom-right
            vertices[2] = Vec3(worldX + 1, worldY + 1, worldZ + 1); // Top-right
            vertices[3] = Vec3(worldX,     worldY + 1, worldZ + 1); // Top-left
            break;
            
        case FaceDirection::BACK: // -Z face
            vertices[0] = Vec3(worldX + 1, worldY,     worldZ); // Bottom-left
            vertices[1] = Vec3(worldX,     worldY,     worldZ); // Bottom-right
            vertices[2] = Vec3(worldX,     worldY + 1, worldZ); // Top-right
            vertices[3] = Vec3(worldX + 1, worldY + 1, worldZ); // Top-left
            break;
            
        case FaceDirection::RIGHT: // +X face
            vertices[0] = Vec3(worldX + 1, worldY,     worldZ + 1); // Bottom-left
            vertices[1] = Vec3(worldX + 1, worldY,     worldZ);     // Bottom-right
            vertices[2] = Vec3(worldX + 1, worldY + 1, worldZ);     // Top-right
            vertices[3] = Vec3(worldX + 1, worldY + 1, worldZ + 1); // Top-left
            break;
            
        case FaceDirection::LEFT: // -X face
            vertices[0] = Vec3(worldX, worldY,     worldZ);     // Bottom-left
            vertices[1] = Vec3(worldX, worldY,     worldZ + 1); // Bottom-right
            vertices[2] = Vec3(worldX, worldY + 1, worldZ + 1); // Top-right
            vertices[3] = Vec3(worldX, worldY + 1, worldZ);     // Top-left
            break;
            
        case FaceDirection::TOP: // +Y face
            vertices[0] = Vec3(worldX,     worldY + 1, worldZ + 1); // Bottom-left
            vertices[1] = Vec3(worldX + 1, worldY + 1, worldZ + 1); // Bottom-right
            vertices[2] = Vec3(worldX + 1, worldY + 1, worldZ);     // Top-right
            vertices[3] = Vec3(worldX,     worldY + 1, worldZ);     // Top-left
            break;
            
        case FaceDirection::BOTTOM: // -Y face
            vertices[0] = Vec3(worldX,     worldY, worldZ);     // Bottom-left
            vertices[1] = Vec3(worldX + 1, worldY, worldZ);     // Bottom-right
            vertices[2] = Vec3(worldX + 1, worldY, worldZ + 1); // Top-right
            vertices[3] = Vec3(worldX,     worldY, worldZ + 1); // Top-left
            break;
    }
}

void MeshGenerator::getFaceTextureUV(FaceDirection direction, VoxelMaterial material, Vec3 uvCoords[4]) const {
    // Simple UV mapping - full texture per face
    // Z component stores material ID for texture atlas lookup
    float materialId = static_cast<float>(material);
    
    uvCoords[0] = Vec3(0.0f, 0.0f, materialId); // Bottom-left
    uvCoords[1] = Vec3(1.0f, 0.0f, materialId); // Bottom-right
    uvCoords[2] = Vec3(1.0f, 1.0f, materialId); // Top-right
    uvCoords[3] = Vec3(0.0f, 1.0f, materialId); // Top-left
}

void MeshGenerator::getFaceLightmapUV(int chunkX, int chunkY, int chunkZ, FaceDirection direction, Vec3 uvCoords[4]) const {
    // TODO: Calculate proper lightmap UV coordinates
    // For now, use simple mapping based on world position
    float u = static_cast<float>(chunkX) / 16.0f;
    float v = static_cast<float>(chunkZ) / 16.0f;
    
    uvCoords[0] = Vec3(u,         v,         0.0f);
    uvCoords[1] = Vec3(u + 0.06f, v,         0.0f);
    uvCoords[2] = Vec3(u + 0.06f, v + 0.06f, 0.0f);
    uvCoords[3] = Vec3(u,         v + 0.06f, 0.0f);
}

} // namespace World
} // namespace Engine
