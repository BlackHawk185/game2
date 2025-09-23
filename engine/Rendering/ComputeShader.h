// ComputeShader.h - OpenGL compute shader management for GPU voxel processing
#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <glad/gl.h>

class ComputeShader {
public:
    ComputeShader();
    ~ComputeShader();

    // Shader management
    bool loadFromFile(const std::string& filePath);
    bool loadFromSource(const std::string& source);
    void use();
    void cleanup();

    // Compute dispatch
    void dispatch(unsigned int numGroupsX, unsigned int numGroupsY, unsigned int numGroupsZ);
    void dispatchIndirect(unsigned int buffer);
    void memoryBarrier(unsigned int barriers = GL_ALL_BARRIER_BITS);

    // Uniform management
    void setInt(const std::string& name, int value);
    void setFloat(const std::string& name, float value);
    void setVec3(const std::string& name, float x, float y, float z);
    void setMat4(const std::string& name, const float* value);

    // Buffer binding
    void bindStorageBuffer(unsigned int buffer, unsigned int binding);
    void bindImageTexture(unsigned int texture, unsigned int unit, unsigned int access, unsigned int format);

    // Getters
    unsigned int getProgram() const { return m_program; }
    bool isValid() const { return m_program != 0; }

private:
    unsigned int m_program;
    std::unordered_map<std::string, int> m_uniformCache;

    // Helper functions
    unsigned int compileShader(const std::string& source);
    int getUniformLocation(const std::string& name);
    std::string readFile(const std::string& filePath);
};

// Helper class for GPU buffer management
class ComputeBuffer {
public:
    ComputeBuffer();
    ~ComputeBuffer();

    // Buffer creation
    bool create(size_t size, const void* data = nullptr, unsigned int usage = GL_DYNAMIC_DRAW);
    void resize(size_t newSize);
    void cleanup();

    // Data access
    void setData(const void* data, size_t size, size_t offset = 0);
    void* mapBuffer(unsigned int access = GL_READ_WRITE);
    void unmapBuffer();

    // Binding
    void bindAsStorageBuffer(unsigned int binding);
    void bindAsVertexBuffer();
    void bindAsIndexBuffer();

    // Getters
    unsigned int getBuffer() const { return m_buffer; }
    size_t getSize() const { return m_size; }
    bool isValid() const { return m_buffer != 0; }

private:
    unsigned int m_buffer;
    size_t m_size;
};
