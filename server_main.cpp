// server_main.cpp - Headless game server executable
#include "GameServer.h"
#include "Network/SharedMemoryIPC.h"
#include <iostream>
#include <thread>
#include <signal.h>

// Global server instance for signal handling
GameServer* g_server = nullptr;

void signalHandler(int signal) {
    std::cout << "\n🛑 Received shutdown signal " << signal << std::endl;
    if (g_server) {
        g_server->shutdown();
    }
}

int main(int argc, char* argv[]) {
    std::cout << "🌍 === MMORPG Game Server Starting === 🌍\n" << std::endl;
    
    // Install signal handlers for graceful shutdown
    signal(SIGINT, signalHandler);   // Ctrl+C
    signal(SIGTERM, signalHandler);  // Termination request
    
    // Create server instance
    GameServer server;
    g_server = &server;
    
    // Initialize server
    if (!server.initialize()) {
        std::cerr << "❌ Failed to initialize game server!" << std::endl;
        return -1;
    }
    
    std::cout << "✅ Game server initialized successfully!" << std::endl;
    std::cout << "📊 Server will run at 60 TPS (ticks per second)" << std::endl;
    std::cout << "🔗 Waiting for client connections via shared memory..." << std::endl;
    std::cout << "🛑 Press Ctrl+C to shutdown gracefully\n" << std::endl;
    
    // Run the server (blocks until shutdown)
    server.run();
    
    std::cout << "\n🏁 Game server shutting down..." << std::endl;
    
    // Final stats
    const auto& stats = server.getStats();
    std::cout << "📊 Final Server Stats:" << std::endl;
    std::cout << "   Average TPS: " << stats.averageTPS << std::endl;
    std::cout << "   Total Clients: " << stats.activeClients << std::endl;
    std::cout << "   Total Entities: " << stats.totalEntities << std::endl;
    
    std::cout << "👋 Server shutdown complete!" << std::endl;
    return 0;
}
