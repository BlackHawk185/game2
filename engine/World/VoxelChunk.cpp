// VoxelChunk.cpp - 32x32x32 dynamic physics-enabled voxel chunks
#include "VoxelChunk.h"

#include "../Rendering/VBORenderer.h"
#include "../Time/DayNightCycle.h"  // For dynamic sun direction

#include <algorithm>
#include <chrono>
#include <cmath>
#include <iostream>
#include <memory>
#include <thread>
#include <vector>
#include <glad/gl.h>  // For OpenGL light map texture functions

#include "Threading/JobSystem.h"
#include "../Core/Profiler.h"
#include "IslandChunkSystem.h"  // For inter-island raycast queries

// External job system reference
extern JobSystem g_jobSystem;

VoxelChunk::VoxelChunk()
{
    // Initialize voxel data to empty (0 = air)
    std::fill(voxels.begin(), voxels.end(), 0);
    meshDirty = true;
    collisionMesh.needsUpdate = true;
    
    // Initialize VBO handles
    mesh.VAO = 0;
    mesh.VBO = 0;
    mesh.EBO = 0;
    mesh.needsUpdate = true;
    
    // Initialize light maps
    for (int face = 0; face < 6; ++face) {
        lightMaps.getFaceMap(face).textureHandle = 0;
        // Fill with default lighting (mid-gray = normal lighting)
        std::fill(lightMaps.getFaceMap(face).data.begin(), lightMaps.getFaceMap(face).data.end(), 128);
    }
}

VoxelChunk::~VoxelChunk()
{
    // Clean up VBO resources if they exist
    // We'll handle this through the VBORenderer to avoid OpenGL context issues
}

uint8_t VoxelChunk::getVoxel(int x, int y, int z) const
{
    if (x < 0 || x >= SIZE || y < 0 || y >= SIZE || z < 0 || z >= SIZE)
        return 0;  // Out of bounds = air
    return voxels[x + y * SIZE + z * SIZE * SIZE];
}

void VoxelChunk::setVoxel(int x, int y, int z, uint8_t type)
{
    if (x < 0 || x >= SIZE || y < 0 || y >= SIZE || z < 0 || z >= SIZE)
        return;
    voxels[x + y * SIZE + z * SIZE * SIZE] = type;
    meshDirty = true;
    lightingDirty = true;  // NEW: Mark lighting as needing update when voxels change
    collisionMesh.needsUpdate = true;
}

void VoxelChunk::setRawVoxelData(const uint8_t* data, uint32_t size)
{
    if (size != VOLUME)
    {
        // TEMPORARY FIX: Handle legacy 32x32x32 chunks by extracting the 16x16x16 corner
        if (size == 32768) // 32*32*32 = 32768
        {
            // Extract the first 16x16x16 corner from the 32x32x32 data
            for (int z = 0; z < SIZE; z++)
            {
                for (int y = 0; y < SIZE; y++)
                {
                    for (int x = 0; x < SIZE; x++)
                    {
                        // Calculate index in 32x32x32 array
                        int oldIndex = x + y * 32 + z * 32 * 32;
                        // Calculate index in 16x16x16 array
                        int newIndex = x + y * SIZE + z * SIZE * SIZE;
                        voxels[newIndex] = data[oldIndex];
                    }
                }
            }
            meshDirty = true;
            collisionMesh.needsUpdate = true;
            return;
        }
        
        return;
    }
    std::copy(data, data + size, voxels.begin());
    meshDirty = true;
    collisionMesh.needsUpdate = true;
}

void VoxelChunk::addCollisionQuad(float x, float y, float z, int face)
{
    // Each face is a quad (4 vertices)
    // Face order: 0=+Z, 1=-Z, 2=+Y, 3=-Y, 4=+X, 5=-X
    static const Vec3 quadVertices[6][4] = {
        // +Z
        {Vec3(0, 0, 1), Vec3(1, 0, 1), Vec3(1, 1, 1), Vec3(0, 1, 1)},
        // -Z
        {Vec3(1, 0, 0), Vec3(0, 0, 0), Vec3(0, 1, 0), Vec3(1, 1, 0)},
        // +Y
        {Vec3(0, 1, 0), Vec3(0, 1, 1), Vec3(1, 1, 1), Vec3(1, 1, 0)},
        // -Y
        {Vec3(1, 0, 0), Vec3(1, 0, 1), Vec3(0, 0, 1), Vec3(0, 0, 0)},
        // +X
        {Vec3(1, 0, 1), Vec3(1, 0, 0), Vec3(1, 1, 0), Vec3(1, 1, 1)},
        // -X
        {Vec3(0, 0, 0), Vec3(0, 0, 1), Vec3(0, 1, 1), Vec3(0, 1, 0)}};

    for (int i = 0; i < 4; ++i)
    {
        collisionMeshVertices.push_back(Vec3(x, y, z) + quadVertices[face][i]);
    }
}

bool VoxelChunk::shouldRenderFace(int x, int y, int z, int faceDir) const
{
    // Only render faces that are exposed to air
    int adjX = x, adjY = y, adjZ = z;
    switch (faceDir)
    {
        case 0:
            adjZ++;
            break;  // Front face (+Z)
        case 1:
            adjZ--;
            break;  // Back face (-Z)
        case 2:
            adjY++;
            break;  // Top face (+Y)
        case 3:
            adjY--;
            break;  // Bottom face (-Y)
        case 4:
            adjX++;
            break;  // Right face (+X)
        case 5:
            adjX--;
            break;  // Left face (-X)
    }
    if (adjX < 0 || adjX >= SIZE || adjY < 0 || adjY >= SIZE || adjZ < 0 || adjZ >= SIZE)
        return true;  // Always render boundary faces
    return getVoxel(adjX, adjY, adjZ) == 0;
}

bool VoxelChunk::isVoxelSolid(int x, int y, int z) const
{
    return getVoxel(x, y, z) != 0;
}

void VoxelChunk::addQuad(std::vector<Vertex>& vertices, std::vector<uint32_t>& indices, float x,
                         float y, float z, int face, uint8_t blockType)
{
    uint32_t startIndex = static_cast<uint32_t>(vertices.size());

    // Face order: 0=+Z, 1=-Z, 2=+Y, 3=-Y, 4=+X, 5=-X
    static const Vec3 quadVertices[6][4] = {
        // +Z (front)
        {Vec3(0, 0, 1), Vec3(1, 0, 1), Vec3(1, 1, 1), Vec3(0, 1, 1)},
        // -Z (back)
        {Vec3(1, 0, 0), Vec3(0, 0, 0), Vec3(0, 1, 0), Vec3(1, 1, 0)},
        // +Y (top)
        {Vec3(0, 1, 0), Vec3(0, 1, 1), Vec3(1, 1, 1), Vec3(1, 1, 0)},
        // -Y (bottom)
        {Vec3(1, 0, 0), Vec3(1, 0, 1), Vec3(0, 0, 1), Vec3(0, 0, 0)},
        // +X (right)
        {Vec3(1, 0, 1), Vec3(1, 0, 0), Vec3(1, 1, 0), Vec3(1, 1, 1)},
        // -X (left)
        {Vec3(0, 0, 0), Vec3(0, 0, 1), Vec3(0, 1, 1), Vec3(0, 1, 0)}};

    // Normals for each face
    static const Vec3 normals[6] = {
        Vec3(0, 0, 1),   // +Z
        Vec3(0, 0, -1),  // -Z
        Vec3(0, 1, 0),   // +Y
        Vec3(0, -1, 0),  // -Y
        Vec3(1, 0, 0),   // +X
        Vec3(-1, 0, 0)   // -X
    };

    // Texture coordinates for each vertex of the quad
    static const float texCoords[4][2] = {{0.0f, 0.0f}, {1.0f, 0.0f}, {1.0f, 1.0f}, {0.0f, 1.0f}};

    // Add 4 vertices for this quad
    for (int i = 0; i < 4; ++i)
    {
        Vertex v;
        Vec3 pos = Vec3(x, y, z) + quadVertices[face][i];
        v.x = pos.x;
        v.y = pos.y;
        v.z = pos.z;
        v.nx = normals[face].x;
        v.ny = normals[face].y;
        v.nz = normals[face].z;
        v.u = texCoords[i][0];
        v.v = texCoords[i][1];
        
        // Light mapping: Map world position to light map UVs (32x32 texture)
        // For now, use simple planar mapping based on face orientation
        switch (face)
        {
            case 0: case 1: // +Z/-Z faces: use X,Y
                v.lu = (pos.x) / SIZE;
                v.lv = (pos.y) / SIZE;
                break;
            case 2: case 3: // +Y/-Y faces: use X,Z
                v.lu = (pos.x) / SIZE;
                v.lv = (pos.z) / SIZE;
                break;
            case 4: case 5: // +X/-X faces: use Z,Y
                v.lu = (pos.z) / SIZE;
                v.lv = (pos.y) / SIZE;
                break;
        }
        
        // Basic ambient occlusion: compute based on neighboring voxels
        v.ao = computeAmbientOcclusion(static_cast<int>(pos.x), static_cast<int>(pos.y), static_cast<int>(pos.z), face);
        v.faceIndex = static_cast<float>(face);  // Store face index for shader
        
        vertices.push_back(v);
    }

    // Add 6 indices for two triangles (quad)
    indices.push_back(startIndex);
    indices.push_back(startIndex + 1);
    indices.push_back(startIndex + 2);
    indices.push_back(startIndex);
    indices.push_back(startIndex + 2);
    indices.push_back(startIndex + 3);
}

void VoxelChunk::generateMesh()
{
    mesh.vertices.clear();
    mesh.indices.clear();
    collisionMeshVertices.clear();

    // Use greedy meshing for optimal performance
    generateGreedyMesh();
    
    mesh.needsUpdate = true;
    collisionMesh.needsUpdate = true;
    meshDirty = false;
    
    // NEW: Mark lighting as needing recalculation since geometry changed
    lightingDirty = true;
    
    // IMMEDIATE LIGHTING GENERATION: Generate lighting when mesh updates
    // This ensures lighting is always in sync with geometry changes
    generatePerFaceLightMaps();
    
    // Only initialize light maps if they're empty (preserve generated lighting data)
    for (int face = 0; face < 6; ++face) {
        FaceLightMap& faceMap = lightMaps.getFaceMap(face);
        if (faceMap.data.empty()) {
            faceMap.data.resize(32 * 32 * 3);
            std::fill(faceMap.data.begin(), faceMap.data.end(), 255); // Full white default for unlit chunks
        }
    }
    
    // Mark lighting as clean since we just generated it
    lightingDirty = false;
    
    // Note: updateLightMapTextures() will be called during rendering when OpenGL context is available
}

void VoxelChunk::updatePhysicsMesh()
{
    // Collision mesh is already generated in generateMesh()
    // This method exists for compatibility with physics system
}

void VoxelChunk::buildCollisionMesh()
{
    collisionMesh.faces.clear();

    // Build collision faces from collisionMeshVertices
    // Each quad (4 vertices) becomes one collision face
    for (size_t i = 0; i < collisionMeshVertices.size(); i += 4)
    {
        if (i + 3 >= collisionMeshVertices.size())
            break;

        // Calculate face center and normal from the quad vertices
        Vec3 v0 = collisionMeshVertices[i];
        Vec3 v1 = collisionMeshVertices[i + 1];
        Vec3 v2 = collisionMeshVertices[i + 2];

        // Face center is average of vertices
        Vec3 faceCenter = (v0 + v1 + v2 + collisionMeshVertices[i + 3]) * 0.25f;

        // Face normal from cross product of edges
        Vec3 edge1 = v1 - v0;
        Vec3 edge2 = v2 - v0;
        Vec3 normal = edge1.cross(edge2).normalized();

        collisionMesh.faces.push_back({faceCenter, normal});
    }

    collisionMesh.needsUpdate = false;
}

bool VoxelChunk::checkRayCollision(const Vec3& rayOrigin, const Vec3& rayDirection,
                                   float maxDistance, Vec3& hitPoint, Vec3& hitNormal) const
{
    // Simple ray-triangle intersection with collision faces
    float closestDistance = maxDistance;
    bool hit = false;

    for (const auto& face : collisionMesh.faces)
    {
        // Ray-plane intersection
        float denom = rayDirection.dot(face.normal);
        if (abs(denom) < 1e-6f)
            continue;  // Ray parallel to plane

        Vec3 planeToRay = face.position - rayOrigin;
        float t = planeToRay.dot(face.normal) / denom;

        if (t < 0 || t > closestDistance)
            continue;

        // Check if intersection point is within face bounds (simple AABB check)
        Vec3 intersection = rayOrigin + rayDirection * t;
        Vec3 localPoint = intersection - face.position;

        // Determine which axes to check based on face normal
        bool withinBounds = true;
        if (abs(face.normal.x) > 0.5f)
        {  // X-facing face
            withinBounds = abs(localPoint.y) <= 0.5f && abs(localPoint.z) <= 0.5f;
        }
        else if (abs(face.normal.y) > 0.5f)
        {  // Y-facing face
            withinBounds = abs(localPoint.x) <= 0.5f && abs(localPoint.z) <= 0.5f;
        }
        else
        {  // Z-facing face
            withinBounds = abs(localPoint.x) <= 0.5f && abs(localPoint.y) <= 0.5f;
        }

        if (withinBounds)
        {
            closestDistance = t;
            hitPoint = intersection;
            hitNormal = face.normal;
            hit = true;
        }
    }

    return hit;
}

void VoxelChunk::render()
{
    render(Vec3(0, 0, 0));
}

void VoxelChunk::render(const Vec3& worldOffset)
{
    PROFILE_SCOPE("VoxelChunk::render");
    
    if (meshDirty)
    {
        PROFILE_SCOPE("generateMesh");
        generateMesh();
    }

    if (mesh.vertices.empty())
        return;

    // Modern VBO path: upload mesh if needed and render via global VBORenderer
    if (g_vboRenderer)
    {
        if (mesh.needsUpdate)
        {
            g_vboRenderer->uploadChunkMesh(this);
        }
        g_vboRenderer->renderChunk(this, worldOffset);
    }
}

void VoxelChunk::renderLOD(int lodLevel, const Vec3& cameraPos)
{
    // Simple LOD implementation - just render normally for now
    render();
}

int VoxelChunk::calculateLOD(const Vec3& cameraPos) const
{
    // Simple distance-based LOD calculation
    Vec3 chunkCenter(SIZE * 0.5f, SIZE * 0.5f, SIZE * 0.5f);
    Vec3 distance = cameraPos - chunkCenter;
    float dist =
        std::sqrt(distance.x * distance.x + distance.y * distance.y + distance.z * distance.z);

    if (dist < 64.0f)
        return 0;  // High detail
    else if (dist < 128.0f)
        return 1;  // Medium detail
    else
        return 2;  // Low detail
}

bool VoxelChunk::shouldRender(const Vec3& cameraPos, float maxDistance) const
{
    Vec3 chunkCenter(SIZE * 0.5f, SIZE * 0.5f, SIZE * 0.5f);
    Vec3 distance = cameraPos - chunkCenter;
    float dist =
        std::sqrt(distance.x * distance.x + distance.y * distance.y + distance.z * distance.z);
    return dist <= maxDistance;
}

// Simple hash-based value noise in [-1,1] for (x,z)
static inline float vc_hashToUnit(int xi, int zi, uint32_t seed)
{
    uint32_t h = static_cast<uint32_t>(xi) * 374761393u ^ static_cast<uint32_t>(zi) * 668265263u ^ (seed * 0x9E3779B9u);
    h ^= h >> 13; h *= 1274126177u; h ^= h >> 16;
    float u = (h & 0x00FFFFFFu) / 16777215.0f; // [0,1]
    return u * 2.0f - 1.0f; // [-1,1]
}

void VoxelChunk::generateFloatingIsland(int seed, bool useNoise)
{
    // Generate a floating island using simple noise (optional)

    // Simple spherical island parameters
    float centerX = SIZE * 0.5f;
    float centerY = SIZE * 0.3f;
    float centerZ = SIZE * 0.5f;
    // Base radius: allow runtime tuning when noise is enabled
    float baseScale = 0.15f;
    if (useNoise)
    {
        if (const char* env = std::getenv("ISLAND_BASE"))
        {
            try
            {
                baseScale = std::max(0.10f, std::min(0.24f, static_cast<float>(std::atof(env))));
            }
            catch (...)
            {
                baseScale = 0.15f;
            }
        }
    }
    float radius = SIZE * baseScale;

    // Optional vertical flatten (noise only)
    float flatten = 1.0f;
    if (useNoise)
    {
        flatten = 0.90f;
        if (const char* env = std::getenv("ISLAND_FLATTEN"))
        {
            try
            {
                float f = static_cast<float>(std::atof(env));
                // Clamp for safety
                flatten = std::max(0.70f, std::min(1.0f, f));
            }
            catch (...) {}
        }
    }

    // If job system is available, use multithreading
    if (g_jobSystem.isInitialized())
    {
        const int numSlices = 8;  // Split Y-axis into 8 slices for threading
        const int sliceHeight = SIZE / numSlices;
        std::vector<uint32_t> jobIDs;

        // Submit jobs for each Y-slice
        for (int slice = 0; slice < numSlices; slice++)
        {
            int startY = slice * sliceHeight;
            int endY = (slice == numSlices - 1) ? SIZE : (slice + 1) * sliceHeight;

            JobPayload payload;
            // Cast chunk pointer as ID
            payload.chunkID = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(this));

            auto work = [this, startY, endY, centerX, centerY, centerZ, radius,
                         payload, seed, useNoise, flatten]() -> JobResult
            {
                // Generate voxels for this Y-slice
                for (int x = 0; x < SIZE; x++)
                {
                    for (int y = startY; y < endY; y++)
                    {
                        for (int z = 0; z < SIZE; z++)
                        {
                            float dx = x - centerX;
                            float dy = y - centerY;
                            float dz = z - centerZ;
                            float dyUse = useNoise ? (dy * flatten) : dy;
                            float distance = static_cast<float>(std::sqrt(dx * dx + dyUse * dyUse + dz * dz));

                            float rLocal = radius;
                            if (useNoise)
                            {
                                const float freq = 1.0f / 12.0f; // gentle variation
                                int xi = static_cast<int>(std::floor(x * freq));
                                int zi = static_cast<int>(std::floor(z * freq));
                                float n = vc_hashToUnit(xi, zi, static_cast<uint32_t>(seed));
                                float noiseAmp = radius * 0.30f;
                                rLocal = std::max(2.0f, std::min(radius + n * noiseAmp, radius * 1.6f));
                            }

                            if (distance < rLocal)
                                {
                                    setVoxel(x, y, z, 1);  // Simple solid block
                                }
                        }
                    }
                }

                JobResult result;
                result.type = JobType::WORLD_GENERATION;
                result.jobID = payload.chunkID;
                result.success = true;
                return result;
            };

            uint32_t jobID = g_jobSystem.submitJob(JobType::WORLD_GENERATION, payload, work);
            jobIDs.push_back(jobID);
        }

        // Wait for all jobs to complete
        std::vector<JobResult> results;
        size_t completedJobs = 0;
        while (completedJobs < jobIDs.size())
        {
            g_jobSystem.drainCompletedJobs(results, 10);
            for (const auto& result : results)
            {
                if (result.type == JobType::WORLD_GENERATION)
                {
                    completedJobs++;
                }
            }
            results.clear();

            // Small sleep to avoid busy waiting
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }
    else
    {
        // Fallback to single-threaded generation
        for (int x = 0; x < SIZE; x++)
        {
            for (int y = 0; y < SIZE; y++)
            {
                for (int z = 0; z < SIZE; z++)
                {
                    float dx = x - centerX;
                    float dy = y - centerY;
                    float dz = z - centerZ;
                    float dyUse = useNoise ? (dy * flatten) : dy;
                    float distance = static_cast<float>(std::sqrt(dx * dx + dyUse * dyUse + dz * dz));

                    float rLocal = radius;
                    if (useNoise)
                    {
                        const float freq = 1.0f / 12.0f;
                        int xi = static_cast<int>(std::floor(x * freq));
                        int zi = static_cast<int>(std::floor(z * freq));
                        float n = vc_hashToUnit(xi, zi, static_cast<uint32_t>(seed));
                        float noiseAmp = radius * 0.30f;
                        rLocal = std::max(2.0f, std::min(radius + n * noiseAmp, radius * 1.6f));
                    }

                    if (distance < rLocal)
                        {
                            setVoxel(x, y, z, 1);  // Simple solid block
                        }
                }
            }
        }
    }

    meshDirty = true;
    collisionMesh.needsUpdate = true;
}

// Light mapping utilities
float VoxelChunk::computeAmbientOcclusion(int x, int y, int z, int face) const
{
    // Simple ambient occlusion calculation based on neighboring voxels
    // Higher values = more occlusion (darker), range [0.0, 1.0]
    
    static const int faceOffsets[6][3] = {
        {0, 0, 1},   // +Z (front)
        {0, 0, -1},  // -Z (back)
        {0, 1, 0},   // +Y (top)
        {0, -1, 0},  // -Y (bottom)
        {1, 0, 0},   // +X (right)
        {-1, 0, 0}   // -X (left)
    };
    
    // Get the face normal to determine which neighbors to check
    int fx = faceOffsets[face][0];
    int fy = faceOffsets[face][1];
    int fz = faceOffsets[face][2];
    
    // Check 8 neighboring positions around this face
    float occlusion = 0.0f;
    int sampleCount = 0;
    
    // Create a 3x3 grid of offsets perpendicular to the face normal
    for (int du = -1; du <= 1; du++)
    {
        for (int dv = -1; dv <= 1; dv++)
        {
            if (du == 0 && dv == 0) continue; // Skip center
            
            int checkX = x, checkY = y, checkZ = z;
            
            // Map du,dv to world space based on face orientation
            if (face <= 1) // Z faces: map to X,Y
            {
                checkX += du;
                checkY += dv;
            }
            else if (face <= 3) // Y faces: map to X,Z
            {
                checkX += du;
                checkZ += dv;
            }
            else // X faces: map to Z,Y
            {
                checkZ += du;
                checkY += dv;
            }
            
            // Also offset by face direction to check the neighboring voxels
            checkX += fx;
            checkY += fy;
            checkZ += fz;
            
            // Sample the voxel at this position
            if (getVoxel(checkX, checkY, checkZ) != 0)
            {
                occlusion += 0.15f; // Each solid neighbor adds occlusion
            }
            sampleCount++;
        }
    }
    
    // Return ambient lighting factor (1.0 = bright, 0.0 = dark)
    // Clamp to reasonable range for subtle effect
    return std::max(0.3f, 1.0f - occlusion);
}

void VoxelChunk::generatePerFaceLightMaps()
{
    // Reduced debug output for performance
    // Generate separate light maps for each face direction with proper inter-chunk raycasting
    
    const int LIGHTMAP_SIZE = FaceLightMap::LIGHTMAP_SIZE;
    const Vec3 sunDirection = g_dayNightCycle ? g_dayNightCycle->getSunDirection() : Vec3(0.3f, 0.8f, 0.5f).normalized();  // Use dynamic sun direction, fallback for early init
    const float sunIntensity = 1.2f;
    const float ambientIntensity = 0.0f;  // DISABLED for lightmap testing - was 0.2f
    
    // Face normals and face offset directions: 0=+Z, 1=-Z, 2=+Y, 3=-Y, 4=+X, 5=-X
    Vec3 faceNormals[6] = {
        Vec3(0, 0, 1),   // +Z (Forward)
        Vec3(0, 0, -1),  // -Z (Back)
        Vec3(0, 1, 0),   // +Y (Up) 
        Vec3(0, -1, 0),  // -Y (Down)
        Vec3(1, 0, 0),   // +X (Right)
        Vec3(-1, 0, 0)   // -X (Left)
    };
    
    // Generate a light map for each face direction
    for (int faceIndex = 0; faceIndex < 6; ++faceIndex) {
        FaceLightMap& faceMap = lightMaps.getFaceMap(faceIndex);
        faceMap.data.resize(LIGHTMAP_SIZE * LIGHTMAP_SIZE * 3);
        
        Vec3 faceNormal = faceNormals[faceIndex];
        
        for (int v = 0; v < LIGHTMAP_SIZE; v++) {
            for (int u = 0; u < LIGHTMAP_SIZE; u++) {
                float normalizedU = static_cast<float>(u) / (LIGHTMAP_SIZE - 1);  // 0 to 1
                float normalizedV = static_cast<float>(v) / (LIGHTMAP_SIZE - 1);  // 0 to 1
                
                // Calculate world position for this light map texel
                Vec3 worldPos = calculateWorldPositionFromLightMapUV(faceIndex, normalizedU, normalizedV);
                
                // Calculate ray start position (slightly offset from surface in face normal direction)
                Vec3 rayStart = worldPos + faceNormal * 0.1f;
                
                // Use full inter-chunk/inter-island raycasting for proper lighting
                // This will check occlusion across chunk boundaries and between islands
                bool isOccluded = performSunRaycast(rayStart, sunDirection, SIZE * 3.0f);  // Extended range for inter-chunk
                
                // Calculate base lighting from face orientation
                Vec3 negSunDirection = Vec3(-sunDirection.x, -sunDirection.y, -sunDirection.z);
                float dotProduct = faceNormal.dot(negSunDirection);  // Negative sun direction to get proper lighting
                
                float finalLight;
                if (dotProduct > 0.0f) {
                    // Surface faces toward sun - check full occlusion including other islands
                    float directionalLight = dotProduct * sunIntensity;
                    if (isOccluded) {
                        // Occluded by another voxel (local or remote) - reduced lighting
                        finalLight = ambientIntensity + directionalLight * 0.1f;  // More dramatic shadow
                    } else {
                        // Clear path to sun - full directional lighting
                        finalLight = ambientIntensity + directionalLight;
                    }
                } else {
                    // Surface faces away from sun - just ambient
                    finalLight = ambientIntensity;
                }
                
                // Add some variation based on texel position for visual interest
                float variation = (sin(normalizedU * 3.14159f * 2.0f) * cos(normalizedV * 3.14159f * 2.0f)) * 0.03f;
                finalLight += variation;
                
                // Store in light map (clamp to valid range)
                int index = (v * LIGHTMAP_SIZE + u) * 3;
                uint8_t lightByte = static_cast<uint8_t>(std::min(255.0f, std::max(0.0f, finalLight * 255.0f)));
                faceMap.data[index] = lightByte;
                faceMap.data[index + 1] = lightByte;
                faceMap.data[index + 2] = lightByte;
            }
        }
    }
}

// Helper function to calculate world position from light map UV coordinates
Vec3 VoxelChunk::calculateWorldPositionFromLightMapUV(int faceIndex, float u, float v) const
{
    // Convert light map UV (0-1) to world position within this chunk
    // Each face maps to different axes based on face orientation
    
    float worldU = u * SIZE;  // Scale to chunk size
    float worldV = v * SIZE;
    
    Vec3 worldPos;
    
    switch (faceIndex) {
        case 0: // +Z face: U=X, V=Y, fixed Z=SIZE
            worldPos = Vec3(worldU, worldV, SIZE - 0.5f);
            break;
        case 1: // -Z face: U=X, V=Y, fixed Z=0
            worldPos = Vec3(worldU, worldV, 0.5f);
            break;
        case 2: // +Y face: U=X, V=Z, fixed Y=SIZE
            worldPos = Vec3(worldU, SIZE - 0.5f, worldV);
            break;
        case 3: // -Y face: U=X, V=Z, fixed Y=0
            worldPos = Vec3(worldU, 0.5f, worldV);
            break;
        case 4: // +X face: U=Y, V=Z, fixed X=SIZE
            worldPos = Vec3(SIZE - 0.5f, worldU, worldV);
            break;
        case 5: // -X face: U=Y, V=Z, fixed X=0
            worldPos = Vec3(0.5f, worldU, worldV);
            break;
        default:
            worldPos = Vec3(SIZE * 0.5f, SIZE * 0.5f, SIZE * 0.5f);
            break;
    }
    
    return worldPos;
}

// Helper function to perform raycast toward sun for occlusion testing (local chunk only)
bool VoxelChunk::performLocalSunRaycast(const Vec3& rayStart, const Vec3& sunDirection, float maxDistance) const
{
    // Perform a simple raycast through the voxel grid toward the sun (local chunk only)
    // Returns true if ray hits a solid voxel (occluded), false if clear path
    
    const float stepSize = 0.4f;  // Smaller step size for more accuracy
    const int maxSteps = static_cast<int>(maxDistance / stepSize);
    
    Vec3 rayPos = rayStart;
    Vec3 rayStep = sunDirection * stepSize;
    
    for (int step = 0; step < maxSteps; ++step) {
        rayPos = rayPos + rayStep;
        
        // Check if ray position is outside chunk bounds
        if (rayPos.x < 0 || rayPos.x >= SIZE || 
            rayPos.y < 0 || rayPos.y >= SIZE || 
            rayPos.z < 0 || rayPos.z >= SIZE) {
            // Ray exited chunk bounds - no local occlusion (floating island characteristic)
            return false;
        }
        
        // Sample voxel at current ray position
        int voxelX = static_cast<int>(rayPos.x);
        int voxelY = static_cast<int>(rayPos.y);
        int voxelZ = static_cast<int>(rayPos.z);
        
        // Clamp to valid bounds
        voxelX = std::max(0, std::min(SIZE - 1, voxelX));
        voxelY = std::max(0, std::min(SIZE - 1, voxelY));
        voxelZ = std::max(0, std::min(SIZE - 1, voxelZ));
        
        uint8_t voxel = getVoxel(voxelX, voxelY, voxelZ);
        if (voxel != 0) {
            // Hit a solid voxel - ray is locally occluded
            return true;
        }
    }
    
    // Ray traveled maximum distance without hitting anything locally
    return false;
}

// Helper function to perform raycast toward sun for occlusion testing
bool VoxelChunk::performSunRaycast(const Vec3& rayStart, const Vec3& sunDirection, float maxDistance) const
{
    // Enhanced raycast that checks across multiple islands for proper inter-chunk lighting
    return performInterIslandSunRaycast(rayStart, sunDirection, maxDistance);
}

// Helper function to perform inter-island raycast for lighting occlusion
bool VoxelChunk::performInterIslandSunRaycast(const Vec3& rayStart, const Vec3& sunDirection, float maxDistance) const
{
    // Optimized raycast that can span multiple islands for proper lighting
    
    const float stepSize = 1.0f;  // Larger step size for performance
    const int maxSteps = static_cast<int>(maxDistance / stepSize);
    
    Vec3 rayPos = rayStart;
    Vec3 rayStep = sunDirection * stepSize;
    
    // We need to find which island this chunk belongs to first
    extern IslandChunkSystem g_islandSystem;
    
    // Find our island ID by checking all islands
    uint32_t currentIslandID = 0;
    const auto& islands = g_islandSystem.getIslands();
    
    for (const auto& [islandID, island] : islands) {
        for (const auto& [chunkCoord, chunk] : island.chunks) {
            if (chunk.get() == this) {
                currentIslandID = islandID;
                break;
            }
        }
        if (currentIslandID != 0) break;
    }
    
    if (currentIslandID == 0) {
        // Fallback to local raycast if we can't find our island
        return performLocalSunRaycast(rayStart, sunDirection, maxDistance);
    }
    
    // Get our island's center for coordinate conversion
    Vec3 islandCenter = g_islandSystem.getIslandCenter(currentIslandID);
    
    // Limit raycast steps for performance - use smaller range for inter-island checks
    const int limitedSteps = std::min(maxSteps, static_cast<int>(SIZE * 1.5f / stepSize));
    
    for (int step = 0; step < limitedSteps; ++step) {
        rayPos = rayPos + rayStep;
        
        // First check local island (fastest)
        if (rayPos.x >= 0 && rayPos.x < SIZE && 
            rayPos.y >= 0 && rayPos.y < SIZE && 
            rayPos.z >= 0 && rayPos.z < SIZE) {
            
            int voxelX = static_cast<int>(rayPos.x);
            int voxelY = static_cast<int>(rayPos.y);
            int voxelZ = static_cast<int>(rayPos.z);
            
            uint8_t voxel = getVoxel(voxelX, voxelY, voxelZ);
            if (voxel != 0) {
                return true;  // Hit local voxel
            }
        } else {
            // Only check nearby islands for efficiency
            Vec3 worldRayPos = rayPos + islandCenter;
            
            // Check only the 2 closest islands to avoid O(n²) complexity
            int islandsChecked = 0;
            for (const auto& [otherIslandID, otherIsland] : islands) {
                if (otherIslandID == currentIslandID) continue;  // Skip our own island
                if (++islandsChecked > 2) break;  // Limit to 2 nearby islands
                
                // Convert world position to island-relative position
                Vec3 otherIslandCenter = g_islandSystem.getIslandCenter(otherIslandID);
                Vec3 islandRelativePos = worldRayPos - otherIslandCenter;
                
                // Quick distance check - skip if too far
                float distToIsland = islandRelativePos.length();
                if (distToIsland > SIZE * 2.0f) continue;
                
                // Query voxel from this island
                uint8_t voxel = g_islandSystem.getVoxelFromIsland(otherIslandID, islandRelativePos);
                if (voxel != 0) {
                    return true;  // Ray is occluded by another island's voxel
                }
            }
        }
    }
    
    // Ray traveled limited distance without hitting anything significant
    return false;
}

void VoxelChunk::updateLightMapTextures()
{
    // Safety check: Only create textures when we have an OpenGL context
    // This should only be called from the rendering thread
    
    for (int faceIndex = 0; faceIndex < 6; ++faceIndex) {
        FaceLightMap& faceMap = lightMaps.getFaceMap(faceIndex);
        
        // Generate OpenGL texture if it doesn't exist
        if (faceMap.textureHandle == 0)
        {
            glGenTextures(1, &faceMap.textureHandle);
            
            // Check for OpenGL errors
            GLenum error = glGetError();
            if (error != GL_NO_ERROR)
            {
                std::cerr << "Failed to generate face " << faceIndex << " light map texture, OpenGL error: " << error << std::endl;
                continue;
            }
        }
        
        // Upload light map data to GPU
        glBindTexture(GL_TEXTURE_2D, faceMap.textureHandle);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, FaceLightMap::LIGHTMAP_SIZE, FaceLightMap::LIGHTMAP_SIZE, 0, GL_RGB, GL_UNSIGNED_BYTE, faceMap.data.data());
        
        // Set texture parameters for light maps
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        
        // Check for any OpenGL errors
        GLenum error = glGetError();
        if (error != GL_NO_ERROR)
        {
            std::cerr << "OpenGL error in updateLightMapTextures for face " << faceIndex << ": " << error << std::endl;
        }
    }
    
    glBindTexture(GL_TEXTURE_2D, 0);
}

void VoxelChunk::markLightMapsDirty() {
    // Mark all face light map textures as needing recreation
    for (int face = 0; face < 6; ++face) {
        lightMaps.getFaceMap(face).textureHandle = 0; // Force recreation
    }
}

bool VoxelChunk::hasValidLightMaps() const {
    // Check if all lightmap faces have valid texture handles
    for (int face = 0; face < 6; ++face) {
        if (lightMaps.getFaceMap(face).textureHandle == 0) {
            return false;
        }
    }
    return true;
}

bool VoxelChunk::hasLightMapData() const {
    // Check if lightmap data is present (even if textures aren't created yet)
    for (int face = 0; face < 6; ++face) {
        if (lightMaps.getFaceMap(face).data.empty()) {
            return false;
        }
    }
    return true;
}

// ========================================
// GREEDY MESHING IMPLEMENTATION
// ========================================

void VoxelChunk::generateGreedyMesh()
{
    PROFILE_SCOPE("VoxelChunk::generateGreedyMesh");
    
    // Generate quads for each of the 6 directions (faces)
    for (int direction = 0; direction < 6; ++direction)
    {
        generateGreedyQuads(direction, mesh.vertices, mesh.indices);
    }
}

void VoxelChunk::generateGreedyQuads(int direction, std::vector<Vertex>& vertices, std::vector<uint32_t>& indices)
{
    // Direction mapping:
    // 0: +X (right), 1: -X (left), 2: +Y (up), 3: -Y (down), 4: +Z (forward), 5: -Z (back)
    
    const int dx[] = {1, -1, 0, 0, 0, 0};
    const int dy[] = {0, 0, 1, -1, 0, 0};
    const int dz[] = {0, 0, 0, 0, 1, -1};
    
    // For each direction, we have a different slicing plane
    int u, v, w;  // u,v = plane coordinates, w = depth coordinate
    int uMax, vMax, wMax;
    
    switch (direction)
    {
        case 0: case 1: // X faces
            u = 1; v = 2; w = 0; // u=Y, v=Z, w=X
            uMax = SIZE; vMax = SIZE; wMax = SIZE;
            break;
        case 2: case 3: // Y faces  
            u = 0; v = 2; w = 1; // u=X, v=Z, w=Y
            uMax = SIZE; vMax = SIZE; wMax = SIZE;
            break;
        case 4: case 5: // Z faces
            u = 0; v = 1; w = 2; // u=X, v=Y, w=Z
            uMax = SIZE; vMax = SIZE; wMax = SIZE;
            break;
        default:
            return;
    }
    
    // Temporary mask to track which voxels need quads
    std::vector<uint8_t> mask(SIZE * SIZE);
    
    // Slice through the chunk in the w direction
    for (int wPos = 0; wPos < wMax; ++wPos)
    {
        // Reset mask for this slice
        std::fill(mask.begin(), mask.end(), 0);
        
        // Build mask for this slice
        for (int vPos = 0; vPos < vMax; ++vPos)
        {
            for (int uPos = 0; uPos < uMax; ++uPos)
            {
                int x, y, z;
                switch (direction)
                {
                    case 0: case 1: x = wPos; y = uPos; z = vPos; break;
                    case 2: case 3: x = uPos; y = wPos; z = vPos; break;
                    case 4: case 5: x = uPos; y = vPos; z = wPos; break;
                }
                
                uint8_t currentVoxel = getVoxel(x, y, z);
                uint8_t neighborVoxel = getVoxel(x + dx[direction], y + dy[direction], z + dz[direction]);
                
                // Check if this face should be rendered
                if (currentVoxel != 0 && (neighborVoxel == 0 || !canMergeVoxels(currentVoxel, neighborVoxel)))
                {
                    mask[uPos + vPos * SIZE] = currentVoxel;
                }
            }
        }
        
        // Generate quads from mask using greedy algorithm
        for (int vPos = 0; vPos < vMax; ++vPos)
        {
            for (int uPos = 0; uPos < uMax; )
            {
                uint8_t blockType = mask[uPos + vPos * SIZE];
                if (blockType == 0)
                {
                    ++uPos;
                    continue;
                }
                
                // Find width of this quad
                int width = 1;
                while (uPos + width < uMax && 
                       mask[uPos + width + vPos * SIZE] == blockType)
                {
                    ++width;
                }
                
                // Find height of this quad
                int height = 1;
                bool canExtendHeight = true;
                while (vPos + height < vMax && canExtendHeight)
                {
                    for (int i = 0; i < width; ++i)
                    {
                        if (mask[uPos + i + (vPos + height) * SIZE] != blockType)
                        {
                            canExtendHeight = false;
                            break;
                        }
                    }
                    if (canExtendHeight)
                        ++height;
                }
                
                // Clear the mask area we just processed
                for (int h = 0; h < height; ++h)
                {
                    for (int w = 0; w < width; ++w)
                    {
                        mask[uPos + w + (vPos + h) * SIZE] = 0;
                    }
                }
                
                // Add the merged quad
                int x, y, z;
                switch (direction)
                {
                    case 0: case 1: x = wPos; y = uPos; z = vPos; break;
                    case 2: case 3: x = uPos; y = wPos; z = vPos; break;
                    case 4: case 5: x = uPos; y = vPos; z = wPos; break;
                }
                
                addGreedyQuad(vertices, indices, x, y, z, width, height, direction, blockType);
                
                // Also add to collision mesh (simplified - could be optimized further)
                for (int h = 0; h < height; ++h)
                {
                    for (int w = 0; w < width; ++w)
                    {
                        int cx, cy, cz;
                        switch (direction)
                        {
                            case 0: case 1: cx = x; cy = y + w; cz = z + h; break;
                            case 2: case 3: cx = x + w; cy = y; cz = z + h; break;
                            case 4: case 5: cx = x + w; cy = y + h; cz = z; break;
                        }
                        addCollisionQuad(static_cast<float>(cx), static_cast<float>(cy), static_cast<float>(cz), direction);
                    }
                }
                
                uPos += width;
            }
        }
    }
}

bool VoxelChunk::canMergeVoxels(uint8_t voxel1, uint8_t voxel2) const
{
    // For now, only merge identical block types
    // Could be extended to merge similar materials later
    return voxel1 == voxel2;
}

void VoxelChunk::addGreedyQuad(std::vector<Vertex>& vertices, std::vector<uint32_t>& indices, 
                               int x, int y, int z, int width, int height, int direction, uint8_t blockType)
{
    // Calculate vertices for the merged quad
    Vec3 quadVertices[4];
    Vec3 normal;
    
    // Calculate normal and base vertices based on direction
    switch (direction)
    {
        case 0: // +X face (right)
            normal = Vec3(1, 0, 0);
            quadVertices[0] = Vec3(x + 1, y, z);
            quadVertices[1] = Vec3(x + 1, y + width, z);
            quadVertices[2] = Vec3(x + 1, y + width, z + height);
            quadVertices[3] = Vec3(x + 1, y, z + height);
            break;
        case 1: // -X face (left)
            normal = Vec3(-1, 0, 0);
            quadVertices[0] = Vec3(x, y, z);
            quadVertices[1] = Vec3(x, y, z + height);
            quadVertices[2] = Vec3(x, y + width, z + height);
            quadVertices[3] = Vec3(x, y + width, z);
            break;
        case 2: // +Y face (up)
            normal = Vec3(0, 1, 0);
            quadVertices[0] = Vec3(x, y + 1, z);
            quadVertices[1] = Vec3(x, y + 1, z + height);
            quadVertices[2] = Vec3(x + width, y + 1, z + height);
            quadVertices[3] = Vec3(x + width, y + 1, z);
            break;
        case 3: // -Y face (down)
            normal = Vec3(0, -1, 0);
            quadVertices[0] = Vec3(x, y, z);
            quadVertices[1] = Vec3(x + width, y, z);
            quadVertices[2] = Vec3(x + width, y, z + height);
            quadVertices[3] = Vec3(x, y, z + height);
            break;
        case 4: // +Z face (forward)
            normal = Vec3(0, 0, 1);
            quadVertices[0] = Vec3(x, y, z + 1);
            quadVertices[1] = Vec3(x + width, y, z + 1);
            quadVertices[2] = Vec3(x + width, y + height, z + 1);
            quadVertices[3] = Vec3(x, y + height, z + 1);
            break;
        case 5: // -Z face (back)
            normal = Vec3(0, 0, -1);
            quadVertices[0] = Vec3(x, y, z);
            quadVertices[1] = Vec3(x, y + height, z);
            quadVertices[2] = Vec3(x + width, y + height, z);
            quadVertices[3] = Vec3(x + width, y, z);
            break;
        default:
            return;
    }
    
    // Calculate texture coordinates (tiled across the merged quad for proper scaling)
    // Need to match the texture coordinates with the actual quad vertex ordering
    float texCoords[4][2];
    
    // Use standard texture coordinates without debug borders
    float uMin = 0.0f;
    float uMax = static_cast<float>(width);
    float vMin = 0.0f;
    float vMax = static_cast<float>(height);
    
    switch (direction)
    {
        case 0: // +X face (right): vertices go Y=0→width, Z=0→height
            texCoords[0][0] = uMin; texCoords[0][1] = vMin;
            texCoords[1][0] = uMax; texCoords[1][1] = vMin;
            texCoords[2][0] = uMax; texCoords[2][1] = vMax;
            texCoords[3][0] = uMin; texCoords[3][1] = vMax;
            break;
        case 1: // -X face (left): vertices go Z=0→height, Y=0→width  
            texCoords[0][0] = uMin; texCoords[0][1] = vMin;
            texCoords[1][0] = uMin; texCoords[1][1] = vMax;
            texCoords[2][0] = uMax; texCoords[2][1] = vMax;
            texCoords[3][0] = uMax; texCoords[3][1] = vMin;
            break;
        case 2: // +Y face (up): vertices go Z=0→height, X=0→width
            texCoords[0][0] = uMin; texCoords[0][1] = vMin;
            texCoords[1][0] = uMin; texCoords[1][1] = vMax;
            texCoords[2][0] = uMax; texCoords[2][1] = vMax;
            texCoords[3][0] = uMax; texCoords[3][1] = vMin;
            break;
        case 3: // -Y face (down): vertices go X=0→width, Z=0→height
            texCoords[0][0] = uMin; texCoords[0][1] = vMin;
            texCoords[1][0] = uMax; texCoords[1][1] = vMin;
            texCoords[2][0] = uMax; texCoords[2][1] = vMax;
            texCoords[3][0] = uMin; texCoords[3][1] = vMax;
            break;
        case 4: // +Z face (forward): vertices go X=0→width, Y=0→height
            texCoords[0][0] = uMin; texCoords[0][1] = vMin;
            texCoords[1][0] = uMax; texCoords[1][1] = vMin;
            texCoords[2][0] = uMax; texCoords[2][1] = vMax;
            texCoords[3][0] = uMin; texCoords[3][1] = vMax;
            break;
        case 5: // -Z face (back): vertices go Y=0→height, X=0→width
            texCoords[0][0] = uMin; texCoords[0][1] = vMin;
            texCoords[1][0] = uMin; texCoords[1][1] = vMax;
            texCoords[2][0] = uMax; texCoords[2][1] = vMax;
            texCoords[3][0] = uMax; texCoords[3][1] = vMin;
            break;
        default:
            // Fallback to default texture coordinates
            texCoords[0][0] = uMin; texCoords[0][1] = vMin;
            texCoords[1][0] = uMax; texCoords[1][1] = vMin;
            texCoords[2][0] = uMax; texCoords[2][1] = vMax;
            texCoords[3][0] = uMin; texCoords[3][1] = vMax;
            break;
    }
    
    // Calculate light map coordinates for each vertex
    // With per-face light maps, each face uses its own texture space (0-1 range)
    float lightMapCoords[4][2];
    
    // Map each vertex to its face's light map space (0-1 range)
    switch (direction)
    {
        case 0: // +X face: map Y,Z to light map UV (full 0-1 range)
            lightMapCoords[0][0] = static_cast<float>(y) / static_cast<float>(SIZE);           // Y=0
            lightMapCoords[0][1] = static_cast<float>(z) / static_cast<float>(SIZE);           // Z=0
            lightMapCoords[1][0] = static_cast<float>(y + width) / static_cast<float>(SIZE);   // Y=width
            lightMapCoords[1][1] = static_cast<float>(z) / static_cast<float>(SIZE);           // Z=0
            lightMapCoords[2][0] = static_cast<float>(y + width) / static_cast<float>(SIZE);   // Y=width
            lightMapCoords[2][1] = static_cast<float>(z + height) / static_cast<float>(SIZE);  // Z=height
            lightMapCoords[3][0] = static_cast<float>(y) / static_cast<float>(SIZE);           // Y=0
            lightMapCoords[3][1] = static_cast<float>(z + height) / static_cast<float>(SIZE);  // Z=height
            break;
        case 1: // -X face: map Z,Y to light map UV
            lightMapCoords[0][0] = static_cast<float>(z) / static_cast<float>(SIZE);           // Z=0
            lightMapCoords[0][1] = static_cast<float>(y) / static_cast<float>(SIZE);           // Y=0
            lightMapCoords[1][0] = static_cast<float>(z + height) / static_cast<float>(SIZE);  // Z=height
            lightMapCoords[1][1] = static_cast<float>(y) / static_cast<float>(SIZE);           // Y=0
            lightMapCoords[2][0] = static_cast<float>(z + height) / static_cast<float>(SIZE);  // Z=height
            lightMapCoords[2][1] = static_cast<float>(y + width) / static_cast<float>(SIZE);   // Y=width
            lightMapCoords[3][0] = static_cast<float>(z) / static_cast<float>(SIZE);           // Z=0
            lightMapCoords[3][1] = static_cast<float>(y + width) / static_cast<float>(SIZE);   // Y=width
            break;
        case 2: // +Y face: map Z,X to light map UV
            lightMapCoords[0][0] = static_cast<float>(z) / static_cast<float>(SIZE);           // Z=0
            lightMapCoords[0][1] = static_cast<float>(x) / static_cast<float>(SIZE);           // X=0
            lightMapCoords[1][0] = static_cast<float>(z + height) / static_cast<float>(SIZE);  // Z=height
            lightMapCoords[1][1] = static_cast<float>(x) / static_cast<float>(SIZE);           // X=0
            lightMapCoords[2][0] = static_cast<float>(z + height) / static_cast<float>(SIZE);  // Z=height
            lightMapCoords[2][1] = static_cast<float>(x + width) / static_cast<float>(SIZE);   // X=width
            lightMapCoords[3][0] = static_cast<float>(z) / static_cast<float>(SIZE);           // Z=0
            lightMapCoords[3][1] = static_cast<float>(x + width) / static_cast<float>(SIZE);   // X=width
            break;
        case 3: // -Y face: map X,Z to light map UV
            lightMapCoords[0][0] = static_cast<float>(x) / static_cast<float>(SIZE);           // X=0
            lightMapCoords[0][1] = static_cast<float>(z) / static_cast<float>(SIZE);           // Z=0
            lightMapCoords[1][0] = static_cast<float>(x + width) / static_cast<float>(SIZE);   // X=width
            lightMapCoords[1][1] = static_cast<float>(z) / static_cast<float>(SIZE);           // Z=0
            lightMapCoords[2][0] = static_cast<float>(x + width) / static_cast<float>(SIZE);   // X=width
            lightMapCoords[2][1] = static_cast<float>(z + height) / static_cast<float>(SIZE);  // Z=height
            lightMapCoords[3][0] = static_cast<float>(x) / static_cast<float>(SIZE);           // X=0
            lightMapCoords[3][1] = static_cast<float>(z + height) / static_cast<float>(SIZE);  // Z=height
            break;
        case 4: // +Z face: map X,Y to light map UV
            lightMapCoords[0][0] = static_cast<float>(x) / static_cast<float>(SIZE);           // X=0
            lightMapCoords[0][1] = static_cast<float>(y) / static_cast<float>(SIZE);           // Y=0
            lightMapCoords[1][0] = static_cast<float>(x + width) / static_cast<float>(SIZE);   // X=width
            lightMapCoords[1][1] = static_cast<float>(y) / static_cast<float>(SIZE);           // Y=0
            lightMapCoords[2][0] = static_cast<float>(x + width) / static_cast<float>(SIZE);   // X=width
            lightMapCoords[2][1] = static_cast<float>(y + height) / static_cast<float>(SIZE);  // Y=height
            lightMapCoords[3][0] = static_cast<float>(x) / static_cast<float>(SIZE);           // X=0
            lightMapCoords[3][1] = static_cast<float>(y + height) / static_cast<float>(SIZE);  // Y=height
            break;
        case 5: // -Z face: map Y,X to light map UV
            lightMapCoords[0][0] = static_cast<float>(y) / static_cast<float>(SIZE);           // Y=0
            lightMapCoords[0][1] = static_cast<float>(x) / static_cast<float>(SIZE);           // X=0
            lightMapCoords[1][0] = static_cast<float>(y + height) / static_cast<float>(SIZE);  // Y=height
            lightMapCoords[1][1] = static_cast<float>(x) / static_cast<float>(SIZE);           // X=0
            lightMapCoords[2][0] = static_cast<float>(y + height) / static_cast<float>(SIZE);  // Y=height
            lightMapCoords[2][1] = static_cast<float>(x + width) / static_cast<float>(SIZE);   // X=width
            lightMapCoords[3][0] = static_cast<float>(y) / static_cast<float>(SIZE);           // Y=0
            lightMapCoords[3][1] = static_cast<float>(x + width) / static_cast<float>(SIZE);   // X=width
            break;
        default:
            // Fallback: use center coordinates for all vertices
            float centerU = (x + width * 0.5f) / static_cast<float>(SIZE);
            float centerV = (y + height * 0.5f) / static_cast<float>(SIZE);
            for (int i = 0; i < 4; ++i) {
                lightMapCoords[i][0] = centerU;
                lightMapCoords[i][1] = centerV;
            }
            break;
    }
    
    // Add vertices
    uint32_t baseIndex = static_cast<uint32_t>(vertices.size());
    for (int i = 0; i < 4; ++i)
    {
        Vertex vertex;
        vertex.x = quadVertices[i].x;
        vertex.y = quadVertices[i].y;
        vertex.z = quadVertices[i].z;
        vertex.nx = normal.x;
        vertex.ny = normal.y;
        vertex.nz = normal.z;
        vertex.u = texCoords[i][0];
        vertex.v = texCoords[i][1];
        vertex.lu = lightMapCoords[i][0];
        vertex.lv = lightMapCoords[i][1];
        vertex.ao = computeAmbientOcclusion(x, y, z, direction);
        vertex.faceIndex = static_cast<float>(direction);  // Store face index for shader
        
        vertices.push_back(vertex);
    }
    
    // Add indices (two triangles)
    indices.push_back(baseIndex);
    indices.push_back(baseIndex + 1);
    indices.push_back(baseIndex + 2);
    
    indices.push_back(baseIndex);
    indices.push_back(baseIndex + 2);
    indices.push_back(baseIndex + 3);
}

