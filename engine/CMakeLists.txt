# Add all engine source files (including new systems)
add_library(engine STATIC
    # Core systems (new modular architecture)
    Core/GameState.cpp
    Core/GameServer.cpp
    Core/GameClient.cpp
    
    # World systems
    World/VoxelChunk.cpp
    World/IslandChunkSystem.cpp
    World/VoxelRaycaster.cpp
    
    # Game objects
    Player.cpp
    
    # Rendering systems
    Rendering/Renderer.cpp
    Rendering/VoxelRenderer.cpp
    
    # Input systems
    Input/Camera.cpp
    
    # Math systems
    Math/Vec3.cpp
    
    # ECS systems
    ECS/ECS.cpp
    
    # Physics systems
    Physics/PhysicsSystem.cpp
    
    # Threading systems
    Threading/JobSystem.cpp
    
    # Culling systems
    Culling/FrustumCuller.cpp
    
    # Time systems
    Time/TimeManager.cpp
    Time/TimeEffects.cpp
    
    # Network systems - Re-enabled with proper ENet integration
    Network/NetworkManager.cpp
    Network/IntegratedServer.cpp
    Network/NetworkClient.cpp
)

# === Third-party sources: Jolt Physics ==Move-Item "c:\Users\steve-17\Desktop\game2\engine\Renderer.h" "c:\Users\steve-17\Desktop\game2\engine\Rendering\Renderer.h"=
# Use proper Jolt CMake integration
add_subdirectory(${CMAKE_SOURCE_DIR}/libs/JoltPhysics/Build jolt_build EXCLUDE_FROM_ALL)

# === Third-party sources: ImGui ===
file(GLOB IMGUI_SRC
    ${CMAKE_SOURCE_DIR}/libs/imgui/*.cpp
)

# Add third-party sources to engine target
target_sources(engine PRIVATE
    ${IMGUI_SRC}
)

# Engine includes
target_include_directories(engine PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/libs/JoltPhysics
    ${CMAKE_SOURCE_DIR}/libs/imgui
)

target_compile_features(engine PUBLIC cxx_std_17)

# Set include directories for third-party libraries
target_include_directories(engine PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}  # For internal includes
    ${CMAKE_SOURCE_DIR}/libs/enet/include  # For ENet headers
)

# MSVC compatibility
if(MSVC)
    target_compile_options(engine PRIVATE /Zc:__cplusplus)
endif()

find_package(OpenGL REQUIRED)

# Link third-party libraries
target_link_libraries(engine PUBLIC
    glfw
    Jolt
    enet
    OpenGL::GL
)
