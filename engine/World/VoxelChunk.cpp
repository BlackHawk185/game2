// VoxelChunk.cpp - Simplified voxel storage for initial demo
#include "VoxelChunk.h"
#include "Math/Vec3.h"
#include "../Threading/JobSystem.h"
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <GL/gl.h>
#include <cmath>
#include <iostream>
#include <cstring>  // For memcpy

#include <thread>
#include <chrono>

VoxelChunk::VoxelChunk() {
    voxels.fill(0); // Initialize all blocks to empty
    mesh.VAO = 0; // Will use immediate mode for simplicity initially
    mesh.VBO = 0;
    mesh.EBO = 0;
}

VoxelChunk::~VoxelChunk() {
    // Simplified cleanup
}

uint8_t VoxelChunk::getVoxel(int x, int y, int z) const {
    if (x < 0 || x >= SIZE || y < 0 || y >= SIZE || z < 0 || z >= SIZE) 
        return 0; // Out of bounds = empty
    return voxels[x + y * SIZE + z * SIZE * SIZE];
}

void VoxelChunk::setVoxel(int x, int y, int z, uint8_t type) {
    if (x < 0 || x >= SIZE || y < 0 || y >= SIZE || z < 0 || z >= SIZE) {
        return; // Out of bounds
    }
    
    int index = x + y * SIZE + z * SIZE * SIZE;
    uint8_t oldValue = voxels[index];
    if (oldValue != type) {
        voxels[index] = type;
        meshDirty = true; // Mark for re-meshing
    }
}

void VoxelChunk::setRawVoxelData(const uint8_t* data, uint32_t size) {
    if (!data || size != VOLUME) {
        std::cerr << "Invalid voxel data size. Expected: " << VOLUME << ", Got: " << size << std::endl;
        return;
    }
    
    // Copy raw voxel data
    std::memcpy(voxels.data(), data, VOLUME);
    meshDirty = true; // Mark for re-meshing since we've changed the data
}

bool VoxelChunk::isVoxelSolid(int x, int y, int z) const {
    return getVoxel(x, y, z) != 0;
}

void VoxelChunk::generateFloatingIsland(int seed) {
    // Generate a floating island using simple noise
    
    // Simple spherical island parameters - 1/4 size
    float centerX = SIZE * 0.5f;
    float centerY = SIZE * 0.3f; 
    float centerZ = SIZE * 0.5f;
    float radius = SIZE * 0.1f; // Reduced from 0.4f to 0.1f for 1/4 size
    
    // Get global job system reference
    extern JobSystem g_jobSystem;
    
    // If job system is available, use multithreading
    if (g_jobSystem.isInitialized()) {
        const int numSlices = 8; // Split Y-axis into 8 slices for threading
        const int sliceHeight = SIZE / numSlices;
        std::vector<uint32_t> jobIDs;
        
        // Submit jobs for each Y-slice
        for (int slice = 0; slice < numSlices; slice++) {
            int startY = slice * sliceHeight;
            int endY = (slice == numSlices - 1) ? SIZE : (slice + 1) * sliceHeight;
            
            JobPayload payload;
            payload.chunkID = reinterpret_cast<uintptr_t>(this); // Use chunk pointer as ID
            
            auto work = [this, startY, endY, centerX, centerY, centerZ, radius]() -> JobResult {
                // Generate voxels for this Y-slice
                for (int x = 0; x < SIZE; x++) {
                    for (int y = startY; y < endY; y++) {
                        for (int z = 0; z < SIZE; z++) {
                            float dx = x - centerX;
                            float dy = y - centerY;
                            float dz = z - centerZ;
                            float distance = sqrt(dx*dx + dy*dy + dz*dz);
                            
                            if (distance < radius) {
                                setVoxel(x, y, z, 1); // Simple solid block
                            }
                        }
                    }
                }
                
                JobResult result;
                result.type = JobType::WORLD_GENERATION;
                result.success = true;
                return result;
            };
            
            uint32_t jobID = g_jobSystem.submitJob(JobType::WORLD_GENERATION, payload, work);
            jobIDs.push_back(jobID);
        }
        
        // Wait for all jobs to complete by draining results
        std::vector<JobResult> results;
        size_t completedJobs = 0;
        while (completedJobs < jobIDs.size()) {
            g_jobSystem.drainCompletedJobs(results, 10);
            for (const auto& result : results) {
                if (result.type == JobType::WORLD_GENERATION) {
                    completedJobs++;
                }
            }
            results.clear();
            
            // Small sleep to avoid busy waiting
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        
    } else {
        // Fallback to single-threaded generation
        for (int x = 0; x < SIZE; x++) {
            for (int y = 0; y < SIZE; y++) {
                for (int z = 0; z < SIZE; z++) {
                    float dx = x - centerX;
                    float dy = y - centerY;
                    float dz = z - centerZ;
                    float distance = sqrt(dx*dx + dy*dy + dz*dz);
                    
                    if (distance < radius) {
                        setVoxel(x, y, z, 1); // Simple solid block
                    }
                }
            }
        }
        
    }
}

void VoxelChunk::generateMesh() {
    // Simplified - just mark as updated
    meshDirty = false;
}

void VoxelChunk::render() {
    render(Vec3(0, 0, 0)); // Render at origin by default
}

void VoxelChunk::render(const Vec3& worldOffset) {
    // Immediate mode rendering for simplicity
    glPushMatrix();
    
    // **APPLY ISLAND WORLD POSITION TRANSFORMATION**
    glTranslatef(worldOffset.x, worldOffset.y, worldOffset.z);
    
    // **ENABLE BASIC GPU CULLING**
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glFrontFace(GL_CCW);
    glEnable(GL_DEPTH_TEST);
    
    // Set up lighting
    glEnable(GL_LIGHTING);
    glEnable(GL_LIGHT0);
    
    float lightPos[] = {1.0f, 1.0f, 1.0f, 0.0f};
    glLightfv(GL_LIGHT0, GL_POSITION, lightPos);
    
    glColor3f(0.6f, 0.8f, 0.4f); // Green color for blocks
    
    int facesRendered = 0, facesSkipped = 0;
    
    // Draw cubes for each voxel - NOW WITH FACE CULLING!
    for (int x = 0; x < SIZE; x++) {
        for (int y = 0; y < SIZE; y++) {
            for (int z = 0; z < SIZE; z++) {
                if (getVoxel(x, y, z) != 0) {
                    glPushMatrix();
                    glTranslatef(x, y, z);
                    
                    // **FACE CULLING - ONLY DRAW EXPOSED FACES**
                    
                    // Front face (+Z)
                    if (shouldRenderFace(x, y, z, 0)) {
                        glBegin(GL_QUADS);
                        glNormal3f(0, 0, 1);
                        glVertex3f(0, 0, 1); glVertex3f(1, 0, 1);
                        glVertex3f(1, 1, 1); glVertex3f(0, 1, 1);
                        glEnd();
                        facesRendered++;
                    } else facesSkipped++;
                    
                    // Back face (-Z)
                    if (shouldRenderFace(x, y, z, 1)) {
                        glBegin(GL_QUADS);
                        glNormal3f(0, 0, -1);
                        glVertex3f(1, 0, 0); glVertex3f(0, 0, 0);
                        glVertex3f(0, 1, 0); glVertex3f(1, 1, 0);
                        glEnd();
                        facesRendered++;
                    } else facesSkipped++;
                    
                    // Top face (+Y)
                    if (shouldRenderFace(x, y, z, 2)) {
                        glBegin(GL_QUADS);
                        glNormal3f(0, 1, 0);
                        glVertex3f(0, 1, 0); glVertex3f(0, 1, 1);
                        glVertex3f(1, 1, 1); glVertex3f(1, 1, 0);
                        glEnd();
                        facesRendered++;
                    } else facesSkipped++;
                    
                    // Bottom face (-Y)
                    if (shouldRenderFace(x, y, z, 3)) {
                        glBegin(GL_QUADS);
                        glNormal3f(0, -1, 0);
                        glVertex3f(1, 0, 0); glVertex3f(1, 0, 1);
                        glVertex3f(0, 0, 1); glVertex3f(0, 0, 0);
                        glEnd();
                        facesRendered++;
                    } else facesSkipped++;
                    
                    // Right face (+X)
                    if (shouldRenderFace(x, y, z, 4)) {
                        glBegin(GL_QUADS);
                        glNormal3f(1, 0, 0);
                        glVertex3f(1, 0, 1); glVertex3f(1, 0, 0);
                        glVertex3f(1, 1, 0); glVertex3f(1, 1, 1);
                        glEnd();
                        facesRendered++;
                    } else facesSkipped++;
                    
                    // Left face (-X)
                    if (shouldRenderFace(x, y, z, 5)) {
                        glBegin(GL_QUADS);
                        glNormal3f(-1, 0, 0);
                        glVertex3f(0, 0, 0); glVertex3f(0, 0, 1);
                        glVertex3f(0, 1, 1); glVertex3f(0, 1, 0);
                        glEnd();
                        facesRendered++;
                    } else facesSkipped++;
                    
                    glPopMatrix();
                }
            }
        }
    }
    
    glPopMatrix();
}

void VoxelChunk::addQuad(std::vector<Vertex>& vertices, std::vector<uint32_t>& indices,
                        float x, float y, float z, int face, uint8_t blockType) {
    // Simplified - not used in immediate mode
}

void VoxelChunk::updatePhysicsMesh() {
    std::cout << "Physics mesh update (placeholder)" << std::endl;
}

bool VoxelChunk::shouldRenderFace(int x, int y, int z, int faceDir) const {
    // **THE CRITICAL OPTIMIZATION - Face culling logic**
    // Only render faces that are exposed to air
    
    // Calculate adjacent voxel position based on face direction
    int adjX = x, adjY = y, adjZ = z;
    
    switch (faceDir) {
        case 0: adjZ++; break; // Front face (+Z)
        case 1: adjZ--; break; // Back face (-Z)
        case 2: adjY++; break; // Top face (+Y)
        case 3: adjY--; break; // Bottom face (-Y)
        case 4: adjX++; break; // Right face (+X)
        case 5: adjX--; break; // Left face (-X)
    }
    
    // If adjacent position is outside chunk bounds, face is exposed
    if (adjX < 0 || adjX >= SIZE || 
        adjY < 0 || adjY >= SIZE || 
        adjZ < 0 || adjZ >= SIZE) {
        return true; // Always render boundary faces
    }
    
    // Only render face if adjacent voxel is empty (air)
    return getVoxel(adjX, adjY, adjZ) == 0;
}

int VoxelChunk::calculateLOD(const Vec3& cameraPos) const {
    // **DISTANCE-BASED LOD CALCULATION**
    // TODO: Get actual chunk world position - for now assume (0,0,0)
    Vec3 chunkCenter(16.0f, 16.0f, 16.0f); // Center of 32x32x32 chunk
    
    float distance = (cameraPos - chunkCenter).length();
    
    if (distance < 64.0f)  return 0; // Full detail
    if (distance < 128.0f) return 1; // Half detail  
    if (distance < 256.0f) return 2; // Quarter detail
    return 3; // Minimal detail
}

bool VoxelChunk::shouldRender(const Vec3& cameraPos, float maxDistance) const {
    // **DISTANCE CULLING**
    Vec3 chunkCenter(16.0f, 16.0f, 16.0f);
    float distance = (cameraPos - chunkCenter).length();
    return distance <= maxDistance;
}

void VoxelChunk::renderLOD(int lodLevel, const Vec3& cameraPos) {
    // **LOD RENDERING - Skip voxels based on distance**
    if (lodLevel == 0) {
        this->render(); // Full detail
        return;
    }
    
    // Immediate mode rendering with LOD
    glPushMatrix();
    
    glEnable(GL_CULL_FACE);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_LIGHTING);
    glEnable(GL_LIGHT0);
    
    float lightPos[] = {1.0f, 1.0f, 1.0f, 0.0f};
    glLightfv(GL_LIGHT0, GL_POSITION, lightPos);
    glColor3f(0.6f, 0.8f, 0.4f);
    
    int step = 1 << lodLevel; // LOD 1 = skip every 2nd, LOD 2 = skip every 4th, etc.
    
    for (int x = 0; x < SIZE; x += step) {
        for (int y = 0; y < SIZE; y += step) {
            for (int z = 0; z < SIZE; z += step) {
                if (this->getVoxel(x, y, z) != 0) {
                    glPushMatrix();
                    glTranslatef(x, y, z);
                    
                    // Render simplified cube (fewer faces for distant LOD)
                    glBegin(GL_QUADS);
                    
                    // Only render most visible faces for high LOD levels
                    if (lodLevel <= 2) {
                        // All faces
                        if (this->shouldRenderFace(x, y, z, 0)) { // Front
                            glNormal3f(0, 0, 1);
                            glVertex3f(0, 0, 1); glVertex3f(step, 0, 1);
                            glVertex3f(step, step, 1); glVertex3f(0, step, 1);
                        }
                        if (this->shouldRenderFace(x, y, z, 2)) { // Top
                            glNormal3f(0, 1, 0);
                            glVertex3f(0, step, 0); glVertex3f(0, step, step);
                            glVertex3f(step, step, step); glVertex3f(step, step, 0);
                        }
                    } else {
                        // Only top face for extreme LOD
                        glNormal3f(0, 1, 0);
                        glVertex3f(0, step, 0); glVertex3f(0, step, step);
                        glVertex3f(step, step, step); glVertex3f(step, step, 0);
                    }
                    
                    glEnd();
                    glPopMatrix();
                }
            }
        }
    }
    
    glPopMatrix();
}
