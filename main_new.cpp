// main_new.cpp - Modular MMORPG Engine with Client-Server Architecture
// This demonstrates the new separated architecture where:
// - GameServer runs the authoritative simulation (can be headless)
// - GameClient handles rendering and input
// - Both can run in the same process (integrated mode) or separately
#include <cctype>
#include <chrono>
#include <cstring>
#include <iostream>
#include <thread>

#include "engine/Core/GameClient.h"
#include "engine/Core/GameServer.h"
#include "engine/Threading/JobSystem.h"
#include "engine/Time/TimeEffects.h"
#include "engine/Time/TimeManager.h"

// Global systems (external declarations - defined in engine library)
extern JobSystem g_jobSystem;
extern TimeManager* g_timeManager;
extern TimeEffects* g_timeEffects;

enum class RunMode
{
    INTEGRATED,   // Run both server and client in same process (default)
    SERVER_ONLY,  // Run headless server only
    CLIENT_ONLY   // Connect to remote server (not implemented yet)
};

int main(int argc, char* argv[])
{
    // Parse command line arguments
    RunMode runMode = RunMode::INTEGRATED;
    std::string serverAddress = "localhost";
    uint16_t serverPort = 12345;  // Changed from 7777 to a higher port number

    for (int i = 1; i < argc; i++)
    {
        if (strcmp(argv[i], "--server") == 0 || strcmp(argv[i], "SERVER_ONLY") == 0)
        {
            runMode = RunMode::SERVER_ONLY;
        }
        else if (strcmp(argv[i], "--client") == 0 && i + 1 < argc)
        {
            runMode = RunMode::CLIENT_ONLY;
            serverAddress = argv[i + 1];
            i++;  // Skip next argument

            // Optional port argument
            if (i + 1 < argc && isdigit(argv[i + 1][0]))
            {
                serverPort = static_cast<uint16_t>(atoi(argv[i + 1]));
                i++;
            }
        }
        else if (strcmp(argv[i], "CLIENT_ONLY") == 0)
        {
            runMode = RunMode::CLIENT_ONLY;
            // Use default localhost
        }
    }

    // Removed verbose debug output

    // Initialize global systems
    if (!g_jobSystem.initialize())
    {
        std::cerr << "Failed to initialize job system!" << std::endl;
        return 1;
    }

    g_timeManager = new TimeManager();
    g_timeEffects = new TimeEffects();

    switch (runMode)
    {
        case RunMode::INTEGRATED:
        {
            // Removed verbose debug output

            // Create and initialize server (without networking for integrated mode)
            GameServer server;
            if (!server.initialize(60.0f, false))
            {  // No networking in integrated mode
                std::cerr << "Failed to initialize game server!" << std::endl;
                return 1;
            }

            // Start server in background thread
            server.runAsync();

            // Give server a moment to start up
            std::this_thread::sleep_for(std::chrono::milliseconds(100));

            // Create and initialize client
            GameClient client;
            if (!client.initialize())
            {
                std::cerr << "Failed to initialize game client!" << std::endl;
                server.stop();
                return 1;
            }

            // Connect client to server's game state
            if (!client.connectToGameState(server.getGameState()))
            {
                std::cerr << "Failed to connect client to server!" << std::endl;
                server.stop();
                return 1;
            }

            // Removed verbose debug output

            // Main client loop
            auto lastTime = std::chrono::high_resolution_clock::now();
            while (true)
            {
                auto currentTime = std::chrono::high_resolution_clock::now();
                float deltaTime = std::chrono::duration<float>(currentTime - lastTime).count();
                lastTime = currentTime;

                // Clamp delta time
                if (deltaTime > 0.05f)
                    deltaTime = 0.05f;

                // Update client
                if (!client.update(deltaTime))
                {
                    break;  // Client wants to exit
                }

                // Update time effects
                if (g_timeEffects)
                {
                    g_timeEffects->update(deltaTime);
                }
            }

            // Shutdown
            // Removed verbose debug output
            client.shutdown();
            server.stop();

            break;
        }

        case RunMode::SERVER_ONLY:
        {
            // Removed verbose debug output

            // Create and initialize server with networking enabled
            GameServer server;
            if (!server.initialize(60.0f, true, serverPort))
            {  // Enable networking
                std::cerr << "Failed to initialize game server!" << std::endl;
                return 1;
            }

            // Removed verbose debug output

            // Run server (blocks until stopped)
            server.run();

            // Removed verbose debug output
            break;
        }

        case RunMode::CLIENT_ONLY:
        {
            // Removed verbose debug output

            // Create and initialize client
            GameClient client;
            if (!client.initialize())
            {
                std::cerr << "Failed to initialize game client!" << std::endl;
                return 1;
            }

            // Connect to remote server
            if (!client.connectToRemoteServer(serverAddress, serverPort))
            {
                std::cerr << "Failed to connect to remote server!" << std::endl;
                return 1;
            }

            // Removed verbose debug output

            // Main client loop
            auto lastTime = std::chrono::high_resolution_clock::now();
            while (true)
            {
                auto currentTime = std::chrono::high_resolution_clock::now();
                float deltaTime = std::chrono::duration<float>(currentTime - lastTime).count();
                lastTime = currentTime;

                // Clamp delta time
                if (deltaTime > 0.05f)
                    deltaTime = 0.05f;

                // Update client
                if (!client.update(deltaTime))
                {
                    break;  // Client wants to exit
                }

                // Update time effects
                if (g_timeEffects)
                {
                    g_timeEffects->update(deltaTime);
                }
            }

            // Shutdown
            // Removed verbose debug output
            client.shutdown();

            break;
        }
    }

    // Cleanup global systems
    // Removed verbose debug output

    delete g_timeEffects;
    delete g_timeManager;
    g_timeEffects = nullptr;
    g_timeManager = nullptr;

    g_jobSystem.shutdown();

    // Removed verbose debug output
    return 0;
}
