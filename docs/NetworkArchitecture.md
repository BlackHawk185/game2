// NetworkArchitecture.md - MMORPG Client-Server Design
# MMORPG Client-Server Architecture Plan

## Current State: Monolithic Engine
- Single process handles everything
- Window close = world destruction
- All state in local memory
- Direct function calls between systems

## Target State: Distributed MMORPG

### 1. Server Process (Headless)
```cpp
class GameServer {
    World world;                    // Authoritative world state
    Physics physics;                // Server-side physics simulation
    NetworkManager network;         // Handle client connections
    PlayerManager players;          // Manage connected players
    ChunkManager chunks;            // Persistent chunk data
    
    void tick(float deltaTime);     // 60Hz game loop
    void broadcastStateUpdates();   // Send to all clients
    void handleClientCommands();    // Process player inputs
};
```

### 2. Client Process (Rendering)
```cpp
class GameClient {
    Renderer renderer;              // Graphics + UI
    NetworkClient network;          // Server connection
    InputManager input;             // Capture user input
    PredictionSystem prediction;    // Client-side prediction
    
    void render();                  // Variable framerate
    void sendInputToServer();       // Forward player actions
    void applyServerUpdates();      // Sync with authoritative state
};
```

### 3. Shared Library (Common)
```cpp
// Shared between client and server
struct NetworkProtocol {
    enum MessageType {
        PLAYER_INPUT,       // Client → Server
        WORLD_UPDATE,       // Server → Client
        CHUNK_DATA,         // Server → Client
        PLAYER_JOIN,        // Bidirectional
        PLAYER_LEAVE        // Bidirectional
    };
};

struct EntityID {
    uint64_t id;                    // Globally unique
    PlayerID owner;                 // Which client owns this
};
```

## Network Considerations Now

### A. Data Ownership Patterns
- **Server Authority**: Physics, world state, player health
- **Client Authority**: Rendering, UI, input prediction
- **Hybrid**: Movement (client predicts, server corrects)

### B. Threading + Networking
```cpp
class NetworkedJobSystem : public JobSystem {
    // Existing job types
    enum JobType { 
        CHUNK_MESHING, 
        PHYSICS_COOKING,
        
        // New network job types
        NETWORK_SEND,           // Serialize and send data
        NETWORK_RECEIVE,        // Deserialize incoming data
        STATE_RECONCILIATION    // Sync client prediction with server
    };
};
```

### C. State Synchronization
- **Snapshot System**: Full world state periodically
- **Delta Compression**: Only send what changed
- **Entity Component System**: Network-aware components

## Migration Path

### Phase 1: Process Separation (Next Step)
1. Split into GameServer.exe + GameClient.exe
2. Local IPC communication (shared memory/pipes)
3. Same machine, different processes

### Phase 2: Network Foundation
1. Replace IPC with TCP/UDP sockets
2. Add message serialization
3. Basic client-server handshake

### Phase 3: MMORPG Features
1. Multiple client support
2. Persistent world state
3. Player authentication

## Benefits of Early Network Design
- **Clean separation of concerns**
- **Easier debugging** (can test server without graphics)
- **Scalable architecture** (add clients without server changes)
- **Development workflow** (designers can modify world while clients reconnect)

## Implementation Priority
1. ✅ Threading foundation (DONE)
2. 🔄 Client-Server separation (THIS WEEK)  
3. 🔄 Local network protocol (NEXT)
4. 🔄 Multi-client support (FUTURE)
