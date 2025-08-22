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

## Extensions Recommended
- C/C++ (ms-vscode.cpptools)
- CMake Tools (ms-vscode.cmake-tools)
- CMake (twxs.cmake)
- Better C++ Syntax (jeff-hykin.better-cpp-syntax)

## Tasks
- Build and run tasks are configured in `.vscode/tasks.json`.

---

See `.github/copilot-instructions.md` for workspace-specific AI guidance.
