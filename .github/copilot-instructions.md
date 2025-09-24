# AI Assistant Guidance for MMORPG Engine Project

This file is for the AI assistant (GitHub Copilot) to reference when generating code, explanations, or project structure for the user.

## Data Layout Directive: SoA (Structure of Arrays)

- **Use SoA (Structure of Arrays) for all entity/component storage and performance-critical systems.**
- When designing systems that manage many objects (players, islands, voxels, etc.), prefer SoA for better cache efficiency and SIMD/vectorization potential.
- Example: Instead of `std::vector<Player>`, use separate arrays for each property: `std::vector<float> positionsX, positionsY, positionsZ; std::vector<int> health;` etc.
- Add comments and explanations to help the user understand SoA patterns as they are implemented.

## Key Reminders

- **User is new to game development**: Explanations should be clear, concise, and avoid assuming prior game dev knowledge.
- **Bite-sized steps**: Break down tasks into small, manageable pieces. Avoid overwhelming the user with large code dumps or complex changes at once.
- **Explain rationale**: Briefly explain why each file, function, or system is being created or changed.
- **Comment code**: Add comments in code to clarify purpose, especially for game engine or C++-specific patterns.
- **Modular design**: Keep code modular and organized per the project structure in the prompt.
- **Iterative approach**: After each step, check with the user before proceeding to the next major system or feature.
- **Cross-platform**: Ensure all code and build steps are cross-platform (Windows, Linux, macOS) unless otherwise specified.
- **Custom physics**: The engine uses a custom voxel-face-based collision system, not external physics engines.
- **Console support**: Keep future console support in mind (no platform-specific hacks).
- **Unified networking**: All game modes use the network layer - avoid direct GameState access from clients.
- **Server authority**: Game state changes should go through server validation, even in integrated mode.
- **Delete over deprecate**: Remove old systems entirely rather than commenting them out. This project evolves rapidly and needs aggressive cleanup.
	- On Windows, always use PowerShell's `Remove-Item` command to delete files (e.g., `Remove-Item -Path "path\to\file" -Force`). This ensures reliable file removal even if standard methods fail.

## Code Quality Guidelines

- **Clean output**: Minimize console logging - only essential messages. Avoid excessive debug output that clutters the console.
- **Performance-first**: When implementing new features, consider cache efficiency, memory layout, and CPU/GPU optimization.
- **Integration over isolation**: New features should integrate cleanly with existing systems rather than being bolted on.
- **Test incrementally**: After significant changes, ensure the engine builds and runs before proceeding to the next feature.
- **Aggressive cleanup**: Remove deprecated code entirely rather than commenting it out. The project evolves rapidly and accumulating old code hurts maintainability.
- **Event-driven architecture**: Prefer event-driven systems over continuous polling for better performance and cleaner code organization.

## Development Workflow & Build System

### Essential Commands
```powershell
# Quick build (AI-friendly with progress feedback)
.\scripts\ai-friendly-build.ps1

# Standard CMake workflow
cmake --preset default                    # Configure
cmake --build build --config Debug       # Build
.\build\bin\MMORPGEngine.exe             # Run (integrated mode)

# Run modes
.\build\bin\MMORPGEngine.exe --server    # Dedicated server
.\build\bin\MMORPGEngine.exe --client localhost  # Connect to server
.\build\bin\MMORPGEngine.exe --help      # Show all options
```

### VS Code Integration
- Use tasks: "Build", "Run", "Debug Run" (avoid terminal commands when tasks exist)
- Three launch configurations: Default (integrated), Connect to Server, Dedicated Server
- Recommended: Always use VS Code tasks rather than manual terminal commands

### Debug Workflow
- Use `.\scripts\ai-friendly-build.ps1` for immediate feedback during development
- Check `build/bin/MMORPGEngine.exe` exists after successful build
- Monitor console output for initialization sequence completion

## Current Engine Architecture Status

### Core Systems (Implemented)
- **ECS Framework**: Entity-Component-System for game objects
- **JobSystem**: Multi-threaded task processing with worker threads (`engine/Threading/JobSystem.h`)
- **Custom Physics**: Voxel-face-based collision detection with dual-mesh generation
- **Rendering**: OpenGL/GLAD-based renderer with Dear ImGui dev tools (transitioning from bgfx)
- **Input**: Camera controls (WASD+mouse, space for jump)
- **Time Effects**: Time manipulation system (keys 1-5, 0, T for various effects)
- **Networking**: ENet-based client-server architecture with unified networking paths

### GPU Compute Infrastructure (In Development)
- **GPUMeshGenerator**: OpenGL 4.6 compute shader system for voxel mesh generation (`engine/Rendering/GPUMeshGenerator.h/.cpp`)
- **ComputeShader**: OpenGL compute shader wrapper with buffer management (`engine/Rendering/ComputeShader.h/.cpp`)
- **Voxel Compute Shader**: `shaders/voxel_mesh_gen.comp` - parallel mesh generation from voxel data
- **Current Status**: Infrastructure complete, debugging initialization crashes
- **Performance Goal**: GPU acceleration to replace CPU-heavy mesh generation (0% GPU → 100% CPU utilization identified)

### Game-Specific Features (Implemented)
- **Floating Islands**: Procedurally generated voxel islands with custom collision detection
- **Player Movement**: Custom sphere-face collision with real-time network synchronization
- **Voxel Collision**: Friction-based collision detection using exposed voxel faces
- **Block Interaction**: Real-time block break/place with network validation
- **Dual-Mesh Generation**: Single-pass generation of both render and collision meshes
- **Event-Driven Lighting**: Dynamic lighting system integrated with mesh generation and day/night cycle
- **Day/Night Cycle**: Real-time sun movement with dynamic lighting updates
- **Light Mapping**: Per-face 32x32 light maps with ambient occlusion (`VoxelChunk::FaceLightMap`)

### Network Architecture (Implemented)
- **Unified Networking**: All modes (integrated + remote) use identical network layer
- **ENet Protocol**: Reliable UDP with automatic packet management
- **Compression**: ~98% compression ratio for world data transmission (`engine/Network/VoxelCompression.h`)
- **Client-Server Model**: Server-authoritative with client prediction ready
- **Multiple Modes**: Integrated (local server + networked client), dedicated server, client-only
- **Entity Synchronization**: Unified `EntityStateUpdate` for players, islands, future entities

### Custom Physics Architecture
- **Voxel-Face Collision**: Custom collision system using voxel face culling data
- **Dual-Mesh Generation**: Render + collision meshes from single voxel pass
- **Sphere-Plane Intersection**: Player collision algorithm with friction response
- **Memory Efficiency**: Shared data between render and collision systems
- **Network Determinism**: Identical collision data on client and server

## Project Structure & Key Files

### Core Engine (`engine/`)
```
Core/          - GameClient, GameServer, GameState (main application logic)
ECS/           - Entity-Component-System framework
Physics/       - Custom voxel-based collision system
Rendering/     - OpenGL renderer, VBORenderer, GPUMeshGenerator, ComputeShader
Network/       - ENet networking, compression, NetworkManager
World/         - IslandChunkSystem, VoxelChunk, raycasting
Input/         - Camera controls and input handling  
Threading/     - JobSystem for multi-threaded processing
Time/          - TimeManager and TimeEffects system
```

### Critical Integration Points
- **GPU Mesh Generation**: `engine/World/IslandChunkSystem.cpp` calls `GPUMeshGenerator` for chunk meshing
- **Network Layer**: All game modes route through `NetworkManager` - no direct GameState access
- **Voxel Data**: `VoxelChunk` (16x16x16) with `std::array<VoxelID, SIZE*SIZE*SIZE>` storage
- **Job System**: `JobType::CHUNK_MESHING` for parallel mesh generation

### Build & Dependencies (`libs/`)
- All dependencies via git submodules (ENet, GLFW, Dear ImGui, LZ4, FastNoise2)
- OpenGL/GLAD for graphics (transitioning from bgfx)
- CMake with presets: `default` (Debug), `release` (Release)

## Technical Debugging Context

### Current Known Issues
- **GPU Mesh Generator**: Initialization crash during OpenGL compute shader setup
- **Performance**: CPU-bound mesh generation identified (100% CPU, 0% GPU utilization)
- **OpenGL Context**: Verify GLAD initialization before GPU compute operations

### Architecture Principles
- **Server Authority**: GameServer owns authoritative world state, clients receive updates
- **Unified Network Path**: Even integrated mode uses NetworkManager for consistent debugging
- **Custom Physics**: No external physics engine - voxel-face collision integrated with mesh generation
- **Performance-First**: SoA data layout, GPU compute acceleration, event-driven systems

---

This file is for internal use by the AI assistant and can be updated as the project evolves.
