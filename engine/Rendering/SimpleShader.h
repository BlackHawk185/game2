#pragma once

#include <string>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include "../Math/Vec3.h"

// Avoid leaking OpenGL headers in this public header to prevent include order issues.
// Use a lightweight alias for GLuint and include the actual GL headers in the .cpp.
using GLuint = unsigned int;

// Structure for chunk lighting data in UBO
struct ChunkLightingData {
    glm::mat4 transforms[64];        // Transform matrices for up to 64 chunks
    glm::vec4 lightColors[64];       // Per-chunk light tinting (rgb + intensity)
    glm::vec4 ambientData[64];       // Per-chunk ambient (rgb + occlusion strength)
    glm::vec2 lightMapOffsets[64];   // UV offsets for light map atlasing
    int numChunks;                   // Number of active chunks
    int padding[3];                  // Align to 16 bytes
};

class SimpleShader {
public:
    SimpleShader();
    ~SimpleShader();
    
    bool initialize();
    void use();
    void cleanup();
    
    void setMatrix4(const std::string& name, const glm::mat4& matrix);
    void setVector3(const std::string& name, const Vec3& vector);
    void setFloat(const std::string& name, float value);
    void setInt(const std::string& name, int value);
    
    // Material system helpers
    void setMaterialColor(const glm::vec4& color);
    void setMaterialType(int type);
    void setMaterialRoughness(float roughness);
    void setMaterialEmissive(const glm::vec3& emissive);
    
    // UBO management
    bool initializeUBO();
    void updateChunkLightingData(const ChunkLightingData& data);
    void updateChunkLightingData(int chunkIndex, const glm::mat4& transform, const Vec3& lightColor, const Vec3& ambientColor, float ambientStrength);
    void setChunkIndex(int chunkIndex);
    
    bool isValid() const { return m_program != 0; }

private:
    GLuint m_program;
    GLuint m_vertexShader;
    GLuint m_fragmentShader;
    GLuint m_uboHandle;              // UBO handle for chunk lighting data
    
    bool compileShader(GLuint shader, const char* source);
    bool linkProgram();
    int getUniformLocation(const std::string& name);
};
