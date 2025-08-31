#pragma once

#include <string>
#include "../Math/Mat4.h"
#include "../Math/Vec3.h"

// Avoid including GL headers in public header
using GLuint = unsigned int;

class LitShader {
public:
    LitShader();
    ~LitShader();

    bool initialize();
    void use();
    void cleanup();

    // Uniform setters
    void setMatrix4(const std::string& name, const Mat4& matrix);
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

