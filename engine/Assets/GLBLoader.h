#pragma once

#include <string>
#include <vector>
#include <glm/glm.hpp>
#include "../Math/Vec3.h"

struct GLBPrimitive {
    // CPU-side buffers
    std::vector<float> interleaved; // pos(3), normal(3), uv(2), lambert(1) = 9 floats per vertex
    std::vector<unsigned int> indices;
    
    // Store normals separately for lighting recalculation
    std::vector<Vec3> normals;
};

struct GLBModelCPU {
    std::vector<GLBPrimitive> primitives;
    bool valid = false;
    
    // Regenerate Lambert lighting for all vertices based on sun direction
    void recalculateLighting(const Vec3& sunDirection);
};

// Minimal GLB loader using tinygltf (positions, normals, uvs, indices only)
namespace GLBLoader {
    bool loadGLB(const std::string& path, GLBModelCPU& outModel);
}

