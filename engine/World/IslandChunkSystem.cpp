// IslandChunkSystem.cpp - Implementation of physics-driven chunking
#include "IslandChunkSystem.h"

#include <cmath>
#include <cstdlib>
#include <ctime>
#include <iostream>
#include <memory>

#include "VoxelChunk.h"

IslandChunkSystem g_islandSystem;

IslandChunkSystem::IslandChunkSystem()
{
    // Initialize system
    // Seed random number generator for island velocity
    std::srand(static_cast<unsigned int>(std::time(nullptr)));
}

IslandChunkSystem::~IslandChunkSystem()
{
    // Clean up all islands
    for (auto& [id, island] : m_islands)
    {
        destroyIsland(id);
    }
}

uint32_t IslandChunkSystem::createIsland(const Vec3& physicsCenter)
{
    uint32_t islandID = m_nextIslandID++;
    FloatingIsland& island = m_islands[islandID];
    island.islandID = islandID;
    island.physicsCenter = physicsCenter;
    std::srand(static_cast<unsigned int>(std::time(nullptr)) + islandID * 137);
    float randomX = (std::rand() % 201 - 100) / 100.0f;
    float randomY = (std::rand() % 201 - 100) / 100.0f;
    float randomZ = (std::rand() % 201 - 100) / 100.0f;
    // Set velocity to ±0.4f for all axes (original drift speed)
    island.velocity = Vec3(randomX * 0.4f, randomY * 0.4f, randomZ * 0.4f);
    island.acceleration = Vec3(0.0f, 0.0f, 0.0f);  // No gravity for islands
    return islandID;
}

void IslandChunkSystem::destroyIsland(uint32_t islandID)
{
    auto it = m_islands.find(islandID);
    if (it == m_islands.end())
        return;

    m_islands.erase(it);
}

FloatingIsland* IslandChunkSystem::getIsland(uint32_t islandID)
{
    auto it = m_islands.find(islandID);
    return (it != m_islands.end()) ? &it->second : nullptr;
}

const FloatingIsland* IslandChunkSystem::getIsland(uint32_t islandID) const
{
    auto it = m_islands.find(islandID);
    return (it != m_islands.end()) ? &it->second : nullptr;
}

Vec3 IslandChunkSystem::getIslandCenter(uint32_t islandID) const
{
    auto it = m_islands.find(islandID);
    if (it != m_islands.end())
    {
        return it->second.physicsCenter;
    }
    return Vec3(0.0f, 0.0f, 0.0f);  // Return zero vector if island not found
}

Vec3 IslandChunkSystem::getIslandVelocity(uint32_t islandID) const
{
    auto it = m_islands.find(islandID);
    if (it != m_islands.end())
    {
        return it->second.velocity;
    }
    return Vec3(0.0f, 0.0f, 0.0f);  // Return zero velocity if island not found
}

void IslandChunkSystem::addChunkToIsland(uint32_t islandID, const Vec3& chunkCoord)
{
    FloatingIsland* island = getIsland(islandID);
    if (!island)
        return;

    // Check if chunk already exists
    if (island->chunks.find(chunkCoord) != island->chunks.end())
        return;

    // Create new chunk
    island->chunks[chunkCoord] = std::make_unique<VoxelChunk>();
}

void IslandChunkSystem::removeChunkFromIsland(uint32_t islandID, const Vec3& chunkCoord)
{
    FloatingIsland* island = getIsland(islandID);
    if (!island)
        return;

    auto it = island->chunks.find(chunkCoord);
    if (it != island->chunks.end())
    {
        island->chunks.erase(it);
    }
}

VoxelChunk* IslandChunkSystem::getChunkFromIsland(uint32_t islandID, const Vec3& chunkCoord)
{
    FloatingIsland* island = getIsland(islandID);
    if (!island)
        return nullptr;

    auto it = island->chunks.find(chunkCoord);
    if (it != island->chunks.end())
    {
        return it->second.get();
    }

    return nullptr;
}

void IslandChunkSystem::generateFloatingIsland(uint32_t islandID, uint32_t seed, float radius)
{
    FloatingIsland* island = getIsland(islandID);
    if (!island)
        return;

    // Create the main chunk at origin (0,0,0) for backward compatibility
    Vec3 originChunk(0, 0, 0);
    addChunkToIsland(islandID, originChunk);
    
    VoxelChunk* chunk = getChunkFromIsland(islandID, originChunk);
    if (chunk)
    {
        const char* noiseEnv = std::getenv("ISLAND_NOISE");
        bool useNoise = (noiseEnv && (noiseEnv[0]=='1' || noiseEnv[0]=='T' || noiseEnv[0]=='t' || noiseEnv[0]=='Y' || noiseEnv[0]=='y'));
        chunk->generateFloatingIsland(seed, useNoise);
    }
}

void IslandChunkSystem::generateFloatingIslandMultiChunk(uint32_t islandID, uint32_t seed, float radius, int chunkRadius)
{
    FloatingIsland* island = getIsland(islandID);
    if (!island)
        return;

    // Generate chunks in a cubic pattern around the origin
    for (int x = -chunkRadius; x <= chunkRadius; x++)
    {
        for (int y = -chunkRadius; y <= chunkRadius; y++)
        {
            for (int z = -chunkRadius; z <= chunkRadius; z++)
            {
                Vec3 chunkCoord(x, y, z);
                addChunkToIsland(islandID, chunkCoord);
                
                VoxelChunk* chunk = getChunkFromIsland(islandID, chunkCoord);
                if (chunk)
                {
                    // Generate island content for this chunk
                    // Use coordinate-based seed variation for different chunks
                    uint32_t chunkSeed = seed + static_cast<uint32_t>(x * 1000 + y * 100 + z * 10);
                    
                    const char* noiseEnv = std::getenv("ISLAND_NOISE");
                    bool useNoise = (noiseEnv && (noiseEnv[0]=='1' || noiseEnv[0]=='T' || noiseEnv[0]=='t' || noiseEnv[0]=='Y' || noiseEnv[0]=='y'));
                    chunk->generateFloatingIsland(chunkSeed, useNoise);
                }
            }
        }
    }
    
    std::cout << "Generated multi-chunk island " << islandID << " with " << (2*chunkRadius+1) << "x" << (2*chunkRadius+1) << "x" << (2*chunkRadius+1) << " chunks" << std::endl;
}

void IslandChunkSystem::generateFloatingIslandOrganic(uint32_t islandID, uint32_t seed, float radius)
{
    FloatingIsland* island = getIsland(islandID);
    if (!island)
        return;

    // Start with a center chunk at origin to ensure we have at least one chunk
    addChunkToIsland(islandID, Vec3(0, 0, 0));
    
    const char* noiseEnv = std::getenv("ISLAND_NOISE");
    bool useNoise = (noiseEnv && (noiseEnv[0]=='1' || noiseEnv[0]=='T' || noiseEnv[0]=='t' || noiseEnv[0]=='Y' || noiseEnv[0]=='y'));
    
    // **ROBUST FLATTEN LOGGING** - Safer approach to flatten to avoid crashes
    float flatten = 1.0f;
    bool flattenEnabled = false;
    
    if (useNoise) {
        flatten = 0.85f; // Safer default than 0.90f
        
        // **SAFE FLATTEN PARSING** with extensive logging
        const char* flattenEnv = std::getenv("ISLAND_FLATTEN");
        if (flattenEnv) {
            std::cout << "[FLATTEN] Environment variable found: ISLAND_FLATTEN=" << flattenEnv << std::endl;
            
            try {
                float parsedFlatten = std::stof(flattenEnv);
                std::cout << "[FLATTEN] Parsed value: " << parsedFlatten << std::endl;
                
                // **STRICT BOUNDS CHECKING** to prevent crashes
                if (parsedFlatten >= 0.5f && parsedFlatten <= 1.0f) {
                    flatten = parsedFlatten;
                    flattenEnabled = true;
                    std::cout << "[FLATTEN] Applied flatten factor: " << flatten << std::endl;
                } else {
                    std::cout << "[FLATTEN] WARNING: Value " << parsedFlatten 
                              << " outside safe range [0.5, 1.0], using default: " << flatten << std::endl;
                }
            } catch (const std::exception& e) {
                std::cout << "[FLATTEN] ERROR: Failed to parse '" << flattenEnv 
                          << "', using default: " << flatten << " (Error: " << e.what() << ")" << std::endl;
            }
        } else {
            std::cout << "[FLATTEN] No ISLAND_FLATTEN environment variable, using default: " << flatten << std::endl;
        }
    }
    
    std::cout << "[ORGANIC] Generating island " << islandID << " with radius " << radius 
              << ", useNoise=" << (useNoise ? 1 : 0) 
              << ", flattenEnabled=" << (flattenEnabled ? 1 : 0)
              << ", flatten=" << flatten 
              << ", approach=" << (useNoise ? "NOISE-FIRST" : "SPHERE-FIRST") << std::endl;
    
    int voxelsGenerated = 0;
    int chunksCreated = 0;
    
    // **OPTIMIZED SPHERICAL ITERATION** - only check voxels that could be inside the sphere
    int radiusInt = static_cast<int>(radius + 1);
    
    // Iterate more efficiently: for each Y layer, calculate the circular bounds
    for (int y = -radiusInt; y <= radiusInt; y++)
    {
        float dy = static_cast<float>(y);
        float maxRadiusAtY = std::sqrt(radius * radius - dy * dy);
        int maxXZ = static_cast<int>(maxRadiusAtY + 1);
        
        // Progress reporting
        if (y % 10 == 0 || y == -radiusInt)
        {
            std::cout << "[ORGANIC] Processing Y layer " << y << "/" << radiusInt 
                      << " (max radius at this Y: " << maxRadiusAtY << ")" << std::endl;
        }
        
        for (int x = -maxXZ; x <= maxXZ; x++)
        {
            for (int z = -maxXZ; z <= maxXZ; z++)
            {
                // Calculate distance from island center (with flatten applied for noise)
                float dx = static_cast<float>(x);
                float dy = static_cast<float>(y);
                float dz = static_cast<float>(z);
                
                // **NOISE-FIRST APPROACH** - Use noise as primary shape driver when enabled
                bool shouldPlaceVoxel = false;
                
                if (useNoise) {
                    // **FLATTEN APPLICATION** with robust error handling
                    float dyFlattened = dy;
                    try {
                        if (flatten != 1.0f && flatten > 0.0f) {
                            dyFlattened = dy * flatten;
                        }
                    } catch (...) {
                        std::cout << "[FLATTEN] ERROR: Exception during flatten calculation, using original Y" << std::endl;
                        dyFlattened = dy;
                    }
                    
                    // **RADIAL CONSTRAINT** - Distance from island center
                    float distanceFromCenter = std::sqrt(dx * dx + dyFlattened * dyFlattened + dz * dz);
                    if (distanceFromCenter <= radius) {
                        // **RADIAL FALLOFF** - Density decreases towards edges
                        float radialFalloff = 1.0f - (distanceFromCenter / radius);
                        radialFalloff = std::max(0.0f, radialFalloff);
                        
                        // **3D NOISE SAMPLING** - Simple multi-octave noise
                        const float noiseScale = 0.05f;
                        const float densityThreshold = 0.3f;
                        const int octaves = 3;
                        const float persistence = 0.5f;
                        
                        float noiseValue = 0.0f;
                        float amplitude = 1.0f;
                        float frequency = noiseScale;
                        
                        for (int octave = 0; octave < octaves; octave++) {
                            float sampleX = dx * frequency + seed * 0.1f;
                            float sampleY = dyFlattened * frequency + seed * 0.2f;
                            float sampleZ = dz * frequency + seed * 0.3f;
                            
                            float octaveNoise = (std::sin(sampleX) * std::cos(sampleY) * std::sin(sampleZ) + 1.0f) * 0.5f;
                            noiseValue += octaveNoise * amplitude;
                            
                            amplitude *= persistence;
                            frequency *= 2.0f;
                        }
                        
                        // **FINAL DENSITY** - Noise modulated by radial falloff
                        float finalDensity = noiseValue * radialFalloff;
                        shouldPlaceVoxel = (finalDensity > densityThreshold);
                    }
                } else {
                    // **SPHERE-FIRST APPROACH** - Traditional spherical generation
                    float distance = std::sqrt(dx * dx + dy * dy + dz * dz);
                    shouldPlaceVoxel = (distance <= radius);
                }
                
                if (shouldPlaceVoxel)
                {
                    // Track chunks before placement
                    size_t chunksBefore = island->chunks.size();
                    
                    // Use island-relative coordinates for placement
                    Vec3 islandRelativePos(dx, dy, dz);
                    setVoxelWithAutoChunk(islandID, islandRelativePos, 1); // Rock/stone
                    
                    voxelsGenerated++;
                    
                    // Check if a new chunk was created
                    if (island->chunks.size() > chunksBefore)
                    {
                        chunksCreated++;
                    }
                }
            }
        }
    }
    
    std::cout << "[ORGANIC] Generated island " << islandID << " organically: " 
              << voxelsGenerated << " voxels across " << island->chunks.size() 
              << " chunks (" << chunksCreated << " created dynamically)" << std::endl;
    
    // **SINGLE MESH GENERATION PASS** - much faster than per-voxel generation
    std::cout << "[ORGANIC] Building meshes and collision for " << island->chunks.size() << " chunks..." << std::endl;
    for (auto& [chunkCoord, chunk] : island->chunks)
    {
        if (chunk)
        {
            chunk->generateMesh();
            chunk->buildCollisionMesh();
        }
    }
    std::cout << "[ORGANIC] Mesh generation complete!" << std::endl;
}

uint8_t IslandChunkSystem::getVoxelFromIsland(uint32_t islandID, const Vec3& islandRelativePosition) const
{
    const FloatingIsland* island = getIsland(islandID);
    if (!island)
        return 0;

    // Convert island-relative position to chunk coordinate and local voxel position
    Vec3 chunkCoord = FloatingIsland::islandPosToChunkCoord(islandRelativePosition);
    Vec3 localPos = FloatingIsland::islandPosToLocalPos(islandRelativePosition);

    // Find the chunk
    auto it = island->chunks.find(chunkCoord);
    if (it == island->chunks.end())
        return 0; // Chunk doesn't exist

    VoxelChunk* chunk = it->second.get();
    if (!chunk)
        return 0;

    // Get voxel from chunk using local coordinates
    int x = static_cast<int>(localPos.x);
    int y = static_cast<int>(localPos.y);
    int z = static_cast<int>(localPos.z);

    // Check bounds (should be 0-31 for 32x32x32 chunks)
    if (x < 0 || x >= VoxelChunk::SIZE || y < 0 || y >= VoxelChunk::SIZE || z < 0 || z >= VoxelChunk::SIZE)
        return 0;

    return chunk->getVoxel(x, y, z);
}

void IslandChunkSystem::setVoxelInIsland(uint32_t islandID, const Vec3& islandRelativePosition, uint8_t voxelType)
{
    FloatingIsland* island = getIsland(islandID);
    if (!island)
        return;

    // Convert island-relative position to chunk coordinate and local position
    Vec3 chunkCoord = FloatingIsland::islandPosToChunkCoord(islandRelativePosition);
    Vec3 localPos = FloatingIsland::islandPosToLocalPos(islandRelativePosition);

    // Find or create the chunk
    auto it = island->chunks.find(chunkCoord);
    if (it == island->chunks.end())
    {
        // Create new chunk if it doesn't exist
        addChunkToIsland(islandID, chunkCoord);
        it = island->chunks.find(chunkCoord);
    }

    VoxelChunk* chunk = it->second.get();
    if (!chunk)
        return;

    // Set voxel in chunk using local coordinates
    int x = static_cast<int>(localPos.x);
    int y = static_cast<int>(localPos.y);
    int z = static_cast<int>(localPos.z);

    // Check bounds (should be 0-31 for 32x32x32 chunks)
    if (x < 0 || x >= VoxelChunk::SIZE || y < 0 || y >= VoxelChunk::SIZE || z < 0 || z >= VoxelChunk::SIZE)
        return;

    chunk->setVoxel(x, y, z, voxelType);
    chunk->generateMesh();  // Update visual mesh
}

void IslandChunkSystem::setVoxelWithAutoChunk(uint32_t islandID, const Vec3& islandRelativePos, uint8_t voxelType)
{
    FloatingIsland* island = getIsland(islandID);
    if (!island)
        return;

    // Calculate which chunk this position belongs to using grid-aligned coordinates
    // Use floor division to ensure proper grid alignment
    int chunkX = static_cast<int>(std::floor(islandRelativePos.x / VoxelChunk::SIZE));
    int chunkY = static_cast<int>(std::floor(islandRelativePos.y / VoxelChunk::SIZE));
    int chunkZ = static_cast<int>(std::floor(islandRelativePos.z / VoxelChunk::SIZE));
    Vec3 chunkCoord(chunkX, chunkY, chunkZ);

    // Calculate local position within the chunk (always 0-31)
    int localX = static_cast<int>(islandRelativePos.x) % VoxelChunk::SIZE;
    int localY = static_cast<int>(islandRelativePos.y) % VoxelChunk::SIZE;
    int localZ = static_cast<int>(islandRelativePos.z) % VoxelChunk::SIZE;
    
    // Handle negative coordinates properly for modulo
    if (localX < 0) localX += VoxelChunk::SIZE;
    if (localY < 0) localY += VoxelChunk::SIZE;
    if (localZ < 0) localZ += VoxelChunk::SIZE;

    // Find or create the chunk at the grid-aligned coordinate
    auto it = island->chunks.find(chunkCoord);
    if (it == island->chunks.end())
    {
        // Create new chunk at grid-aligned position
        addChunkToIsland(islandID, chunkCoord);
        it = island->chunks.find(chunkCoord);
    }

    VoxelChunk* chunk = it->second.get();
    if (!chunk)
        return;

    // Set voxel in chunk using local coordinates
    chunk->setVoxel(localX, localY, localZ, voxelType);
    // NOTE: Don't generate mesh here - do it once at the end for performance
}

void IslandChunkSystem::getAllChunks(std::vector<VoxelChunk*>& outChunks)
{
    outChunks.clear();

    for (auto& [id, island] : m_islands)
    {
        // Add all chunks from this island
        for (auto& [chunkCoord, chunk] : island.chunks)
        {
            if (chunk)
            {
                outChunks.push_back(chunk.get());
            }
        }
    }
}

void IslandChunkSystem::getVisibleChunks(const Vec3& viewPosition,
                                         std::vector<VoxelChunk*>& outChunks)
{
    // Frustum culling will be added when we implement proper camera frustum
    getAllChunks(outChunks);
}

void IslandChunkSystem::renderAllIslands()
{
    // Render each island at its physics center position
    for (auto& [id, island] : m_islands)
    {
        // Render all chunks in this island
        for (const auto& [chunkCoord, chunk] : island.chunks)
        {
            if (chunk)
            {
                // Calculate world position for this chunk
                Vec3 chunkWorldPos = island.physicsCenter + FloatingIsland::chunkCoordToWorldPos(chunkCoord);
                chunk->render(chunkWorldPos);
            }
        }
    }
}

void IslandChunkSystem::updateIslandPhysics(float deltaTime)
{
    for (auto& [id, island] : m_islands)
    {
        // Only apply random drift, no gravity, bobbing, or wind
        island.physicsCenter.x += island.velocity.x * deltaTime;
        island.physicsCenter.y += island.velocity.y * deltaTime;
        island.physicsCenter.z += island.velocity.z * deltaTime;
        island.needsPhysicsUpdate = true;
    }
}

void IslandChunkSystem::syncPhysicsToChunks()
{
    // Sync physics positions to chunk rendering positions
    for (auto& [id, island] : m_islands)
    {
        if (island.needsPhysicsUpdate)
        {
            // All chunks in the island move together with the physics center
            // Individual chunk positions are calculated during rendering
            island.needsPhysicsUpdate = false;
        }
    }
}

void IslandChunkSystem::updatePlayerChunks(const Vec3& playerPosition)
{
    // Infinite world generation will be implemented in a future version
    // For now, we manually create islands in GameState
}

void IslandChunkSystem::generateChunksAroundPoint(const Vec3& center)
{
    // Chunk generation around points will be used for infinite world expansion
    // Currently handled manually through createIsland()
}


