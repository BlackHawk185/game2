# Add all engine source files (including new systems)
add_library(engine STATIC
    World/VoxelChunk.cpp
    Player.cpp
    Rendering/Renderer.cpp
    Input/Camera.cpp
    Math/Vec3.cpp
    World/IslandChunkSystem.cpp
    ECS/ECS.cpp
    Physics/PhysicsSystem.cpp
    Rendering/VoxelRenderer.cpp
    Threading/JobSystem.cpp
    Culling/FrustumCuller.cpp
    World/VoxelRaycaster.cpp
    Time/TimeManager.cpp
    Time/TimeEffects.cpp
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

# MSVC compatibility
if(MSVC)
    target_compile_options(engine PRIVATE /Zc:__cplusplus)
endif()

find_package(OpenGL REQUIRED)

# Link third-party libraries
target_link_libraries(engine PUBLIC
    glfw
    Jolt
    OpenGL::GL
)
