// client_main.cpp - Rendering client executable
#include "GameClient.h"
#include "Network/SharedMemoryIPC.h"
#include <iostream>
#include <thread>

int main(int argc, char* argv[]) {
    std::cout << "🎮 === MMORPG Game Client Starting === 🎮\n" << std::endl;
    
    // Create client instance
    GameClient client;
    
    // Initialize client (graphics, input, etc.)
    if (!client.initialize()) {
        std::cerr << "❌ Failed to initialize game client!" << std::endl;
        std::cout << "💡 Make sure the game server is running first!" << std::endl;
        return -1;
    }
    
    std::cout << "✅ Game client initialized successfully!" << std::endl;
    std::cout << "🔗 Attempting to connect to game server..." << std::endl;
    
    // Connect to server
    if (!client.connectToServer()) {
        std::cerr << "❌ Failed to connect to game server!" << std::endl;
        std::cout << "💡 Make sure the server is running and accessible" << std::endl;
        return -1;
    }
    
    std::cout << "🌐 Connected to game server!" << std::endl;
    std::cout << "🎮 Client ready - starting render loop..." << std::endl;
    std::cout << "⌨️  Controls:" << std::endl;
    std::cout << "   WASD - Move camera" << std::endl;
    std::cout << "   Mouse - Look around (click and drag)" << std::endl;
    std::cout << "   ESC - Disconnect and exit\n" << std::endl;
    
    // Run the client (blocks until window closed)
    client.run();
    
    std::cout << "\n🏁 Game client shutting down..." << std::endl;
    
    // Final stats
    const auto& stats = client.getStats();
    std::cout << "📊 Final Client Stats:" << std::endl;
    std::cout << "   Average FPS: " << stats.averageFPS << std::endl;
    std::cout << "   Final Camera Position: (" 
              << stats.cameraPosition.x << ", " 
              << stats.cameraPosition.y << ", " 
              << stats.cameraPosition.z << ")" << std::endl;
    std::cout << "   Network Latency: " << stats.networkLatency << "ms" << std::endl;
    
    client.disconnectFromServer();
    std::cout << "👋 Client shutdown complete!" << std::endl;
    return 0;
}
