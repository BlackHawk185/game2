# MMORPG Engine Prototype

A modular C++17 MMORPG engine prototype featuring custom voxel-based physics, unified networking architecture, and floating island worlds. Built with cross-platform compatibility and performance in mind.

## 🚀 Quick Start

### Prerequisites
- CMake (>=3.15) 
- C++17 compiler (Visual Studio 2019+, GCC 8+, Clang 9+)
- Git (for submodules)

### Build & Run
```bash
# Configure with CMake preset
cmake --preset default

# Build the project
cmake --build build --config Debug

# Run in different modes
./build/bin/MMORPGEngine.exe                    # Integrated mode (recommended)
./build/bin/MMORPGEngine.exe --server           # Dedicated server
./build/bin/MMORPGEngine.exe --client localhost # Connect to server
./build/bin/MMORPGEngine.exe --help             # Show all options
```

### VS Code Integration
Three launch configurations available:
1. **Default**: Integrated mode (local server + networked client)
2. **Connect to Server**: Client connecting to localhost:12345
3. **Dedicated Server**: Headless server mode

Recommended extensions:
- C/C++ (ms-vscode.cpptools)
- CMake Tools (ms-vscode.cmake-tools)
- CMake (twxs.cmake)

## 🏗️ Architecture Overview

### Core Design Principles
- **Modular Client-Server Architecture**: Separated GameServer/GameClient processes
- **Unified Networking**: All modes use ENet networking (even integrated mode)
- **Custom Physics System**: Voxel-face-based collision detection integrated with mesh generation
- **Structure of Arrays (SoA)**: Performance-optimized data layout for entity/component storage
- **Cross-Platform**: Windows, Linux, macOS compatibility

### Technology Stack
- **Rendering**: bgfx (cross-platform graphics abstraction)
- **Networking**: ENet (reliable UDP)
- **UI/Dev Tools**: Dear ImGui
- **Window/Input**: GLFW
- **Build System**: CMake with multiple presets
- **Dependencies**: All managed via git submodules

## 🎮 Current Features

### ✅ Implemented Systems

#### Core Engine
- **ECS Framework**: Entity-Component-System for game objects
- **JobSystem**: Multi-threaded task processing with worker threads
- **Custom Physics**: Dual-mesh generation (render + collision) from voxel face culling
- **Time Effects**: Time manipulation system (keys 1-5, 0, T for various effects)
- **Input System**: WASD+mouse camera controls, space for jump

#### World & Gameplay
- **Floating Islands**: Procedurally generated voxel terrain with custom collision detection
- **Player Movement**: Sphere-face collision with friction-based responses
- **Block Interaction**: Real-time block break/place with server validation
- **Voxel Rendering**: Optimized mesh generation with face culling

#### Networking
- **Client-Server Architecture**: Authoritative server with client prediction
- **Unified Network Layer**: Single code path for all game modes
- **Entity Synchronization**: Unified system for players, islands, and future entity types
- **Compression**: ~98% compression ratio for world data transmission
- **Multiple Run Modes**: Integrated, dedicated server, client-only

### 🎯 Game Modes

#### Integrated Mode (Default)
```bash
MMORPGEngine.exe
```
- Local server + client in same process with unified networking
- Server runs on `127.0.0.1:12345`
- Best for development and single-player testing
- Single debug path for consistent development

#### Dedicated Server
```bash
MMORPGEngine.exe --server
```
- Headless server accepting connections on port 12345
- For multiplayer testing and dedicated hosting
- Optimized for performance without rendering overhead

#### Client-Only
```bash
MMORPGEngine.exe --client <server_address>
```
- Connect to remote server
- Full networking path, identical to integrated mode
- For multiplayer testing and gameplay

## 🔧 Custom Physics System

### Overview
The engine implements a unique voxel-based collision detection system that eliminates the need for external physics engines by leveraging existing voxel face culling data.

### Dual-Mesh Generation
The system generates two meshes simultaneously during voxel chunk processing:

1. **Render Mesh**: Visual rendering (vertices, indices, normals, UVs)
2. **Collision Mesh**: Physics collision detection (face positions and normals)

```cpp
void VoxelChunk::generateMesh() {
    // Single-pass generation for both meshes
    for (int x = 0; x < SIZE; ++x) {
        for (int y = 0; y < SIZE; ++y) {
            for (int z = 0; z < SIZE; ++z) {
                if (shouldRenderFace(x, y, z, face)) {
                    // Add to render mesh
                    addQuad(mesh.vertices, mesh.indices, x, y, z, face, blockType);
                    
                    // Add to collision mesh
                    addCollisionQuad(x, y, z, face);
                }
            }
        }
    }
}
```

### Collision Detection Features
- **Algorithm**: Sphere-plane intersection for player-world collision
- **Performance**: Only checks chunks within player radius
- **Response**: Provides collision normals for friction-based movement
- **Ray Casting**: Block interaction and line-of-sight calculations
- **Memory Efficient**: Shared data between render and collision systems

### Advantages over External Physics Engines
1. **Perfect Integration**: Collision data directly from voxel geometry
2. **Memory Efficiency**: No duplicate collision geometry storage
3. **Deterministic**: Exact same collision data on client and server
4. **Lightweight**: No external dependencies or complex physics simulation
5. **Voxel-Optimized**: Designed specifically for blocky voxel worlds
6. **Network Friendly**: Collision state is implicit in voxel data

## 🌐 Network Architecture

### Unified Networking Design
All game modes use the same network layer for consistent debugging and development:

```cpp
class GameServer {
    GameState world;                // Authoritative world state  
    PhysicsSystem physics;          // Custom voxel-face collision system
    NetworkManager network;         // ENet-based client connections
    PlayerManager players;          // Manage connected players
    IslandChunkSystem chunks;       // Floating island management
    
    void tick(float deltaTime);     // 60Hz game loop
    void broadcastStateUpdates();   // Send to all clients
    void handleClientCommands();    // Process player inputs
};

class GameClient {
    Renderer renderer;              // Graphics + UI
    NetworkManager network;         // ENet server connection
    InputManager input;             // Capture user input
    Camera camera;                  // First-person camera
    
    void render();                  // Variable framerate
    void sendInputToServer();       // Forward player actions
    void applyServerUpdates();      // Sync with authoritative state
};
```

### Entity Synchronization System

#### Unified Entity Updates
Both islands and players use the same `EntityStateUpdate` message:

```cpp
struct EntityStateUpdate {
    uint8_t type = ENTITY_STATE_UPDATE;
    uint32_t sequenceNumber;
    uint32_t entityID;           // Unique entity identifier
    uint8_t entityType;          // 0=Player, 1=Island, 2=NPC, etc.
    Vec3 position;               // Current position
    Vec3 velocity;               // Current velocity  
    Vec3 acceleration;           // For smooth prediction/interpolation
    uint32_t serverTimestamp;    // Server time for lag compensation
    uint8_t flags;               // Bit flags (isGrounded, needsCorrection, etc.)
};
```

#### Voxel Change Synchronization
```cpp
struct VoxelChangeRequest {
    uint8_t type = VOXEL_CHANGE_REQUEST;
    uint32_t sequenceNumber;
    uint32_t islandID;
    Vec3 localPos;
    uint8_t voxelType; // 0 = air (break), 1+ = place block
};

struct VoxelChangeUpdate {
    uint8_t type = VOXEL_CHANGE_UPDATE;
    uint32_t sequenceNumber;
    uint32_t islandID;
    Vec3 localPos;
    uint8_t voxelType;
    uint32_t authorPlayerId; // Player who made the change
};
```

### Network Features
- **Server Authority**: Physics, world state, player health validation
- **Client Prediction**: Movement prediction with server correction
- **Lag Compensation**: Server timestamps for smooth gameplay
- **Compression**: Efficient world data transmission
- **Multiple Clients**: Tested with 2+ simultaneous connections

## 📁 Project Structure

```
/engine/                 # Core engine code (modular systems)
├── Core/               # GameClient, GameServer, GameState
├── ECS/                # Entity-Component-System framework  
├── Physics/            # Custom voxel-based collision system
├── Rendering/          # bgfx-based renderer and VoxelRenderer
├── Network/            # ENet networking and compression
├── World/              # IslandChunkSystem, VoxelChunk, raycasting
├── Input/              # Camera controls and input handling
├── Threading/          # JobSystem for multi-threaded processing
├── Time/               # TimeManager and TimeEffects system
└── Math/               # Vec3, Mat4 and math utilities

/libs/                  # Third-party libraries (git submodules)
├── bgfx/               # Cross-platform graphics
├── bimg/               # Image processing for bgfx
├── bx/                 # Base utilities for bgfx
├── enet/               # Reliable UDP networking
├── glfw/               # Window and input handling
├── imgui/              # Immediate mode GUI
├── lz4/                # Fast compression
└── FastNoise2/         # Procedural noise generation

/build/                 # CMake build output
├── bin/                # Compiled executables
└── lib/                # Static libraries

/docs/                  # Documentation (will be removed)
/tests/                 # Unit tests (planned)
/tools/                 # Development tools and scripts
```

## 🚀 Development Workflow

### Build System
CMake with multiple presets for different configurations:

```bash
# Available presets
cmake --list-presets    

# Common build commands
cmake --preset default              # Debug build
cmake --preset release             # Optimized build  
cmake --build build --config Debug # Build debug
cmake --build build --config Release # Build release
```

### VS Code Tasks
Configured tasks for streamlined development:
- **Configure**: Run CMake configuration
- **Build**: Compile the project (depends on Configure)
- **Run**: Execute the engine (depends on Build)

### Code Quality
- **clang-format**: Automatic code formatting
- **clang-tidy**: Static analysis and linting
- **Cross-platform**: Ensure compatibility across Windows, Linux, macOS

## 🎮 Controls & Features

### Player Controls
- **WASD**: Movement
- **Mouse**: Look around (first-person camera)
- **Space**: Jump
- **Left Click**: Break blocks
- **Right Click**: Place blocks (when implemented)

### Time Effects System
- **Keys 1-5**: Various time manipulation effects
- **Key 0**: Reset time effects
- **Key T**: Toggle time effects

### Debug Features
- **Dear ImGui**: Integrated developer tools and debug UI
- **Console Output**: Minimal, essential logging
- **Network Stats**: Connection and synchronization information

## 🔮 Future Enhancements

### Planned Physics Features
- **Compound Collision**: Multiple collision shapes per entity
- **Continuous Collision**: Swept sphere collision for fast-moving objects
- **Friction Materials**: Different friction coefficients per voxel type
- **Collision Layers**: Separate collision masks for different entity types

### Planned Network Features
- **Player Authentication**: Secure connection validation
- **Chat System**: Text communication between players
- **Player Visibility**: See other players moving in real-time
- **Block Event Sync**: Real-time block break/place from other players

### Performance Optimizations
- **Spatial Hashing**: Broad-phase collision culling
- **LOD Collision**: Simplified collision for distant objects
- **Async Updates**: Background collision mesh generation
- **SIMD Optimization**: Vectorized collision detection

## 🛠️ Technical Implementation Details

### Structure of Arrays (SoA) Design
For performance-critical systems, the engine uses SoA layout:

```cpp
// Instead of Array of Structures (AoS):
// std::vector<Player> players;

// Use Structure of Arrays (SoA):
std::vector<float> positionsX, positionsY, positionsZ;
std::vector<float> velocitiesX, velocitiesY, velocitiesZ; 
std::vector<int> health;
std::vector<uint32_t> entityIDs;
```

Benefits:
- **Cache Efficiency**: Better memory access patterns
- **SIMD Potential**: Vectorization opportunities
- **Memory Layout**: Optimal for bulk operations

### Job System Integration
Multi-threaded processing for performance-critical operations:

```cpp
enum JobType {
    CHUNK_MESHING,          // Generate render + collision meshes
    COLLISION_CHECK,        // Validate player movement
    NETWORK_SEND,           // Serialize and send data
    NETWORK_RECEIVE,        // Deserialize incoming data
    STATE_RECONCILIATION    // Sync client prediction with server
};
```

### Cross-Platform Compatibility
- **Graphics**: bgfx handles platform-specific rendering APIs
- **Networking**: ENet provides cross-platform UDP networking
- **Build System**: CMake ensures consistent builds across platforms
- **Dependencies**: All third-party libraries support target platforms

## 📊 Performance Characteristics

### Target Performance
- **Framerate**: 60+ FPS with multiple islands
- **Network**: Sub-100ms latency for local connections
- **Memory**: Efficient voxel storage with face culling
- **Scaling**: Support for multiple simultaneous clients

### Optimization Strategies
- **Frustum Culling**: Only render visible chunks
- **Mesh Caching**: Reuse generated geometry when possible
- **Network Compression**: ~98% compression for world data
- **Lazy Updates**: Only regenerate when data changes

## 🐛 Debugging & Development

### Debugging Features
- **Single Debug Path**: Unified networking eliminates dual code paths
- **Verbose Logging**: Configurable debug output levels
- **ImGui Integration**: Real-time parameter adjustment
- **Network Inspection**: Packet monitoring and statistics

### Development Best Practices
- **Modular Design**: Clean separation between systems
- **Iterative Development**: Test after each major change
- **Cross-Platform Testing**: Verify on multiple platforms
- **Performance Monitoring**: Regular performance profiling

---

## 🙏 Acknowledgments

Built with excellent open-source libraries:
- [bgfx](https://github.com/bkaradzic/bgfx) - Cross-platform graphics
- [ENet](http://enet.bespin.org/) - Reliable UDP networking  
- [Dear ImGui](https://github.com/ocornut/imgui) - Immediate mode GUI
- [GLFW](https://www.glfw.org/) - Window and input handling
- [LZ4](https://github.com/lz4/lz4) - Fast compression

---

*This engine demonstrates modern C++ game development practices with a focus on modularity, performance, and cross-platform compatibility. The custom physics system showcases innovative approaches to voxel-based collision detection without external dependencies.*
