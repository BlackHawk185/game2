#pragma once

#include "../Math/Vec3.h"
#include <vector>
#include <cstdint>

namespace Engine {
namespace World {

// Forward declarations
class Chunk;

/**
 * Represents a material type for voxels
 */
enum class VoxelMaterial : uint8_t {
    AIR = 0,
    DIRT = 1,
    GRASS = 2,
    STONE = 3
};

/**
 * Represents a face direction for voxel face culling
 */
enum class FaceDirection : uint8_t {
    FRONT = 0,   // +Z
    BACK = 1,    // -Z
    RIGHT = 2,   // +X
    LEFT = 3,    // -X
    TOP = 4,     // +Y
    BOTTOM = 5   // -Y
};

/**
 * Represents a quad face for greedy meshing
 */
struct MeshQuad {
    Vec3 vertices[4];       // 4 corners of the quad
    Vec3 normal;            // Face normal
    Vec3 textureUV[4];      // Texture coordinates (u,v,material_id)
    Vec3 lightmapUV[4];     // Lightmap coordinates (u,v,0)
    VoxelMaterial material; // Material type
    
    MeshQuad() : normal(0, 0, 0), material(VoxelMaterial::AIR) {
        for (int i = 0; i < 4; i++) {
            vertices[i] = Vec3(0, 0, 0);
            textureUV[i] = Vec3(0, 0, 0);
            lightmapUV[i] = Vec3(0, 0, 0);
        }
    }
};

/**
 * Data structure for physics collision mesh
 */
struct PhysicsMesh {
    std::vector<Vec3> vertices;
    std::vector<Vec3> normals;
    std::vector<uint32_t> indices;
    
    void clear() {
        vertices.clear();
        normals.clear();
        indices.clear();
    }
    
    size_t getVertexCount() const { return vertices.size(); }
    size_t getTriangleCount() const { return indices.size() / 3; }
};

/**
 * Data structure for rendering mesh
 */
struct RenderMesh {
    std::vector<Vec3> vertices;
    std::vector<Vec3> normals;
    std::vector<Vec3> textureUV;    // (u, v, material_id)
    std::vector<Vec3> lightmapUV;   // (u, v, 0)
    std::vector<uint32_t> indices;
    
    void clear() {
        vertices.clear();
        normals.clear();
        textureUV.clear();
        lightmapUV.clear();
        indices.clear();
    }
    
    size_t getVertexCount() const { return vertices.size(); }
    size_t getTriangleCount() const { return indices.size() / 3; }
};

/**
 * Unified mesh generator with face culling and greedy meshing
 * Generates both physics and rendering data in a single pass
 */
class MeshGenerator {
public:
    MeshGenerator();
    ~MeshGenerator();
    
    /**
     * Generate mesh data from a chunk
     * @param chunk The chunk to generate mesh from
     * @param generateRenderData Whether to generate rendering data (client-only)
     * @param physicsMesh Output physics mesh (always generated)
     * @param renderMesh Output render mesh (only if generateRenderData=true)
     */
    void generateMesh(const Chunk& chunk, 
                     bool generateRenderData,
                     PhysicsMesh& physicsMesh, 
                     RenderMesh& renderMesh);
    
    /**
     * Generate mesh data for multiple chunks with optimization
     * This allows cross-chunk face culling and better greedy meshing
     */
    void generateMeshMultiChunk(const std::vector<const Chunk*>& chunks,
                               bool generateRenderData,
                               std::vector<PhysicsMesh>& physicsMeshes,
                               std::vector<RenderMesh>& renderMeshes);

private:
    // Face culling helpers
    bool shouldCullFace(const Chunk& chunk, int x, int y, int z, FaceDirection direction) const;
    VoxelMaterial getVoxelMaterial(const Chunk& chunk, int x, int y, int z) const;
    
    // Greedy meshing helpers
    MeshQuad expandFaceGreedily(const Chunk& chunk, int startX, int startY, int startZ, 
                               FaceDirection direction, VoxelMaterial material);
    bool canExpandQuad(const Chunk& chunk, const MeshQuad& quad, int deltaX, int deltaY, int deltaZ,
                      FaceDirection direction, VoxelMaterial material);
    
    // Mesh generation helpers
    void addQuadToPhysics(const MeshQuad& quad, PhysicsMesh& mesh);
    void addQuadToRender(const MeshQuad& quad, RenderMesh& mesh);
    
    // Face direction utilities
    Vec3 getFaceNormal(FaceDirection direction) const;
    void getFaceVertices(int x, int y, int z, FaceDirection direction, Vec3 vertices[4]) const;
    void getFaceTextureUV(FaceDirection direction, VoxelMaterial material, Vec3 uvCoords[4]) const;
    void getFaceLightmapUV(int chunkX, int chunkY, int chunkZ, FaceDirection direction, Vec3 uvCoords[4]) const;
    
    // Optimization tracking
    mutable uint32_t m_totalFacesConsidered;
    mutable uint32_t m_facesCulled;
    mutable uint32_t m_quadsGenerated;
    mutable uint32_t m_greedyMerges;
    
public:
    // Performance statistics
    void resetStatistics();
    uint32_t getTotalFacesConsidered() const { return m_totalFacesConsidered; }
    uint32_t getFacesCulled() const { return m_facesCulled; }
    uint32_t getQuadsGenerated() const { return m_quadsGenerated; }
    uint32_t getGreedyMerges() const { return m_greedyMerges; }
    float getCullPercentage() const { 
        return m_totalFacesConsidered > 0 ? 
            (float)m_facesCulled / m_totalFacesConsidered * 100.0f : 0.0f; 
    }
};

} // namespace World
} // namespace Engine
