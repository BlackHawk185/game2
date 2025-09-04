// Core/GameClient.cpp - Client implementation
#include "pch.h"
#include "GameClient.h"
#include "../Network/NetworkManager.h"
#include "../World/Chunk.h"
#include "../World/MeshGenerator.h"
#include "../Rendering/VoxelRenderer.h"
#include "../Input/Camera.h"
#include <GLFW/glfw3.h>

namespace Engine {
namespace Core {

GameClient::GameClient() 
    : m_isRunning(false)
    , m_isConnected(false)
    , m_window(nullptr)
    , m_renderer(nullptr)
    , m_camera(nullptr)
    , m_meshGenerator(nullptr)
    , m_inputManager(nullptr)
    , m_networkManager(nullptr)
{
    m_localPlayer.position = Vec3(0, 0, 0);
    m_localPlayer.velocity = Vec3(0, 0, 0);
    m_localPlayer.rotation = Vec3(0, 0, 0);
    m_localPlayer.playerID = 0;
}

GameClient::~GameClient() {
    shutdown();
}

bool GameClient::initialize() {
    std::cout << "GameClient: Initializing..." << std::endl;
    
    // Initialize Window and OpenGL context
    if (!m_window) {
        m_window = new Window();
        if (!m_window->initialize(1280, 720, "MMORPG Engine - Client")) {
            std::cerr << "GameClient: Failed to initialize Window" << std::endl;
            delete m_window;
            m_window = nullptr;
            return false;
        }
        std::cout << "GameClient: Window and OpenGL context initialized" << std::endl;
    }
    
    // Initialize NetworkManager
    if (!m_networkManager) {
        m_networkManager = new NetworkManager();
        // Note: In integrated mode, ENet is already initialized by the server
        // so we don't call initializeNetworking() here
        std::cout << "GameClient: NetworkManager initialized" << std::endl;
    }
    
    // Initialize MeshGenerator
    if (!m_meshGenerator) {
        m_meshGenerator = new Engine::World::MeshGenerator();
        std::cout << "GameClient: MeshGenerator initialized" << std::endl;
    }

    // Initialize VoxelRenderer(now that we have OpenGL context)
    if (!m_renderer) {
        m_renderer = new Engine::Rendering::VoxelRenderer();
        if (!m_renderer->initialize()) {
            std::cerr << "GameClient: Failed to initialize VoxelRenderer" << std::endl;
            delete m_renderer;
            m_renderer = nullptr;
            return false;
        }
        std::cout << "GameClient: VoxelRenderer initialized" << std::endl;
    }

    // Initialize camera system
    if (!m_camera) {
        m_camera = new Camera();
        std::cout << "GameClient: Camera initialized" << std::endl;
    }
    
    // TODO: Initialize input management
    
    m_isRunning = true;
    std::cout << "GameClient: Initialization complete" << std::endl;
    return true;
}

bool GameClient::initializeStandalone() {
    std::cout << "GameClient: Initializing in standalone mode..." << std::endl;
    
    // Initialize window and OpenGL context
    if (!m_window) {
        m_window = new Window();
        if (!m_window->initialize(1280, 720, "MMORPG Engine - Standalone Mode")) {
            std::cerr << "GameClient: Failed to initialize window" << std::endl;
            return false;
        }
        std::cout << "GameClient: Window and OpenGL context initialized" << std::endl;
    }
    
    // Skip NetworkManager initialization for standalone mode
    std::cout << "GameClient: Skipping NetworkManager for standalone mode" << std::endl;
    
    // Initialize MeshGenerator
    if (!m_meshGenerator) {
        m_meshGenerator = new Engine::World::MeshGenerator();
        std::cout << "GameClient: MeshGenerator initialized" << std::endl;
    }

    // Initialize VoxelRenderer (now that we have OpenGL context)
    if (!m_renderer) {
        m_renderer = new Engine::Rendering::VoxelRenderer();
        if (!m_renderer->initialize()) {
            std::cerr << "GameClient: Failed to initialize VoxelRenderer" << std::endl;
            delete m_renderer;
            m_renderer = nullptr;
            return false;
        }
        std::cout << "GameClient: VoxelRenderer initialized" << std::endl;
    }

    // Generate multiple test chunks to create a small world
    generateStandaloneWorld();
    
    // Initialize camera system
    if (!m_camera) {
        m_camera = new Camera();
        std::cout << "GameClient: Camera initialized" << std::endl;
    }
    
    // TODO: Initialize input management
    
    m_isRunning = true;
    std::cout << "GameClient: Standalone initialization complete" << std::endl;
    return true;
}

void GameClient::shutdown() {
    if (!m_isRunning) return;
    
    std::cout << "GameClient: Shutting down..." << std::endl;
    
    // Cleanup rendered chunks
    for (auto& pair : m_renderedChunks) {
        // OpenGL cleanup is handled by VoxelRenderer
    }
    m_renderedChunks.clear();
    
    // Cleanup chunks
    for (auto& pair : m_chunks) {
        delete pair.second;
    }
    m_chunks.clear();
    
    // Cleanup rendering system
    if (m_renderer) {
        m_renderer->shutdown();
        delete m_renderer;
        m_renderer = nullptr;
    }
    
    // Cleanup mesh generator
    if (m_meshGenerator) {
        delete m_meshGenerator;
        m_meshGenerator = nullptr;
    }
    
    // Cleanup network connections
    if (m_networkManager) {
        m_networkManager->leaveServer();  // Use leaveServer instead of disconnect
        // Note: Don't call shutdownNetworking() in integrated mode since server handles ENet
        delete m_networkManager;
        m_networkManager = nullptr;
    }
    
    // TODO: Cleanup input systems
    
    m_isRunning = false;
    std::cout << "GameClient: Shutdown complete" << std::endl;
}

void GameClient::update(float deltaTime) {
    if (!m_isRunning) return;
    
    // Update window and handle events
    if (m_window) {
        m_window->update();
        // Check if window should close
        if (m_window->shouldClose()) {
            m_isRunning = false;
            return;
        }
    }
    
    // Update network manager and check connection status
    if (m_networkManager) {
        m_networkManager->update();
        
        // Check if we just connected
        if (!m_isConnected && m_networkManager->isConnectedToServer()) {
            m_isConnected = true;
            std::cout << "GameClient: Connected to server successfully" << std::endl;
        }
        
        // Check if we got disconnected
        if (m_isConnected && !m_networkManager->isConnectedToServer()) {
            m_isConnected = false;
            std::cout << "GameClient: Disconnected from server" << std::endl;
        }
    }
    
    // Process input
    processInput(deltaTime);
    
    // Update camera
    if (m_camera) {
        m_camera->processInput(m_window->getHandle(), deltaTime);
        m_camera->update(deltaTime);
    }
    
    // Handle network messages
    handleNetworkMessages();
    
    // Update rendering data
    updateRendering();
    
    // TODO: Update client-side prediction
}

void GameClient::render() {
    if (!m_isRunning || !m_window) return;
    
    // Clear the screen
    if (m_renderer) {
        // Begin frame
        m_renderer->beginFrame();
        
        // Set camera matrices from the camera
        if (m_camera) {
            float viewMatrix[16];
            m_camera->getViewMatrix(viewMatrix);
            
            float projMatrix[16];
            int width, height;
            m_window->getSize(width, height);
            float aspect = static_cast<float>(width) / static_cast<float>(height);
            m_camera->getProjectionMatrix(projMatrix, aspect);
            
            m_renderer->setViewMatrix(viewMatrix);
            m_renderer->setProjectionMatrix(projMatrix);
        } else {
            // Fallback identity matrices if no camera
            float viewMatrix[16] = {
                1, 0, 0, 0,
                0, 1, 0, 0,
                0, 0, 1, 0,
                0, 0, -5, 1  // Move camera back 5 units
            };
            
            float projMatrix[16] = {
                1, 0, 0, 0,
                0, 1, 0, 0,
                0, 0, -1, 0,
                0, 0, 0, 1
            };
            
            m_renderer->setViewMatrix(viewMatrix);
            m_renderer->setProjectionMatrix(projMatrix);
        }
        
        // Render all chunks
        if (m_renderedChunks.empty()) {
            // Only log this occasionally to avoid spam
            static int emptyRenderCount = 0;
            emptyRenderCount++;
            if (emptyRenderCount % 300 == 1) { // Log every 5 seconds at 60fps
                std::cout << "GameClient: No chunks to render (" << emptyRenderCount << " empty renders)" << std::endl;
            }
        } else {
            static bool loggedRenderStart = false;
            if (!loggedRenderStart) {
                std::cout << "GameClient: Rendering " << m_renderedChunks.size() << " chunks" << std::endl;
                loggedRenderStart = true;
            }
        }
        
        for (const auto& pair : m_renderedChunks) {
            m_renderer->renderChunk(pair.second, m_renderer->getDirtTexture());
        }
        
        // End frame
        m_renderer->endFrame();
        
        // Print statistics occasionally (every 5 seconds)
        static int frameCount = 0;
        frameCount++;
        if (m_renderedChunks.size() > 0 && frameCount % 300 == 0) { // Every 5 seconds at 60 FPS
            m_renderer->printRenderStatistics();
        }
    } else {
        // Just print statistics about what we would render
        if (!m_renderedChunks.empty()) {
            std::cout << "GameClient: Would render " << m_renderedChunks.size() << " chunks" << std::endl;
        }
    }
    
    // Buffer swapping is handled by Window::update()
}

void GameClient::processInput(float deltaTime) {
    if (m_window) {
        // Check for ESC key to close the window
        if (m_window->isKeyPressed(GLFW_KEY_ESCAPE)) {
            m_window->setShouldClose(true);
        }
    }
    
    // TODO: Handle keyboard/mouse input
    // TODO: Update player movement intent
    // TODO: Send input to server
}

void GameClient::handleNetworkMessages() {
    // TODO: Receive world updates from server
    // TODO: Apply server state corrections
    // TODO: Handle player state updates
}

void GameClient::generateTestChunk() {
    std::cout << "GameClient: Generating test chunk for rendering verification..." << std::endl;
    
    if (!m_meshGenerator) {
        std::cout << "GameClient: MeshGenerator not available for test chunk" << std::endl;
        return;
    }
    
    // Create a test chunk at coordinates (0,0,0)
    auto testChunk = std::make_unique<Engine::World::Chunk>(0, 0, 0);
    
    // Create a simple pattern - a platform at y=8 and some blocks
    const int CHUNK_SIZE = 16;
    for (int x = 0; x < CHUNK_SIZE; ++x) {
        for (int z = 0; z < CHUNK_SIZE; ++z) {
            // Platform at y=8
            testChunk->setBlock(x, 8, z, 1); // Dirt block
            
            // Add some variety - pillars at corners  
            if ((x == 2 || x == 13) && (z == 2 || z == 13)) {
                for (int y = 9; y <= 12; ++y) {
                    testChunk->setBlock(x, y, z, 1);
                }
            }
        }
    }
    
    // Mark the chunk as generated and clean
    testChunk->markGenerated();
    testChunk->markClean();
    
    // Generate mesh for the test chunk
    Engine::World::PhysicsMesh physicsMesh;
    Engine::World::RenderMesh renderMesh;
    
    m_meshGenerator->generateMesh(*testChunk, true, physicsMesh, renderMesh);
    
    if (!renderMesh.vertices.empty()) {
        std::cout << "GameClient: Test chunk mesh generated successfully (" 
                  << renderMesh.vertices.size()/6 << " vertices)" << std::endl;
        
        // Convert to OpenGL renderable format using VoxelRenderer
        if (m_renderer) {
            Engine::Rendering::RenderedChunk renderedChunk;
            m_renderer->generateChunkRenderData(renderMesh, renderedChunk);
            
            // Store with a simple key (combine x,z coordinates into uint64_t)
            uint64_t chunkKey = (uint64_t(0) << 32) | uint64_t(0); // x=0, z=0
            m_renderedChunks[chunkKey] = renderedChunk;
            
            std::cout << "GameClient: Test chunk added to render queue" << std::endl;
        } else {
            std::cout << "GameClient: VoxelRenderer not available for test chunk conversion" << std::endl;
        }
    } else {
        std::cout << "GameClient: Test chunk mesh generation failed!" << std::endl;
    }
}

void GameClient::generateStandaloneWorld() {
    std::cout << "GameClient: Generating standalone world..." << std::endl;
    
    if (!m_meshGenerator) {
        std::cout << "GameClient: MeshGenerator not available for standalone world" << std::endl;
        return;
    }
    
    // Generate a 3x3 grid of chunks centered around origin
    const int WORLD_SIZE = 3; // 3x3 = 9 chunks
    const int OFFSET = WORLD_SIZE / 2; // Center offset
    
    for (int cx = -OFFSET; cx <= OFFSET; ++cx) {
        for (int cz = -OFFSET; cz <= OFFSET; ++cz) {
            // Create chunk at world coordinates
            auto chunk = std::make_unique<Engine::World::Chunk>(cx, 0, cz);
            
            // Generate terrain for this chunk
            generateChunkTerrain(*chunk, cx, cz);
            
            // Mark chunk as generated and clean
            chunk->markGenerated();
            chunk->markClean();
            
            // Generate mesh for the chunk
            Engine::World::PhysicsMesh physicsMesh;
            Engine::World::RenderMesh renderMesh;
            
            m_meshGenerator->generateMesh(*chunk, true, physicsMesh, renderMesh);
            
            if (!renderMesh.vertices.empty()) {
                std::cout << "GameClient: Generated chunk (" << cx << ", " << cz 
                          << ") with " << renderMesh.vertices.size()/6 << " vertices" << std::endl;
                
                // Convert to OpenGL renderable format
                if (m_renderer) {
                    Engine::Rendering::RenderedChunk renderedChunk;
                    m_renderer->generateChunkRenderData(renderMesh, renderedChunk);
                    
                    // Store with chunk coordinates as key
                    uint64_t chunkKey = (uint64_t(cx + 1000) << 32) | uint64_t(cz + 1000); // Offset to avoid negative coords
                    m_renderedChunks[chunkKey] = renderedChunk;
                }
            }
        }
    }
    
    std::cout << "GameClient: Standalone world generation complete - " 
              << m_renderedChunks.size() << " chunks rendered" << std::endl;
}

void GameClient::generateChunkTerrain(Engine::World::Chunk& chunk, int chunkX, int chunkZ) {
    const int CHUNK_SIZE = 16;
    
    // Simple terrain generation - create rolling hills
    for (int x = 0; x < CHUNK_SIZE; ++x) {
        for (int z = 0; z < CHUNK_SIZE; ++z) {
            // Calculate world coordinates
            int worldX = chunkX * CHUNK_SIZE + x;
            int worldZ = chunkZ * CHUNK_SIZE + z;
            
            // Simple height map using sine waves for variety
            float height = 8.0f + 3.0f * sin(worldX * 0.1f) + 2.0f * cos(worldZ * 0.15f);
            int terrainHeight = static_cast<int>(height);
            
            // Fill from bottom up to terrain height
            for (int y = 0; y <= terrainHeight && y < CHUNK_SIZE; ++y) {
                if (y == terrainHeight) {
                    chunk.setBlock(x, y, z, 1); // Grass/dirt on top
                } else if (y >= terrainHeight - 2) {
                    chunk.setBlock(x, y, z, 1); // Dirt layer
                } else {
                    chunk.setBlock(x, y, z, 1); // Stone below
                }
            }
            
            // Add some random features
            if ((worldX + worldZ) % 7 == 0 && terrainHeight < 14) {
                // Occasional tree/pillar
                for (int y = terrainHeight + 1; y <= terrainHeight + 3 && y < CHUNK_SIZE; ++y) {
                    chunk.setBlock(x, y, z, 1);
                }
            }
        }
    }
}

bool GameClient::connectToServer(const std::string& address, uint16_t port) {
    std::cout << "GameClient: Connecting to server " << address << ":" << port << std::endl;
    
    if (m_networkManager) {
        if (m_networkManager->joinServer(address, port)) {  // Use joinServer instead of connectToServer
            m_isConnected = true;
            std::cout << "GameClient: Connected to server successfully" << std::endl;
            return true;
        } else {
            std::cerr << "GameClient: Failed to connect to server" << std::endl;
            return false;
        }
    }
    
    return false;
}

void GameClient::startConnecting(const std::string& address, uint16_t port) {
    std::cout << "GameClient: Starting connection to server " << address << ":" << port << std::endl;
    
    if (m_networkManager) {
        if (m_networkManager->joinServer(address, port)) {
            m_isConnected = true;
            std::cout << "GameClient: Connected to server successfully" << std::endl;
        } else {
            std::cerr << "GameClient: Failed to connect to server" << std::endl;
        }
    }
}

void GameClient::handleChunkDataReceived(const std::vector<uint8_t>& chunkData) {
    std::cout << "GameClient: Received chunk data (" << chunkData.size() << " bytes)" << std::endl;
    
    // Create a temporary chunk to deserialize the data
    Engine::World::Chunk tempChunk(0, 0, 0);
    tempChunk.deserialize(chunkData);
    
    // Get chunk coordinates
    int chunkX = tempChunk.getChunkX();
    int chunkY = tempChunk.getChunkY();
    int chunkZ = tempChunk.getChunkZ();
    
    std::cout << "GameClient: Processing chunk at (" << chunkX << ", " << chunkY << ", " << chunkZ << ")" << std::endl;
    
    // Store chunk in client-side world representation
    uint64_t chunkKey = getChunkKey(chunkX, chunkY, chunkZ);
    
    // If we already have this chunk, delete the old one
    auto it = m_chunks.find(chunkKey);
    if (it != m_chunks.end()) {
        delete it->second;
    }
    
    // Store the new chunk
    Engine::World::Chunk* newChunk = new Engine::World::Chunk(chunkX, chunkY, chunkZ);
    newChunk->deserialize(chunkData);
    m_chunks[chunkKey] = newChunk;
    
    // Generate mesh for rendering
    generateChunkMesh(newChunk);
    
    std::cout << "GameClient: Chunk data processed and mesh generated for chunk (" 
              << chunkX << ", " << chunkY << ", " << chunkZ << ")" << std::endl;
}

void GameClient::updateRendering() {
    // This method is called each frame to update any dynamic rendering data
    // For now, chunks are only updated when received from server
}

void GameClient::generateChunkMesh(Engine::World::Chunk* chunk) {
    if (!chunk || !m_meshGenerator) {
        return;
    }
    
    std::cout << "GameClient: Generating mesh for chunk (" 
              << chunk->getChunkX() << ", " << chunk->getChunkY() << ", " << chunk->getChunkZ() << ")" << std::endl;
    
    // Generate mesh data
    Engine::World::PhysicsMesh physicsMesh;
    Engine::World::RenderMesh renderMesh;
    
    m_meshGenerator->generateMesh(*chunk, true, physicsMesh, renderMesh);
    
    // Print mesh statistics (since we can't render yet)
    std::cout << "GameClient: Mesh generation complete for chunk - " 
              << "Physics: " << physicsMesh.getVertexCount() << " vertices, " 
              << physicsMesh.getTriangleCount() << " triangles | "
              << "Render: " << renderMesh.getVertexCount() << " vertices, " 
              << renderMesh.getTriangleCount() << " triangles" << std::endl;
    
    // Skip OpenGL data generation if no renderer
    if (!m_renderer) {
        std::cout << "GameClient: Skipping OpenGL data generation (no renderer)" << std::endl;
        return;
    }
    
    // Create rendered chunk
    uint64_t chunkKey = getChunkKey(chunk->getChunkX(), chunk->getChunkY(), chunk->getChunkZ());
    Engine::Rendering::RenderedChunk& renderedChunk = m_renderedChunks[chunkKey];
    
    // Generate OpenGL data
    m_renderer->generateChunkRenderData(renderMesh, renderedChunk);
    renderedChunk.chunkPosition = Vec3(chunk->getChunkX(), chunk->getChunkY(), chunk->getChunkZ());
}

uint64_t GameClient::getChunkKey(int x, int y, int z) const {
    // Pack chunk coordinates into a 64-bit key
    // Assuming coordinates fit in 21 bits each (supports -1M to +1M range)
    uint64_t ux = static_cast<uint64_t>(x + 1048576) & 0x1FFFFF; // 21 bits
    uint64_t uy = static_cast<uint64_t>(y + 1048576) & 0x1FFFFF; // 21 bits  
    uint64_t uz = static_cast<uint64_t>(z + 1048576) & 0x1FFFFF; // 21 bits
    return (ux << 42) | (uy << 21) | uz;
}

} // namespace Core
} // namespace Engine
