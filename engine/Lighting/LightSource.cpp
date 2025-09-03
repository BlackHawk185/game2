// LightSource.cpp - Implementation of lighting system
// This implements the light management system for raycast lighting
#include "LightSource.h"
#include <cmath>
#include <algorithm>

LightingSystem::LightingSystem()
    : m_nextLightID(1), m_sunLightID(0), m_currentTimeOfDay(0.5f)
{
    std::cout << "🌅 LightingSystem constructor: Initial time=" << m_currentTimeOfDay << std::endl;
    
    // **SETUP DEFAULT LIGHTING** - Create basic sun light for outdoor scenes
    // This gives us immediate lighting without requiring manual setup
    LightColor sunColor(1.0f, 0.95f, 0.8f, 1.5f); // Slightly warm white
    
    addSunlight(Vec3(0.3f, -0.7f, 0.2f), sunColor); // Angled like afternoon sun
    
    std::cout << "🌅 LightingSystem constructor complete: time=" << m_currentTimeOfDay 
              << ", sun color=(" << getSunColor().x << "," << getSunColor().y << "," << getSunColor().z << ")" << std::endl;
}

uint32_t LightingSystem::addLight(const LightSource& light)
{
    uint32_t lightID = m_nextLightID++;
    m_lights.addLight(light);
    return lightID;
}

void LightingSystem::removeLight(uint32_t lightID)
{
    // **NOTE**: For simplicity, we're not implementing removal yet
    // In a full implementation, you'd need to track ID->index mapping
    // and handle array compaction when removing lights
    // This is sufficient for the initial raycast lighting demo
}

void LightingSystem::updateLight(uint32_t lightID, const LightSource& light)
{
    // **NOTE**: Similar to removal, update needs ID->index mapping
    // For now, we'll focus on adding and using lights
}

void LightingSystem::addTorchLight(const Vec3& worldPos, uint32_t islandID)
{
    // **TORCH LIGHT** - Warm, flickering point light
    // Perfect for underground areas and nighttime
    LightColor torchColor(1.0f, 0.7f, 0.3f, 2.0f); // Warm orange/yellow
    
    LightSource torch(LightType::POINT, worldPos, torchColor, 15.0f);
    torch.islandID = islandID;
    torch.castsShadows = true;
    
    addLight(torch);
}

void LightingSystem::addSunlight(const Vec3& direction, const LightColor& color)
{
    // **SUN LIGHT** - Directional light for outdoor lighting
    // Provides the main illumination for the world
    LightSource sun(LightType::DIRECTIONAL, Vec3(0, 100, 0), color, 1000.0f);
    sun.direction = direction;
    sun.castsShadows = true;
    sun.islandID = 0; // World light, not attached to any island
    
    m_sunLightID = addLight(sun);
}

void LightingSystem::addLavaLight(const Vec3& worldPos, uint32_t islandID)
{
    // **LAVA LIGHT** - Bright red-orange light for volcanic areas
    // Creates dramatic lighting with strong color tinting
    LightColor lavaColor(1.0f, 0.3f, 0.1f, 3.0f); // Bright red-orange
    
    LightSource lava(LightType::POINT, worldPos, lavaColor, 25.0f);
    lava.islandID = islandID;
    lava.castsShadows = true;
    
    addLight(lava);
}

void LightingSystem::setTimeOfDay(float timeOfDay)
{
    m_currentTimeOfDay = std::clamp(timeOfDay, 0.0f, 1.0f);
    updateSunlight();
}

void LightingSystem::updateDynamicLights(float deltaTime)
{
    // **DYNAMIC LIGHTING EFFECTS**
    // Update flickering torches, moving lights, etc.
    
    // Update time of day (optional automatic progression)
    // m_currentTimeOfDay += deltaTime * 0.01f; // Very slow day/night cycle
    // if (m_currentTimeOfDay > 1.0f) m_currentTimeOfDay = 0.0f;
    
    updateSunlight();
    
    // **TORCH FLICKERING** - Add subtle animation to torch lights
    // This could be expanded to modify torch intensities slightly over time
    // for (size_t i = 0; i < m_lights.size(); ++i) {
    //     if (m_lights.types[i] == LightType::POINT) {
    //         // Add flickering logic here
    //     }
    // }
}

void LightingSystem::updateSunlight()
{
    if (m_sunLightID == 0 || m_lights.size() == 0) return;
    
    // **DAY/NIGHT CYCLE CALCULATION**
    // 0.0 = midnight, 0.25 = sunrise, 0.5 = noon, 0.75 = sunset
    
    // Calculate sun angle based on time of day
    // Convert time to angle: 0.0 -> π, 0.5 -> 0, 1.0 -> π
    float sunAngle = m_currentTimeOfDay * 2.0f * 3.14159f; // 0 to 2π
    float sunHeight = std::sin(sunAngle); // -1 to 1, but we want noon=1
    
    // Fix the calculation: noon (0.5) should give sunHeight = 1
    // Use cosine instead: cos(0.5 * 2π) = cos(π) = -1, so adjust
    sunHeight = -std::cos(sunAngle); // Now noon (π) gives cos(π) = -1, so -cos(π) = 1
    
    // Update sun direction - moves across the sky
    Vec3 sunDirection;
    sunDirection.x = std::sin(sunAngle);          // East to west movement
    sunDirection.y = -std::abs(sunHeight);        // Always pointing down (negative Y)
    sunDirection.z = std::cos(sunAngle) * 0.3f;   // Slight north/south variation
    
    // Normalize the direction
    float length = std::sqrt(sunDirection.x * sunDirection.x + 
                           sunDirection.y * sunDirection.y + 
                           sunDirection.z * sunDirection.z);
    if (length > 0.001f) {
        sunDirection.x /= length;
        sunDirection.y /= length;
        sunDirection.z /= length;
    }
    
    // Update sun color based on time of day
    LightColor sunColor;
    if (sunHeight > 0.0f) {
        // **DAYTIME** - Bright white/yellow light
        float dayBrightness = sunHeight * 1.5f; // Brighter at noon
        sunColor.r = 1.0f;
        sunColor.g = 0.95f + sunHeight * 0.05f; // Slightly more yellow at noon
        sunColor.b = 0.8f + sunHeight * 0.2f;   // Less blue at horizon
        sunColor.intensity = dayBrightness;
    } else {
        // **NIGHTTIME** - Dark blue ambient light
        float nightBrightness = std::max(0.1f, -sunHeight * 0.3f);
        sunColor.r = 0.2f;
        sunColor.g = 0.2f;
        sunColor.b = 0.4f; // Slightly blue nighttime
        sunColor.intensity = nightBrightness;
    }
    
    // Find and update the sun light (simplified - assumes it's the first directional light)
    for (size_t i = 0; i < m_lights.size(); ++i) {
        if (m_lights.types[i] == LightType::DIRECTIONAL) {
            m_lights.directions[i] = sunDirection;
            m_lights.colors[i] = sunColor;
            break;
        }
    }
}

Vec3 LightingSystem::getSunDirection() const
{
    // Find the first directional light (sun)
    for (size_t i = 0; i < m_lights.size(); ++i) {
        if (m_lights.types[i] == LightType::DIRECTIONAL) {
            return m_lights.directions[i];
        }
    }
    // Fallback to default sun direction
    return Vec3(0.5f, -0.8f, 0.3f);
}

Vec3 LightingSystem::getSunColor() const
{
    // Find the first directional light (sun)
    for (size_t i = 0; i < m_lights.size(); ++i) {
        if (m_lights.types[i] == LightType::DIRECTIONAL) {
            const LightColor& color = m_lights.colors[i];
            Vec3 result = Vec3(color.r, color.g, color.b) * color.intensity;
            return result;
        }
    }
    // Fallback to default sun color
    return Vec3(1.0f, 0.9f, 0.8f);
}
