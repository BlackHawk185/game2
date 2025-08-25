# Unified Entity Synchronization System

## Overview

The engine now includes a unified entity synchronization system that handles networking for both islands and players using the same core system. This eliminates the need for separate synchronization mechanisms and provides a consistent approach to entity networking.

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
// In your physics update loop for islands
void updateIslandPhysics(FloatingIsland& island, float deltaTime) {
    // Update island physics
    island.velocity.y -= 9.81f * deltaTime; // Gravity
    island.position += island.velocity * deltaTime;
    
    // Broadcast state to all clients
    if (integratedServer && integratedServer->isRunning()) {
        integratedServer->broadcastEntityStateUpdate(
            island.islandID,           // entityID
            1,                         // entityType (Island)
            island.physicsCenter,      // position
            island.velocity,           // velocity
            island.acceleration,       // acceleration
            0                          // flags
        );
    }
}
```

### Server Side - Broadcasting Player Movement

```cpp
// In your player update loop
void updatePlayerMovement(Player& player, float deltaTime) {
    // Update player physics
    player.position += player.velocity * deltaTime;
    
    // Broadcast state to all clients
    if (integratedServer && integratedServer->isRunning()) {
        integratedServer->broadcastEntityStateUpdate(
            player.playerID,           // entityID
            0,                         // entityType (Player)
            player.position,           // position
            player.velocity,           // velocity
            player.acceleration,       // acceleration
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
                // Update island physics state
                island->physicsCenter = update.position;
                island->velocity = update.velocity;
                island->acceleration = update.acceleration;
                
                // Update all players on this island
                updatePlayersOnIsland(island);
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

1. Server updates island position/velocity
2. Server broadcasts island entity update
3. Clients receive island update and move all players on that island
4. Players maintain their LOCAL position relative to the island

### Lag Compensation
The system includes server timestamps for proper lag compensation:

```cpp
// Client-side position interpolation
void interpolateEntityPosition(Entity& entity, float currentTime) {
    float timeSinceUpdate = currentTime - entity.lastUpdateTime;
    Vec3 predictedPos = entity.networkPosition + 
                       entity.networkVelocity * timeSinceUpdate;
    
    // Smoothly blend current position toward predicted position
    entity.position = lerp(entity.position, predictedPos, 0.1f);
}
```

### Message Frequency
- **Islands**: Update every physics tick (~60Hz) when moving
- **Players**: Update every movement tick (~60Hz) when moving  
- **Voxels**: Only on change events (event-driven)

## Architecture Benefits

1. **Consistency**: Same message format for all entity types
2. **Efficiency**: Single network system instead of multiple specialized systems
3. **Scalability**: Easy to add new entity types
4. **Maintainability**: Less code duplication and complexity
5. **Performance**: Unified batching and compression opportunities

This unified system provides a solid foundation for the MMORPG's networking needs while remaining simple and extensible.
