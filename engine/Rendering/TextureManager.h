// TextureManager.h - Handles loading and managing textures
#pragma once

#include <GL/gl.h>
#include <string>
#include <unordered_map>

class TextureManager
{
public:
    TextureManager();
    ~TextureManager();

    // Load texture from file
    GLuint loadTexture(const std::string& filepath);
    
    // Load texture with specific settings
    GLuint loadTexture(const std::string& filepath, bool generateMipmaps, bool pixelArt = false);
    
    // Get existing texture by name
    GLuint getTexture(const std::string& name);
    
    // Unload specific texture
    void unloadTexture(const std::string& name);
    
    // Unload all textures
    void unloadAllTextures();
    
    // Create texture from raw data
    GLuint createTexture(const unsigned char* data, int width, int height, int channels, bool pixelArt = false);

private:
    std::unordered_map<std::string, GLuint> m_textures;
    
    // Helper functions
    std::string getFileName(const std::string& filepath);
    void setTextureParameters(bool generateMipmaps, bool pixelArt);
};

// Global texture manager instance
extern TextureManager* g_textureManager;
