# MMORPG Engine - Clean Foundation

A modular C++17 MMORPG engine rebuilt from scratch with clean architecture. Currently in **clean foundation** state - core systems implemented and ready for world generation and rendering systems.

## Current Status: Clean Foundation 🚀

After removing architectural violations and problematic legacy code, we now have a minimal, buildable foundation with:
- **Server-Client Architecture**: Clear separation between authoritative server and rendering client
- **Unified Networking**: All game modes use ENet networking (even single-player)
- **Modular Design**: Clean system boundaries with no circular dependencies
- **Cross-Platform**: OpenGL-based rendering with GLFW/ENet for platform compatibility

## Architecture Overview

### Core Principle: Server-Client Separation
```
┌─────────────────┐    Network     ┌─────────────────┐
│   GameServer    │◄──────────────►│   GameClient    │
│                 │(LZ4 compressed)│                 │
│ • World State   │                │ • Renderer      │
│ • World Gen     │                │ • Input         │
│ • Validation    │                │ • UI/ImGui      │
└─────────────────┘                └─────────────────┘
```

**GameServer** (Authoritative):
- Contains all world state internally (players, voxels, islands)
- Handles procedural world generation and voxel data
- Generates physics meshes for movement validation (no noclipping)
- No rendering - purely simulation
- Sends compressed chunk data to clients via LZ4

**GameClient** (Presentation):
- Handles all rendering (OpenGL + Dear ImGui + modern VBO/VAO/UBO pipeline)
- Processes user input (WASD, mouse, block interaction)
- Runs client-side physics for responsiveness
- Receives compressed world data from server
- Generates lightmaps for realistic per-pixel voxel-aware lighting

**Main.cpp**:
- Contains startup code for both server and client
- Handles run mode selection (integrated, dedicated server, client-only)
- Manages the server-client lifecycle

## World Generation System

### Island-Based Coordinate System
- **Islands**: Transformable entities with local coordinate systems
- **Chunks**: Generated on-demand using grid-based approach (like voxels but larger scale)
- **Voxels**: Block-level detail within chunks
- **Hierarchy**: `Island → Chunk → Voxel` for scalable world management

### Unified Mesh Generation
The core innovation is a **single loop** that generates both physics and render data with advanced optimizations:

#### Face Culling & Greedy Meshing
- **Face Culling**: Only generate faces between air and solid voxels (hidden faces eliminated)
- **Greedy Meshing**: Merge adjacent faces of same material into larger quads for performance
- **Unified Generation**: Both physics and rendering use the same optimized face data

```cpp
// Unified mesh generation loop with optimizations
for (each voxel) {
    if (voxel.isEmpty()) continue;
    
    for (each face direction) {
        if (shouldCullFace(neighborVoxel)) continue;  // Face culling
        
        // Try to expand face greedily (greedy meshing)
        Quad greedyQuad = expandFaceGreedily(position, direction, material);
        
        // ALWAYS generate physics data (server + client need this)
        addCollisionQuad(greedyQuad.vertices, greedyQuad.normal);
        
        if (generateRenderData) {  // Only true on client
            addVertices(greedyQuad.vertices);
            addTextureUV(greedyQuad.uvCoords);     // dirt.png texture coordinates
            addLightmapUV(greedyQuad.lightmapUV);  // Lightmap texture coordinates
            addNormal(greedyQuad.normal);
            // Send to VBO/VAO for rendering
        }
    }
}
```

**Benefits:**
- **Face Culling**: 80%+ face reduction (only exterior faces rendered)
- **Greedy Meshing**: Major performance boost from quad merging
- **Server Optimization**: Physics gets same culled data without rendering overhead
- **Client Rendering**: Optimized mesh data ready for OpenGL VBO/VAO
- **Texture Ready**: dirt.png and other textures applied via UV mapping
- **SIMD Ready**: Vertex/normal calculations can be vectorized

### Lighting System
- **Dark by Default**: Everything starts unlit for realistic lighting
- **Lightmap Texture Approach**: 2D texture containing shadow/light data for multiple chunks
- **Post-Generation Workflow**: 
  1. Generate chunks with static VBOs (including lightmapUV coordinates)
  2. After player spawn, collect 5-chunk radius
  3. Generate lightmap texture using raycast-based occlusion
  4. Fragment shader samples both albedo texture and lightmap texture
- **Per-Pixel Voxel-Aware**: High-quality lighting without modifying VBO data
- **GPU Optimized**: Fast texture sampling, no vertex buffer updates

## File Structure (Implemented & Planned)

```
/World/
  ├── Island.h/cpp           // Island entities with transforms ✅
  ├── ChunkManager.h/cpp     // On-demand chunk generation ✅
  ├── Chunk.h/cpp            // 16x16x16 voxel chunks ✅
  ├── MeshGenerator.h/cpp    // Unified physics+render mesh with face culling & greedy meshing
  ├── LightmapGenerator.h/cpp // Post-generation lighting system
  └── VoxelRaycaster.h/cpp   // GLM-based raycasting utilities

/Rendering/ (client-only)
  ├── VoxelRenderer.h/cpp    // Modern OpenGL VBO/VAO/UBO management
  ├── TextureManager.h/cpp   // dirt.png and texture atlas management
  └── Shader.h/cpp           // Vertex/fragment shaders for voxel rendering

/Physics/ (server + client)
  └── MovementValidator.h/cpp // Server-side movement validation

/Core/
  ├── GameServer.h/cpp       // Authoritative server logic ✅
  └── GameClient.h/cpp       // Presentation and input handling ✅
```

## Structure
- `/engine` — Core engine code with modular systems
- `/libs` — Third-party libraries (ENet, GLFW, Dear ImGui)
- `/docs` — Architecture documentation
- `/tests` — Unit tests (planned)

## Getting Started
1. Install CMake (>=3.15) and a C++17 compiler.
2. Clone the repository and its submodules.
3. Configure and build:
   ```sh
   mkdir build
   cd build
   cmake ..
   cmake --build .
   ```
4. Run the engine from `build/bin/`.

## Run Modes

### Default (Integrated Mode)
```sh
MMORPGEngine.exe
```
- Local server + client with unified networking
- Best for development and single-player testing
- Server runs on `127.0.0.1:7777`

### Dedicated Server
```sh
MMORPGEngine.exe --server
```
- Headless server accepting connections on port 7777
- For multiplayer testing and dedicated hosting

### Client-Only
```sh
MMORPGEngine.exe --client <server_address>
```
- Connect to remote server
- For multiplayer testing

## Current Features

### ✅ Implemented Systems
- **Client-Server Architecture**: Separated GameServer/GameClient with ENet networking
- **Unified Networking**: All modes use network layer (no direct local calls)
- **World Generation**: Procedural floating islands with voxel terrain
- **Player Movement**: Custom collision detection with network synchronization
- **Block Interaction**: Real-time block break/place with server validation
- **Time Effects**: Time manipulation system
- **Rendering**: OpenGL-based renderer with Dear ImGui dev tools
- **Input System**: WASD+mouse camera controls, space for jump
- **Custom Physics**: Dual-mesh generation (render + collision) from voxel face culling

### 🔧 Technical Features
- **Cross-Platform**: Windows, Linux, macOS support
- **CMake Build System**: Multiple presets (review, debug, sanitized, release)
- **Code Quality Tools**: clang-format, clang-tidy integration
- **Modular Design**: Clean separation between systems
- **SoA Optimization**: Structure of Arrays for performance-critical code

## VS Code Integration
Three launch configurations available in VS Code:
1. **Default**: Integrated mode (recommended)
2. **Connect to Server**: Client connecting to localhost:12345
3. **Dedicated Server**: Headless server mode

## Extensions Recommended
- C/C++ (ms-vscode.cpptools)
- CMake Tools (ms-vscode.cmake-tools)
- CMake (twxs.cmake)
- Better C++ Syntax (jeff-hykin.better-cpp-syntax)

## Tasks
- Build and run tasks are configured in `.vscode/tasks.json`.

---

See `.github/copilot-instructions.md` for workspace-specific AI guidance.
