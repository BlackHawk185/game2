// VoxelChunk.h - 32x32x32 dynamic physics-enabled voxel chunks
#pragma once
#include <array>
#include <memory>
#include <vector>

#include "Math/Vec3.h"  // Include Vec3 instead of forward declaration

struct Vertex
{
    float x, y, z;     // Position
    float nx, ny, nz;  // Normal
    float u, v;        // Texture coordinates
};

struct VoxelMesh
{
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;
    uint32_t VAO, VBO, EBO;  // OpenGL handles
    bool needsUpdate = true;
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
    static constexpr int SIZE = 32;
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
    uint32_t getVoxelDataSize() const
    {
        return VOLUME;
    }
    void setRawVoxelData(const uint8_t* data, uint32_t size);

    // Dynamic mesh generation for physics
    void generateMesh();
    void updatePhysicsMesh();  // Convert to collision mesh

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
    void buildCollisionMesh();
    bool checkRayCollision(const Vec3& rayOrigin, const Vec3& rayDirection, float maxDistance,
                           Vec3& hitPoint, Vec3& hitNormal) const;

    // Island generation (for floating terrain) - noise-based islands by default
    void generateFloatingIsland(int seed, bool useNoise = false);

   private:
    std::array<uint8_t, VOLUME> voxels;
    VoxelMesh mesh;
    CollisionMesh collisionMesh;
    bool meshDirty = true;
    uint32_t physicsBodyID = 0;  // Physics body ID

    // **FACE CULLING OPTIMIZATION HELPER**
    bool shouldRenderFace(int x, int y, int z, int faceDir) const;

    // Mesh generation helpers
    void addQuad(std::vector<Vertex>& vertices, std::vector<uint32_t>& indices, float x, float y,
                 float z, int face, uint8_t blockType);
    void addCollisionQuad(float x, float y, float z, int face);
    bool isVoxelSolid(int x, int y, int z) const;
    std::vector<Vec3>
        collisionMeshVertices;  // Collision mesh: stores positions of exposed faces for physics
};


