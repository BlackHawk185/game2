// BlockProperties.h - Block metadata and behavior properties
#pragma once

#include <cstdint>

// Block property flags and metadata
struct BlockProperties {
    // Basic properties
    float hardness = 1.0f;           // Mining time multiplier (0 = instant, higher = slower)
    bool isTransparent = false;      // For rendering/lighting (affects neighbor face culling)
    bool isLiquid = false;           // Fluid physics behavior
    bool isSolid = true;             // Has collision (players can't walk through)
    
    // Lighting
    bool emitsLight = false;         // Light source
    uint8_t lightLevel = 0;          // 0-15 (0 = no light, 15 = full brightness like sunlight)
    
    // Interaction
    bool isInteractable = false;     // Can right-click to open UI (for QFG config, chests, etc.)
    bool requiresSupport = false;    // Must be placed on solid block (like grass tufts, torches)
    
    // Rendering
    uint32_t textureIndex = 0;       // For voxel blocks (texture atlas index)
    
    // Special behaviors
    bool isQuantumField = false;     // For QFG territory/attunement system
    float tickRate = 0.0f;           // For blocks that update over time (0 = no ticking)
    
    // Default constructor (solid, non-special block)
    BlockProperties() = default;
    
    // Helper factory methods for common block types
    static BlockProperties Air() {
        BlockProperties props;
        props.isSolid = false;
        props.isTransparent = true;
        props.hardness = 0.0f;
        return props;
    }
    
    static BlockProperties Solid(float hardness = 1.0f) {
        BlockProperties props;
        props.hardness = hardness;
        props.isSolid = true;
        props.isTransparent = false;
        return props;
    }
    
    static BlockProperties Transparent(float hardness = 0.5f) {
        BlockProperties props;
        props.hardness = hardness;
        props.isSolid = false;
        props.isTransparent = true;
        return props;
    }
    
    static BlockProperties LightSource(uint8_t level, float hardness = 1.0f) {
        BlockProperties props;
        props.hardness = hardness;
        props.emitsLight = true;
        props.lightLevel = level;
        return props;
    }
    
    static BlockProperties QuantumFieldGenerator() {
        BlockProperties props;
        props.hardness = 10.0f;           // Very hard to break
        props.emitsLight = true;
        props.lightLevel = 15;            // Maximum brightness
        props.isQuantumField = true;
        props.isInteractable = true;      // Right-click to configure
        props.isSolid = true;
        props.tickRate = 1.0f;            // Updates once per second
        return props;
    }
};
