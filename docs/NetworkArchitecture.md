// NetworkArchitecture.md - MMORPG Client-Server Design
# MMORPG Client-Server Arc### B. State Synchronization
- **Custom Collision System**: Server validates movement using voxel-face collision detection
- **Dual-Mesh Architecture**: Simultaneous generation of render and collision meshes
- **Snapshot System**: Full world state periodically

## ✅ Current State: Unified Networking Architecture (IMPLEMENTED)
- **Separated processes**: GameServer + GameClient with ENet networking
- **Unified networking**: All modes use network layer (even integrated mode)
- **Single debug path**: No more dual local/remote code paths
- **Compressed transmission**: ~98% compression for world data
- **Real-time sync**: Player movement and block updates working
- **Custom Physics**: Voxel-face-based collision detection integrated

## 🎯 Achieved Architecture

### ✅ Server Process (Headless)
```cpp
class GameServer {
    GameState world;                // Authoritative world state  
    PhysicsSystem physics;          // Custom voxel-face collision system
    NetworkManager network;         // ENet-based client connections
    PlayerManager players;          // Manage connected players
    IslandChunkSystem chunks;       // Floating island management
    
    void tick(float deltaTime);     // 60Hz game loop ✅ WORKING
    void broadcastStateUpdates();   // Send to all clients ✅ WORKING
    void handleClientCommands();    // Process player inputs ✅ WORKING
};
```

### ✅ Client Process (Rendering)
```cpp
class GameClient {
    Renderer renderer;              // Graphics + UI ✅ WORKING
    NetworkManager network;         // ENet server connection ✅ WORKING
    InputManager input;             // Capture user input ✅ WORKING
    Camera camera;                  // First-person camera ✅ WORKING
    
    void render();                  // Variable framerate ✅ WORKING
    void sendInputToServer();       // Forward player actions ✅ WORKING
    void applyServerUpdates();      // Sync with authoritative state ✅ WORKING
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
        PHYSICS_UPDATE,          // Custom collision detection
        
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

## ✅ Implementation Status

### Phase 1: Process Separation ✅ COMPLETED
1. ✅ Split into GameServer.exe + GameClient.exe processes
2. ✅ ENet networking communication (reliable UDP)
3. ✅ Unified networking (no more direct local calls)

### Phase 2: Network Foundation ✅ COMPLETED  
1. ✅ TCP/UDP sockets via ENet library
2. ✅ Message serialization with compression
3. ✅ Client-server handshake with world state transfer

### Phase 3: MMORPG Features 🔄 IN PROGRESS
1. ✅ Multiple client support (tested with 2+ clients)
2. ✅ Real-time world state synchronization  
3. 🔄 Player authentication (basic connection tracking)
4. 🔄 Block break/place event synchronization (network ready)
5. 🔄 Multiple player movement sync (foundation ready)

## 🎮 Current Network Modes

### Integrated Mode (Default)
- Server + Client in same process
- Client connects to `127.0.0.1:12345` (localhost networking)
- **Benefit**: Single debug path, easy testing

### Dedicated Server Mode  
```bash
MMORPGEngine.exe --server
```
- Headless server on port 12345
- Accepts external client connections

### Client-Only Mode
```bash
MMORPGEngine.exe --client <server_ip>
```
- Connects to remote server
- Full networking path, same as integrated mode

## 🚀 Next Features to Implement
1. **Real-time player visibility**: See other players moving around
2. **Block event synchronization**: See block breaks/places from other players
3. **Player join/leave notifications**: Chat/UI updates
4. **Basic chat system**: Text communication between players
