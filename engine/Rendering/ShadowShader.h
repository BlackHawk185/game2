#pragma once

#include <glad/glad.h>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>

class ShadowShader {
public:
    ShadowShader();
    ~ShadowShader();

    bool initialize();
    void cleanup();
    void use();
    
    void setLightSpaceMatrix(const glm::mat4& lightSpaceMatrix);
    void setModelMatrix(const glm::mat4& model);

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
