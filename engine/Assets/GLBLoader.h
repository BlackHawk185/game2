#pragma once

#include <string>
#include <vector>
#include <glm/glm.hpp>

struct GLBPrimitive {
    // CPU-side buffers
    std::vector<float> interleaved; // pos(3), normal(3), uv(2)
    std::vector<unsigned int> indices;
};

struct GLBModelCPU {
    std::vector<GLBPrimitive> primitives;
    bool valid = false;
};

// Minimal GLB loader using tinygltf (positions, normals, uvs, indices only)
namespace GLBLoader {
    bool loadGLB(const std::string& path, GLBModelCPU& outModel);
}

