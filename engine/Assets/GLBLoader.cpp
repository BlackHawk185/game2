#include "GLBLoader.h"
#include "../Math/Vec3.h"
#include <iostream>
#include <string>
#include <vector>
#include <algorithm>

#define TINYGLTF_NO_EXTERNAL_IMAGE
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#define TINYGLTF_IMPLEMENTATION
#include <tiny_gltf.h>

using namespace std;

namespace {
    template<typename T>
    bool ReadAccessor(const tinygltf::Model& model, const tinygltf::Accessor& accessor, vector<T>& out) {
        const tinygltf::BufferView& view = model.bufferViews[accessor.bufferView];
        const tinygltf::Buffer& buf = model.buffers[view.buffer];
        const unsigned char* dataPtr = buf.data.data() + view.byteOffset + accessor.byteOffset;
        size_t count = accessor.count;
        out.resize(count);
        // Only support tightly packed same-type for minimal implementation
        memcpy(out.data(), dataPtr, count * sizeof(T));
        return true;
    }
}

bool GLBLoader::loadGLB(const std::string& path, GLBModelCPU& outModel) {
    tinygltf::TinyGLTF loader;
    tinygltf::Model model;
    string err, warn;
    bool ok = loader.LoadBinaryFromFile(&model, &err, &warn, path);
    if (!warn.empty()) cerr << "GLB warn: " << warn << endl;
    if (!ok) {
        cerr << "Failed to load GLB: " << path << "\n" << err << endl;
        return false;
    }

    // Minimal: take first scene, flatten meshes/primitives
    if (model.scenes.empty()) {
        cerr << "GLB has no scenes: " << path << endl;
        return false;
    }

    const tinygltf::Scene& scene = model.scenes[model.defaultScene >= 0 ? model.defaultScene : 0];
    vector<int> nodeQueue(scene.nodes.begin(), scene.nodes.end());
    while (!nodeQueue.empty()) {
        int nodeIdx = nodeQueue.back(); nodeQueue.pop_back();
        const tinygltf::Node& node = model.nodes[nodeIdx];
        for (int child : node.children) nodeQueue.push_back(child);
        if (node.mesh < 0) continue;
        const tinygltf::Mesh& mesh = model.meshes[node.mesh];
        for (const auto& prim : mesh.primitives) {
            if (prim.mode != TINYGLTF_MODE_TRIANGLES) continue;
            auto itPos = prim.attributes.find("POSITION");
            auto itNor = prim.attributes.find("NORMAL");
            auto itUV  = prim.attributes.find("TEXCOORD_0");
            if (itPos == prim.attributes.end()) continue;
            const tinygltf::Accessor& accPos = model.accessors[itPos->second];
            const tinygltf::Accessor* accNor = itNor != prim.attributes.end() ? &model.accessors[itNor->second] : nullptr;
            const tinygltf::Accessor* accUV  = itUV  != prim.attributes.end() ? &model.accessors[itUV->second]  : nullptr;
            const tinygltf::Accessor& accIdx = model.accessors[prim.indices];
            const tinygltf::BufferView& idxView = model.bufferViews[accIdx.bufferView];
            const tinygltf::Buffer& idxBuf = model.buffers[idxView.buffer];

            // Positions
            vector<float> pos;
            {
                const tinygltf::BufferView& view = model.bufferViews[accPos.bufferView];
                const tinygltf::Buffer& buf = model.buffers[view.buffer];
                const unsigned char* data = buf.data.data() + view.byteOffset + accPos.byteOffset;
                size_t stride = accPos.ByteStride(view);
                size_t count = accPos.count;
                pos.resize(count * 3);
                for (size_t i=0;i<count;i++) memcpy(&pos[i*3], data + i*stride, sizeof(float)*3);
            }
            // Normals
            vector<float> nor;
            if (accNor) {
                const tinygltf::BufferView& view = model.bufferViews[accNor->bufferView];
                const tinygltf::Buffer& buf = model.buffers[view.buffer];
                const unsigned char* data = buf.data.data() + view.byteOffset + accNor->byteOffset;
                size_t stride = accNor->ByteStride(view);
                size_t count = accNor->count;
                nor.resize(count * 3);
                for (size_t i=0;i<count;i++) memcpy(&nor[i*3], data + i*stride, sizeof(float)*3);
            } else {
                nor.resize(pos.size());
                // leave zeros
            }
            // UVs
            vector<float> uv;
            if (accUV) {
                const tinygltf::BufferView& view = model.bufferViews[accUV->bufferView];
                const tinygltf::Buffer& buf = model.buffers[view.buffer];
                const unsigned char* data = buf.data.data() + view.byteOffset + accUV->byteOffset;
                size_t stride = accUV->ByteStride(view);
                size_t count = accUV->count;
                uv.resize(count * 2);
                for (size_t i=0;i<count;i++) memcpy(&uv[i*2], data + i*stride, sizeof(float)*2);
            } else {
                uv.resize((pos.size()/3) * 2, 0.0f);
            }

            // Interleave: pos(3), normal(3), uv(2), lambert(1) = 9 floats per vertex
            GLBPrimitive cpuPrim;
            size_t vcount = pos.size()/3;
            cpuPrim.interleaved.resize(vcount * 9);  // Changed from 8 to 9
            cpuPrim.normals.resize(vcount);
            
            for (size_t i=0;i<vcount;i++) {
                cpuPrim.interleaved[i*9+0] = pos[i*3+0];
                cpuPrim.interleaved[i*9+1] = pos[i*3+1];
                cpuPrim.interleaved[i*9+2] = pos[i*3+2];
                cpuPrim.interleaved[i*9+3] = nor[i*3+0];
                cpuPrim.interleaved[i*9+4] = nor[i*3+1];
                cpuPrim.interleaved[i*9+5] = nor[i*3+2];
                cpuPrim.interleaved[i*9+6] = uv[i*2+0];
                cpuPrim.interleaved[i*9+7] = uv[i*2+1];
                cpuPrim.interleaved[i*9+8] = 1.0f;  // Default Lambert value (will be recalculated)
                
                // Store normal separately for lighting recalculation
                cpuPrim.normals[i] = Vec3(nor[i*3+0], nor[i*3+1], nor[i*3+2]);
            }

            // Indices
            cpuPrim.indices.resize(accIdx.count);
            const unsigned char* idata = idxBuf.data.data() + idxView.byteOffset + accIdx.byteOffset;
            if (accIdx.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT) {
                const uint16_t* p = reinterpret_cast<const uint16_t*>(idata);
                for (size_t i=0;i<accIdx.count;i++) cpuPrim.indices[i] = p[i];
            } else if (accIdx.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT) {
                const uint32_t* p = reinterpret_cast<const uint32_t*>(idata);
                for (size_t i=0;i<accIdx.count;i++) cpuPrim.indices[i] = p[i];
            } else if (accIdx.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE) {
                const uint8_t* p = reinterpret_cast<const uint8_t*>(idata);
                for (size_t i=0;i<accIdx.count;i++) cpuPrim.indices[i] = p[i];
            } else {
                cerr << "Unsupported index type in GLB: " << accIdx.componentType << endl;
                continue;
            }

            outModel.primitives.emplace_back(std::move(cpuPrim));
        }
    }

    outModel.valid = !outModel.primitives.empty();
    return outModel.valid;
}

// Recalculate Lambert lighting for all vertices based on sun direction
void GLBModelCPU::recalculateLighting(const Vec3& sunDirection) {
    Vec3 negSunDir = Vec3(-sunDirection.x, -sunDirection.y, -sunDirection.z);
    
    for (auto& prim : primitives) {
        size_t vcount = prim.normals.size();
        for (size_t i = 0; i < vcount; i++) {
            // Calculate Lambert: max(dot(normal, -sunDir), 0.0)
            float lambert = std::max(0.0f, prim.normals[i].dot(negSunDir));
            
            // Store in interleaved buffer at index 8 (pos(3) + normal(3) + uv(2) + lambert(1))
            prim.interleaved[i*9 + 8] = lambert;
        }
    }
}
