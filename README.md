# MMORPG Engine Prototype Workspace

This workspace is set up for a modular C++17 MMORPG engine prototype using CMake. It is designed for cross-platform development (Windows, Linux, macOS) and integrates:
- **OpenGL** for rendering
- **Dear ImGui** for dev tools
- **ENet** for networking
- **GLFW** for window/input management

## Architecture Overview

The engine uses a **modular client-server architecture** with:
- **GameServer**: Authoritative simulation (physics, world state, player management)
- **GameClient**: Rendering and input handling
- **Bullet Physics**: Integrated physics simulation for realistic island movement and collision detection
- **Unified Networking**: All modes use ENet networking (even integrated mode)
- **SoA Data Layout**: Structure of Arrays for performance-critical systems

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
- Server runs on `127.0.0.1:12345`

### Dedicated Server
```sh
MMORPGEngine.exe --server
```
- Headless server accepting connections on port 12345
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
- **Player Movement**: Physics-based movement with network synchronization
- **Block Interaction**: Real-time block break/place with server validation
- **Time Effects**: Time manipulation system (keys 1-5, 0, T)
- **Rendering**: OpenGL-based renderer with Dear ImGui dev tools
- **Input System**: WASD+mouse camera controls, space for jump

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
