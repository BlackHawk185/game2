# Add all engine source files (including new systems)
add_library(engine STATIC
    # Core systems (new modular architecture)
    Core/GameState.cpp
    Core/GameServer.cpp
    Core/GameClient.cpp
    Core/Window.cpp
    Profiling/Profiler.cpp
    Profiling/DebugDiagnostics.cpp
    
    # World systems
    World/VoxelChunk.cpp
    World/IslandChunkSystem.cpp
    World/VoxelRaycaster.cpp
    
    # Game objects
    Player.cpp
    
    # Rendering systems
    Rendering/Renderer.cpp
    Rendering/VBORenderer.cpp  # RE-ENABLED
    Rendering/TextureManager.cpp
    Rendering/ShaderManager.cpp
    Rendering/SimpleShader.cpp  # Simple shader for basic rendering
    Rendering/GlobalLightingManager.cpp  # NEW: Global lighting system
    Rendering/ShadowMap.cpp
    Rendering/CascadedShadowMap.cpp
    
    # Input systems
    Input/Camera.cpp
    
    # Math systems
    Math/Vec3.cpp
    
    # ECS systems
    ECS/ECS.cpp
    
    # Physics systems
    Physics/PhysicsSystem.cpp
    Physics/FluidSystem.cpp
    
    # Threading systems
    Threading/JobSystem.cpp
    
    # Culling systems
    Culling/FrustumCuller.cpp
    
    # Time systems
    Time/TimeManager.cpp
    Time/TimeEffects.cpp
    Time/DayNightCycle.cpp
    
    # Network systems - Re-enabled with proper ENet integration
    Network/NetworkManager.cpp
    Network/IntegratedServer.cpp
    Network/NetworkClient.cpp
    Network/VoxelCompression.cpp
)

# === Third-party sources: ImGui ===
file(GLOB IMGUI_SRC
    ${CMAKE_SOURCE_DIR}/libs/imgui/*.cpp
)

# Add third-party sources to engine target
target_sources(engine PRIVATE
    ${IMGUI_SRC}
)

# Enable precompiled headers (PCH)
target_precompile_headers(engine PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/pch.h")

# Engine includes
target_include_directories(engine PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
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
    # Ensure correct __cplusplus value
    target_compile_options(engine PRIVATE /Zc:__cplusplus)
    # Allow parallel compilation and avoid PDB write conflicts by enabling /MP and /FS
    # /MP enables multi-processor compilation; /FS serializes access to PDB to avoid races
    target_compile_options(engine PRIVATE /MP /FS)
endif()

find_package(OpenGL REQUIRED)

# Link third-party libraries
target_link_libraries(engine PUBLIC
    glfw
    enet
    lz4
    stb
    glad
    OpenGL::GL
    glm::glm
)

# Ensure GLFW does not include a GL header (glad provides it)
target_compile_definitions(engine PUBLIC GLFW_INCLUDE_NONE)
