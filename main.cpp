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

void printHelp() {
    std::cout << "🎮 MMORPG Engine Prototype - Usage:" << std::endl;
    std::cout << "  Default (no args):     Integrated mode - local game with physics" << std::endl;
    std::cout << "  --enable-networking:   Integrated mode + allow external connections" << std::endl;
    std::cout << "  --server:              Server-only mode (headless)" << std::endl;
    std::cout << "  --client <address>:    Connect to remote server (experimental)" << std::endl;
    std::cout << "  --help:                Show this help" << std::endl;
    std::cout << std::endl;
    std::cout << "💡 Recommended: Use default mode or --enable-networking for multiplayer" << std::endl;
}

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
    INTEGRATED,    // Run both server and client in same process (default, recommended)
    SERVER_ONLY,   // Run headless server only (for dedicated servers)
    CLIENT_ONLY    // Connect to remote server (experimental, has physics/camera issues)
};

int main(int argc, char* argv[]) {
    // Check for help first (before any initialization)
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            printHelp();
            return 0;
        }
    }
    
    // Parse command line arguments
    RunMode runMode = RunMode::INTEGRATED;  // Default to working integrated mode
    std::string serverAddress = "localhost";
    uint16_t serverPort = 12345;  // Changed from 7777 to a higher port number
    bool enableNetworking = false;  // Allow external connections in integrated mode
    
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--server") == 0 || strcmp(argv[i], "SERVER_ONLY") == 0) {
            runMode = RunMode::SERVER_ONLY;
        } else if (strcmp(argv[i], "--enable-networking") == 0) {
            enableNetworking = true;  // Allow external connections to integrated mode
        } else if (strcmp(argv[i], "--client") == 0 && i + 1 < argc) {
            runMode = RunMode::CLIENT_ONLY;
            serverAddress = argv[i + 1];
            i++; // Skip next argument
            
            // Optional port argument
            if (i + 1 < argc && isdigit(argv[i + 1][0])) {
                serverPort = static_cast<uint16_t>(atoi(argv[i + 1]));
                i++;
            }
        } else if (strcmp(argv[i], "CLIENT_ONLY") == 0) {
            runMode = RunMode::CLIENT_ONLY;
            // Use default localhost
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
            if (enableNetworking) {
                std::cout << "🎮 Running in INTEGRATED mode (client + networked local server)" << std::endl;
                std::cout << "🌐 Server accepting external connections on port " << serverPort << std::endl;
            } else {
                std::cout << "🔗 Running in INTEGRATED mode (local server + client)" << std::endl;
            }
            
            // Create and initialize server (with networking if enabled)
            GameServer server;
            if (!server.initialize(60.0f, enableNetworking, serverPort)) {  // Use networking flag
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
            
            // Create and initialize server with networking enabled
            GameServer server;
            if (!server.initialize(60.0f, true, serverPort)) {  // Enable networking
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
            std::cout << "🖼️  Running in CLIENT-ONLY mode (experimental)" << std::endl;
            std::cout << "⚠️  WARNING: This mode has physics/camera issues!" << std::endl;
            std::cout << "💡 Recommended: Use default integrated mode instead" << std::endl;
            std::cout << "🔗 Target server: " << serverAddress << ":" << serverPort << std::endl;
            
            // Create and initialize client
            GameClient client;
            if (!client.initialize()) {
                std::cerr << "Failed to initialize game client!" << std::endl;
                return 1;
            }
            
            // Connect to remote server
            if (!client.connectToRemoteServer(serverAddress, serverPort)) {
                std::cerr << "Failed to connect to remote server!" << std::endl;
                return 1;
            }
            
            std::cout << "✅ Client-only mode initialized successfully" << std::endl;
            std::cout << "🎮 Controls: WASD+mouse to move, ESC to exit" << std::endl;
            std::cout << "📡 Connected to remote server - waiting for world data..." << std::endl;
            
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
            std::cout << "🔄 Shutting down client..." << std::endl;
            client.shutdown();
            
            break;
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
