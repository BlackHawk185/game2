// LightSource.h - Light source definitions for raycast lighting system
// This defines different types of lights that can exist in the voxel world
#pragma once

#include "Math/Vec3.h"
#include <cstdint>
#include <vector>

// **LIGHT TYPES** - Different kinds of light sources in the game world
enum class LightType : uint8_t {
    DIRECTIONAL = 0,  // Sun/moon - infinite distance, parallel rays
    POINT = 1,        // Torch/lamp - radiates in all directions  
    SPOT = 2,         // Flashlight - cone of light
    AREA = 3          // Large light panels - soft shadows
};

// **LIGHT PROPERTIES** - Color and intensity information
struct LightColor {
    float r, g, b;    // RGB color components (0.0 to 1.0)
    float intensity;  // Brightness multiplier (1.0 = normal, 2.0 = twice as bright)
    
    LightColor() : r(1.0f), g(1.0f), b(1.0f), intensity(1.0f) {}
    LightColor(float red, float green, float blue, float intense = 1.0f) 
        : r(red), g(green), b(blue), intensity(intense) {}
    
    // Multiply color by intensity for final light contribution
    Vec3 getFinalColor() const {
        return Vec3(r * intensity, g * intensity, b * intensity);
    }
};

// **INDIVIDUAL LIGHT SOURCE** - Single light in the world
struct LightSource {
    LightType type;
    Vec3 position;          // World position (for point/spot lights)
    Vec3 direction;         // Direction vector (for directional/spot lights)
    LightColor color;       // Color and intensity
    float range;            // Maximum distance light travels (for point/spot)
    float spotAngle;        // Cone angle in degrees (for spot lights only)
    bool castsShadows;      // Whether this light creates shadows
    uint32_t islandID;      // Which island this light is attached to (0 = world light)
    
    // Constructor for different light types
    LightSource(LightType lightType, const Vec3& pos, const LightColor& col, float lightRange = 50.0f)
        : type(lightType), position(pos), direction(0, -1, 0), color(col), 
          range(lightRange), spotAngle(45.0f), castsShadows(true), islandID(0) {}
};

// **SoA LIGHT STORAGE** - Structure of Arrays for performance
// Following the AI_ASSISTANT_GUIDE.md directive for SoA layout
struct LightSystemSoA {
    // **PARALLEL ARRAYS** - Each light's data stored separately for cache efficiency
    std::vector<LightType> types;           // Light type for each light
    std::vector<Vec3> positions;            // World positions  
    std::vector<Vec3> directions;           // Direction vectors
    std::vector<LightColor> colors;         // Color and intensity data
    std::vector<float> ranges;              // Light range values
    std::vector<float> spotAngles;          // Spot light angles
    std::vector<bool> shadowCasters;        // Shadow casting flags
    std::vector<uint32_t> islandIDs;        // Island attachments
    
    // **PERFORMANCE BENEFITS** of SoA:
    // - CPU can process all positions at once (vectorization)
    // - Better cache usage when updating only certain properties
    // - GPU can efficiently stream data for compute shaders
    
    size_t size() const { return types.size(); }
    
    void addLight(const LightSource& light) {
        types.push_back(light.type);
        positions.push_back(light.position);
        directions.push_back(light.direction);
        colors.push_back(light.color);
        ranges.push_back(light.range);
        spotAngles.push_back(light.spotAngle);
        shadowCasters.push_back(light.castsShadows);
        islandIDs.push_back(light.islandID);
    }
    
    void clear() {
        types.clear();
        positions.clear();
        directions.clear();
        colors.clear();
        ranges.clear();
        spotAngles.clear();
        shadowCasters.clear();
        islandIDs.clear();
    }
    
    // Get light data at index (for compatibility with old code)
    LightSource getLight(size_t index) const {
        LightSource light(types[index], positions[index], colors[index], ranges[index]);
        light.direction = directions[index];
        light.spotAngle = spotAngles[index];
        light.castsShadows = shadowCasters[index];
        light.islandID = islandIDs[index];
        return light;
    }
};

// **LIGHTING SYSTEM** - Manages all lights in the world
class LightingSystem {
public:
    LightingSystem();
    ~LightingSystem() = default;
    
    // **LIGHT MANAGEMENT**
    uint32_t addLight(const LightSource& light);
    void removeLight(uint32_t lightID);
    void updateLight(uint32_t lightID, const LightSource& light);
    
    // **WORLD INTEGRATION**
    void addTorchLight(const Vec3& worldPos, uint32_t islandID);
    void addSunlight(const Vec3& direction, const LightColor& color);
    void addLavaLight(const Vec3& worldPos, uint32_t islandID);
    
    // **DATA ACCESS** - SoA for performance-critical operations
    const LightSystemSoA& getLightData() const { return m_lights; }
    size_t getLightCount() const { return m_lights.size(); }
    
    // **UTILITY METHODS**
    void setTimeOfDay(float timeOfDay); // 0.0 = midnight, 0.5 = noon
    void updateDynamicLights(float deltaTime);
    
    // **GETTER METHODS FOR RENDERER**
    Vec3 getSunDirection() const;
    Vec3 getSunColor() const;
    float getTimeOfDay() const { return m_currentTimeOfDay; }
    
private:
    LightSystemSoA m_lights;            // All lights in SoA format
    uint32_t m_nextLightID;             // ID counter for lights
    
    // **DAY/NIGHT CYCLE**
    uint32_t m_sunLightID;              // ID of the sun light
    float m_currentTimeOfDay;           // Current time (0.0 to 1.0)
    
    void updateSunlight();              // Update sun position/color based on time
};
