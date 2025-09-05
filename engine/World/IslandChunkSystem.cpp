// IslandChunkSystem.cpp - Implementation of physics-driven chunking
#include "IslandChunkSystem.h"

#include <cmath>
#include <cstdlib>
#include <ctime>
#include <iostream>
#include <memory>
#include <string>

#include "VoxelChunk.h"
#include "../Profiling/Profiler.h"
#include "../Rendering/VBORenderer.h"  // RE-ENABLED - VBO only, no immediate mode

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
    // Collect IDs first to avoid iterator invalidation
    std::vector<uint32_t> islandIDs;
    for (auto& [id, island] : m_islands)
    {
        islandIDs.push_back(id);
    }
    
    // Now safely destroy all islands
    for (uint32_t id : islandIDs)
    {
        destroyIsland(id);
    }
}

uint32_t IslandChunkSystem::createIsland(const Vec3& physicsCenter)
{
    std::lock_guard<std::mutex> lock(m_islandsMutex);
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
    std::lock_guard<std::mutex> lock(m_islandsMutex);
    auto it = m_islands.find(islandID);
    if (it == m_islands.end())
        return;

    m_islands.erase(it);
}

FloatingIsland* IslandChunkSystem::getIsland(uint32_t islandID)
{
    std::lock_guard<std::mutex> lock(m_islandsMutex);
    auto it = m_islands.find(islandID);
    return (it != m_islands.end()) ? &it->second : nullptr;
}

const FloatingIsland* IslandChunkSystem::getIsland(uint32_t islandID) const
{
    std::lock_guard<std::mutex> lock(m_islandsMutex);
    auto it = m_islands.find(islandID);
    return (it != m_islands.end()) ? &it->second : nullptr;
}

Vec3 IslandChunkSystem::getIslandCenter(uint32_t islandID) const
{
    std::lock_guard<std::mutex> lock(m_islandsMutex);
    auto it = m_islands.find(islandID);
    if (it != m_islands.end())
    {
        return it->second.physicsCenter;
    }
    return Vec3(0.0f, 0.0f, 0.0f);  // Return zero vector if island not found
}

Vec3 IslandChunkSystem::getIslandVelocity(uint32_t islandID) const
{
    std::lock_guard<std::mutex> lock(m_islandsMutex);
    auto it = m_islands.find(islandID);
    if (it != m_islands.end())
    {
        return it->second.velocity;
    }
    return Vec3(0.0f, 0.0f, 0.0f);  // Return zero velocity if island not found
}

void IslandChunkSystem::addChunkToIsland(uint32_t islandID, const Vec3& chunkCoord)
{
    std::lock_guard<std::mutex> lock(m_islandsMutex);
    auto itIsl = m_islands.find(islandID);
    FloatingIsland* island = (itIsl != m_islands.end()) ? &itIsl->second : nullptr;
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
    std::lock_guard<std::mutex> lock(m_islandsMutex);
    auto itIsl = m_islands.find(islandID);
    FloatingIsland* island = (itIsl != m_islands.end()) ? &itIsl->second : nullptr;
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
    std::lock_guard<std::mutex> lock(m_islandsMutex);
    auto itIsl = m_islands.find(islandID);
    FloatingIsland* island = (itIsl != m_islands.end()) ? &itIsl->second : nullptr;
    if (!island)
        return nullptr;

    auto it = island->chunks.find(chunkCoord);
    if (it != island->chunks.end())
    {
        return it->second.get();
    }

    return nullptr;
}

void IslandChunkSystem::generateFloatingIslandOrganic(uint32_t islandID, uint32_t seed, float radius)
{
    FloatingIsland* island = getIsland(islandID);
    if (!island)
        return;

    // Start with a center chunk at origin to ensure we have at least one chunk
    addChunkToIsland(islandID, Vec3(0, 0, 0));
    
    // **ORGANIC GENERATION DEFAULTS TO NOISE-FIRST** - This is the whole point of the "Organic" function
    const char* noiseEnv = std::getenv("ISLAND_NOISE");
    bool useNoise = true; // Default to true for organic generation
    
    // Allow environment variable to override (mainly for testing sphere vs noise)
    if (noiseEnv && (noiseEnv[0]=='0' || noiseEnv[0]=='F' || noiseEnv[0]=='f' || noiseEnv[0]=='N' || noiseEnv[0]=='n')) {
        useNoise = false;
    }
    
    // **NOISE PARAMETERS** - Tuned for island-like terrain
    float noiseScale = 0.08f;  // Medium-scale features for island detail
    float densityThreshold = 0.35f;  // Balanced threshold for natural terrain
    int octaves = 3;  // Sufficient detail without over-complexity
    float persistence = 0.5f;
    
    // **ENVIRONMENT OVERRIDES** for noise parameters
    const char* scaleEnv = std::getenv("NOISE_SCALE");
    if (scaleEnv) noiseScale = std::stof(scaleEnv);
    
    const char* thresholdEnv = std::getenv("NOISE_THRESHOLD");
    if (thresholdEnv) densityThreshold = std::stof(thresholdEnv);
    
    const char* octavesEnv = std::getenv("NOISE_OCTAVES");
    if (octavesEnv) octaves = std::stoi(octavesEnv);
    
    // **ROBUST FLATTEN LOGGING** - Safer approach to flatten to avoid crashes
    float flatten = 1.0f;
    bool flattenEnabled = false;
    
    if (useNoise) {
        flatten = 0.85f; // Safer default than 0.90f
        
        // **SAFE FLATTEN PARSING** with extensive logging
        const char* flattenEnv = std::getenv("ISLAND_FLATTEN");
        if (flattenEnv) {
            try {
                float parsedFlatten = std::stof(flattenEnv);
                
                // **STRICT BOUNDS CHECKING** to prevent crashes
                if (parsedFlatten >= 0.5f && parsedFlatten <= 1.0f) {
                    flatten = parsedFlatten;
                    flattenEnabled = true;
                }
            } catch (const std::exception& e) {
                // Failed to parse, use default
            }
        }
    }
    
    int voxelsGenerated = 0;
    int chunksCreated = 0;
    
    // **OPTIMIZED ISLAND ITERATION** - Wide, low islands with dynamic height scaling
    int radiusInt = static_cast<int>(radius + 1);
    
    // **DYNAMIC HEIGHT SCALING** - Larger islands get taller, but maintain wide/low proportions
    // Base height ratio is 0.075f (half of previous 0.15f)
    // Add scaling factor so larger islands get proportionally more height
    float baseHeightRatio = 0.075f;
    float heightScaling = 1.0f + (radius / 200.0f); // Gradual increase: +50% height at 100 units, +100% at 200 units
    int islandHeight = static_cast<int>(radius * baseHeightRatio * heightScaling);
    
    int searchRadius = static_cast<int>(radius * 1.4f); // Slightly wider search area
    
    // Iterate more efficiently: for each Y layer, use wider horizontal bounds
    for (int y = -islandHeight; y <= islandHeight; y++)
    {
        float dy = static_cast<float>(y);
        int maxXZ = searchRadius; // Use fixed horizontal search area, not spherical constraint
        
        for (int x = -maxXZ; x <= maxXZ; x++)
        {
            for (int z = -maxXZ; z <= maxXZ; z++)
            {
                // Calculate distance from island center (with flatten applied for noise)
                float dx = static_cast<float>(x);
                float dy = static_cast<float>(y);
                float dz = static_cast<float>(z);
                
                // **TRUE NOISE-FIRST APPROACH** - Noise defines the terrain shape directly
                bool shouldPlaceVoxel = false;
                
                if (useNoise) {
                    // **FLATTEN APPLICATION** with robust error handling
                    float dyFlattened = dy;
                    try {
                        if (flatten != 1.0f && flatten > 0.0f) {
                            dyFlattened = dy * flatten;
                        }
                    } catch (...) {
                        dyFlattened = dy;
                    }
                    
                    // **ISLAND-SHAPED GENERATION** - Create realistic wide, low floating islands
                    float distanceFromCenter = std::sqrt(dx * dx + dz * dz); // Only horizontal distance
                    
                    if (distanceFromCenter <= radius * 1.4f) {  // Wider generation area
                        
                        // **BASE ISLAND SHAPE** - Strong radial falloff for island edges
                        float islandBase = 1.0f - (distanceFromCenter / (radius * 1.2f));
                        islandBase = std::max(0.0f, islandBase);
                        islandBase = islandBase * islandBase * islandBase; // Sharper falloff for defined island edges
                        
                        // **HEIGHT-BASED TERRAIN** - More dramatic height variation for low islands
                        float islandHeightRange = islandHeight * 2.0f; // Total height range
                        float normalizedY = (dyFlattened + islandHeight) / islandHeightRange; // 0 at bottom, 1 at top
                        float heightFactor = 1.0f;
                        
                        if (normalizedY < 0.2f) {
                            // Bottom foundation - very solid
                            heightFactor = 1.5f;
                        } else if (normalizedY < 0.6f) {
                            // Middle bulk - main island mass
                            heightFactor = 1.2f;
                        } else {
                            // Upper surface - lighter, creates hills/peaks
                            heightFactor = 0.4f;
                        }
                        
                        // **3D NOISE FOR SURFACE DETAIL** - Multiple octaves for realism
                        float surfaceNoise = 0.0f;
                        float amplitude = 1.0f;
                        float frequency = noiseScale;
                        
                        for (int octave = 0; octave < octaves; octave++) {
                            float sampleX = dx * frequency + seed * 0.1f;
                            float sampleY = dyFlattened * frequency + seed * 0.2f;
                            float sampleZ = dz * frequency + seed * 0.3f;
                            
                            float octaveNoise = (std::sin(sampleX) * std::cos(sampleY) * std::sin(sampleZ) + 1.0f) * 0.5f;
                            surfaceNoise += octaveNoise * amplitude;
                            
                            amplitude *= persistence;
                            frequency *= 2.0f;
                        }
                        
                        // **LARGE-SCALE TERRAIN FEATURES** - Hills and valleys
                        float terrainNoise = 0.0f;
                        float terrainScale = noiseScale * 0.5f; // Larger features
                        amplitude = 0.8f;
                        frequency = terrainScale;
                        
                        for (int octave = 0; octave < 2; octave++) {
                            float sampleX = dx * frequency + seed * 0.4f;
                            float sampleZ = dz * frequency + seed * 0.5f;
                            
                            float octaveNoise = (std::sin(sampleX) * std::cos(sampleZ) + 1.0f) * 0.5f;
                            terrainNoise += octaveNoise * amplitude;
                            
                            amplitude *= 0.5f;
                            frequency *= 2.0f;
                        }
                        
                        // **FINAL ISLAND DENSITY** - Combine all factors
                        float finalDensity = islandBase * heightFactor * (surfaceNoise * 0.7f + terrainNoise * 0.3f);
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
    
    // **SINGLE MESH GENERATION PASS** - much faster than per-voxel generation
    for (auto& [chunkCoord, chunk] : island->chunks)
    {
        if (chunk)
        {
            chunk->generateMesh();
            chunk->buildCollisionMesh();
        }
    }
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

    // Check bounds (should be 0-15 for 16x16x16 chunks)
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

    // Check bounds (should be 0-15 for 16x16x16 chunks)
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
    std::lock_guard<std::mutex> lock(m_islandsMutex);
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

void IslandChunkSystem::getAllChunksWithWorldPos(std::vector<std::pair<VoxelChunk*, Vec3>>& out) const
{
    out.clear();
    std::lock_guard<std::mutex> lock(m_islandsMutex);
    for (const auto& [id, island] : m_islands)
    {
        for (const auto& [chunkCoord, chunk] : island.chunks)
        {
            if (chunk)
            {
                Vec3 worldPos = island.physicsCenter + FloatingIsland::chunkCoordToWorldPos(chunkCoord);
                out.emplace_back(chunk.get(), worldPos);
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
    PROFILE_SCOPE("IslandChunkSystem::renderAllIslands");
    if (!g_vboRenderer) return;
    std::vector<std::pair<VoxelChunk*, Vec3>> snapshot;
    getAllChunksWithWorldPos(snapshot);
    g_vboRenderer->beginBatch();
    for (auto& p : snapshot)
    {
        g_vboRenderer->uploadChunkMesh(p.first);
        g_vboRenderer->renderChunk(p.first, p.second);
    }
    g_vboRenderer->endBatch();
}

void IslandChunkSystem::updateIslandPhysics(float deltaTime)
{
    std::lock_guard<std::mutex> lock(m_islandsMutex);
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
    std::lock_guard<std::mutex> lock(m_islandsMutex);
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


