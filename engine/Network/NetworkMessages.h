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

// Network message types - Essential for entity-based MMORPG
enum NetworkMessageType : uint8_t {
    HELLO_WORLD = 1,
    COMPRESSED_CHUNK_DATA = 2,
    PLAYER_MOVEMENT_REQUEST = 3,
    PLAYER_POSITION_UPDATE = 4,
    ENTITY_STATE_UPDATE = 5
};

// Simple hello world message for connection testing
struct PACKED HelloWorldMessage {
    uint8_t type = HELLO_WORLD;
    char message[32] = "Hello from server!";
};

// Individual chunk data for world transmission
struct PACKED CompressedChunkHeader {
    uint8_t type = COMPRESSED_CHUNK_DATA;
    uint32_t islandID;              // Which island this chunk belongs to
    Vec3 chunkCoord;                // Chunk coordinate within the island (0,0,0), (1,0,0), etc.
    Vec3 islandPosition;            // Island's physics center for positioning
    uint32_t originalSize;          // Uncompressed voxel data size (should be 16*16*16 = 4096)
    uint32_t compressedSize;        // Size of the compressed data that follows
    // Compressed voxel data follows this header (variable length)
};

// Player movement request (client -> server)
struct PACKED PlayerMovementRequest {
    uint8_t type = PLAYER_MOVEMENT_REQUEST;
    uint32_t sequenceNumber;        // For client-side prediction
    Vec3 intendedPosition;          // Where player wants to move
    Vec3 velocity;                  // Player's velocity
    float deltaTime;                // Time delta for this movement
};

// Player position update (server -> client)
struct PACKED PlayerPositionUpdate {
    uint8_t type = PLAYER_POSITION_UPDATE;
    uint32_t playerId;              // Which player this update is for
    Vec3 position;                  // Authoritative position
    Vec3 velocity;                  // Current velocity
    uint32_t timestamp;             // Server timestamp
};

// Generic entity state update (for islands, NPCs, etc.)
struct PACKED EntityStateUpdate {
    uint8_t type = ENTITY_STATE_UPDATE;
    uint32_t entityId;              // Unique entity ID
    uint8_t entityType;             // Type of entity (island=1, player=2, npc=3, etc.)
    Vec3 position;                  // Entity position
    Vec3 velocity;                  // Entity velocity
    uint32_t timestamp;             // Server timestamp
    // Additional entity-specific data can follow
};

// Maximum size for compressed data
constexpr uint32_t MAX_COMPRESSED_CHUNK_SIZE = 16384;  // 16KB max compressed chunk size

// Restore packing
#ifdef _MSC_VER
    #pragma pack(pop)
#endif

#undef PACKED
