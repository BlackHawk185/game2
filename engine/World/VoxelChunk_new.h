// VoxelChunk.h - 16x16x16 dynamic physics-enabled voxel chunks with light mapping
#pragma once

#include "../Math/Vec3.h"
#include <array>
#include <vector>
#include <cstdint>

// Forward declaration for OpenGL types
using GLuint = uint32_t;

struct Vertex
{
    float x, y, z;     // Position
    float nx, ny, nz;  // Normal
    float u, v;        // Texture coordinates
    float lu, lv;      // Light map coordinates
    float ao;          // Ambient occlusion (0.0 = fully occluded, 1.0 = no occlusion)
};

struct VoxelMesh
{
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;
    uint32_t VAO, VBO, EBO;  // OpenGL handles
    bool needsUpdate = true;
};

// Light mapping data for the chunk
struct ChunkLightMap 
{
    static constexpr int LIGHTMAP_SIZE = 32;
    uint32_t textureHandle = 0;  // OpenGL texture handle
    std::array<uint8_t, LIGHTMAP_SIZE * LIGHTMAP_SIZE * 3> data;  // RGB data (32x32 = 3072 bytes)
    bool needsUpdate = true;
    
    ~ChunkLightMap() {
        if (textureHandle != 0) {
            // Note: glDeleteTextures should only be called from OpenGL context
            // This will be handled by proper cleanup in VoxelChunk destructor
        }
    }
};

struct CollisionFace
{
    Vec3 position;  // Center position of the face
    Vec3 normal;    // Normal vector of the face
};

struct CollisionMesh
{
    std::vector<CollisionFace> faces;
    bool needsUpdate = true;
};

class VoxelChunk
{
   public:
    static constexpr int SIZE = 16;  // 16x16x16 chunks
    static constexpr int VOLUME = SIZE * SIZE * SIZE;

    VoxelChunk();
    ~VoxelChunk();

    // Voxel data access
    uint8_t getVoxel(int x, int y, int z) const;
    void setVoxel(int x, int y, int z, uint8_t type);

    // Network serialization - get raw voxel data for transmission
    const uint8_t* getRawVoxelData() const
    {
        return voxels.data();
    }

    // Mesh generation and management
    void generateMesh();
    void updatePhysicsMesh();

    // Rendering
    void render();                                        // Render at origin (0,0,0)
    void render(const Vec3& worldOffset);                 // Render at island's world position
    void renderLOD(int lodLevel, const Vec3& cameraPos);  // New LOD rendering
    bool isDirty() const
    {
        return meshDirty;
    }

    // **LOD AND CULLING SUPPORT**
    int calculateLOD(const Vec3& cameraPos) const;
    bool shouldRender(const Vec3& cameraPos, float maxDistance = 256.0f) const;

    // Physics integration
    uint32_t getPhysicsBodyID() const
    {
        return physicsBodyID;
    }

    // Collision detection methods
    const CollisionMesh& getCollisionMesh() const
    {
        return collisionMesh;
    }
    
    // Mesh access for VBO rendering
    VoxelMesh& getMesh() { return mesh; }
    const VoxelMesh& getMesh() const { return mesh; }
    
    // Light mapping access
    ChunkLightMap& getLightMap() { return lightMap; }
    const ChunkLightMap& getLightMap() const { return lightMap; }
    
    void buildCollisionMesh();
    bool checkRayCollision(const Vec3& rayOrigin, const Vec3& rayDirection, float maxDistance,
                           Vec3& hitPoint, Vec3& hitNormal) const;

    // Island generation (for floating terrain) - noise-based islands by default
    void generateFloatingIsland(int seed, bool useNoise = false);

   private:
    std::array<uint8_t, VOLUME> voxels;
    VoxelMesh mesh;
    CollisionMesh collisionMesh;
    ChunkLightMap lightMap;  // NEW: Light mapping data
    bool meshDirty = true;
    uint32_t physicsBodyID = 0;  // Physics body ID

    // **FACE CULLING OPTIMIZATION HELPER**
    bool shouldRenderFace(int x, int y, int z, int faceDir) const;

    // Mesh generation helpers
    void addQuad(std::vector<Vertex>& vertices, std::vector<uint32_t>& indices, float x, float y,
                 float z, int face, uint8_t blockType);
    void addCollisionQuad(float x, float y, float z, int face);
    bool isVoxelSolid(int x, int y, int z) const;
    
    // Light mapping utilities
    float computeAmbientOcclusion(int x, int y, int z, int face) const;
    void generateLightMap();  // Generate light map using simple raytracing
    void updateLightMapTexture();  // Upload light map data to GPU
    
    std::vector<Vec3>
        collisionMeshVertices;  // Collision mesh: stores positions of exposed faces for physics
};
