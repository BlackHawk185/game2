#pragma once

#include "../Math/Mat4.h"
#include "../Math/Vec3.h"
#include <vector>
#include <utility>

// Forward declare basic GL types
using GLuint = unsigned int;

class VoxelChunk;

// Minimal scaffold for a shadow depth pass. This version computes light matrices
// and exposes an interface for depth rendering, but defers actual FBO usage until
// the GL loader exposes framebuffer functions.
class ShadowPass {
public:
    ShadowPass();
    ~ShadowPass();

    bool initialize(int size = 1024);
    void shutdown();

    // Compute light view/projection around a focus point (usually camera)
    void computeLightMatrices(const Vec3& sunDir, const Vec3& focusPoint,
                              float extent, float nearPlane, float farPlane);

    // Intended to render all chunk geometry into the shadow map (no-op for now)
    void renderDepth(const std::vector<std::pair<VoxelChunk*, Vec3>>& chunks);

    const Mat4& getLightView() const { return m_lightView; }
    const Mat4& getLightProj() const { return m_lightProj; }
    unsigned int getDepthTexture() const { return m_depthTex; }
    bool hasFBO() const { return m_hasFBO; }
    int getSize() const { return m_size; }

private:
    int m_size;
    Mat4 m_lightView;
    Mat4 m_lightProj;
    bool m_initialized;
    unsigned int m_fbo = 0;
    unsigned int m_depthTex = 0;
    bool m_hasFBO = false;
    // Depth-only shader program
    GLuint m_depthProgram = 0;
    int m_uLightVP = -1;
    int m_uModel = -1;
};

// Global instance
extern ShadowPass* g_shadowPass;
