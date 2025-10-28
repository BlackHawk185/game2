// BlockHighlightRenderer.cpp - Wireframe cube renderer
#include "BlockHighlightRenderer.h"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <iostream>

BlockHighlightRenderer::BlockHighlightRenderer() = default;

BlockHighlightRenderer::~BlockHighlightRenderer()
{
    shutdown();
}

bool BlockHighlightRenderer::initialize()
{
    if (m_initialized) return true;
    
    // Cube vertices (8 corners of a 1x1x1 cube, slightly enlarged for visibility)
    float offset = 0.501f; // Slightly larger than block (1.0) to prevent z-fighting
    float vertices[] = {
        // X, Y, Z
        -offset, -offset, -offset,  // 0: Front-bottom-left
         offset, -offset, -offset,  // 1: Front-bottom-right
         offset,  offset, -offset,  // 2: Front-top-right
        -offset,  offset, -offset,  // 3: Front-top-left
        -offset, -offset,  offset,  // 4: Back-bottom-left
         offset, -offset,  offset,  // 5: Back-bottom-right
         offset,  offset,  offset,  // 6: Back-top-right
        -offset,  offset,  offset   // 7: Back-top-left
    };
    
    // Indices for 12 edges (lines)
    unsigned int indices[] = {
        // Front face
        0, 1,  1, 2,  2, 3,  3, 0,
        // Back face
        4, 5,  5, 6,  6, 7,  7, 4,
        // Connecting edges
        0, 4,  1, 5,  2, 6,  3, 7
    };
    
    // Create VAO
    glGenVertexArrays(1, &m_vao);
    glBindVertexArray(m_vao);
    
    // Create VBO
    glGenBuffers(1, &m_vbo);
    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    
    // Create EBO
    glGenBuffers(1, &m_ebo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);
    
    // Position attribute
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    
    glBindVertexArray(0);
    
    // Compile shader
    if (!compileShader()) {
        std::cerr << "Failed to compile block highlight shader!" << std::endl;
        shutdown();
        return false;
    }
    
    m_initialized = true;
    return true;
}

bool BlockHighlightRenderer::compileShader()
{
    // Simple vertex shader
    const char* vertexSource = R"(
        #version 330 core
        layout (location = 0) in vec3 aPos;
        
        uniform mat4 uModel;
        uniform mat4 uView;
        uniform mat4 uProjection;
        
        void main()
        {
            gl_Position = uProjection * uView * uModel * vec4(aPos, 1.0);
        }
    )";
    
    // Simple fragment shader (bright yellow wireframe)
    const char* fragmentSource = R"(
        #version 330 core
        out vec4 FragColor;
        
        void main()
        {
            FragColor = vec4(1.0, 1.0, 0.0, 1.0); // Bright yellow
        }
    )";
    
    // Compile vertex shader
    GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertexShader, 1, &vertexSource, nullptr);
    glCompileShader(vertexShader);
    
    GLint success;
    glGetShaderiv(vertexShader, GL_COMPILE_STATUS, &success);
    if (!success) {
        char infoLog[512];
        glGetShaderInfoLog(vertexShader, 512, nullptr, infoLog);
        std::cerr << "Vertex shader compilation failed:\n" << infoLog << std::endl;
        glDeleteShader(vertexShader);
        return false;
    }
    
    // Compile fragment shader
    GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragmentShader, 1, &fragmentSource, nullptr);
    glCompileShader(fragmentShader);
    
    glGetShaderiv(fragmentShader, GL_COMPILE_STATUS, &success);
    if (!success) {
        char infoLog[512];
        glGetShaderInfoLog(fragmentShader, 512, nullptr, infoLog);
        std::cerr << "Fragment shader compilation failed:\n" << infoLog << std::endl;
        glDeleteShader(vertexShader);
        glDeleteShader(fragmentShader);
        return false;
    }
    
    // Link shader program
    m_shader = glCreateProgram();
    glAttachShader(m_shader, vertexShader);
    glAttachShader(m_shader, fragmentShader);
    glLinkProgram(m_shader);
    
    glGetProgramiv(m_shader, GL_LINK_STATUS, &success);
    if (!success) {
        char infoLog[512];
        glGetProgramInfoLog(m_shader, 512, nullptr, infoLog);
        std::cerr << "Shader program linking failed:\n" << infoLog << std::endl;
        glDeleteShader(vertexShader);
        glDeleteShader(fragmentShader);
        glDeleteProgram(m_shader);
        m_shader = 0;
        return false;
    }
    
    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);
    
    return true;
}

void BlockHighlightRenderer::render(const Vec3& blockPos, const float* islandTransform, const float* viewMatrix, const float* projectionMatrix)
{
    if (!m_initialized) return;
    
    // Island transform includes both position and rotation
    glm::mat4 islandMatrix = glm::make_mat4(islandTransform);
    
    // Create local offset to block center (in island-local space)
    glm::mat4 blockOffset = glm::translate(glm::mat4(1.0f), 
        glm::vec3(blockPos.x + 0.5f, blockPos.y + 0.5f, blockPos.z + 0.5f));
    
    // Final model matrix: island transform * block offset
    // This transforms from block-local -> island-local -> world space
    glm::mat4 model = islandMatrix * blockOffset;
    
    // Convert float arrays to glm matrices
    glm::mat4 view = glm::make_mat4(viewMatrix);
    glm::mat4 projection = glm::make_mat4(projectionMatrix);
    
    // Use shader
    glUseProgram(m_shader);
    
    // Set uniforms
    GLint modelLoc = glGetUniformLocation(m_shader, "uModel");
    GLint viewLoc = glGetUniformLocation(m_shader, "uView");
    GLint projLoc = glGetUniformLocation(m_shader, "uProjection");
    
    glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(model));
    glUniformMatrix4fv(viewLoc, 1, GL_FALSE, glm::value_ptr(view));
    glUniformMatrix4fv(projLoc, 1, GL_FALSE, glm::value_ptr(projection));
    
    // Render wireframe
    glBindVertexArray(m_vao);
    glLineWidth(2.0f); // Thick lines for visibility
    glDrawElements(GL_LINES, 24, GL_UNSIGNED_INT, 0);
    glLineWidth(1.0f); // Reset
    glBindVertexArray(0);
    
    glUseProgram(0);
}

void BlockHighlightRenderer::shutdown()
{
    if (m_vao) {
        glDeleteVertexArrays(1, &m_vao);
        m_vao = 0;
    }
    if (m_vbo) {
        glDeleteBuffers(1, &m_vbo);
        m_vbo = 0;
    }
    if (m_ebo) {
        glDeleteBuffers(1, &m_ebo);
        m_ebo = 0;
    }
    if (m_shader) {
        glDeleteProgram(m_shader);
        m_shader = 0;
    }
    
    m_initialized = false;
}
