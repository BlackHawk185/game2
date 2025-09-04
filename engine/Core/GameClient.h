#pragma once

#include "../Network/NetworkManager.h"
#include "../Input/Camera.h"
#include "../Math/Vec3.h"
#include "../World/MeshGenerator.h"
#include "../Rendering/VoxelRenderer.h"
#include "Window.h"
#include <memory>
#include <unordered_map>

namespace Engine {
namespace Core {

// Forward declarations
class InputManager;

namespace World {
    class Chunk;
}

/**
 * GameClient - Presentation and input handling
 * Handles all rendering (OpenGL + Dear ImGui)
 * Processes user input and sends to server
 * Runs client-side physics for responsiveness
 * Receives world data from server
 */
class GameClient {
public:
    GameClient();
    ~GameClient();

    // Core lifecycle
    bool initialize();
    bool initializeStandalone();  // Initialize without networking for local world testing
    void update(float deltaTime);
    void render();
    void shutdown();

    // Client state
    bool isRunning() const { return m_isRunning; }
    bool isConnected() const { return m_isConnected; }
    
    // Connection management
    bool connectToServer(const std::string& serverAddress, uint16_t port = 7777);
    void startConnecting(const std::string& serverAddress, uint16_t port = 7777);
    void disconnect();
    
    // World data handling
    void handleChunkDataReceived(const std::vector<uint8_t>& chunkData);
    
    // TEMPORARY: Test rendering without server
    void generateTestChunk();
    void generateStandaloneWorld();  // Generate multiple chunks for standalone testing
    void generateChunkTerrain(Engine::World::Chunk& chunk, int chunkX, int chunkZ);  // Generate terrain for a single chunk

private:
    // Core client state
    bool m_isRunning;
    bool m_isConnected;
    
    // Rendering
    Window* m_window;                                     // GLFW window and OpenGL context
    Engine::Rendering::VoxelRenderer* m_renderer;        // Modern voxel renderer
    Camera* m_camera;                                     // Camera for view matrix
    
    // Mesh generation
    Engine::World::MeshGenerator* m_meshGenerator;   // Unified mesh generation
    
    // Input handling
    InputManager* m_inputManager;                     // Input processing
    
    // Networking
    NetworkManager* m_networkManager;                 // Network communication
    
    // Client-side world state (received from server)
    std::unordered_map<uint64_t, Engine::World::Chunk*> m_chunks;  // Received chunks (key = chunk coords)
    std::unordered_map<uint64_t, Engine::Rendering::RenderedChunk> m_renderedChunks; // OpenGL data for chunks
    
    // Client-side world state (received from server)
    struct ClientWorldState {
        // TODO: Received chunk data
        // TODO: Other player positions
        // TODO: World change notifications
    };
    ClientWorldState m_worldState;
    
    // Player state
    struct LocalPlayer {
        Vec3 position;
        Vec3 velocity;
        Vec3 rotation;
        uint32_t playerID;
    };
    LocalPlayer m_localPlayer;
    
    // Internal methods
    void handleNetworkMessages();
    void processInput(float deltaTime);
    void updateClientPrediction(float deltaTime);
    void sendInputToServer();
    void updateRendering();
    void generateChunkMesh(Engine::World::Chunk* chunk);
    uint64_t getChunkKey(int x, int y, int z) const;
    
    // Rendering methods
    void renderWorld();
    void renderUI();
    
    // World state management
    void applyServerWorldUpdate();
    void handleChunkDataReceived();
};

} // namespace Core
} // namespace Engine
