// ComputeShader.cpp - OpenGL compute shader implementation
#include "ComputeShader.h"
#include <iostream>
#include <fstream>
#include <sstream>

// =============================================================================
// ComputeShader Implementation
// =============================================================================

ComputeShader::ComputeShader() : m_program(0) {
}

ComputeShader::~ComputeShader() {
    cleanup();
}

bool ComputeShader::loadFromFile(const std::string& filePath) {
    std::string source = readFile(filePath);
    if (source.empty()) {
        std::cerr << "Failed to read compute shader file: " << filePath << std::endl;
        return false;
    }
    return loadFromSource(source);
}

bool ComputeShader::loadFromSource(const std::string& source) {
    cleanup(); // Clean up any existing shader

    unsigned int shader = compileShader(source);
    if (shader == 0) {
        return false;
    }

    // Create and link program
    m_program = glCreateProgram();
    glAttachShader(m_program, shader);
    glLinkProgram(m_program);

    // Check linking
    int success;
    glGetProgramiv(m_program, GL_LINK_STATUS, &success);
    if (!success) {
        char infoLog[1024];
        glGetProgramInfoLog(m_program, 1024, nullptr, infoLog);
        std::cerr << "Compute shader linking failed:\n" << infoLog << std::endl;
        glDeleteProgram(m_program);
        m_program = 0;
        glDeleteShader(shader);
        return false;
    }

    glDeleteShader(shader); // Shader no longer needed after linking
    m_uniformCache.clear();
    
    std::cout << "Compute shader compiled and linked successfully" << std::endl;
    return true;
}

void ComputeShader::use() {
    if (m_program != 0) {
        glUseProgram(m_program);
    }
}

void ComputeShader::cleanup() {
    if (m_program != 0) {
        glDeleteProgram(m_program);
        m_program = 0;
    }
    m_uniformCache.clear();
}

void ComputeShader::dispatch(unsigned int numGroupsX, unsigned int numGroupsY, unsigned int numGroupsZ) {
    if (m_program == 0) {
        std::cerr << "Cannot dispatch: compute shader not loaded" << std::endl;
        return;
    }
    
    use();
    glDispatchCompute(numGroupsX, numGroupsY, numGroupsZ);
}

void ComputeShader::dispatchIndirect(unsigned int buffer) {
    if (m_program == 0) {
        std::cerr << "Cannot dispatch indirect: compute shader not loaded" << std::endl;
        return;
    }
    
    use();
    glBindBuffer(GL_DISPATCH_INDIRECT_BUFFER, buffer);
    glDispatchComputeIndirect(0);
}

void ComputeShader::memoryBarrier(unsigned int barriers) {
    glMemoryBarrier(barriers);
}

void ComputeShader::setInt(const std::string& name, int value) {
    glUniform1i(getUniformLocation(name), value);
}

void ComputeShader::setFloat(const std::string& name, float value) {
    glUniform1f(getUniformLocation(name), value);
}

void ComputeShader::setVec3(const std::string& name, float x, float y, float z) {
    glUniform3f(getUniformLocation(name), x, y, z);
}

void ComputeShader::setMat4(const std::string& name, const float* value) {
    glUniformMatrix4fv(getUniformLocation(name), 1, GL_FALSE, value);
}

void ComputeShader::bindStorageBuffer(unsigned int buffer, unsigned int binding) {
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, binding, buffer);
}

void ComputeShader::bindImageTexture(unsigned int texture, unsigned int unit, unsigned int access, unsigned int format) {
    glBindImageTexture(unit, texture, 0, GL_FALSE, 0, access, format);
}

unsigned int ComputeShader::compileShader(const std::string& source) {
    unsigned int shader = glCreateShader(GL_COMPUTE_SHADER);
    const char* src = source.c_str();
    glShaderSource(shader, 1, &src, nullptr);
    glCompileShader(shader);

    // Check compilation
    int success;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        char infoLog[1024];
        glGetShaderInfoLog(shader, 1024, nullptr, infoLog);
        std::cerr << "Compute shader compilation failed:\n" << infoLog << std::endl;
        glDeleteShader(shader);
        return 0;
    }

    return shader;
}

int ComputeShader::getUniformLocation(const std::string& name) {
    if (m_uniformCache.find(name) != m_uniformCache.end()) {
        return m_uniformCache[name];
    }

    int location = glGetUniformLocation(m_program, name.c_str());
    m_uniformCache[name] = location;
    return location;
}

std::string ComputeShader::readFile(const std::string& filePath) {
    std::cout << "🔍 Attempting to read shader file: " << filePath << std::endl;
    
    std::ifstream file(filePath);
    if (!file.is_open()) {
        std::cerr << "❌ Failed to open file: " << filePath << std::endl;
        std::cerr << "📂 Current working directory hint: Check relative path from executable location" << std::endl;
        return "";
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string content = buffer.str();
    
    if (content.empty()) {
        std::cerr << "⚠️ File opened but content is empty: " << filePath << std::endl;
        return "";
    }
    
    std::cout << "✅ Successfully read " << content.length() << " characters from " << filePath << std::endl;
    return content;
}

// =============================================================================
// ComputeBuffer Implementation
// =============================================================================

ComputeBuffer::ComputeBuffer() : m_buffer(0), m_size(0) {
}

ComputeBuffer::~ComputeBuffer() {
    cleanup();
}

bool ComputeBuffer::create(size_t size, const void* data, unsigned int usage) {
    cleanup(); // Clean up any existing buffer

    glGenBuffers(1, &m_buffer);
    if (m_buffer == 0) {
        std::cerr << "Failed to generate buffer" << std::endl;
        return false;
    }

    glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_buffer);
    glBufferData(GL_SHADER_STORAGE_BUFFER, size, data, usage);
    
    // Check for errors
    GLenum error = glGetError();
    if (error != GL_NO_ERROR) {
        std::cerr << "Failed to create buffer, OpenGL error: " << error << std::endl;
        glDeleteBuffers(1, &m_buffer);
        m_buffer = 0;
        return false;
    }

    m_size = size;
    std::cout << "Created compute buffer: " << size << " bytes" << std::endl;
    return true;
}

void ComputeBuffer::resize(size_t newSize) {
    if (m_buffer == 0) {
        create(newSize);
        return;
    }

    glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_buffer);
    glBufferData(GL_SHADER_STORAGE_BUFFER, newSize, nullptr, GL_DYNAMIC_DRAW);
    m_size = newSize;
}

void ComputeBuffer::cleanup() {
    if (m_buffer != 0) {
        glDeleteBuffers(1, &m_buffer);
        m_buffer = 0;
        m_size = 0;
    }
}

void ComputeBuffer::setData(const void* data, size_t size, size_t offset) {
    if (m_buffer == 0) {
        std::cerr << "Cannot set data: buffer not created" << std::endl;
        return;
    }

    glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_buffer);
    glBufferSubData(GL_SHADER_STORAGE_BUFFER, offset, size, data);
}

void* ComputeBuffer::mapBuffer(unsigned int access) {
    if (m_buffer == 0) {
        std::cerr << "Cannot map: buffer not created" << std::endl;
        return nullptr;
    }

    glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_buffer);
    return glMapBuffer(GL_SHADER_STORAGE_BUFFER, access);
}

void ComputeBuffer::unmapBuffer() {
    if (m_buffer != 0) {
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_buffer);
        glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);
    }
}

void ComputeBuffer::bindAsStorageBuffer(unsigned int binding) {
    if (m_buffer != 0) {
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, binding, m_buffer);
    }
}

void ComputeBuffer::bindAsVertexBuffer() {
    if (m_buffer != 0) {
        glBindBuffer(GL_ARRAY_BUFFER, m_buffer);
    }
}

void ComputeBuffer::bindAsIndexBuffer() {
    if (m_buffer != 0) {
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_buffer);
    }
}
