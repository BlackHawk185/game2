// BloomRenderer.h - HDR bloom post-processing effect
#pragma once

typedef unsigned int GLuint;

/**
 * Implements bloom post-processing effect using dual-pass Gaussian blur.
 * Makes bright areas (sun, sky highlights) glow and bleed into surrounding pixels.
 */
class BloomRenderer {
public:
    BloomRenderer();
    ~BloomRenderer();

    // Initialize with render target size
    bool initialize(int width, int height);
    void shutdown();
    
    // Resize framebuffers when window size changes
    void resize(int width, int height);
    
    // Apply bloom effect
    // Input: sceneTexture (the rendered scene with HDR colors)
    // Output: returns texture ID with bloom applied
    GLuint applyBloom(GLuint sceneTexture);
    
    // Configuration
    void setBloomIntensity(float intensity) { m_bloomIntensity = intensity; }
    void setBloomThreshold(float threshold) { m_bloomThreshold = threshold; }
    void setBlurPasses(int passes) { m_blurPasses = passes; }
    
    float getBloomIntensity() const { return m_bloomIntensity; }
    float getBloomThreshold() const { return m_bloomThreshold; }

private:
    // Framebuffer dimensions
    int m_width;
    int m_height;
    
    // Configuration
    float m_bloomIntensity;   // How strong the bloom effect is (0.0-1.0)
    float m_bloomThreshold;   // Brightness threshold for bloom (0.0-1.0)
    int m_blurPasses;         // Number of blur iterations (more = softer glow)
    
    // Framebuffers for bloom processing
    GLuint m_brightnessFBO;       // Extract bright pixels
    GLuint m_brightnessTexture;
    
    GLuint m_blurFBO[2];          // Ping-pong buffers for blur
    GLuint m_blurTexture[2];
    
    GLuint m_compositeFBO;        // Final composite
    GLuint m_compositeTexture;
    
    // Shaders
    GLuint m_brightnessShader;    // Extract bright pixels
    GLuint m_blurShader;          // Gaussian blur
    GLuint m_compositeShader;     // Composite bloom onto scene
    
    // Fullscreen quad for post-processing
    GLuint m_quadVAO;
    GLuint m_quadVBO;
    
    // Internal methods
    void createFramebuffers();
    void createShaders();
    void createQuad();
    
    // Render steps
    void extractBrightness(GLuint sceneTexture);
    void blurBrightness();
    void composite(GLuint sceneTexture);
    
    // Shader compilation
    GLuint compileShader(const char* source, unsigned int type);
    GLuint linkProgram(GLuint vertexShader, GLuint fragmentShader);
};
