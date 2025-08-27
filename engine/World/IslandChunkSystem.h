// IslandChunkSystem.h - Floating island chunking system
#pragma once
#include <memory>
#include <unordered_map>
#include <vector>

#include "Math/Vec3.h"

// Forward declarations
class VoxelChunk;

// An Island is a collection of chunks that move together as one physics body
struct FloatingIsland
{
    uint32_t physicsBodyHandle;             // Our internal handle for the physics body
    Vec3 physicsCenter{0, 0, 0};            // Center of mass for physics
    Vec3 velocity{0, 0, 0};                 // Island velocity for physics simulation
    Vec3 acceleration{0, 0, 0};             // Island acceleration (gravity, wind, etc.)
    std::unique_ptr<VoxelChunk> mainChunk;  // For now, just one main chunk per island
    uint32_t islandID;                      // Unique island identifier
    bool needsPhysicsUpdate = false;
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
    void addChunkToIsland(uint32_t islandID, const Vec3& localPosition);
    void removeChunkFromIsland(uint32_t islandID, const Vec3& localPosition);

    // **ISLAND-CENTRIC VOXEL ACCESS** (Only way to access voxels)
    uint8_t getVoxelFromIsland(uint32_t islandID, const Vec3& localPosition) const;
    void setVoxelInIsland(uint32_t islandID, const Vec3& localPosition, uint8_t voxelType);

    // Physics integration
    void updateIslandPhysics(float deltaTime);
    void syncPhysicsToChunks();  // Update chunk world positions from physics

    // Player-relative chunk loading (for infinite worlds)
    void updatePlayerChunks(const Vec3& playerPosition);
    void setRenderDistance(int chunks)
    {
        m_renderDistance = chunks;
    }

    // Generation
    void generateFloatingIsland(uint32_t islandID, uint32_t seed, float radius = 16.0f);

    // Rendering interface
    void getAllChunks(std::vector<VoxelChunk*>& outChunks);
    void getVisibleChunks(const Vec3& viewPosition, std::vector<VoxelChunk*>& outChunks);
    void renderAllIslands();  // New: Render all islands with proper positioning

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
