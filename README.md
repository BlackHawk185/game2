# MMORPG Engine Prototype Workspace

This workspace is set up for a modular C++17 MMORPG engine prototype using CMake. It is designed for cross-platform development (Windows, Linux, macOS) and integrates:
- **bgfx** for rendering
- **Jolt Physics** for simulation
- **Dear ImGui** for dev tools

## Structure
- `/engine` — Core engine code
- `/libs` — Third-party libraries (bgfx, Jolt Physics, etc.)
- `/assets` — Placeholder and real assets

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
