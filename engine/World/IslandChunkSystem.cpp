// IslandChunkSystem.cpp - Implementation of physics-driven chunking
#include "IslandChunkSystem.h"

#include <cmath>
#include <cstdlib>
#include <ctime>
#include <iostream>
#include <memory>
#include <string>
#include <chrono>
#include <thread>

#include <glad/gl.h>  // For OpenGL functions

#include "VoxelChunk.h"
#include "BlockType.h"  // Add this include for BlockID constants
#include "../Profiling/Profiler.h"
#include "../Rendering/VBORenderer.h"  // RE-ENABLED - VBO only, no immediate mode
#include "../Rendering/GPUMeshGenerator.h"  // NEW: GPU-accelerated mesh generation
#include "../Threading/JobSystem.h"    // For parallel chunk processing

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
    
    // Clean up GPU mesh generator
    cleanupGPUMeshGeneration();
}

bool IslandChunkSystem::initializeGPUMeshGeneration()
{
    std::cout << "🚀 Initializing GPU mesh generation system..." << std::endl;
    std::flush(std::cout);
    
    // First, check if OpenGL is available
    std::cout << "� Checking OpenGL availability..." << std::endl;
    std::flush(std::cout);
    
    try {
        // Get OpenGL version to verify GL is available
        const char* versionStr = (const char*)glGetString(GL_VERSION);
        if (versionStr == nullptr) {
            std::cerr << "❌ OpenGL context not available or not current" << std::endl;
            return false;
        }
        
        std::cout << "✅ OpenGL context available: " << versionStr << std::endl;
        std::flush(std::cout);
        
        // Check for OpenGL errors before proceeding
        GLenum error = glGetError();
        if (error != GL_NO_ERROR) {
            std::cerr << "❌ OpenGL error before GPU mesh generator creation: " << error << std::endl;
            return false;
        }
        
        std::cout << "�🔧 About to create GPUMeshGenerator instance..." << std::endl;
        std::flush(std::cout);
        
        m_gpuMeshGenerator = std::make_unique<GPUMeshGenerator>();
        std::cout << "✅ GPUMeshGenerator instance created successfully" << std::endl;
        std::flush(std::cout);
        
        std::cout << "🔧 About to call initialize()..." << std::endl;
        std::flush(std::cout);
        
        if (!m_gpuMeshGenerator->initialize()) {
            std::cerr << "❌ Failed to initialize GPU mesh generator" << std::endl;
            m_gpuMeshGenerator.reset();
            return false;
        }
        
        std::cout << "✅ GPU mesh generation system initialized successfully" << std::endl;
        return true;
        
    } catch (const std::exception& e) {
        std::cerr << "💥 Exception in initializeGPUMeshGeneration: " << e.what() << std::endl;
        return false;
    } catch (...) {
        std::cerr << "💥 Unknown exception in initializeGPUMeshGeneration" << std::endl;
        return false;
    }
}

void IslandChunkSystem::cleanupGPUMeshGeneration()
{
    if (m_gpuMeshGenerator) {
        m_gpuMeshGenerator.reset();
        std::cout << "🧹 GPU mesh generation system cleaned up" << std::endl;
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

// =============================================================================
// ORGANIC ISLAND GENERATION - Multi-Chunk System Level  
// =============================================================================
// This generates complex, organic floating islands that span multiple chunks.
// Uses sophisticated noise systems for natural-looking terrain generation.
// For simple single-chunk islands, see VoxelChunk::generateFloatingIsland()
//
// TODO: INTEGRATE FASTNOISE2 HERE
// - Replace placeholder surface and terrain noise with FastNoise2
// - Use OpenSimplex2 for surface detail (~0.1 scale)
// - Use Perlin for terrain features (~0.05 scale) 
// - Add cellular noise for block type distribution
// - Consider domain warping for more organic shapes
// =============================================================================

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
    float baseHeightRatio = 0.01875f; // Halved for shorter islands
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
                            // Only keep the middle layer (main island mass)
                            heightFactor = 1.2f;
                        }

                        // TODO: INTEGRATE FASTNOISE2 FOR ORGANIC TERRAIN
                        // Phase 1: Surface detail noise (high frequency, small features)
                        float surfaceNoise = 0.5f; // Placeholder: FastNoise2 OpenSimplex2 at ~0.1 scale
                        
                        // Phase 2: Terrain features noise (low frequency, large formations) 
                        float terrainNoise = 0.5f; // Placeholder: FastNoise2 Perlin at ~0.05 scale
                        
                        // **FINAL ISLAND DENSITY** - Combine all factors
                        float finalDensity = islandBase * heightFactor * (surfaceNoise * 0.7f + terrainNoise * 0.3f);
                        
                        // Apply gentle top falloff only when we're in the upper portion of the island
                        if (normalizedY > 0.6f) {
                            float topFactor = (normalizedY - 0.6f) / 0.4f; // 0.0 at Y=0.6, 1.0 at Y=1.0
                            float topFalloff = 1.0f - (topFactor * 0.3f); // Gentle 30% reduction at the very top
                            finalDensity *= topFalloff;
                        }
                        
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
                    
                    // TODO: INTEGRATE FASTNOISE2 FOR BLOCK SELECTION
                    // Use noise to determine block type distribution across terrain
                    uint8_t blockID;
                    
                    // Placeholder: Replace with FastNoise2 cellular/domain warping for block distribution
                    float blockSelectionNoise = 0.5f; // Will determine dirt vs stone placement
                    
                    if (blockSelectionNoise > 0.5f) {
                        blockID = BlockID::DIRT;
                    } else {
                        blockID = BlockID::STONE;
                    }
                    
                    setBlockIDWithAutoChunk(islandID, islandRelativePos, blockID);
                    // Occasionally place decorative grass above dirt
                    if (blockID == BlockID::DIRT) {
                        // Only place if above within island bounds and with low probability
                        if ((std::rand() % 100) < 3) { // ~3%
                            Vec3 above = islandRelativePos + Vec3(0, 1, 0);
                            setBlockIDWithAutoChunk(islandID, above, BlockID::DECOR_GRASS);
                        }
                    }
                    
                    voxelsGenerated++;
                    
                    // Debug: Count block types for verification
                    static int dirtCount = 0, stoneCount = 0;
                    if (blockID == BlockID::DIRT) dirtCount++;
                    else if (blockID == BlockID::STONE) stoneCount++;
                    
                    // Print stats every 10000 voxels
                    if (voxelsGenerated % 10000 == 0) {
                        float dirtPercent = (float)dirtCount / (float)(dirtCount + stoneCount) * 100.0f;
                        float stonePercent = (float)stoneCount / (float)(dirtCount + stoneCount) * 100.0f;
                        std::cout << "[DEBUG] Block distribution: " << dirtPercent << "% dirt, " << stonePercent << "% stone (total: " << (dirtCount + stoneCount) << ")" << std::endl;
                    }
                    
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

// =============================================================================
// FASTNOISE2 ISLAND GENERATION - Full island noise generation with job system
// =============================================================================
void IslandChunkSystem::generateFloatingIslandFastNoise(uint32_t islandID, uint32_t seed, int width, int height, int depth)
{
    auto totalStartTime = std::chrono::high_resolution_clock::now();
    
    std::cout << "=== MEGA ISLAND GENERATION ===" << std::endl;
    std::cout << "Generating FastNoise2 island (" << width << "x" << height << "x" << depth << ")..." << std::endl;
    std::cout << "Total voxel volume: " << (static_cast<long long>(width) * height * depth) << " voxels" << std::endl;
    
    FloatingIsland* island = getIsland(islandID);
    if (!island) {
        std::cout << "Error: Island " << islandID << " not found!" << std::endl;
        return;
    }

    // **STEP 1: Generate optimized chunk-level noise map using FastNoise2 SIMD**
    auto fnSimplex = FastNoise::New<FastNoise::OpenSimplex2>();
    auto fnGenerator = FastNoise::New<FastNoise::FractalFBm>();
    fnGenerator->SetSource(fnSimplex);
    fnGenerator->SetOctaveCount(2);    // REDUCED: 2 octaves instead of 4 for faster generation
    fnGenerator->SetLacunarity(2.0f);
    fnGenerator->SetGain(0.5f);
    // Note: FastNoise2 uses seed in the generation call, not as a property
    
    // Terrain parameters
    float terrainScale = 0.015f;               // Scale for terrain features
    float terrainDensityThreshold = 0.001f;     // INCREASED: Less dense terrain generation (was 0.03f)
    
    // Falloff parameters for island shape
    float halfWidth = width * 0.5f;
    float halfHeight = height * 0.5f;
    float halfDepth = depth * 0.5f;
    float falloffRadius = std::min(halfWidth, halfDepth) * 0.9f; // INCREASED: 90% radius for more terrain (was 85%)
    
    // **OPTIMIZATION: Chunk-level sampling instead of per-voxel sampling**
    // Sample noise at chunk resolution and interpolate for individual voxels
    int chunkSampleRes = 8; // Sample every 8 voxels (reduces 16M to ~25K samples!)
    int samplesX = (width + chunkSampleRes - 1) / chunkSampleRes;
    int samplesY = (height + chunkSampleRes - 1) / chunkSampleRes;
    int samplesZ = (depth + chunkSampleRes - 1) / chunkSampleRes;
    int totalSamples = samplesX * samplesY * samplesZ;
    
    std::cout << "OPTIMIZED: Sampling " << totalSamples << " noise points instead of " << (width * height * depth) << " (reduction: " << ((width * height * depth) / totalSamples) << "x)" << std::endl;
    
    // Generate coordinate arrays for optimized SIMD processing
    std::vector<float> coordinatesX, coordinatesY, coordinatesZ;
    coordinatesX.reserve(totalSamples);
    coordinatesY.reserve(totalSamples);
    coordinatesZ.reserve(totalSamples);
    
    // Build coordinate arrays for chunk-level sampling
    for (int sy = 0; sy < samplesY; sy++) {
        for (int sx = 0; sx < samplesX; sx++) {
            for (int sz = 0; sz < samplesZ; sz++) {
                int x = (sx * chunkSampleRes) - halfWidth;
                int y = (sy * chunkSampleRes) - halfHeight;
                int z = (sz * chunkSampleRes) - halfDepth;
                coordinatesX.push_back(x * terrainScale);
                coordinatesY.push_back(y * terrainScale);
                coordinatesZ.push_back(z * terrainScale);
            }
        }
    }
    
    // Generate noise for optimized sample points in one SIMD call
    std::cout << "Generating noise with FastNoise2 SIMD for " << coordinatesX.size() << " sample points..." << std::endl;
    std::cout << "Expected time: ~1-2 seconds (vs 30-60 seconds for per-voxel)" << std::endl;
    
    auto noiseStartTime = std::chrono::high_resolution_clock::now();
    std::vector<float> terrainNoise(coordinatesX.size());
    fnGenerator->GenPositionArray3D(terrainNoise.data(), coordinatesX.size(),
                                   coordinatesX.data(), coordinatesY.data(), coordinatesZ.data(),
                                   0.0f, 0.0f, 0.0f, seed);
    auto noiseEndTime = std::chrono::high_resolution_clock::now();
    auto noiseTime = std::chrono::duration_cast<std::chrono::milliseconds>(noiseEndTime - noiseStartTime).count();
    
    std::cout << "✓ SIMD noise generation completed in " << noiseTime << "ms!" << std::endl;
    
    std::cout << "Noise generation complete! Processing voxels with interpolation and creating chunks..." << std::endl;
    
    // **STEP 2: Process voxels using interpolated noise data**
    std::map<Vec3, std::vector<std::pair<Vec3, uint8_t>>> chunkVoxelData; // chunkCoord -> [(localPos, blockType)]
    int solidVoxelsProcessed = 0;
    int totalVoxelsProcessed = 0;
    
    // Helper function for trilinear interpolation of noise values
    auto getInterpolatedNoise = [&](int x, int y, int z) -> float {
        // Calculate sample grid coordinates
        float fx = (float)(x + halfWidth) / chunkSampleRes;
        float fy = (float)(y + halfHeight) / chunkSampleRes;
        float fz = (float)(z + halfDepth) / chunkSampleRes;
        
        // Get integer coordinates and fractional parts
        int x0 = (int)floor(fx), x1 = x0 + 1;
        int y0 = (int)floor(fy), y1 = y0 + 1;
        int z0 = (int)floor(fz), z1 = z0 + 1;
        float dx = fx - x0, dy = fy - y0, dz = fz - z0;
        
        // Bounds checking
        x0 = std::max(0, std::min(samplesX-1, x0));
        x1 = std::max(0, std::min(samplesX-1, x1));
        y0 = std::max(0, std::min(samplesY-1, y0));
        y1 = std::max(0, std::min(samplesY-1, y1));
        z0 = std::max(0, std::min(samplesZ-1, z0));
        z1 = std::max(0, std::min(samplesZ-1, z1));
        
        // Get 8 noise samples for trilinear interpolation
        auto getNoise = [&](int sx, int sy, int sz) -> float {
            int index = sy * (samplesX * samplesZ) + sx * samplesZ + sz;
            return (index >= 0 && index < terrainNoise.size()) ? terrainNoise[index] : 0.0f;
        };
        
        float n000 = getNoise(x0, y0, z0), n001 = getNoise(x0, y0, z1);
        float n010 = getNoise(x0, y1, z0), n011 = getNoise(x0, y1, z1);
        float n100 = getNoise(x1, y0, z0), n101 = getNoise(x1, y0, z1);
        float n110 = getNoise(x1, y1, z0), n111 = getNoise(x1, y1, z1);
        
        // Trilinear interpolation
        float nx00 = n000 * (1-dx) + n100 * dx;
        float nx01 = n001 * (1-dx) + n101 * dx;
        float nx10 = n010 * (1-dx) + n110 * dx;
        float nx11 = n011 * (1-dx) + n111 * dx;
        
        float nxy0 = nx00 * (1-dy) + nx10 * dy;
        float nxy1 = nx01 * (1-dy) + nx11 * dy;
        
        return nxy0 * (1-dz) + nxy1 * dz;
    };
    
    // Process each voxel position using interpolated noise
    for (int y = -halfHeight; y < halfHeight; y++) {
        for (int x = -halfWidth; x < halfWidth; x++) {
            for (int z = -halfDepth; z < halfDepth; z++) {
                totalVoxelsProcessed++;
                
                // Get interpolated noise value
                float noiseValue = getInterpolatedNoise(x, y, z);
                
                // Apply radial falloff in X/Z
                float dx = static_cast<float>(x);
                float dz = static_cast<float>(z);
                float distanceFromCenter = std::sqrt(dx * dx + dz * dz);
                float radialFalloff = 1.0f - std::min(1.0f, distanceFromCenter / falloffRadius);
                
                // Apply vertical density gradient in Y
                float normalizedY = static_cast<float>(y + halfHeight) / height; // 0.0 at bottom, 1.0 at top
                float verticalFalloff = 1.0f - normalizedY; // Higher density at bottom
                
                // Combine noise with falloffs
                float finalDensity = noiseValue * radialFalloff * verticalFalloff;
                
                // Check if this voxel should be solid
                if (finalDensity > terrainDensityThreshold) {
                    solidVoxelsProcessed++;
                    
                    // Calculate chunk coordinates (relative to island center)
                    Vec3 worldPos(x, y, z);
                    Vec3 chunkCoord = island->islandPosToChunkCoord(worldPos);
                    Vec3 localPos = island->islandPosToLocalPos(worldPos);
                    
                    // Assign block type based on height/depth
                    uint8_t blockType = BlockID::DIRT; // Default to dirt
                    if (normalizedY < 0.3f) {
                        blockType = BlockID::STONE; // Stone at bottom
                    } else if (normalizedY > 0.8f && radialFalloff > 0.7f) {
                        blockType = BlockID::GRASS; // Grass on top surfaces
                    }
                    
                    // Add to chunk data
                    chunkVoxelData[chunkCoord].emplace_back(localPos, blockType);
                }
            }
        }
    }
    
    std::cout << "Processed " << totalVoxelsProcessed << " voxels, found " << solidVoxelsProcessed 
              << " solid voxels in " << chunkVoxelData.size() << " chunks" << std::endl;
    
    // **STEP 3: Create and populate chunks using SINGLE-THREADED processing**
    int chunksToCreate = 0;
    int emptyCullsSkipped = 0;
    
    // First pass: count non-empty chunks and cull empty ones
    for (const auto& [chunkCoord, voxelList] : chunkVoxelData) {
        if (voxelList.empty()) {
            emptyCullsSkipped++;
        } else {
            chunksToCreate++;
        }
    }
    
    std::cout << "Processing " << chunksToCreate << " chunks with SINGLE-THREADED meshing..." << std::endl;
    std::cout << "Culled " << emptyCullsSkipped << " empty chunks for performance" << std::endl;
    
    // **GPU OPTIMIZATION: Batch chunk creation and mesh uploads**
    std::cout << "🎯 GPU OPTIMIZATION: Batching mesh generation and uploads for better GPU utilization" << std::endl;
    std::vector<VoxelChunk*> chunksToMesh;
    chunksToMesh.reserve(chunksToCreate);
    
    // Single-threaded chunk creation and meshing
    int completedChunks = 0;
    int failedChunks = 0;
    auto startTime = std::chrono::high_resolution_clock::now();
    
    for (const auto& [chunkCoord, voxelList] : chunkVoxelData) {
        // **EMPTY CHUNK CULLING** - Skip chunks with no voxels
        if (voxelList.empty()) {
            continue;
        }
        
        // Create the chunk on main thread
        addChunkToIsland(islandID, chunkCoord);
        VoxelChunk* chunk = getChunkFromIsland(islandID, chunkCoord);
        
        if (chunk) {
            try {
                // Populate the chunk with voxel data
                for (const auto& [localPos, blockType] : voxelList) {
                    chunk->setVoxel(static_cast<int>(localPos.x), static_cast<int>(localPos.y), 
                                   static_cast<int>(localPos.z), blockType);
                }
                
                // **OPTIMIZATION: Separate CPU mesh generation from GPU upload**
                // Generate mesh on CPU first (unavoidable CPU work)
                chunk->generateMesh();
                chunk->buildCollisionMesh();
                
                // Add to batch for GPU upload (we'll batch upload later)
                chunksToMesh.push_back(chunk);
                completedChunks++;
                
                // Progress reporting every 100 chunks
                if (completedChunks % 100 == 0) {
                    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                        std::chrono::high_resolution_clock::now() - startTime).count();
                    std::cout << "[PROGRESS] " << completedChunks << "/" << chunksToCreate 
                              << " chunks completed (" << elapsed << "ms elapsed)" << std::endl;
                }
            } catch (...) {
                failedChunks++;
            }
        } else {
            failedChunks++;
        }
    }
    
    auto cpuTime = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::high_resolution_clock::now() - startTime).count();
    
    std::cout << "CPU mesh generation complete!" << std::endl;
    std::cout << "✓ Successfully generated " << completedChunks << " chunk meshes on CPU" << std::endl;
    std::cout << "⏱ CPU mesh generation time: " << cpuTime << "ms" << std::endl;
    
    // **GPU BATCH UPLOAD PHASE** - Upload all meshes to GPU efficiently
    std::cout << "🚀 Starting GPU batch upload of " << chunksToMesh.size() << " meshes..." << std::endl;
    auto gpuStartTime = std::chrono::high_resolution_clock::now();
    
    if (g_vboRenderer) {
        g_vboRenderer->beginBatch();
        for (size_t i = 0; i < chunksToMesh.size(); ++i) {
            VoxelChunk* chunk = chunksToMesh[i];
            g_vboRenderer->uploadChunkMesh(chunk);
            
            // Progress for GPU uploads every 200 chunks (GPU uploads are faster)
            if ((i + 1) % 200 == 0) {
                auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::high_resolution_clock::now() - gpuStartTime).count();
                std::cout << "[GPU UPLOAD] " << (i + 1) << "/" << chunksToMesh.size() 
                          << " meshes uploaded (" << elapsed << "ms elapsed)" << std::endl;
            }
        }
        g_vboRenderer->endBatch();
    }
    
    auto gpuTime = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::high_resolution_clock::now() - gpuStartTime).count();
    
    auto totalTime = cpuTime + gpuTime;
    
    std::cout << "GPU batch upload complete!" << std::endl;
    std::cout << "🖥 CPU mesh generation: " << cpuTime << "ms" << std::endl;
    std::cout << "🎮 GPU mesh upload: " << gpuTime << "ms" << std::endl;
    
    std::cout << "Single-threaded chunk generation complete!" << std::endl;
    std::cout << "✓ Successfully meshed " << completedChunks << " chunks" << std::endl;
    if (failedChunks > 0) {
        std::cout << "✗ Failed to mesh " << failedChunks << " chunks" << std::endl;
    }
    std::cout << "⏱ Total SINGLE-THREADED processing time: " << totalTime << "ms" << std::endl;
    std::cout << "📊 GPU utilization ratio: " << ((float)gpuTime / totalTime * 100.0f) << "% GPU vs " << ((float)cpuTime / totalTime * 100.0f) << "% CPU" << std::endl;
    
    auto totalEndTime = std::chrono::high_resolution_clock::now();
    auto totalGenerationTime = std::chrono::duration_cast<std::chrono::milliseconds>(totalEndTime - totalStartTime).count();
    
    std::cout << "=== MEGA ISLAND GENERATION COMPLETE ===" << std::endl;
    std::cout << "🏝 Total island generation time: " << totalGenerationTime << "ms (" << (totalGenerationTime/1000.0) << "s)" << std::endl;
    std::cout << "📊 Performance: " << (static_cast<long long>(width) * height * depth / 1000) << "K voxels in " << totalGenerationTime << "ms" << std::endl;
    std::cout << "⚡ Rate: " << (static_cast<long long>(width) * height * depth / std::max(1LL, (long long)totalGenerationTime)) << "K voxels/second" << std::endl;
    
    std::cout << "FastNoise2 island generation complete!" << std::endl;
}

uint8_t IslandChunkSystem::getVoxelFromIsland(uint32_t islandID, const Vec3& islandRelativePosition) const
{
    // Hold lock across the entire access to prevent races
    std::lock_guard<std::mutex> lock(m_islandsMutex);
    auto itIsl = m_islands.find(islandID);
    if (itIsl == m_islands.end())
        return 0;

    const FloatingIsland& island = itIsl->second;

    // Convert island-relative position to chunk coordinate and local voxel position
    Vec3 chunkCoord = FloatingIsland::islandPosToChunkCoord(islandRelativePosition);
    Vec3 localPos = FloatingIsland::islandPosToLocalPos(islandRelativePosition);

    // Find the chunk
    auto it = island.chunks.find(chunkCoord);
    if (it == island.chunks.end())
        return 0; // Chunk doesn't exist

    const VoxelChunk* chunk = it->second.get();
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
    // Acquire chunk pointer under lock, then perform heavy work without holding the map mutex
    VoxelChunk* chunk = nullptr;
    Vec3 localPos;
    {
        std::lock_guard<std::mutex> lock(m_islandsMutex);
        auto itIsl = m_islands.find(islandID);
        if (itIsl == m_islands.end())
            return;
        FloatingIsland& island = itIsl->second;
        Vec3 chunkCoord = FloatingIsland::islandPosToChunkCoord(islandRelativePosition);
        localPos = FloatingIsland::islandPosToLocalPos(islandRelativePosition);
        std::unique_ptr<VoxelChunk>& chunkPtr = island.chunks[chunkCoord];
        if (!chunkPtr)
            chunkPtr = std::make_unique<VoxelChunk>();
        chunk = chunkPtr.get();
    }

    // Set voxel and rebuild mesh outside of islands mutex to avoid deadlocks
    int x = static_cast<int>(localPos.x);
    int y = static_cast<int>(localPos.y);
    int z = static_cast<int>(localPos.z);
    if (x < 0 || x >= VoxelChunk::SIZE || y < 0 || y >= VoxelChunk::SIZE || z < 0 || z >= VoxelChunk::SIZE)
        return;
    chunk->setVoxel(x, y, z, voxelType);
    chunk->generateMesh();
}

void IslandChunkSystem::setVoxelWithAutoChunk(uint32_t islandID, const Vec3& islandRelativePos, uint8_t voxelType)
{
    // Acquire chunk pointer under lock, then write outside the lock
    VoxelChunk* chunk = nullptr;
    int localX = 0, localY = 0, localZ = 0;
    {
        std::lock_guard<std::mutex> lock(m_islandsMutex);
        auto itIsl = m_islands.find(islandID);
        if (itIsl == m_islands.end())
            return;
        FloatingIsland& island = itIsl->second;

        int chunkX = static_cast<int>(std::floor(islandRelativePos.x / VoxelChunk::SIZE));
        int chunkY = static_cast<int>(std::floor(islandRelativePos.y / VoxelChunk::SIZE));
        int chunkZ = static_cast<int>(std::floor(islandRelativePos.z / VoxelChunk::SIZE));
        Vec3 chunkCoord(chunkX, chunkY, chunkZ);

        localX = static_cast<int>(islandRelativePos.x) % VoxelChunk::SIZE;
        localY = static_cast<int>(islandRelativePos.y) % VoxelChunk::SIZE;
        localZ = static_cast<int>(islandRelativePos.z) % VoxelChunk::SIZE;
        if (localX < 0) localX += VoxelChunk::SIZE;
        if (localY < 0) localY += VoxelChunk::SIZE;
        if (localZ < 0) localZ += VoxelChunk::SIZE;

        std::unique_ptr<VoxelChunk>& chunkPtr = island.chunks[chunkCoord];
        if (!chunkPtr)
            chunkPtr = std::make_unique<VoxelChunk>();
        chunk = chunkPtr.get();
    }

    if (!chunk) return;
    chunk->setVoxel(localX, localY, localZ, voxelType);
    // Mesh generation can be deferred/batched; leave as-is to avoid stutters
}

// ID-based block methods (clean and efficient)
void IslandChunkSystem::setBlockIDWithAutoChunk(uint32_t islandID, const Vec3& islandRelativePos, uint8_t blockID)
{
    setVoxelWithAutoChunk(islandID, islandRelativePos, blockID);
}

uint8_t IslandChunkSystem::getBlockIDInIsland(uint32_t islandID, const Vec3& islandRelativePosition) const
{
    return getVoxelFromIsland(islandID, islandRelativePosition);
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
    
    g_vboRenderer->beginBatch();
    
    // Render each island as a batch (much more efficient)
    {
        PROFILE_SCOPE("renderAllIslands::islandIteration");
        std::lock_guard<std::mutex> lock(m_islandsMutex);
        
        for (const auto& [islandId, island] : m_islands)
        {
            PROFILE_SCOPE("renderAllIslands::singleIsland");
            
            // Get island world position
            Vec3 islandWorldPos = island.physicsCenter;
            
            // Begin optimized batch for this island
            g_vboRenderer->beginIslandBatch(islandWorldPos);
            
            // Render all chunks for this island
            for (const auto& [chunkCoord, chunk] : island.chunks)
            {
                if (!chunk) continue;
                
                // Upload mesh if needed
                VoxelMesh& mesh = chunk->getMesh();
                if (mesh.VAO == 0 || mesh.needsUpdate) {
                    g_vboRenderer->uploadChunkMesh(chunk.get());
                }
                
                // Calculate chunk's relative offset to island center
                Vec3 chunkRelativeOffset = FloatingIsland::chunkCoordToWorldPos(chunkCoord);
                
                // Render chunk with optimized method (only sets model matrix)
                g_vboRenderer->renderIslandChunk(chunk.get(), chunkRelativeOffset);
            }
            
            // End batch for this island
            g_vboRenderer->endIslandBatch();
        }
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


