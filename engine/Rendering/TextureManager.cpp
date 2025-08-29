// TextureManager.cpp - Implementation of texture loading and management
#include "TextureManager.h"
#include "stb_image.h"
#include <iostream>
#include <filesystem>

// Global texture manager instance
TextureManager* g_textureManager = nullptr;

TextureManager::TextureManager()
{
}

TextureManager::~TextureManager()
{
    unloadAllTextures();
}

GLuint TextureManager::loadTexture(const std::string& filepath)
{
    return loadTexture(filepath, true, false);
}

GLuint TextureManager::loadTexture(const std::string& filepath, bool generateMipmaps, bool pixelArt)
{
    // Check if texture is already loaded
    std::string filename = getFileName(filepath);
    auto it = m_textures.find(filename);
    if (it != m_textures.end()) {
        std::cout << "Texture already loaded: " << filename << std::endl;
        return it->second;
    }
    
    // Load image data
    int width, height, channels;
    unsigned char* data = stbi_load(filepath.c_str(), &width, &height, &channels, 0);
    
    if (!data) {
        std::cerr << "Failed to load texture: " << filepath << std::endl;
        std::cerr << "STB Error: " << stbi_failure_reason() << std::endl;
        return 0;
    }
    
    // Create OpenGL texture
    GLuint textureID = createTexture(data, width, height, channels, pixelArt);
    
    // Free image data
    stbi_image_free(data);
    
    if (textureID != 0) {
        m_textures[filename] = textureID;
        std::cout << "Loaded texture: " << filename << " (" << width << "x" << height << ", " << channels << " channels)" << std::endl;
    }
    
    return textureID;
}

GLuint TextureManager::getTexture(const std::string& name)
{
    auto it = m_textures.find(name);
    if (it != m_textures.end()) {
        return it->second;
    }
    return 0;
}

void TextureManager::unloadTexture(const std::string& name)
{
    auto it = m_textures.find(name);
    if (it != m_textures.end()) {
        glDeleteTextures(1, &it->second);
        m_textures.erase(it);
        std::cout << "Unloaded texture: " << name << std::endl;
    }
}

void TextureManager::unloadAllTextures()
{
    for (auto& pair : m_textures) {
        glDeleteTextures(1, &pair.second);
    }
    m_textures.clear();
    std::cout << "Unloaded all textures" << std::endl;
}

GLuint TextureManager::createTexture(const unsigned char* data, int width, int height, int channels, bool pixelArt)
{
    GLuint textureID;
    glGenTextures(1, &textureID);
    glBindTexture(GL_TEXTURE_2D, textureID);
    
    // Determine format
    GLenum format;
    switch (channels) {
        case 1: format = GL_LUMINANCE; break;
        case 3: format = GL_RGB; break;
        case 4: format = GL_RGBA; break;
        default:
            std::cerr << "Unsupported number of channels: " << channels << std::endl;
            glDeleteTextures(1, &textureID);
            return 0;
    }
    
    // Upload texture data
    glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);
    
    // Set texture parameters
    if (pixelArt) {
        // Crisp pixel art settings (like Minecraft)
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    } else {
        // Smooth filtering
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    }
    
    // Wrapping
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    
    // Generate mipmaps if requested
    if (!pixelArt) {
        // Note: glGenerateMipmap requires OpenGL 3.0+, fallback for older versions
        // For now we'll skip mipmaps to keep compatibility
    }
    
    glBindTexture(GL_TEXTURE_2D, 0);
    
    return textureID;
}

std::string TextureManager::getFileName(const std::string& filepath)
{
    std::filesystem::path path(filepath);
    return path.filename().string();
}
