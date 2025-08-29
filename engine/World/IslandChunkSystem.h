// IslandChunkSystem.h - Floating island chunking system
#pragma once
#include <memory>
#include <unordered_map>
#include <map>
#include <vector>
#include <cmath>

#include "Math/Vec3.h"
#include "VoxelChunk.h"

// An Island is a collection of chunks that move together as one physics body
struct FloatingIsland
{
    uint32_t physicsBodyHandle;                                      // Our internal handle for the physics body
    Vec3 physicsCenter{0, 0, 0};                                     // Center of mass for physics
    Vec3 velocity{0, 0, 0};                                          // Island velocity for physics simulation
    Vec3 acceleration{0, 0, 0};                                      // Island acceleration (gravity, wind, etc.)
    std::map<Vec3, std::unique_ptr<VoxelChunk>> chunks;              // Multi-chunk support: chunkCoord -> VoxelChunk
    uint32_t islandID;                                               // Unique island identifier
    bool needsPhysicsUpdate = false;

    // Helper functions for chunk coordinate conversion (operates on island-relative coordinates)
    static Vec3 islandPosToChunkCoord(const Vec3& islandRelativePos) {
        return Vec3(
            static_cast<int>(std::floor(islandRelativePos.x / VoxelChunk::SIZE)),
            static_cast<int>(std::floor(islandRelativePos.y / VoxelChunk::SIZE)),
            static_cast<int>(std::floor(islandRelativePos.z / VoxelChunk::SIZE))
        );
    }

    static Vec3 islandPosToLocalPos(const Vec3& islandRelativePos) {
        Vec3 chunkCoord = islandPosToChunkCoord(islandRelativePos);
        return Vec3(
            islandRelativePos.x - (chunkCoord.x * VoxelChunk::SIZE),
            islandRelativePos.y - (chunkCoord.y * VoxelChunk::SIZE),
            islandRelativePos.z - (chunkCoord.z * VoxelChunk::SIZE)
        );
    }

    static Vec3 chunkCoordToWorldPos(const Vec3& chunkCoord) {
        return Vec3(
            chunkCoord.x * VoxelChunk::SIZE,
            chunkCoord.y * VoxelChunk::SIZE,
            chunkCoord.z * VoxelChunk::SIZE
        );
    }
};

// A chunk within an island - has LOCAL coordinates relative to island center
struct IslandChunk
{
    Vec3 localPosition{0, 0, 0};   // Position relative to island center
    uint32_t islandID;             // Which island this chunk belongs to
    uint8_t* voxelData = nullptr;  // 32x32x32 voxel data
    bool needsRemesh = true;
    uint32_t meshVertexCount = 0;
    float* meshVertices = nullptr;

    // Get world position by combining island physics position + local offset
    Vec3 getWorldPosition(const FloatingIsland& island, const Vec3& islandPhysicsPos) const
    {
        return islandPhysicsPos + localPosition;
    }
};

// This system manages islands that can move through space
class IslandChunkSystem
{
   public:
    IslandChunkSystem();
    ~IslandChunkSystem();

    // Island management
    uint32_t createIsland(const Vec3& physicsCenter);
    void destroyIsland(uint32_t islandID);
    FloatingIsland* getIsland(uint32_t islandID);
    const FloatingIsland* getIsland(uint32_t islandID) const;

    // Chunk management within islands
    void addChunkToIsland(uint32_t islandID, const Vec3& chunkCoord);
    void removeChunkFromIsland(uint32_t islandID, const Vec3& chunkCoord);
    VoxelChunk* getChunkFromIsland(uint32_t islandID, const Vec3& chunkCoord);

    // **ISLAND-CENTRIC VOXEL ACCESS** (Only way to access voxels)
    // Uses world coordinates - automatically converts to chunk + local coordinates
    // Get a specific voxel from an island using island-relative coordinates (for raycasting and collision detection)
    uint8_t getVoxelFromIsland(uint32_t islandID, const Vec3& islandRelativePosition) const;
    // Set a specific voxel in an island using island-relative coordinates (for block placement and breaking)
    void setVoxelInIsland(uint32_t islandID, const Vec3& islandRelativePosition, uint8_t voxelType);
    
    // **DYNAMIC VOXEL PLACEMENT** (Creates chunks as needed)
    // Uses island-relative coordinates - automatically creates chunks on grid-aligned boundaries
    void setVoxelWithAutoChunk(uint32_t islandID, const Vec3& islandRelativePos, uint8_t voxelType);

    // Physics integration
    void updateIslandPhysics(float deltaTime);
    void syncPhysicsToChunks();  // Update chunk world positions from physics

    // Player-relative chunk loading (for infinite worlds)
    void updatePlayerChunks(const Vec3& playerPosition);
    void setRenderDistance(int chunks)
    {
        m_renderDistance = chunks;
    }

    // Rendering interface
    void getAllChunks(std::vector<VoxelChunk*>& outChunks);
    void getVisibleChunks(const Vec3& viewPosition, std::vector<VoxelChunk*>& outChunks);
    void renderAllIslands();  // Render all islands with proper positioning

    // **ORGANIC ISLAND GENERATION** (Creates chunks dynamically based on island shape)
    // This is now the primary and only island generation method
    void generateFloatingIslandOrganic(uint32_t islandID, uint32_t seed, float radius = 48.0f);

    // Island queries
    Vec3 getIslandCenter(uint32_t islandID) const;    // Get current physics center of island
    Vec3 getIslandVelocity(uint32_t islandID) const;  // Get current velocity of island
    const std::unordered_map<uint32_t, FloatingIsland>& getIslands() const { return m_islands; }

   private:
    std::unordered_map<uint32_t, FloatingIsland> m_islands;
    uint32_t m_nextIslandID = 1;
    int m_renderDistance = 8;

    // Generate chunks around a center point (for infinite worlds)
    void generateChunksAroundPoint(const Vec3& center);
};

// Global island system
extern IslandChunkSystem g_islandSystem;

