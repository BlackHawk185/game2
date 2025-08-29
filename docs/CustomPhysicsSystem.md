# Custom Physics System Documentation

## Overview

The MMORPG engine implements a custom voxel-based collision detection system that is tightly integrated with the mesh generation pipeline. This approach eliminates the need for external physics engines like Bullet or Jolt by leveraging the existing voxel face culling data.

## Architecture

### Dual-Mesh Generation

The system generates two meshes simultaneously during voxel chunk processing:

1. **Render Mesh**: For visual rendering (vertices, indices, normals, UVs)
2. **Collision Mesh**: For physics collision detection (face positions and normals)

```cpp
void VoxelChunk::generateMesh() {
    // Single-pass generation for both meshes
    for (int x = 0; x < SIZE; ++x) {
        for (int y = 0; y < SIZE; ++y) {
            for (int z = 0; z < SIZE; ++z) {
                if (shouldRenderFace(x, y, z, face)) {
                    // Add to render mesh
                    addQuad(mesh.vertices, mesh.indices, x, y, z, face, blockType);
                    
                    // Add to collision mesh
                    addCollisionQuad(x, y, z, face);
                }
            }
        }
    }
}
```

### Collision Detection

#### Player-World Collision
- **Algorithm**: Sphere-plane intersection
- **Data**: Uses exposed voxel faces from collision mesh
- **Performance**: Only checks chunks within player radius
- **Response**: Provides collision normal for friction-based movement

```cpp
bool PhysicsSystem::checkPlayerCollision(const Vec3& playerPos, Vec3& outNormal, float playerRadius) {
    // Check collision with all collision faces in nearby chunks
    for (const auto& face : collisionMesh.faces) {
        // Sphere-plane collision detection
        Vec3 faceToPlayer = playerPos - face.position;
        float distanceToPlane = faceToPlayer.dot(face.normal);
        
        if (abs(distanceToPlane) <= playerRadius) {
            // Check if within face bounds
            if (withinFaceBounds(playerPos, face)) {
                outNormal = face.normal;
                return true;
            }
        }
    }
}
```

#### Ray-World Collision
- **Purpose**: Block interaction (break/place) and line-of-sight
- **Algorithm**: Ray-plane intersection with collision faces
- **Optimization**: Early termination on first hit

## Data Structures

### CollisionFace
```cpp
struct CollisionFace {
    Vec3 position;  // Face center
    Vec3 normal;    // Face normal (outward)
};
```

### CollisionMesh
```cpp
struct CollisionMesh {
    std::vector<CollisionFace> faces;
    bool needsUpdate = true;
};
```

## Performance Characteristics

### Memory Efficiency
- **Shared Data**: Face culling data used for both render and collision
- **SoA Layout**: Collision faces stored in Structure of Arrays format
- **Minimal Overhead**: Only exposed faces stored, not solid voxels

### CPU Performance
- **Single-Pass Generation**: Both meshes created in one iteration
- **Spatial Optimization**: Only check chunks within collision radius
- **Cache Friendly**: Linear iteration through collision faces

### Scalability
- **Per-Chunk**: Collision detection scales with visible geometry
- **Multi-threaded**: Chunk processing can be parallelized
- **Lazy Updates**: Collision mesh only rebuilt when voxels change

## Integration Points

### Network Synchronization
The custom physics system integrates seamlessly with the network layer:

```cpp
// Server validates movement using custom collision
if (g_physics.checkPlayerCollision(newPosition, normal)) {
    // Apply collision response
    // Broadcast corrected position to clients
}
```

### Island Movement
Islands can move as rigid bodies while maintaining per-voxel collision:

```cpp
// Update island position
island.physicsCenter += island.velocity * deltaTime;

// All chunks move together
// Individual voxel collision remains relative to island
```

### Job System Integration
Collision detection can be distributed across worker threads:

```cpp
enum JobType {
    CHUNK_MESHING,          // Generate render + collision meshes
    COLLISION_CHECK,        // Validate player movement
    COLLISION_RESPONSE      // Apply collision forces
};
```

## Advantages over External Physics Engines

1. **Perfect Integration**: Collision data directly from voxel geometry
2. **Memory Efficiency**: No duplicate collision geometry storage
3. **Deterministic**: Exact same collision data on client and server
4. **Lightweight**: No external dependencies or complex physics simulation
5. **Voxel-Optimized**: Designed specifically for blocky voxel worlds
6. **Network Friendly**: Collision state is implicit in voxel data

## Future Enhancements

### Planned Features
- **Compound Collision**: Multiple collision shapes per entity
- **Continuous Collision**: Swept sphere collision for fast-moving objects
- **Friction Materials**: Different friction coefficients per voxel type
- **Collision Layers**: Separate collision masks for different entity types

### Performance Optimizations
- **Spatial Hashing**: Broad-phase collision culling
- **LOD Collision**: Simplified collision for distant objects
- **Async Updates**: Background collision mesh generation
- **SIMD Optimization**: Vectorized collision detection
