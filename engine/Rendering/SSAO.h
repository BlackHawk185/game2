// SSAO.h - Screen Space Ambient Occlusion post-process
#pragma once

#include <glad/glad.h>
#include <vector>

class SSAO
{
public:
    SSAO();
    ~SSAO();

    bool initialize();
    void shutdown();

    // Ensure AO textures/FBOs match viewport size
    void ensureResources(int width, int height);

    // Compute AO from the scene depth texture.
    // Inputs are viewport size, depth texture, and camera params used for reconstruction.
    void computeAO(GLuint depthTex,
                   int width,
                   int height,
                   float tanHalfFov,
                   float aspect,
                   float nearPlane,
                   float farPlane);

    // Optional blur to smooth AO
    void blurAO(int width, int height);

    // Composite AO over scene color into the default framebuffer.
    void composite(GLuint sceneColorTex, int width, int height, float intensity = 1.0f);

    GLuint getAOTexture() const { return m_aoBlurTex ? m_aoBlurTex : m_aoTex; }

private:
    // GL resources
    GLuint m_fullscreenVAO = 0;
    GLuint m_fullscreenVBO = 0;

    GLuint m_aoFBO = 0;
    GLuint m_aoTex = 0;

    GLuint m_aoBlurFBO = 0;
    GLuint m_aoBlurTex = 0;

    GLuint m_noiseTex = 0;

    // Shaders
    GLuint m_aoProgram = 0;
    GLuint m_blurProgram = 0;
    GLuint m_compositeProgram = 0;

    // Kernel samples
    std::vector<float> m_kernel; // 3 floats per sample
    int m_kernelCount = 16;

    // Internal helpers
    bool compileProgram(const char* vs, const char* fs, GLuint& programOut);
    void createFullscreenQuad();
    void createNoiseTexture();
    void createOrResizeAOTextures(int width, int height);
};

