#pragma once

#include "../Math/Mat4.h"
using GLuint = unsigned int;
using GLint = int;

class ShadowShader {
public:
    ShadowShader();
    ~ShadowShader();

    bool initialize();
    void cleanup();
    void use();
    
    void setLightSpaceMatrix(const Mat4& lightSpaceMatrix);
    void setModelMatrix(const Mat4& model);

private:
    GLuint m_program;
    GLuint m_vertexShader;
    GLuint m_fragmentShader;
    
    // Uniform locations
    GLint m_lightSpaceMatrixLoc;
    GLint m_modelMatrixLoc;
    
    bool compileShader(GLuint shader, const char* source);
    bool m_initialized;
};
