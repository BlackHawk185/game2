// VoxelChunk.h - 16x16x16 dynamic physics-enabled voxel chunks with light mapping
#pragma once

#include "../Math/Vec3.h"
#include "BlockType.h"
#include <array>
#include <vector>
#include <unordered_map>
#include <cstdint>
#include <mutex>
#include <string>
#include <memory>
#include <atomic>

// Forward declaration for OpenGL types
using GLuint = uint32_t;

struct Vertex
{
    float x, y, z;     // Position
    float nx, ny, nz;  // Normal
    float u, v;        // Texture coordinates
    float lu, lv;      // Light map coordinates
    float ao;          // Ambient occlusion (0.0 = fully occluded, 1.0 = no occlusion)
    float faceIndex;   // Face index (0-5) for selecting correct light map texture
    float blockType;   // Block type ID for texture selection
    
    // Equality operator for vertex deduplication
    bool operator==(const Vertex& other) const {
        return x == other.x && y == other.y && z == other.z &&
               nx == other.nx && ny == other.ny && nz == other.nz &&
               u == other.u && v == other.v &&
               lu == other.lu && lv == other.lv &&
               ao == other.ao && faceIndex == other.faceIndex &&
               blockType == other.blockType;
    }
};

// Hash function for Vertex
namespace std {
    template<>
    struct hash<Vertex> {
        size_t operator()(const Vertex& v) const {
            // Combine hashes of all fields
            size_t h1 = hash<float>()(v.x);
            size_t h2 = hash<float>()(v.y);
            size_t h3 = hash<float>()(v.z);
            size_t h4 = hash<float>()(v.nx);
            size_t h5 = hash<float>()(v.ny);
            size_t h6 = hash<float>()(v.nz);
            size_t h7 = hash<float>()(v.u);
            size_t h8 = hash<float>()(v.v);
            // Combine using XOR and bit shifting
            return h1 ^ (h2 << 1) ^ (h3 << 2) ^ (h4 << 3) ^ 
                   (h5 << 4) ^ (h6 << 5) ^ (h7 << 6) ^ (h8 << 7);
        }
    };
}

struct VoxelMesh
{
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;
    uint32_t VAO, VBO, EBO;  // OpenGL handles
    bool needsUpdate = true;
};

// Per-face light mapping data for the chunk
struct FaceLightMap 
{
    static constexpr int LIGHTMAP_SIZE = 32; // 32x32 per face
    uint32_t textureHandle = 0;  // OpenGL texture handle
    std::vector<uint8_t> data;  // RGB data
    bool needsUpdate = true;
    
    FaceLightMap() {
        // Initialize with RGB data
        data.resize(LIGHTMAP_SIZE * LIGHTMAP_SIZE * 3);
    }
    
    ~FaceLightMap() {
        if (textureHandle != 0) {
            // Note: glDeleteTextures should only be called from OpenGL context
            // This will be handled by proper cleanup in VoxelChunk destructor
        }
    }
};

// Light mapping data for the chunk - one light map per face direction
struct ChunkLightMaps 
{
    // 6 face directions: +X, -X, +Y, -Y, +Z, -Z (matches face indices 4, 5, 2, 3, 0, 1)
    std::array<FaceLightMap, 6> faceMaps;
    
    FaceLightMap& getFaceMap(int faceDirection) {
        return faceMaps[faceDirection];
    }
    
    const FaceLightMap& getFaceMap(int faceDirection) const {
        return faceMaps[faceDirection];
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
    
    CollisionMesh() = default;
    CollisionMesh(const CollisionMesh& other) : faces(other.faces) {}
    CollisionMesh& operator=(const CollisionMesh& other) {
        if (this != &other) {
            faces = other.faces;
        }
        return *this;
    }
};

class IslandChunkSystem;  // Forward declaration

class VoxelChunk
{
   public:
    static constexpr int SIZE = 16;  // 16x16x16 chunks
    static constexpr int VOLUME = SIZE * SIZE * SIZE;
    
    // Static island system pointer for inter-chunk queries
    static void setIslandSystem(IslandChunkSystem* system) { s_islandSystem = system; }

    VoxelChunk();
    ~VoxelChunk();

    // Voxel data access (ID-based - clean and efficient)
    uint8_t getVoxel(int x, int y, int z) const;
    void setVoxel(int x, int y, int z, uint8_t type);
    
    // Block type access using IDs (for debugging only)
    uint8_t getBlockID(int x, int y, int z) const { return getVoxel(x, y, z); }
    void setBlockID(int x, int y, int z, uint8_t blockID) { setVoxel(x, y, z, blockID); }
    bool hasBlockID(int x, int y, int z, uint8_t blockID) const { return getVoxel(x, y, z) == blockID; }

    // Network serialization - get raw voxel data for transmission
    const uint8_t* getRawVoxelData() const
    {
        return voxels.data();
    }
    
    // Network serialization helpers
    void setRawVoxelData(const uint8_t* data, uint32_t size);
    uint32_t getVoxelDataSize() const { return VOLUME; }

    // Mesh generation and management
    void generateMesh(bool generateLighting = true);

    // Mesh state
    bool isDirty() const
    {
        return meshDirty;
    }

    // **LOD AND CULLING SUPPORT**
    int calculateLOD(const Vec3& cameraPos) const;
    bool shouldRender(const Vec3& cameraPos, float maxDistance = 1024.0f) const;

    // Collision detection methods - thread-safe atomic access
    std::shared_ptr<const CollisionMesh> getCollisionMesh() const
    {
        return std::atomic_load(&collisionMesh);
    }
    
    void setCollisionMesh(std::shared_ptr<CollisionMesh> newMesh)
    {
        std::atomic_store(&collisionMesh, newMesh);
    }
    
    // Mesh access for VBO rendering
    VoxelMesh& getMesh() { return mesh; }
    const VoxelMesh& getMesh() const { return mesh; }
    std::mutex& getMeshMutex() const { return meshMutex; }

    // Decorative/model instance positions (generic per block type)
    const std::vector<Vec3>& getModelInstances(uint8_t blockID) const;
    void addModelInstance(uint8_t blockID, const Vec3& position);
    void clearModelInstances(uint8_t blockID);
    void clearAllModelInstances();
    
    // Legacy grass-specific accessor (for backwards compatibility during refactor)
    const std::vector<Vec3>& getGrassInstancePositions() const { return getModelInstances(BlockID::DECOR_GRASS); }
    
    // Light mapping access
    ChunkLightMaps& getLightMaps() { return lightMaps; }
    const ChunkLightMaps& getLightMaps() const { return lightMaps; }
    void updateLightMapTextures();  // Create/update OpenGL textures from light map data
    void markLightMapsDirty();      // Mark light maps as needing GPU texture update
    bool hasValidLightMaps() const; // Check if all lightmap textures are created
    bool hasLightMapData() const;   // Check if lightmap data exists (before texture creation)
    
    // NEW: Lighting dirty state management
    bool needsLightingUpdate() const { return lightingDirty; }
    void markLightingDirty() { lightingDirty = true; }
    void markLightingClean() { lightingDirty = false; }
    
    // MDI renderer integration
    int getMDIIndex() const { return mdiIndex; }
    void setMDIIndex(int index) { mdiIndex = index; }
    
    // Light mapping utilities - public for GlobalLightingManager
    Vec3 calculateWorldPositionFromLightMapUV(int faceIndex, float u, float v) const;  // Convert UV to world pos
    
    void buildCollisionMesh();
    void buildCollisionMeshFromVertices();  // Internal method called during generateMesh()
    bool checkRayCollision(const Vec3& rayOrigin, const Vec3& rayDirection, float maxDistance,
                           Vec3& hitPoint, Vec3& hitNormal) const;

   public:
    // Island context for inter-chunk culling
    void setIslandContext(uint32_t islandID, const Vec3& chunkCoord);
    

   private:
    std::array<uint8_t, VOLUME> voxels;
    VoxelMesh mesh;
    mutable std::mutex meshMutex;
    std::shared_ptr<CollisionMesh> collisionMesh;  // Thread-safe atomic access via getCollisionMesh/setCollisionMesh
    ChunkLightMaps lightMaps;  // NEW: Per-face light mapping data
    bool meshDirty = true;
    bool lightingDirty = true;  // NEW: Lighting needs recalculation
    int mdiIndex = -1;  // Index in MDI renderer for transform updates (-1 = not registered)
    
    // Island context for inter-chunk culling
    uint32_t m_islandID = 0;
    Vec3 m_chunkCoord{0, 0, 0};
    
    // Static island system for inter-chunk queries
    static IslandChunkSystem* s_islandSystem;

    // NEW: Per-block-type model instance positions (for BlockRenderType::OBJ blocks)
    // Key: BlockID, Value: list of instance positions within this chunk
    std::unordered_map<uint8_t, std::vector<Vec3>> m_modelInstances;

    // Mesh generation helpers
    void addQuadWithSharing(std::vector<Vertex>& vertices, std::vector<uint32_t>& indices,
                            std::unordered_map<Vertex, uint32_t>& vertexCache,
                            float x, float y, float z, int face, uint8_t blockType);
    void addCollisionQuad(float x, float y, float z, int face);
    bool isVoxelSolid(int x, int y, int z) const;
    
    // Unified culling - works for intra-chunk AND inter-chunk
    bool isFaceExposed(int x, int y, int z, int face) const;
    
    // Simple meshing implementation
    void generateSimpleMesh();
    
    // Light mapping utilities
    float computeAmbientOcclusion(int x, int y, int z, int face) const;
    void generatePerFaceLightMaps();  // Generate separate light map per face direction
    bool performSunRaycast(const Vec3& rayStart, const Vec3& sunDirection, float maxDistance) const;  // Raycast for occlusion
    bool performLocalSunRaycast(const Vec3& rayStart, const Vec3& sunDirection, float maxDistance) const;  // Local chunk raycast for floating islands
    bool performInterIslandSunRaycast(const Vec3& rayStart, const Vec3& sunDirection, float maxDistance) const;  // Inter-island raycast for lighting
    
    std::vector<Vec3>
        collisionMeshVertices;  // Collision mesh: stores positions of exposed faces for physics
};
