# Unified Entity Synchronization System

## Overview

The engine includes a unified entity synchronization system that handles networking for both islands and players using the same core system. This eliminates the need for separate synchronization mechanisms and provides a consistent approach to entity networking with custom voxel-based collision detection.

## Key Features

- **Unified System**: Both islands and players use the same `EntityStateUpdate` message
- **Physics Properties**: Synchronizes position, velocity, and acceleration for smooth prediction
- **Entity Types**: Supports different entity types (Player=0, Island=1, NPC=2, etc.)
- **Server Timestamps**: Includes server timing for lag compensation
- **Extensible**: Easy to add new entity types without changing the core system

## Network Messages

### EntityStateUpdate
```cpp
struct EntityStateUpdate {
    uint8_t type = ENTITY_STATE_UPDATE;
    uint32_t sequenceNumber;
    uint32_t entityID;           // Unique entity identifier
    uint8_t entityType;          // 0=Player, 1=Island, 2=NPC, etc.
    Vec3 position;               // Current position
    Vec3 velocity;               // Current velocity  
    Vec3 acceleration;           // For smooth prediction/interpolation
    uint32_t serverTimestamp;    // Server time for lag compensation
    uint8_t flags;               // Bit flags (isGrounded, needsCorrection, etc.)
};
```

### VoxelChangeRequest/Update
```cpp
struct VoxelChangeRequest {
    uint8_t type = VOXEL_CHANGE_REQUEST;
    uint32_t sequenceNumber;
    uint32_t islandID;
    Vec3 localPos;
    uint8_t voxelType; // 0 = air (break), 1+ = place block
};

struct VoxelChangeUpdate {
    uint8_t type = VOXEL_CHANGE_UPDATE;
    uint32_t sequenceNumber;
    uint32_t islandID;
    Vec3 localPos;
    uint8_t voxelType;
    uint32_t authorPlayerId; // Player who made the change
};
```

## Usage Examples

### Server Side - Broadcasting Island Movement

```cpp
// In your island physics update loop (custom collision system)
void updateIslandPhysics(FloatingIsland& island, float deltaTime) {
    // Update island position using custom movement system
    // island.velocity.y -= 9.81f * deltaTime; // Gravity (if implemented)
    // island.position += island.velocity * deltaTime;
    
    // Broadcast state to all clients (custom physics active)
    if (integratedServer && integratedServer->isRunning()) {
        integratedServer->broadcastEntityStateUpdate(
            island.islandID,           // entityID
            1,                         // entityType (Island)
            island.physicsCenter,      // position
            island.velocity,           // velocity (custom physics)
            island.acceleration,       // acceleration (custom physics)
            0                          // flags
        );
    }
}
```

### Server Side - Broadcasting Player Movement

```cpp
// In your player update loop (custom collision system)
void updatePlayerMovement(Player& player, float deltaTime) {
    // Update player using custom voxel-face collision detection
    // player.position += player.velocity * deltaTime;
    
    // Broadcast state to all clients (custom physics active)
    if (integratedServer && integratedServer->isRunning()) {
        integratedServer->broadcastEntityStateUpdate(
            player.playerID,           // entityID
            0,                         // entityType (Player)
            player.position,           // position
            player.velocity,           // velocity (custom physics)
            player.acceleration,       // acceleration (custom physics)
            player.isGrounded ? 1 : 0  // flags
        );
    }
}
```

### Client Side - Handling Entity Updates

```cpp
// Set up entity update callback
networkClient->onEntityStateReceived = [this](const EntityStateUpdate& update) {
    switch (update.entityType) {
        case 0: { // Player
            auto* player = findPlayerByID(update.entityID);
            if (player) {
                // Apply position smoothing/interpolation
                player->networkPosition = update.position;
                player->networkVelocity = update.velocity;
                player->lastUpdateTime = getCurrentTime();
            }
            break;
        }
        
        case 1: { // Island
            auto* island = findIslandByID(update.entityID);
            if (island) {
                // Update island state (custom physics system)
                island->physicsCenter = update.position;
                // island->velocity = update.velocity;      // When custom physics active
                // island->acceleration = update.acceleration; // When custom physics active
                
                // Update all players on this island (custom collision system)
                // updatePlayersOnIsland(island);
            }
            break;
        }
    }
};
```

### Client Side - Sending Voxel Changes

```cpp
// In block interaction handler
void GameClient::processBlockInteraction(const Vec3& worldPos, bool isPlacing) {
    // Find which island this position belongs to
    auto* island = findIslandContainingPosition(worldPos);
    if (!island) return;
    
    // Convert to local island coordinates
    Vec3 localPos = worldPos - island->physicsCenter;
    uint8_t voxelType = isPlacing ? 1 : 0; // 0 = air (break), 1+ = place
    
    // Send request to server
    if (networkClient && networkClient->isConnected()) {
        networkClient->sendVoxelChangeRequest(island->islandID, localPos, voxelType);
    }
    
    // Apply optimistically for immediate visual feedback
    applyVoxelChange(island, localPos, voxelType);
}
```

### Client Side - Handling Voxel Updates

```cpp
// Set up voxel change callback  
networkClient->onVoxelChangeReceived = [this](const VoxelChangeUpdate& update) {
    auto* island = findIslandByID(update.islandID);
    if (island) {
        // Apply the authoritative voxel change
        applyVoxelChange(island, update.localPos, update.voxelType);
        
        // Mark chunk for mesh regeneration
        markChunkForUpdate(island, update.localPos);
        
        std::cout << "Player " << update.authorPlayerId 
                  << " modified voxel at island " << update.islandID << std::endl;
    }
};
```

## Entity Types

- **0 = Player**: Player characters that can move between islands
- **1 = Island**: Floating islands that are the primary game entities
- **2 = NPC**: Non-player characters (future expansion)
- **3 = Projectile**: Arrows, spells, etc. (future expansion)
- **4 = Vehicle**: Ships, flying mounts, etc. (future expansion)

## Implementation Notes

### Island-Player Relationship
When islands move, all players on them need to move with them. The relationship is handled as follows:

1. Server updates island position/velocity (custom movement system)
2. Server broadcasts island entity update
3. Clients receive island update and move all players on that island
4. Players maintain their LOCAL position relative to the island

### Current Physics Status
- **Custom Physics System**: Voxel-face-based collision detection integrated with mesh generation
- **Dual-Mesh Generation**: Single-pass creation of both render and collision meshes
- **Sphere-Face Collision**: Player collision uses sphere-plane intersection with voxel faces
- **Network Synchronization**: Entity synchronization system integrated with custom collision
- **Performance**: SoA data layout optimized for voxel-based collision calculations

### Lag Compensation
The system includes server timestamps for proper lag compensation:

```cpp
// Client-side position interpolation (when physics is active)
void interpolateEntityPosition(Entity& entity, float currentTime) {
    float timeSinceUpdate = currentTime - entity.lastUpdateTime;
    Vec3 predictedPos = entity.networkPosition + 
                       entity.networkVelocity * timeSinceUpdate;
    
    // Smoothly blend current position toward predicted position
    entity.position = lerp(entity.position, predictedPos, 0.1f);
}
```

### Message Frequency
- **Islands**: Update every physics tick (~60Hz) when moving (when physics active)
- **Players**: Update every movement tick (~60Hz) when moving (when physics active)
- **Voxels**: Only on change events (event-driven)

## Architecture Benefits

1. **Consistency**: Same message format for all entity types
2. **Efficiency**: Single network system instead of multiple specialized systems
3. **Scalability**: Easy to add new entity types
4. **Maintainability**: Less code duplication and complexity
5. **Performance**: Unified batching and compression opportunities
6. **Future-Ready**: Designed to work seamlessly with future physics engine integration

## Physics Integration Status

The unified entity synchronization system is **fully ready** for physics engine integration:

- ✅ **Network Messages**: EntityStateUpdate supports velocity, acceleration, and physics flags
- ✅ **Server Architecture**: GameServer has physics update loops and broadcasting
- ✅ **Client Architecture**: GameClient has interpolation and prediction systems
- ✅ **Data Layout**: SoA optimization ready for physics calculations
- ⏳ **Physics Engine**: Currently stub implementation, ready for future integration

When a physics engine is integrated:
1. Uncomment physics calculations in server update loops
2. Enable velocity/acceleration broadcasting  
3. Activate client-side interpolation and prediction
4. Enable island-player momentum relationships

This unified system provides a solid foundation for the MMORPG's networking needs while remaining simple and extensible.
