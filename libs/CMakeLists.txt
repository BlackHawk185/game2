# GLFW
add_subdirectory(glfw)

# ENet networking - Manual build instead of subdirectory to avoid CMake version conflicts
add_library(enet STATIC
    enet/callbacks.c
    enet/compress.c
    enet/host.c
    enet/list.c
    enet/packet.c
    enet/peer.c
    enet/protocol.c
    enet/win32.c
)

# Set up ENet target properties
target_include_directories(enet PUBLIC enet/include)
if(WIN32)
    target_link_libraries(enet PUBLIC winmm ws2_32)
endif()

# LZ4 compression library
add_library(lz4 STATIC
    lz4/lz4_simple.c
)

# Set up LZ4 target properties
target_include_directories(lz4 PUBLIC lz4)

# This target will be linked by the engine
