# AI Assistant Guidance for MMORPG Engine Project

This file is for the AI assistant (GitHub Copilot) to reference when generating code, explanations, or project structure for the user.

## Current Project Status: CLEAN FOUNDATION

**Architecture rebuilt from scratch - ready for clean implementation**

The project has been cleaned of problematic legacy code and architectural violations. We now have a minimal, buildable foundation ready for proper system implementation.

## Data Layout Directive: SoA (Structure of Arrays)

- **Use SoA (Structure of Arrays) for all entity/component storage and performance-critical systems.**
- When designing systems that manage many objects (players, islands, voxels, etc.), prefer SoA for better cache efficiency and SIMD/vectorization potential.
- Example: Instead of `std::vector<Player>`, use separate arrays for each property: `std::vector<float> positionsX, positionsY, positionsZ; std::vector<int> health;` etc.
- Add comments and explanations to help the user understand SoA patterns as they are implemented.

## Key Development Principles

- **User is new to game development**: Explanations should be clear, concise, and avoid assuming prior game dev knowledge.
- **Bite-sized steps**: Break down tasks into small, manageable pieces. Avoid overwhelming the user with large code dumps or complex changes at once.
- **Explain rationale**: Briefly explain why each file, function, or system is being created or changed.
- **Comment code**: Add comments in code to clarify purpose, especially for game engine or C++-specific patterns.
- **Modular design**: Keep code modular and organized per the project structure.
- **Iterative approach**: After each step, check with the user before proceeding to the next major system or feature.
- **Cross-platform**: Ensure all code and build steps are cross-platform (Windows, Linux, macOS) unless otherwise specified.
- **Clean architecture**: Avoid tight coupling between systems - use clear interfaces and abstraction layers.
- **Delete over deprecate**: Remove old systems entirely rather than commenting them out. This project evolves rapidly and needs aggressive cleanup.

## Architecture Guidelines

### Rendering Pipeline (TO BE IMPLEMENTED)
- **Modern OpenGL 4.6**: Use OpenGL 4.6 Core Profile for cross-platform compatibility and modern features
- **Direct State Access**: Leverage OpenGL 4.6 DSA for more efficient state management
- **Direct OpenGL usage**: No need for abstraction layer - OpenGL has excellent cross-platform support
- **Organized rendering code**: Keep OpenGL calls organized in rendering modules, but don't over-abstract
- **Shader management**: Simple shader loading and compilation system using `#version 460 core`

### Physics System (TO BE IMPLEMENTED)
- **Custom voxel-face-based collision** - Not external physics engines
- **Dual-mesh generation** - Single-pass generation of both render and collision meshes
- **Performance-first** - Leverage voxel face culling data for collision detection

### Networking Architecture (PARTIALLY IMPLEMENTED)
- **Unified networking**: All game modes use the network layer - avoid direct GameState access from clients
- **Server authority**: Game state changes should go through server validation, even in integrated mode
- **ENet-based**: Reliable UDP with automatic packet management

### Data Organization
- **SoA for performance systems**: Entity storage, chunk data, particle systems
- **Event-driven architecture**: Prefer event-driven systems over continuous polling
- **Job system integration**: Design systems to work with multi-threaded job processing

## Code Quality Guidelines

- **Clean output**: Minimize console logging - only essential messages. Avoid excessive debug output that clutters the console.
- **Performance-first**: When implementing new features, consider cache efficiency, memory layout, and CPU/GPU optimization.
- **Integration over isolation**: New features should integrate cleanly with existing systems rather than being bolted on.
- **Test incrementally**: After significant changes, ensure the engine builds and runs before proceeding to the next feature.
- **Aggressive cleanup**: Remove deprecated code entirely rather than commenting it out.

## Current Engine Architecture Status

### ✅ Core Systems (IMPLEMENTED - Minimal Foundation)
- **Basic CMake Structure**: Modular engine library with clean build system
- **Essential Math**: Vec3 basic vector operations
- **ECS Framework**: Basic Entity-Component-System framework
- **Threading**: JobSystem for multi-threaded task processing
- **Input**: Basic Camera class for input handling
- **Time Systems**: TimeManager and TimeEffects for time manipulation
- **Network Foundation**: ENet-based networking (NetworkManager, IntegratedServer, NetworkClient)
- **Compression**: VoxelCompression system for efficient data transmission
- **Profiling**: Basic performance monitoring tools
- **Culling**: Basic FrustumCuller system
- **Physics**: Minimal FluidSystem (PhysicsSystem removed due to dependencies)

### 🚧 Systems TO BE IMPLEMENTED (Clean Rebuild Needed)
- **Rendering Pipeline**: Clean abstraction layer (prepare for bgfx)
- **World Systems**: Voxel chunks, island generation, collision detection
- **Game Logic**: Player management, game state, client-server game loop
- **Lighting**: Dynamic lighting integrated with world generation
- **Audio**: Sound system integration
- **Asset Management**: Resource loading and management

### 🗑️ Systems REMOVED (Architectural Violations)
- **Old Rendering**: VBORenderer, OpenGL-specific code, shader management
- **Legacy World**: VoxelChunk, IslandChunkSystem with rendering dependencies
- **Game Logic**: GameClient, GameServer, GameState with tight coupling
- **Player System**: Player class with world dependencies

## Dependencies and Libraries

### Currently Integrated
- **ENet**: Reliable UDP networking
- **GLFW**: Window and input (minimal usage)
- **Dear ImGui**: Developer UI tools
- **GLM**: Math library
- **LZ4**: Fast compression
- **Glad**: OpenGL loading (to be abstracted)

### Planned Integration
- **FastNoise2**: Procedural noise generation for world generation

## Build System
- **CMake 3.15+**: Cross-platform build configuration
- **C++17**: Modern C++ features for clean, efficient code
- **Multiple Presets**: Debug, Release, with sanitizers for development
- **VS Code Integration**: Configured tasks and launch configurations

---

This file is for internal use by the AI assistant and should be updated as the project architecture evolves during the clean rebuild process.
