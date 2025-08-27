// VoxelChunk.cpp - 32x32x32 dynamic physics-enabled voxel chunks
#include "VoxelChunk.h"
#include <GL/gl.h>
#include <GLFW/glfw3.h>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <iostream>
#include <memory>
#include <thread>
#include <vector>

#include "Threading/JobSystem.h"

// External job system reference
extern JobSystem g_jobSystem;

VoxelChunk::VoxelChunk()
{
    // Initialize voxel data to empty (0 = air)
    std::fill(voxels.begin(), voxels.end(), 0);
    meshDirty = true;
    collisionMesh.needsUpdate = true;
}

VoxelChunk::~VoxelChunk()
{
    // No OpenGL resources to clean up in immediate mode
}

uint8_t VoxelChunk::getVoxel(int x, int y, int z) const
{
    if (x < 0 || x >= SIZE || y < 0 || y >= SIZE || z < 0 || z >= SIZE)
        return 0; // Out of bounds = air
    return voxels[x + y * SIZE + z * SIZE * SIZE];
}

void VoxelChunk::setVoxel(int x, int y, int z, uint8_t type)
{
    if (x < 0 || x >= SIZE || y < 0 || y >= SIZE || z < 0 || z >= SIZE)
        return;
    voxels[x + y * SIZE + z * SIZE * SIZE] = type;
    meshDirty = true;
    collisionMesh.needsUpdate = true;
}

void VoxelChunk::setRawVoxelData(const uint8_t* data, uint32_t size)
{
    if (size != VOLUME)
        return;
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

    for (int i = 0; i < 4; ++i) {
        collisionMeshVertices.push_back(Vec3(x, y, z) + quadVertices[face][i]);
    }
}

bool VoxelChunk::shouldRenderFace(int x, int y, int z, int faceDir) const
{
    // Only render faces that are exposed to air
    int adjX = x, adjY = y, adjZ = z;
    switch (faceDir) {
        case 0: adjZ++; break;  // Front face (+Z)
        case 1: adjZ--; break;  // Back face (-Z)
        case 2: adjY++; break;  // Top face (+Y)
        case 3: adjY--; break;  // Bottom face (-Y)
        case 4: adjX++; break;  // Right face (+X)
        case 5: adjX--; break;  // Left face (-X)
    }
    if (adjX < 0 || adjX >= SIZE || adjY < 0 || adjY >= SIZE || adjZ < 0 || adjZ >= SIZE)
        return true;  // Always render boundary faces
    return getVoxel(adjX, adjY, adjZ) == 0;
}

bool VoxelChunk::isVoxelSolid(int x, int y, int z) const
{
    return getVoxel(x, y, z) != 0;
}

void VoxelChunk::addQuad(std::vector<Vertex>& vertices, std::vector<uint32_t>& indices,
                        float x, float y, float z, int face, uint8_t blockType)
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
    static const float texCoords[4][2] = {
        {0.0f, 0.0f}, {1.0f, 0.0f}, {1.0f, 1.0f}, {0.0f, 1.0f}
    };

    // Add 4 vertices for this quad
    for (int i = 0; i < 4; ++i) {
        Vertex v;
        Vec3 pos = Vec3(x, y, z) + quadVertices[face][i];
        v.x = pos.x; v.y = pos.y; v.z = pos.z;
        v.nx = normals[face].x; v.ny = normals[face].y; v.nz = normals[face].z;
        v.u = texCoords[i][0]; v.v = texCoords[i][1];
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

    // Single-pass generation: iterate through voxels once, generate both render and collision meshes
    for (int x = 0; x < SIZE; ++x) {
        for (int y = 0; y < SIZE; ++y) {
            for (int z = 0; z < SIZE; ++z) {
                uint8_t blockType = getVoxel(x, y, z);
                if (blockType == 0) continue; // Skip air blocks

                // Check each face and add quads for exposed faces
                for (int face = 0; face < 6; ++face) {
                    if (shouldRenderFace(x, y, z, face)) {
                        // Add to render mesh
                        addQuad(mesh.vertices, mesh.indices, static_cast<float>(x),
                               static_cast<float>(y), static_cast<float>(z), face, blockType);

                        // Add to collision mesh
                        addCollisionQuad(static_cast<float>(x), static_cast<float>(y),
                                       static_cast<float>(z), face);
                    }
                }
            }
        }
    }

    mesh.needsUpdate = true;
    collisionMesh.needsUpdate = true;
    meshDirty = false;
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
    for (size_t i = 0; i < collisionMeshVertices.size(); i += 4) {
        if (i + 3 >= collisionMeshVertices.size()) break;

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

bool VoxelChunk::checkRayCollision(const Vec3& rayOrigin, const Vec3& rayDirection, float maxDistance,
                                  Vec3& hitPoint, Vec3& hitNormal) const
{
    // Simple ray-triangle intersection with collision faces
    float closestDistance = maxDistance;
    bool hit = false;

    for (const auto& face : collisionMesh.faces) {
        // Ray-plane intersection
        float denom = rayDirection.dot(face.normal);
        if (abs(denom) < 1e-6f) continue; // Ray parallel to plane

        Vec3 planeToRay = face.position - rayOrigin;
        float t = planeToRay.dot(face.normal) / denom;

        if (t < 0 || t > closestDistance) continue;

        // Check if intersection point is within face bounds (simple AABB check)
        Vec3 intersection = rayOrigin + rayDirection * t;
        Vec3 localPoint = intersection - face.position;

        // Determine which axes to check based on face normal
        bool withinBounds = true;
        if (abs(face.normal.x) > 0.5f) { // X-facing face
            withinBounds = abs(localPoint.y) <= 0.5f && abs(localPoint.z) <= 0.5f;
        } else if (abs(face.normal.y) > 0.5f) { // Y-facing face
            withinBounds = abs(localPoint.x) <= 0.5f && abs(localPoint.z) <= 0.5f;
        } else { // Z-facing face
            withinBounds = abs(localPoint.x) <= 0.5f && abs(localPoint.y) <= 0.5f;
        }

        if (withinBounds) {
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
    if (meshDirty) {
        generateMesh();
    }

    if (mesh.vertices.empty()) return;

    // Immediate mode rendering
    glPushMatrix();
    glTranslatef(worldOffset.x, worldOffset.y, worldOffset.z);

    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glFrontFace(GL_CCW);
    glEnable(GL_DEPTH_TEST);

    // Simple lighting setup
    glEnable(GL_LIGHTING);
    glEnable(GL_LIGHT0);
    GLfloat lightPos[] = {1.0f, 1.0f, 1.0f, 0.0f};
    glLightfv(GL_LIGHT0, GL_POSITION, lightPos);

    glColor3f(0.7f, 0.7f, 0.9f); // Light blue-gray color

    // Render using immediate mode
    glBegin(GL_TRIANGLES);
    for (size_t i = 0; i < mesh.indices.size(); i += 3) {
        for (int j = 0; j < 3; ++j) {
            uint32_t index = mesh.indices[i + j];
            const Vertex& v = mesh.vertices[index];
            glNormal3f(v.nx, v.ny, v.nz);
            glVertex3f(v.x, v.y, v.z);
        }
    }
    glEnd();

    glDisable(GL_LIGHTING);
    glPopMatrix();
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
    float dist = std::sqrt(distance.x * distance.x + distance.y * distance.y + distance.z * distance.z);

    if (dist < 64.0f) return 0;      // High detail
    else if (dist < 128.0f) return 1; // Medium detail
    else return 2;                   // Low detail
}

bool VoxelChunk::shouldRender(const Vec3& cameraPos, float maxDistance) const
{
    Vec3 chunkCenter(SIZE * 0.5f, SIZE * 0.5f, SIZE * 0.5f);
    Vec3 distance = cameraPos - chunkCenter;
    float dist = std::sqrt(distance.x * distance.x + distance.y * distance.y + distance.z * distance.z);
    return dist <= maxDistance;
}

void VoxelChunk::generateFloatingIsland(int seed)
{
    // Generate a floating island using simple noise

    // Simple spherical island parameters
    float centerX = SIZE * 0.5f;
    float centerY = SIZE * 0.3f;
    float centerZ = SIZE * 0.5f;
    float radius = SIZE * 0.15f;  // Reduced from 0.25f to make islands smaller

    // If job system is available, use multithreading
    if (g_jobSystem.isInitialized()) {
        const int numSlices = 8;  // Split Y-axis into 8 slices for threading
        const int sliceHeight = SIZE / numSlices;
        std::vector<uint32_t> jobIDs;

        // Submit jobs for each Y-slice
        for (int slice = 0; slice < numSlices; slice++) {
            int startY = slice * sliceHeight;
            int endY = (slice == numSlices - 1) ? SIZE : (slice + 1) * sliceHeight;

            JobPayload payload;
            // Cast chunk pointer as ID
            payload.chunkID = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(this));

            auto work = [this, startY, endY, centerX, centerY, centerZ, radius, payload]() -> JobResult {
                // Generate voxels for this Y-slice
                for (int x = 0; x < SIZE; x++) {
                    for (int y = startY; y < endY; y++) {
                        for (int z = 0; z < SIZE; z++) {
                            float dx = x - centerX;
                            float dy = y - centerY;
                            float dz = z - centerZ;
                            float distance = static_cast<float>(std::sqrt(dx * dx + dy * dy + dz * dz));

                            if (distance < radius) {
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
                    float distance = static_cast<float>(std::sqrt(dx * dx + dy * dy + dz * dz));

                    if (distance < radius) {
                        setVoxel(x, y, z, 1);  // Simple solid block
                    }
                }
            }
        }
    }

    meshDirty = true;
    collisionMesh.needsUpdate = true;
}
