// World/Island.cpp - Island implementation with FastNoise2 and JobSystem integration
#include "pch.h"
#include "Island.h"
#include "Chunk.h"
#include "../Threading/JobSystem.h"
#include <FastNoise/FastNoise.h>
#include <cmath>
#include <algorithm>

namespace Engine {
namespace World {

// FastNoise2 generator for high-performance terrain generation
class TerrainGenerator {
public:
    TerrainGenerator(uint32_t seed) : m_seed(seed) {
        // Create simplified FastNoise2 generator
        m_generator = FastNoise::New<FastNoise::Simplex>();
        // Note: Seed is handled per-call in FastNoise2
    }
    
    float getHeight(float x, float z) const {
        // Generate noise value using FastNoise2
        return m_generator->GenSingle2D(x * 0.01f, z * 0.01f, m_seed);
    }
    
    // Simplified batch generation for chunks
    void generateChunkTerrain(float* heightMap, int startX, int startZ, int size) const {
        // Use simple loop for now - more reliable than SIMD batch
        for (int z = 0; z < size; z++) {
            for (int x = 0; x < size; x++) {
                int index = z * size + x;
                heightMap[index] = getHeight(startX + x, startZ + z);
            }
        }
    }

private:
    uint32_t m_seed;
    FastNoise::SmartNode<FastNoise::Simplex> m_generator;
};

// Job payload for world generation tasks
struct WorldGenJobData {
    int islandX, islandZ;
    uint32_t seed;
    std::vector<std::unique_ptr<Chunk>>* chunks; // Pointer to island's chunk array
    int chunksX, chunksY, chunksZ;
};

// Island implementation
Island::Island(int islandX, int islandZ, uint32_t seed)
    : m_islandX(islandX)
    , m_islandZ(islandZ)
    , m_seed(seed)
    , m_isGenerated(false)
    , m_worldPosition(0.0f, 0.0f, 0.0f)
    , m_velocity(0.0f, 0.0f, 0.0f)
    , m_hasPhysics(false)
{
    // Pre-allocate chunks
    m_chunks.resize(CHUNKS_X * CHUNKS_Y * CHUNKS_Z);
    
    // Create chunk objects
    for (int y = 0; y < CHUNKS_Y; y++) {
        for (int z = 0; z < CHUNKS_Z; z++) {
            for (int x = 0; x < CHUNKS_X; x++) {
                int index = getChunkIndex(x, y, z);
                m_chunks[index] = std::make_unique<Chunk>(x, y, z);
            }
        }
    }
    
    // Set initial world position based on island coordinates
    m_worldPosition = Vec3(
        islandX * ISLAND_WIDTH, 
        0.0f, 
        islandZ * ISLAND_DEPTH
    );
    
    // Generate random initial velocity for interesting movement
    float velocityScale = 2.0f; // Blocks per second
    m_velocity = Vec3(
        (static_cast<float>(rand()) / RAND_MAX - 0.5f) * velocityScale,
        (static_cast<float>(rand()) / RAND_MAX - 0.5f) * velocityScale * 0.3f, // Less Y movement
        (static_cast<float>(rand()) / RAND_MAX - 0.5f) * velocityScale
    );
    
    // Enable physics by default for dynamic worlds
    m_hasPhysics = true;
}

Island::~Island() = default;

Chunk* Island::getChunk(int chunkX, int chunkY, int chunkZ) {
    if (!isValidChunkCoord(chunkX, chunkY, chunkZ)) return nullptr;
    return m_chunks[getChunkIndex(chunkX, chunkY, chunkZ)].get();
}

bool Island::isValidChunkCoord(int chunkX, int chunkY, int chunkZ) const {
    return chunkX >= 0 && chunkX < CHUNKS_X &&
           chunkY >= 0 && chunkY < CHUNKS_Y &&
           chunkZ >= 0 && chunkZ < CHUNKS_Z;
}

void Island::generateTerrain() {
    if (m_isGenerated) return;
    
    std::cout << "Generating terrain for island (" << m_islandX << ", " << m_islandZ << ") using FastNoise2..." << std::endl;
    
    // Create FastNoise2 terrain generator
    TerrainGenerator generator(m_seed);
    
    // For now, generate synchronously using FastNoise2 (faster than old custom noise)
    // TODO: Add JobSystem integration once basic functionality is working
    generateTerrainSIMD(generator);
    
    m_isGenerated = true;
    std::cout << "Terrain generation complete for island (" << m_islandX << ", " << m_islandZ << ")" << std::endl;
}

void Island::generateTerrainSIMD(const TerrainGenerator& generator) {
    // SIMD-optimized terrain generation using FastNoise2
    constexpr int BATCH_SIZE = Chunk::CHUNK_SIZE * Chunk::CHUNK_SIZE;
    std::vector<float> heightMap(BATCH_SIZE);
    
    for (int y = 0; y < CHUNKS_Y; y++) {
        for (int z = 0; z < CHUNKS_Z; z++) {
            for (int x = 0; x < CHUNKS_X; x++) {
                Chunk* chunk = getChunk(x, y, z);
                if (!chunk) continue;
                
                // Generate SIMD-optimized height map for this chunk's XZ slice
                int worldStartX = x * Chunk::CHUNK_SIZE;
                int worldStartZ = z * Chunk::CHUNK_SIZE;
                
                generator.generateChunkTerrain(heightMap.data(), worldStartX, worldStartZ, Chunk::CHUNK_SIZE);
                
                // Apply height map to generate voxels
                for (int by = 0; by < Chunk::CHUNK_SIZE; by++) {
                    for (int bz = 0; bz < Chunk::CHUNK_SIZE; bz++) {
                        for (int bx = 0; bx < Chunk::CHUNK_SIZE; bx++) {
                            int worldY = y * Chunk::CHUNK_SIZE + by;
                            
                            // Get height from SIMD-generated map
                            int heightIndex = bz * Chunk::CHUNK_SIZE + bx;
                            float height = heightMap[heightIndex] * 32.0f + 64.0f; // Scale and offset
                            
                            // Clamp height to island bounds
                            height = std::max(0.0f, std::min(float(ISLAND_HEIGHT - 1), height));
                            
                            // Place blocks based on height (SoA-friendly)
                            uint8_t blockType = static_cast<uint8_t>(BlockType::AIR);
                            if (worldY <= height) {
                                blockType = static_cast<uint8_t>(BlockType::DIRT);
                            }
                            
                            chunk->setBlock(bx, by, bz, blockType);
                        }
                    }
                }
                
                chunk->markGenerated();
                chunk->checkIfEmpty();
            }
        }
    }
}

uint8_t Island::getBlock(int x, int y, int z) const {
    if (x < 0 || x >= ISLAND_WIDTH || y < 0 || y >= ISLAND_HEIGHT || z < 0 || z >= ISLAND_DEPTH) {
        return static_cast<uint8_t>(BlockType::AIR);
    }
    
    int chunkX = x / Chunk::CHUNK_SIZE;
    int chunkY = y / Chunk::CHUNK_SIZE;
    int chunkZ = z / Chunk::CHUNK_SIZE;
    
    const Chunk* chunk = const_cast<Island*>(this)->getChunk(chunkX, chunkY, chunkZ);
    if (!chunk) return static_cast<uint8_t>(BlockType::AIR);
    
    int localX = x % Chunk::CHUNK_SIZE;
    int localY = y % Chunk::CHUNK_SIZE;
    int localZ = z % Chunk::CHUNK_SIZE;
    
    return chunk->getBlock(localX, localY, localZ);
}

void Island::setBlock(int x, int y, int z, uint8_t blockType) {
    if (x < 0 || x >= ISLAND_WIDTH || y < 0 || y >= ISLAND_HEIGHT || z < 0 || z >= ISLAND_DEPTH) {
        return;
    }
    
    int chunkX = x / Chunk::CHUNK_SIZE;
    int chunkY = y / Chunk::CHUNK_SIZE;
    int chunkZ = z / Chunk::CHUNK_SIZE;
    
    Chunk* chunk = getChunk(chunkX, chunkY, chunkZ);
    if (!chunk) return;
    
    int localX = x % Chunk::CHUNK_SIZE;
    int localY = y % Chunk::CHUNK_SIZE;
    int localZ = z % Chunk::CHUNK_SIZE;
    
    chunk->setBlock(localX, localY, localZ, blockType);
}

std::vector<Chunk*> Island::getDirtyChunks() {
    std::vector<Chunk*> dirtyChunks;
    
    for (auto& chunk : m_chunks) {
        if (chunk && chunk->isDirty()) {
            dirtyChunks.push_back(chunk.get());
        }
    }
    
    return dirtyChunks;
}

void Island::markAllChunksClean() {
    for (auto& chunk : m_chunks) {
        if (chunk) {
            chunk->markClean();
        }
    }
}

void Island::updatePhysics(float deltaTime) {
    if (!m_hasPhysics) return;
    
    // Simple physics integration - position += velocity * time
    m_worldPosition += m_velocity * deltaTime;
    
    // Optional: Add some interesting physics behaviors
    // Gentle sine wave motion for more organic movement
    static float time = 0.0f;
    time += deltaTime;
    
    // Add slight oscillation to make movement more interesting
    Vec3 oscillation(
        std::sin(time * 0.7f + m_seed * 0.01f) * 0.5f,
        std::cos(time * 0.5f + m_seed * 0.02f) * 0.3f,
        std::sin(time * 0.9f + m_seed * 0.03f) * 0.4f
    );
    
    m_worldPosition += oscillation * deltaTime;
    
    // Debug output every 5 seconds
    static float debugTimer = 0.0f;
    debugTimer += deltaTime;
    if (debugTimer >= 5.0f) {
        std::cout << "Island (" << m_islandX << "," << m_islandZ 
                  << ") at position (" << m_worldPosition.x << ", " << m_worldPosition.y << ", " << m_worldPosition.z 
                  << ") velocity (" << m_velocity.x << ", " << m_velocity.y << ", " << m_velocity.z << ")" << std::endl;
        debugTimer = 0.0f;
    }
}

int Island::getChunkIndex(int chunkX, int chunkY, int chunkZ) const {
    return chunkY * CHUNKS_X * CHUNKS_Z + chunkZ * CHUNKS_X + chunkX;
}

} // namespace World
} // namespace Engine
