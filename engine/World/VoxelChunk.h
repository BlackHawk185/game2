// VoxelChunk.h - 32x32x32 dynamic physics-enabled voxel chunks
#pragma once
#include <array>
#include <vector>
#include <memory>
#include "Math/Vec3.h"  // Include Vec3 instead of forward declaration

struct Vertex {
    float x, y, z;    // Position
    float nx, ny, nz; // Normal
    float u, v;       // Texture coordinates
};

struct VoxelMesh {
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;
    uint32_t VAO, VBO, EBO; // OpenGL handles
    bool needsUpdate = true;
};

class VoxelChunk {
public:
    static constexpr int SIZE = 32;
    static constexpr int VOLUME = SIZE * SIZE * SIZE;
    
    VoxelChunk();
    ~VoxelChunk();
    
    // Voxel data access
    uint8_t getVoxel(int x, int y, int z) const;
    void setVoxel(int x, int y, int z, uint8_t type);
    
    // Network serialization - get raw voxel data for transmission
    const uint8_t* getRawVoxelData() const { return voxels.data(); }
    uint32_t getVoxelDataSize() const { return VOLUME; }
    void setRawVoxelData(const uint8_t* data, uint32_t size);
    
    // Dynamic mesh generation for physics
    void generateMesh();
    void updatePhysicsMesh(); // Convert to Jolt collision mesh
    
    // Rendering
    void render();                          // Render at origin (0,0,0) 
    void render(const Vec3& worldOffset);   // Render at island's world position
    void renderLOD(int lodLevel, const Vec3& cameraPos); // New LOD rendering
    bool isDirty() const { return meshDirty; }
    
    // **LOD AND CULLING SUPPORT**
    int calculateLOD(const Vec3& cameraPos) const;
    bool shouldRender(const Vec3& cameraPos, float maxDistance = 256.0f) const;
    
    // Physics integration
    void* getJoltBodyID() const { return joltBodyID; }
    
    // Island generation (for floating terrain)
    void generateFloatingIsland(int seed);
    
private:
    std::array<uint8_t, VOLUME> voxels;
    VoxelMesh mesh;
    bool meshDirty = true;
    void* joltBodyID = nullptr; // Jolt physics body
    
    // **FACE CULLING OPTIMIZATION HELPER**
    bool shouldRenderFace(int x, int y, int z, int faceDir) const;
    
    // Mesh generation helpers
    void addQuad(std::vector<Vertex>& vertices, std::vector<uint32_t>& indices,
                 float x, float y, float z, int face, uint8_t blockType);
    bool isVoxelSolid(int x, int y, int z) const;
};
