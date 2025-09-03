# Fluid Particle System Guide

## Overview

The fluid particle system implements a hybrid approach to water physics in the MMORPG engine. It uses particles for movement and entity-aware physics, while utilizing grid-based calculations for visual effects and container filling.

## Key Features

### ✅ Implemented
- **Particle-based Fluid Physics**: Each water particle is an ECS entity with physics properties
- **Gravity and Basic Collision**: Particles fall and bounce off terrain
- **Particle Spawning**: Press `F` key to spawn fluid particles in front of the camera
- **Visual Rendering**: Blue translucent cubes represent water particles
- **Evaporation System**: Particles automatically evaporate after 30 seconds if not in a container
- **Spatial Partitioning**: Efficient neighbor queries for performance
- **Container Detection**: Particles detect when they're in "containers" (slow-moving near ground)

### 🚧 Planned Features
- **Particle Connections**: Visual metaball/mesh blending for connected particles
- **Flow Simulation**: Water flowing from containers and between islands
- **Waterfall Effects**: Connected falling particle streams
- **Container/Pool Filling**: Grid-based water level calculation in holes
- **Shallow Water**: Thin water layers on flat surfaces
- **Inter-Island Transfer**: Water pouring from one floating island to another

## Controls

- **F Key**: Spawn a fluid particle in front of the camera with forward velocity
- **WASD**: Move camera around to observe particles
- **Mouse**: Look around

## Technical Architecture

### Components

1. **FluidParticleComponent**: Core particle properties (velocity, mass, radius, density)
2. **FluidContainerComponent**: Tracks particles in containers and fill levels
3. **FluidRenderComponent**: Visual effects and particle connections
4. **TransformComponent**: Position in world space
5. **VelocityComponent**: Physics velocity and acceleration

### Systems

- **FluidSystem**: Main update loop handling physics, collisions, and containers
- **VBORenderer**: Renders particles as blue translucent cubes
- **Spatial Grid**: Efficient neighbor queries for particle interactions

### Performance

- Uses Structure-of-Arrays (SoA) for cache efficiency
- Spatial partitioning for O(1) neighbor queries
- Configurable particle limits (default: 1000 particles)
- Efficient rendering with modern OpenGL

## Configuration

Key parameters in `FluidSystem.h`:

```cpp
m_gravity = Vec3(0, -9.81f, 0);      // Gravity force
m_evaporationTime = 30.0f;           // Evaporation time in seconds
m_maxParticles = 1000;               // Maximum particles
CELL_SIZE = 2.0f;                    // Spatial grid cell size
```

## Future Development

### Immediate Next Steps
1. **Metaball Rendering**: Implement visual blending between nearby particles
2. **Container Logic**: Add proper container detection based on terrain geometry
3. **Flow Physics**: Implement SPH (Smoothed Particle Hydrodynamics) for realistic flow

### Advanced Features
1. **Spring Systems**: Connect particles with springs for cohesion
2. **Pressure Simulation**: Implement pressure-based fluid dynamics
3. **Surface Tension**: Add surface tension effects for realistic water behavior
4. **Performance Optimization**: GPU-based particle simulation using compute shaders

## Integration Notes

- Fully integrated with ECS system
- Compatible with island entity architecture
- Uses existing physics system for terrain collision
- Properly initialized in main game loop
- Performance profiled and optimized for real-time gameplay

## Testing

To test the fluid system:
1. Run the game
2. Press `F` to spawn particles
3. Observe particles falling with gravity
4. Watch particles bounce off terrain
5. Notice particles evaporate after 30 seconds
6. Check console output for particle lifecycle messages

The system is designed to be modular and extensible, allowing for easy addition of new fluid mechanics and visual effects as the project evolves.
