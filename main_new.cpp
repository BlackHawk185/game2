// main_new.cpp - Modular MMORPG Engine with Client-Server Architecture
// This demonstrates the new separated architecture where:
// - GameServer runs the authoritative simulation (can be headless)
// - GameClient handles rendering and input
// - Both can run in the same process (integrated mode) or separately
#include <iostream>
#include <chrono>
#include <thread>
#include <cstring>
#include <cctype>

#include "engine/Core/GameServer.h"
#include "engine/Core/GameClient.h"
#include "engine/Threading/JobSystem.h"
#include "engine/Time/TimeManager.h"
#include "engine/Time/TimeEffects.h"

// Global systems (external declarations - defined in engine library)
extern JobSystem g_jobSystem;
extern TimeManager* g_timeManager;
extern TimeEffects* g_timeEffects;

enum class RunMode {
    INTEGRATED,    // Run both server and client in same process (default)
    SERVER_ONLY,   // Run headless server only
    CLIENT_ONLY    // Connect to remote server (not implemented yet)
};

int main(int argc, char* argv[]) {
    // Parse command line arguments
    RunMode runMode = RunMode::INTEGRATED;
    std::string serverAddress = "localhost";
    uint16_t serverPort = 7777;
    
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--server") == 0) {
            runMode = RunMode::SERVER_ONLY;
        } else if (strcmp(argv[i], "--client") == 0 && i + 1 < argc) {
            runMode = RunMode::CLIENT_ONLY;
            serverAddress = argv[i + 1];
            i++; // Skip next argument
            
            // Optional port argument
            if (i + 1 < argc && isdigit(argv[i + 1][0])) {
                serverPort = static_cast<uint16_t>(atoi(argv[i + 1]));
                i++;
            }
        }
    }
    
    std::cout << "🏝️ MMORPG Engine - Modular Architecture" << std::endl;
    std::cout << "=========================================" << std::endl;
    
    // Initialize global systems
    if (!g_jobSystem.initialize()) {
        std::cerr << "Failed to initialize job system!" << std::endl;
        return 1;
    }
    
    g_timeManager = new TimeManager();
    g_timeEffects = new TimeEffects();
    
    switch (runMode) {
        case RunMode::INTEGRATED: {
            std::cout << "🔗 Running in INTEGRATED mode (local server + client)" << std::endl;
            
            // Create and initialize server
            GameServer server;
            if (!server.initialize(60.0f)) {
                std::cerr << "Failed to initialize game server!" << std::endl;
                return 1;
            }
            
            // Start server in background thread
            server.runAsync();
            
            // Give server a moment to start up
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            
            // Create and initialize client
            GameClient client;
            if (!client.initialize()) {
                std::cerr << "Failed to initialize game client!" << std::endl;
                server.stop();
                return 1;
            }
            
            // Connect client to server's game state
            if (!client.connectToGameState(server.getGameState())) {
                std::cerr << "Failed to connect client to server!" << std::endl;
                server.stop();
                return 1;
            }
            
            std::cout << "✅ Integrated mode initialized successfully" << std::endl;
            std::cout << "🎮 Controls: WASD+mouse to move, SPACE to jump, 1-5/0/T for time effects, ESC to exit" << std::endl;
            
            // Main client loop
            auto lastTime = std::chrono::high_resolution_clock::now();
            while (true) {
                auto currentTime = std::chrono::high_resolution_clock::now();
                float deltaTime = std::chrono::duration<float>(currentTime - lastTime).count();
                lastTime = currentTime;
                
                // Clamp delta time
                if (deltaTime > 0.05f) deltaTime = 0.05f;
                
                // Update client
                if (!client.update(deltaTime)) {
                    break; // Client wants to exit
                }
                
                // Update time effects
                if (g_timeEffects) {
                    g_timeEffects->update(deltaTime);
                }
            }
            
            // Shutdown
            std::cout << "🔄 Shutting down integrated mode..." << std::endl;
            client.shutdown();
            server.stop();
            
            break;
        }
        
        case RunMode::SERVER_ONLY: {
            std::cout << "🖥️  Running in SERVER-ONLY mode (headless)" << std::endl;
            
            // Create and initialize server
            GameServer server;
            if (!server.initialize(60.0f)) {
                std::cerr << "Failed to initialize game server!" << std::endl;
                return 1;
            }
            
            std::cout << "✅ Server initialized successfully" << std::endl;
            std::cout << "🚀 Starting server simulation (press Ctrl+C to stop)..." << std::endl;
            
            // Run server (blocks until stopped)
            server.run();
            
            std::cout << "🔄 Server stopped" << std::endl;
            break;
        }
        
        case RunMode::CLIENT_ONLY: {
            std::cout << "🖼️  Running in CLIENT-ONLY mode" << std::endl;
            std::cout << "🔗 Target server: " << serverAddress << ":" << serverPort << std::endl;
            
            // TODO: Implement remote server connection
            std::cerr << "❌ Remote server connection not implemented yet!" << std::endl;
            std::cerr << "   Use integrated mode for now: run without --client flag" << std::endl;
            return 1;
        }
    }
    
    // Cleanup global systems
    std::cout << "🧹 Cleaning up global systems..." << std::endl;
    
    delete g_timeEffects;
    delete g_timeManager;
    g_timeEffects = nullptr;
    g_timeManager = nullptr;
    
    g_jobSystem.shutdown();
    
    std::cout << "👋 MMORPG Engine shutdown complete" << std::endl;
    return 0;
}
