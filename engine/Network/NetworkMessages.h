#pragma once

#include <cstdint>
#include "../Math/Vec3.h"

// Cross-platform structure packing
#ifdef _MSC_VER
    #pragma pack(push, 1)
    #define PACKED
#else
    #define PACKED __attribute__((packed))
#endif

// Network message types - use simple enum instead of enum class for network compatibility
enum NetworkMessageType : uint8_t {
    HELLO_WORLD = 1,
    PLAYER_MOVEMENT_REQUEST = 2,
    PLAYER_POSITION_UPDATE = 3,
    CHAT_MESSAGE = 4,
    WORLD_STATE = 5,
    COMPRESSED_ISLAND_DATA = 6
};

// Simple hello world message
struct PACKED HelloWorldMessage {
    uint8_t type = HELLO_WORLD;
    char message[32] = "Hello from server!";
};

// Player movement request from client to server
struct PACKED PlayerMovementRequest {
    uint8_t type = PLAYER_MOVEMENT_REQUEST;
    uint32_t sequenceNumber;
    Vec3 intendedPosition;
    Vec3 velocity;
    float deltaTime;
};

// Player position update from server to clients
struct PACKED PlayerPositionUpdate {
    uint8_t type = PLAYER_POSITION_UPDATE;
    uint32_t playerId;
    uint32_t sequenceNumber;
    Vec3 position;
    Vec3 velocity;
};

// Simple chat message
struct PACKED ChatMessage {
    uint8_t type = CHAT_MESSAGE;
    char message[256];
};

// Basic world state - simplified for initial implementation
struct PACKED WorldStateMessage {
    uint8_t type = WORLD_STATE;
    uint32_t numIslands;
    // For simplicity, include positions of first 3 islands
    Vec3 islandPositions[3];
    Vec3 playerSpawnPosition;
};

// Compressed island chunk data for efficient transmission
struct PACKED CompressedIslandHeader {
    uint8_t type = COMPRESSED_ISLAND_DATA;
    uint32_t islandID;
    Vec3 position;
    uint32_t originalSize;      // Uncompressed voxel data size (should be 32*32*32 = 32768)
    uint32_t compressedSize;    // Size of the compressed data that follows
    // Compressed voxel data follows this header (variable length)
};

// Maximum size for compressed data (conservative estimate)
constexpr uint32_t MAX_COMPRESSED_ISLAND_SIZE = 16384; // 16KB max compressed size

// Restore packing
#ifdef _MSC_VER
    #pragma pack(pop)
#endif

#undef PACKED
