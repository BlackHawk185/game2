#include "VoxelRenderer.h"
#include <glad/gl.h>
#include <iostream>
#include <fstream>
#include <sstream>

// STB for image loading - use in separate compilation unit to avoid redefinition
#include <stb_image.h>

namespace Engine {
namespace Rendering {

// Vertex shader source (OpenGL 4.6 Core Profile)
const char* vertexShaderSource = R"(
#version 460 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec3 aTexCoord;     // (u, v, material_id)
// ...existing code...

uniform mat4 uView;
uniform mat4 uProjection;
uniform mat4 uModel;

out vec3 FragPos;
out vec3 Normal;
out vec2 TexCoord;
// ...existing code...
out float MaterialId;
out float ViewDistance;  // Distance from camera for fog

void main()
{
    vec4 worldPos = uModel * vec4(aPos, 1.0);
    FragPos = vec3(worldPos);
    Normal = mat3(transpose(inverse(uModel))) * aNormal;
    TexCoord = aTexCoord.xy;
    // ...existing code...
    MaterialId = aTexCoord.z;
    
    vec4 viewPos = uView * worldPos;
    ViewDistance = length(viewPos.xyz);  // Distance in view space
    
    gl_Position = uProjection * viewPos;
}
)";

// Fragment shader source (OpenGL 4.6 Core Profile)
const char* fragmentShaderSource = R"(
#version 460 core
out vec4 FragColor;

in vec3 FragPos;
in vec3 Normal;
in vec2 TexCoord;
// ...existing code...
in float MaterialId;
in float ViewDistance;

uniform sampler2D uTexture;
// ...existing code...
uniform vec3 uFogColor;
uniform float uFogDensity;
uniform float uFogStart;
uniform float uFogEnd;

void main()
{
    // Sample texture color
    vec4 textureColor = texture(uTexture, TexCoord);
    
    // ...existing code...
    
    // Simple lighting calculation
    vec3 lightDir = normalize(vec3(1.0, 1.0, 1.0)); // Directional light
    float diff = max(dot(normalize(Normal), lightDir), 0.2); // Minimum ambient
    
    // Combine texture, lightmap, and simple lighting
    vec3 finalColor = textureColor.rgb * diff;
    
    // Apply atmospheric fog
    float fogFactor = clamp((uFogEnd - ViewDistance) / (uFogEnd - uFogStart), 0.0, 1.0);
    finalColor = mix(uFogColor, finalColor, fogFactor);
    
    FragColor = vec4(finalColor, textureColor.a);
}
)";

VoxelRenderer::VoxelRenderer() 
    : m_shaderProgram(0)
    , m_uniformView(-1)
    , m_uniformProjection(-1)
    , m_uniformModel(-1)
    , m_uniformTexture(-1)
    // ...existing code...
    , m_chunksRendered(0)
    , m_trianglesRendered(0)
{
}

VoxelRenderer::~VoxelRenderer() {
    shutdown();
}

bool VoxelRenderer::initialize() {
    std::cout << "VoxelRenderer: Initializing..." << std::endl;
    
    // Load shaders
    if (!loadShaders()) {
        std::cerr << "VoxelRenderer: Failed to load shaders" << std::endl;
        return false;
    }
    
    // Load dirt texture
    m_dirtTexture = loadTexture("assets/textures/dirt.png");
    if (m_dirtTexture.textureId == 0) {
        std::cerr << "VoxelRenderer: Failed to load dirt.png texture" << std::endl;
        return false;
    }
    
    // ...existing code...
    unsigned char whitePixel[4] = {255, 255, 255, 255};
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, whitePixel);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glBindTexture(GL_TEXTURE_2D, 0);
    
    // Enable depth testing
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    
    // Enable face culling
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glFrontFace(GL_CCW);
    
    std::cout << "VoxelRenderer: Initialization complete" << std::endl;
    return true;
}

void VoxelRenderer::shutdown() {
    if (m_shaderProgram != 0) {
        glDeleteProgram(m_shaderProgram);
        m_shaderProgram = 0;
    }
    
    if (m_dirtTexture.textureId != 0) {
        glDeleteTextures(1, &m_dirtTexture.textureId);
        m_dirtTexture.textureId = 0;
    }
    
    // ...existing code...
    }
    
    std::cout << "VoxelRenderer: Shutdown complete" << std::endl;
}

Texture VoxelRenderer::loadTexture(const std::string& filePath) {
    std::cout << "VoxelRenderer: Loading texture: " << filePath << std::endl;
    
    Texture texture;
    texture.filePath = filePath;
    
    // Load image data
    stbi_set_flip_vertically_on_load(true);
    unsigned char* data = stbi_load(filePath.c_str(), &texture.width, &texture.height, &texture.channels, 0);
    
    if (!data) {
        std::cerr << "VoxelRenderer: Failed to load texture: " << filePath << std::endl;
        return texture;
    }
    
    // Determine format
    GLenum format = GL_RGB;
    if (texture.channels == 1) format = GL_RED;
    else if (texture.channels == 3) format = GL_RGB;
    else if (texture.channels == 4) format = GL_RGBA;
    
    // Generate OpenGL texture
    glGenTextures(1, &texture.textureId);
    glBindTexture(GL_TEXTURE_2D, texture.textureId);
    
    glTexImage2D(GL_TEXTURE_2D, 0, format, texture.width, texture.height, 0, format, GL_UNSIGNED_BYTE, data);
    glGenerateMipmap(GL_TEXTURE_2D);
    
    // Set texture parameters
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    
    glBindTexture(GL_TEXTURE_2D, 0);
    stbi_image_free(data);
    
    std::cout << "VoxelRenderer: Loaded texture " << filePath << " (" 
              << texture.width << "x" << texture.height << ", " 
              << texture.channels << " channels)" << std::endl;
    
    return texture;
}

void VoxelRenderer::generateChunkRenderData(const Engine::World::RenderMesh& mesh, RenderedChunk& renderedChunk) {
    if (mesh.vertices.empty()) {
        std::cout << "VoxelRenderer: Skipping empty mesh" << std::endl;
        return;
    }
    
    std::cout << "VoxelRenderer: Generating render data for chunk with " 
              << mesh.getVertexCount() << " vertices, " 
              << mesh.getTriangleCount() << " triangles" << std::endl;
    
    // Generate VAO
    glGenVertexArrays(1, &renderedChunk.VAO);
    glBindVertexArray(renderedChunk.VAO);
    
    // Generate VBOs
    glGenBuffers(1, &renderedChunk.VBO_vertices);
    glGenBuffers(1, &renderedChunk.VBO_normals);
    glGenBuffers(1, &renderedChunk.VBO_textureUV);
    // ...existing code...
    glGenBuffers(1, &renderedChunk.EBO);
    
    // Upload vertex data
    glBindBuffer(GL_ARRAY_BUFFER, renderedChunk.VBO_vertices);
    glBufferData(GL_ARRAY_BUFFER, mesh.vertices.size() * sizeof(Vec3), mesh.vertices.data(), GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    
    // Upload normal data
    glBindBuffer(GL_ARRAY_BUFFER, renderedChunk.VBO_normals);
    glBufferData(GL_ARRAY_BUFFER, mesh.normals.size() * sizeof(Vec3), mesh.normals.data(), GL_STATIC_DRAW);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(1);
    
    // Upload texture UV data
    glBindBuffer(GL_ARRAY_BUFFER, renderedChunk.VBO_textureUV);
    glBufferData(GL_ARRAY_BUFFER, mesh.textureUV.size() * sizeof(Vec3), mesh.textureUV.data(), GL_STATIC_DRAW);
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(2);
    
    // ...existing code...
    glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(3);
    
    // Upload index data
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, renderedChunk.EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, mesh.indices.size() * sizeof(uint32_t), mesh.indices.data(), GL_STATIC_DRAW);
    
    renderedChunk.indexCount = static_cast<uint32_t>(mesh.indices.size());
    renderedChunk.isGenerated = true;
    
    glBindVertexArray(0);
    
    std::cout << "VoxelRenderer: Generated OpenGL data - VAO: " << renderedChunk.VAO 
              << ", " << renderedChunk.indexCount << " indices" << std::endl;
}

void VoxelRenderer::renderChunk(const RenderedChunk& renderedChunk, const Texture& texture) {
    renderChunk(renderedChunk, texture, Vec3(0.0f, 0.0f, 0.0f));
}

void VoxelRenderer::renderChunk(const RenderedChunk& renderedChunk, const Texture& texture, const Vec3& worldOffset) {
    if (!renderedChunk.isGenerated || renderedChunk.indexCount == 0) {
        return;
    }
    
    // Bind textures
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, texture.textureId);
    glUniform1i(m_uniformTexture, 0);
    
    glActiveTexture(GL_TEXTURE1);
    // ...existing code...
    
    // Set model matrix with world offset
    float modelMatrix[16] = {
        1, 0, 0, worldOffset.x,
        0, 1, 0, worldOffset.y,
        0, 0, 1, worldOffset.z,
        0, 0, 0, 1
    };
    glUniformMatrix4fv(m_uniformModel, 1, GL_FALSE, modelMatrix);
    
    // Render
    glBindVertexArray(renderedChunk.VAO);
    glDrawElements(GL_TRIANGLES, renderedChunk.indexCount, GL_UNSIGNED_INT, 0);
    glBindVertexArray(0);
    
    // Update statistics
    m_chunksRendered++;
    m_trianglesRendered += renderedChunk.indexCount / 3;
}

void VoxelRenderer::renderChunks(const std::vector<RenderedChunk>& chunks, const Texture& texture) {
    renderChunks(chunks, texture, Vec3(0.0f, 0.0f, 0.0f));
}

void VoxelRenderer::renderChunks(const std::vector<RenderedChunk>& chunks, const Texture& texture, const Vec3& worldOffset) {
    for (const auto& chunk : chunks) {
        renderChunk(chunk, texture, worldOffset);
    }
}

void VoxelRenderer::beginFrame() {
    resetStatistics();
    
    // Use shader program
    glUseProgram(m_shaderProgram);
    
    // Clear buffers
    glClearColor(0.53f, 0.81f, 0.92f, 1.0f); // Sky blue
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

void VoxelRenderer::endFrame() {
    glUseProgram(0);
}

void VoxelRenderer::setViewMatrix(const float* viewMatrix) {
    glUniformMatrix4fv(m_uniformView, 1, GL_FALSE, viewMatrix);
}

void VoxelRenderer::setProjectionMatrix(const float* projectionMatrix) {
    glUniformMatrix4fv(m_uniformProjection, 1, GL_FALSE, projectionMatrix);
}

void VoxelRenderer::setFogColor(float r, float g, float b) {
    glUseProgram(m_shaderProgram);
    glUniform3f(m_uniformFogColor, r, g, b);
}

void VoxelRenderer::setFogDistance(float start, float end) {
    glUseProgram(m_shaderProgram);
    glUniform1f(m_uniformFogStart, start);
    glUniform1f(m_uniformFogEnd, end);
}

void VoxelRenderer::setFogDensity(float density) {
    glUseProgram(m_shaderProgram);
    glUniform1f(m_uniformFogDensity, density);
}

bool VoxelRenderer::loadShaders() {
    m_shaderProgram = createShaderProgram(vertexShaderSource, fragmentShaderSource);
    if (m_shaderProgram == 0) {
        return false;
    }
    
    // Get uniform locations
    m_uniformView = glGetUniformLocation(m_shaderProgram, "uView");
    m_uniformProjection = glGetUniformLocation(m_shaderProgram, "uProjection");
    m_uniformModel = glGetUniformLocation(m_shaderProgram, "uModel");
    m_uniformTexture = glGetUniformLocation(m_shaderProgram, "uTexture");
    m_uniformFogColor = glGetUniformLocation(m_shaderProgram, "uFogColor");
    m_uniformFogDensity = glGetUniformLocation(m_shaderProgram, "uFogDensity");
    m_uniformFogStart = glGetUniformLocation(m_shaderProgram, "uFogStart");
    m_uniformFogEnd = glGetUniformLocation(m_shaderProgram, "uFogEnd");
    // ...existing code...
    
    // Set default fog parameters
    glUseProgram(m_shaderProgram);
    glUniform3f(m_uniformFogColor, 0.6f, 0.7f, 0.9f);  // Light blue
    glUniform1f(m_uniformFogDensity, 0.015f);
    glUniform1f(m_uniformFogStart, 50.0f);
    glUniform1f(m_uniformFogEnd, 300.0f);
    
    std::cout << "VoxelRenderer: Loaded shaders successfully" << std::endl;
    return true;
}

GLuint VoxelRenderer::compileShader(const std::string& source, unsigned int type) {
    GLuint shader = glCreateShader(type);
    const char* src = source.c_str();
    glShaderSource(shader, 1, &src, nullptr);
    glCompileShader(shader);
    
    // Check compilation
    int success;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        char infoLog[512];
        glGetShaderInfoLog(shader, 512, nullptr, infoLog);
        std::cerr << "VoxelRenderer: Shader compilation failed: " << infoLog << std::endl;
        glDeleteShader(shader);
        return 0;
    }
    
    return shader;
}

GLuint VoxelRenderer::createShaderProgram(const std::string& vertexSource, const std::string& fragmentSource) {
    GLuint vertexShader = compileShader(vertexSource, GL_VERTEX_SHADER);
    GLuint fragmentShader = compileShader(fragmentSource, GL_FRAGMENT_SHADER);
    
    if (vertexShader == 0 || fragmentShader == 0) {
        if (vertexShader) glDeleteShader(vertexShader);
        if (fragmentShader) glDeleteShader(fragmentShader);
        return 0;
    }
    
    GLuint program = glCreateProgram();
    glAttachShader(program, vertexShader);
    glAttachShader(program, fragmentShader);
    glLinkProgram(program);
    
    // Check linking
    int success;
    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if (!success) {
        char infoLog[512];
        glGetProgramInfoLog(program, 512, nullptr, infoLog);
        std::cerr << "VoxelRenderer: Program linking failed: " << infoLog << std::endl;
        glDeleteProgram(program);
        program = 0;
    }
    
    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);
    
    return program;
}

void VoxelRenderer::resetStatistics() {
    m_chunksRendered = 0;
    m_trianglesRendered = 0;
}

void VoxelRenderer::printChunkInfo(const RenderedChunk& chunk) const {
    std::cout << "RenderedChunk - Generated: " << chunk.isGenerated 
              << ", Indices: " << chunk.indexCount
              << ", VAO: " << chunk.VAO << std::endl;
}

void VoxelRenderer::printRenderStatistics() const {
    std::cout << "VoxelRenderer Statistics - Chunks: " << m_chunksRendered 
              << ", Triangles: " << m_trianglesRendered << std::endl;
}

} // namespace Rendering
} // namespace Engine
