#include "pch.h"
#include "ModernRenderer.h"
#include "../Math/Mat4.h"
#include "../Math/Vec3.h"
#include "../Core/GameState.h"
#include "../World/VoxelChunk.h"
#include <iostream>

// Basic vertex shader for instanced cube rendering
const char* g_vertexShaderSource = R"(
#version 460 core

layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec3 aInstancePos;
layout (location = 3) in uint aVoxelType;

layout (std140, binding = 0) uniform CameraData {
    mat4 view;
    mat4 projection;
    vec3 cameraPos;
};

out vec3 worldPos;
out vec3 normal;
out float voxelType;

void main() {
    worldPos = aPos + aInstancePos;
    normal = aNormal;
    voxelType = float(aVoxelType);
    
    gl_Position = projection * view * vec4(worldPos, 1.0);
}
)";

// Basic fragment shader with raycast-ready lighting
const char* g_fragmentShaderSource = R"(
#version 460 core

in vec3 worldPos;
in vec3 normal;
in float voxelType;

layout (std140, binding = 0) uniform CameraData {
    mat4 view;
    mat4 projection;
    vec3 cameraPos;
};

layout (std140, binding = 1) uniform LightingData {
    vec3 sunDirection;
    vec3 sunColor;
    vec3 ambientColor;
    float sunIntensity;
};

out vec4 FragColor;

vec3 getVoxelColor(float type) {
    if (type < 0.5) return vec3(0.0); // Air
    if (type < 1.5) return vec3(0.4, 0.8, 0.2); // Grass
    if (type < 2.5) return vec3(0.6, 0.4, 0.2); // Dirt
    return vec3(0.5, 0.5, 0.5); // Stone
}

void main() {
    vec3 baseColor = getVoxelColor(voxelType);
    
    // Basic diffuse lighting
    float NdotL = max(0.0, dot(normalize(normal), -normalize(sunDirection)));
    vec3 diffuse = sunColor * sunIntensity * NdotL;
    
    vec3 finalColor = baseColor * (ambientColor + diffuse);
    
    FragColor = vec4(finalColor, 1.0);
}
)";

ModernRenderer::ModernRenderer() = default;

ModernRenderer::~ModernRenderer() {
    shutdown();
}

bool ModernRenderer::initialize() {
    if (m_initialized) return true;

    // Initialize GLAD
    if (!gladLoadGL()) {
        std::cerr << "Failed to initialize GLAD" << std::endl;
        return false;
    }

    std::cout << "OpenGL Version: " << glGetString(GL_VERSION) << std::endl;
    std::cout << "OpenGL Renderer: " << glGetString(GL_RENDERER) << std::endl;

    // Enable required OpenGL features
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glFrontFace(GL_CCW);

    // Create shaders and buffers
    if (!createShaders()) {
        std::cerr << "Failed to create shaders" << std::endl;
        return false;
    }

    if (!createBuffers()) {
        std::cerr << "Failed to create buffers" << std::endl;
        return false;
    }

    setupCubeGeometry();

    m_initialized = true;
    std::cout << "ModernRenderer initialized with OpenGL 4.6" << std::endl;
    return true;
}

void ModernRenderer::shutdown() {
    if (!m_initialized) return;

    // Clean up OpenGL resources
    if (m_cubeVAO) glDeleteVertexArrays(1, &m_cubeVAO);
    if (m_cubeVBO) glDeleteBuffers(1, &m_cubeVBO);
    if (m_instanceVBO) glDeleteBuffers(1, &m_instanceVBO);
    if (m_cameraUBO) glDeleteBuffers(1, &m_cameraUBO);
    if (m_lightingUBO) glDeleteBuffers(1, &m_lightingUBO);
    if (m_voxelShader) glDeleteProgram(m_voxelShader);

    // Clean up chunk SSBOs
    for (auto& pair : m_chunkSSBOs) {
        glDeleteBuffers(1, &pair.second);
    }
    m_chunkSSBOs.clear();
    m_chunkVoxelCounts.clear();

    m_initialized = false;
    std::cout << "ModernRenderer shut down" << std::endl;
}

void ModernRenderer::beginFrame() {
    glClearColor(0.6f, 0.8f, 1.0f, 1.0f); // Sky blue
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

void ModernRenderer::endFrame() {
    // Frame complete
}

bool ModernRenderer::createShaders() {
    GLuint vertexShader = compileShader(GL_VERTEX_SHADER, g_vertexShaderSource);
    GLuint fragmentShader = compileShader(GL_FRAGMENT_SHADER, g_fragmentShaderSource);
    
    if (!vertexShader || !fragmentShader) return false;
    
    m_voxelShader = linkProgram(vertexShader, fragmentShader);
    
    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);
    
    return m_voxelShader != 0;
}

bool ModernRenderer::createBuffers() {
    // Create camera uniform buffer
    glGenBuffers(1, &m_cameraUBO);
    glBindBuffer(GL_UNIFORM_BUFFER, m_cameraUBO);
    glBufferData(GL_UNIFORM_BUFFER, sizeof(Mat4) * 2 + sizeof(Vec3), nullptr, GL_DYNAMIC_DRAW);
    glBindBufferBase(GL_UNIFORM_BUFFER, 0, m_cameraUBO);

    // Create lighting uniform buffer  
    glGenBuffers(1, &m_lightingUBO);
    glBindBuffer(GL_UNIFORM_BUFFER, m_lightingUBO);
    glBufferData(GL_UNIFORM_BUFFER, sizeof(Vec3) * 3 + sizeof(float), nullptr, GL_DYNAMIC_DRAW);
    glBindBufferBase(GL_UNIFORM_BUFFER, 1, m_lightingUBO);

    return true;
}

void ModernRenderer::setupCubeGeometry() {
    // Basic cube vertices (positions + normals) - Complete cube with all faces
    float cubeVertices[] = {
        // Front face (Z+)
        -0.5f, -0.5f,  0.5f,  0.0f,  0.0f,  1.0f,
         0.5f, -0.5f,  0.5f,  0.0f,  0.0f,  1.0f,
         0.5f,  0.5f,  0.5f,  0.0f,  0.0f,  1.0f,
         0.5f,  0.5f,  0.5f,  0.0f,  0.0f,  1.0f,
        -0.5f,  0.5f,  0.5f,  0.0f,  0.0f,  1.0f,
        -0.5f, -0.5f,  0.5f,  0.0f,  0.0f,  1.0f,

        // Back face (Z-)
        -0.5f, -0.5f, -0.5f,  0.0f,  0.0f, -1.0f,
         0.5f, -0.5f, -0.5f,  0.0f,  0.0f, -1.0f,
         0.5f,  0.5f, -0.5f,  0.0f,  0.0f, -1.0f,
         0.5f,  0.5f, -0.5f,  0.0f,  0.0f, -1.0f,
        -0.5f,  0.5f, -0.5f,  0.0f,  0.0f, -1.0f,
        -0.5f, -0.5f, -0.5f,  0.0f,  0.0f, -1.0f,

        // Left face (X-)
        -0.5f,  0.5f,  0.5f, -1.0f,  0.0f,  0.0f,
        -0.5f,  0.5f, -0.5f, -1.0f,  0.0f,  0.0f,
        -0.5f, -0.5f, -0.5f, -1.0f,  0.0f,  0.0f,
        -0.5f, -0.5f, -0.5f, -1.0f,  0.0f,  0.0f,
        -0.5f, -0.5f,  0.5f, -1.0f,  0.0f,  0.0f,
        -0.5f,  0.5f,  0.5f, -1.0f,  0.0f,  0.0f,

        // Right face (X+)
         0.5f,  0.5f,  0.5f,  1.0f,  0.0f,  0.0f,
         0.5f,  0.5f, -0.5f,  1.0f,  0.0f,  0.0f,
         0.5f, -0.5f, -0.5f,  1.0f,  0.0f,  0.0f,
         0.5f, -0.5f, -0.5f,  1.0f,  0.0f,  0.0f,
         0.5f, -0.5f,  0.5f,  1.0f,  0.0f,  0.0f,
         0.5f,  0.5f,  0.5f,  1.0f,  0.0f,  0.0f,

        // Bottom face (Y-)
        -0.5f, -0.5f, -0.5f,  0.0f, -1.0f,  0.0f,
         0.5f, -0.5f, -0.5f,  0.0f, -1.0f,  0.0f,
         0.5f, -0.5f,  0.5f,  0.0f, -1.0f,  0.0f,
         0.5f, -0.5f,  0.5f,  0.0f, -1.0f,  0.0f,
        -0.5f, -0.5f,  0.5f,  0.0f, -1.0f,  0.0f,
        -0.5f, -0.5f, -0.5f,  0.0f, -1.0f,  0.0f,

        // Top face (Y+)
        -0.5f,  0.5f, -0.5f,  0.0f,  1.0f,  0.0f,
         0.5f,  0.5f, -0.5f,  0.0f,  1.0f,  0.0f,
         0.5f,  0.5f,  0.5f,  0.0f,  1.0f,  0.0f,
         0.5f,  0.5f,  0.5f,  0.0f,  1.0f,  0.0f,
        -0.5f,  0.5f,  0.5f,  0.0f,  1.0f,  0.0f,
        -0.5f,  0.5f, -0.5f,  0.0f,  1.0f,  0.0f
    };

    glGenVertexArrays(1, &m_cubeVAO);
    glGenBuffers(1, &m_cubeVBO);
    glGenBuffers(1, &m_instanceVBO);

    glBindVertexArray(m_cubeVAO);

    // Cube geometry
    glBindBuffer(GL_ARRAY_BUFFER, m_cubeVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(cubeVertices), cubeVertices, GL_STATIC_DRAW);

    // Position attribute
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    // Normal attribute
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    // Instance buffer (will be filled per chunk)
    glBindBuffer(GL_ARRAY_BUFFER, m_instanceVBO);
    
    // Instance position
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(2);
    glVertexAttribDivisor(2, 1);

    // Voxel type
    glVertexAttribIPointer(3, 1, GL_UNSIGNED_INT, 4 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(3);
    glVertexAttribDivisor(3, 1);

    glBindVertexArray(0);
}

GLuint ModernRenderer::compileShader(GLenum type, const char* source) {
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);

    GLint success;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        char infoLog[512];
        glGetShaderInfoLog(shader, 512, nullptr, infoLog);
        std::cerr << "Shader compilation failed: " << infoLog << std::endl;
        glDeleteShader(shader);
        return 0;
    }

    return shader;
}

GLuint ModernRenderer::linkProgram(GLuint vertexShader, GLuint fragmentShader) {
    GLuint program = glCreateProgram();
    glAttachShader(program, vertexShader);
    glAttachShader(program, fragmentShader);
    glLinkProgram(program);

    GLint success;
    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if (!success) {
        char infoLog[512];
        glGetProgramInfoLog(program, 512, nullptr, infoLog);
        std::cerr << "Program linking failed: " << infoLog << std::endl;
        glDeleteProgram(program);
        return 0;
    }

    return program;
}

void ModernRenderer::updateCameraUniforms(const Mat4& view, const Mat4& projection) {
    glBindBuffer(GL_UNIFORM_BUFFER, m_cameraUBO);
    
    // Upload view matrix
    glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(Mat4), &view);
    
    // Upload projection matrix
    glBufferSubData(GL_UNIFORM_BUFFER, sizeof(Mat4), sizeof(Mat4), &projection);
    
    // Upload camera position (extract from view matrix)
    Vec3 cameraPos(0.0f, 50.0f, 0.0f); // Placeholder for now
    glBufferSubData(GL_UNIFORM_BUFFER, sizeof(Mat4) * 2, sizeof(Vec3), &cameraPos);
}

void ModernRenderer::setProjectionMatrix(const Mat4& matrix) {
    // Store for later use in renderChunk
}

void ModernRenderer::setViewMatrix(const Mat4& matrix) {
    // Store for later use in renderChunk
}

void ModernRenderer::setModelMatrix(const Mat4& matrix) {
    // Not needed for instanced rendering
}

void ModernRenderer::setGameState(GameState* gameState) {
    m_gameState = gameState;
}

void ModernRenderer::uploadChunkMesh(VoxelChunk* chunk) {
    // Create SSBO for this chunk's voxel data
    if (m_chunkSSBOs.find(chunk) == m_chunkSSBOs.end()) {
        GLuint ssbo;
        glGenBuffers(1, &ssbo);
        m_chunkSSBOs[chunk] = ssbo;
    }
    
    // Generate some test voxels to render (simple grid)
    struct InstanceData {
        float x, y, z;
        unsigned int type;
    };
    
    std::vector<InstanceData> instances;
    
    // Create a simple 8x8x8 grid of test voxels
    for (int x = 0; x < 8; x++) {
        for (int y = 0; y < 8; y++) {
            for (int z = 0; z < 8; z++) {
                // Only add voxels in a checkered pattern to see some structure
                if ((x + y + z) % 2 == 0) {
                    InstanceData instance;
                    instance.x = float(x);
                    instance.y = float(y);
                    instance.z = float(z);
                    instance.type = (y < 4) ? 2 : 1; // Dirt bottom, grass top
                    instances.push_back(instance);
                }
            }
        }
    }
    
    m_chunkVoxelCounts[chunk] = instances.size();
    
    // Upload instance data
    glBindBuffer(GL_ARRAY_BUFFER, m_instanceVBO);
    glBufferData(GL_ARRAY_BUFFER, instances.size() * sizeof(InstanceData), instances.data(), GL_STATIC_DRAW);
}

void ModernRenderer::renderChunk(VoxelChunk* chunk, const Vec3& worldPos) {
    if (!m_gameState || m_chunkVoxelCounts[chunk] == 0) return;

    glUseProgram(m_voxelShader);
    glBindVertexArray(m_cubeVAO);

    // Set up basic view and projection matrices
    Mat4 view, projection;
    view.loadIdentity();
    view.translate(0.0f, -10.0f, -30.0f); // Move camera back and up
    
    projection.loadPerspective(45.0f, 1920.0f/1080.0f, 0.1f, 1000.0f);
    
    updateCameraUniforms(view, projection);
    
    // Set up basic lighting
    glBindBuffer(GL_UNIFORM_BUFFER, m_lightingUBO);
    Vec3 sunDirection(0.3f, -1.0f, 0.2f);
    Vec3 sunColor(1.0f, 0.9f, 0.8f);
    Vec3 ambientColor(0.2f, 0.2f, 0.3f);
    float sunIntensity = 0.8f;
    
    glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(Vec3), &sunDirection);
    glBufferSubData(GL_UNIFORM_BUFFER, sizeof(Vec3), sizeof(Vec3), &sunColor);
    glBufferSubData(GL_UNIFORM_BUFFER, sizeof(Vec3) * 2, sizeof(Vec3), &ambientColor);
    glBufferSubData(GL_UNIFORM_BUFFER, sizeof(Vec3) * 3, sizeof(float), &sunIntensity);
    
    // Render instanced cubes
    glDrawArraysInstanced(GL_TRIANGLES, 0, 36, m_chunkVoxelCounts[chunk]);
}

void ModernRenderer::beginBatch() {
    // Prepare for batch rendering
}

void ModernRenderer::endBatch() {
    // Finalize batch rendering
}
