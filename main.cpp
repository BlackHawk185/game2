// MMORPG Engine - Main Entry Point
#include <iostream>
#include <string>
#include <thread>
#include <chrono>
#include "engine/Core/GameServer.h"
#include "engine/Core/GameClient.h"
#include "engine/Network/NetworkManager.h"

using namespace Engine::Core;

enum class RunMode {
    INTEGRATED,   // Run both server and client in same process (default)
    SERVER_ONLY,  // Run headless server only
    CLIENT_ONLY,  // Connect to remote server
    STANDALONE    // Run client-only with local world generation (no networking)
};

int main(int argc, char* argv[])
{
    std::cout << "MMORPG Engine - Alpha Build" << std::endl;
    
    // Parse command line arguments
    RunMode mode = RunMode::INTEGRATED;
    std::string serverAddress = "localhost";
    uint16_t port = 7777;
    
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "--server") {
            mode = RunMode::SERVER_ONLY;
        } else if (arg == "--client") {
            mode = RunMode::CLIENT_ONLY;
            if (i + 1 < argc) {
                serverAddress = argv[++i];
            }
        } else if (arg == "--standalone") {
            mode = RunMode::STANDALONE;
        } else if (arg == "--port") {
            if (i + 1 < argc) {
                port = static_cast<uint16_t>(std::stoi(argv[++i]));
            }
        } else if (arg == "--help") {
            std::cout << "Usage: " << argv[0] << " [options]" << std::endl;
            std::cout << "Options:" << std::endl;
            std::cout << "  --server              Run as dedicated server" << std::endl;
            std::cout << "  --client <address>    Connect to remote server" << std::endl;
            std::cout << "  --standalone          Run client with local world (no networking)" << std::endl;
            std::cout << "  --port <port>         Set server port (default: 7777)" << std::endl;
            std::cout << "  --help                Show this help" << std::endl;
            return 0;
        }
    }
    
    // Initialize networking before creating server/client (skip for standalone mode)
    if (mode != RunMode::STANDALONE) {
        if (!NetworkManager::initializeNetworking()) {
            std::cerr << "Failed to initialize networking!" << std::endl;
            return 1;
        }
    }
    
    // Initialize systems based on run mode
    std::unique_ptr<GameServer> server;
    std::unique_ptr<GameClient> client;
    
    switch (mode) {
        case RunMode::STANDALONE:
            std::cout << "Starting in STANDALONE mode (client-only with local world)" << std::endl;
            client = std::make_unique<GameClient>();
            
            if (!client->initializeStandalone()) {
                std::cerr << "Failed to initialize standalone client!" << std::endl;
                return 1;
            }
            break;
    
        case RunMode::INTEGRATED:
            std::cout << "Starting in INTEGRATED mode (server + client)" << std::endl;
            server = std::make_unique<GameServer>();
            client = std::make_unique<GameClient>();
            
            if (!server->initialize(port)) {
                std::cerr << "Failed to initialize server!" << std::endl;
                return 1;
            }
            
            // Give the server a moment to start listening
            std::cout << "Waiting for server to fully initialize..." << std::endl;
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            
            if (!client->initialize()) {
                std::cerr << "Failed to initialize client!" << std::endl;
                return 1;
            }
            
            std::cout << "Starting integrated mode - will connect client in game loop..." << std::endl;
            break;
            
        case RunMode::SERVER_ONLY:
            std::cout << "Starting in SERVER_ONLY mode" << std::endl;
            server = std::make_unique<GameServer>();
            
            if (!server->initialize(port)) {
                std::cerr << "Failed to initialize server!" << std::endl;
                return 1;
            }
            break;
            
        case RunMode::CLIENT_ONLY:
            std::cout << "Starting in CLIENT_ONLY mode" << std::endl;
            client = std::make_unique<GameClient>();
            
            if (!client->initialize()) {
                std::cerr << "Failed to initialize client!" << std::endl;
                return 1;
            }
            
            if (!client->connectToServer(serverAddress, port)) {
                std::cerr << "Failed to connect to server!" << std::endl;
                return 1;
            }
            break;
    }
    
    // Main game loop
    std::cout << "Entering main game loop..." << std::endl;
    const float deltaTime = 1.0f / 60.0f; // 60 FPS target
    
    int frameCount = 0;
    bool clientConnectionAttempted = false;
    
    while ((server && server->isRunning()) || (client && client->isRunning())) {
        // Update server first (this processes network events including connection requests)
        if (server) {
            server->update(deltaTime);
        }
        
        // For integrated mode, attempt client connection after server has started processing
        if (mode == RunMode::INTEGRATED && client && !clientConnectionAttempted && frameCount > 60) {
            std::cout << "Attempting to connect client to local server..." << std::endl;
            
            // Use asynchronous connection for integrated mode
            if (client->connectToServer("127.0.0.1", port)) {
                std::cout << "Client connected successfully!" << std::endl;
            } else {
                std::cerr << "Failed to connect client to server!" << std::endl;
                break; // Exit the game loop
            }
            clientConnectionAttempted = true;
        }
        
        // Update and render client
        if (client) {
            client->update(deltaTime);
            client->render();
        }
        
        // Simple timing - TODO: Replace with proper frame timing
        std::this_thread::sleep_for(std::chrono::milliseconds(16)); // ~60 FPS
        
        frameCount++;
    }
    
    std::cout << "Exiting main game loop..." << std::endl;
    std::cout << "Shutting down..." << std::endl;
    
    // Shutdown networking (skip for standalone mode)
    if (mode != RunMode::STANDALONE) {
        NetworkManager::shutdownNetworking();
    }
    
    return 0;
}
