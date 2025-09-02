#pragma once

#include <string>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include "../Math/Vec3.h"

// Avoid leaking OpenGL headers in this public header to prevent include order issues.
// Use a lightweight alias for GLuint and include the actual GL headers in the .cpp.
using GLuint = unsigned int;

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
    
    bool isValid() const { return m_program != 0; }

private:
    GLuint m_program;
    GLuint m_vertexShader;
    GLuint m_fragmentShader;
    
    bool compileShader(GLuint shader, const char* source);
    bool linkProgram();
    int getUniformLocation(const std::string& name);
};
