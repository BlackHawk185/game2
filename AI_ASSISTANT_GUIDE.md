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
- **Physics abstraction**: Always use the custom physics interface, not direct Jolt calls, unless implementing the abstraction layer.
- **Console support**: Keep future console support in mind (no platform-specific hacks).
- **Unified networking**: All game modes use the network layer - avoid direct GameState access from clients.
- **Server authority**: Game state changes should go through server validation, even in integrated mode.

## Code Quality Guidelines

- **Clean output**: Minimize console logging - only essential messages. Avoid excessive debug output that clutters the console.
- **Performance-first**: When implementing new features, consider cache efficiency, memory layout, and CPU/GPU optimization.
- **Integration over isolation**: New features should integrate cleanly with existing systems rather than being bolted on.
- **Test incrementally**: After significant changes, ensure the engine builds and runs before proceeding to the next feature.

## Current Engine Architecture Status

### Core Systems (Implemented)
- **ECS Framework**: Entity-Component-System for game objects
- **JobSystem**: Multi-threaded task processing with worker threads
- **Physics**: Jolt Physics integration with custom abstraction layer
- **Rendering**: bgfx-based renderer with Dear ImGui dev tools
- **Input**: Camera controls (WASD+mouse, space for jump)
- **Time Effects**: Time manipulation system (keys 1-5, 0, T for various effects)
- **Networking**: ENet-based client-server architecture with unified networking paths

### Game-Specific Features (Implemented)
- **Floating Islands**: Procedurally generated voxel islands with physics
- **Player Movement**: Physics-based movement with real-time network synchronization
- **Island Momentum**: Players inherit velocity from moving platforms
- **Collision Detection**: Efficient voxel-based collision with islands
- **Island Drift**: Islands move with realistic physics simulation
- **Block Interaction**: Real-time block break/place with network validation

### Network Architecture (Implemented)
- **Unified Networking**: All modes (integrated + remote) use identical network layer
- **ENet Protocol**: Reliable UDP with automatic packet management
- **Compression**: ~98% compression ratio for world data transmission
- **Client-Server Model**: Server-authoritative with client prediction ready
- **Multiple Modes**: Integrated (local server + networked client), dedicated server, client-only

### Technical Details
- **Build System**: CMake with Visual Studio support
- **Launch Configurations**: VS Code integration with 3 run modes
- **Cross-Platform**: Windows, Linux, macOS compatibility maintained
- **Dependencies**: bgfx, Jolt Physics, GLFW, Dear ImGui
- **Platform**: Currently Windows-focused but designed for cross-platform
- **Performance**: Optimized for 60+ FPS with multiple islands and physics

---

This file is for internal use by the AI assistant and can be updated as the project evolves.
