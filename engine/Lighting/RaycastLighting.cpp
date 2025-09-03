// RaycastLighting.cpp - Reverse raycast lighting implementation for voxel cube faces
// This shoots rays FROM cube faces TO light sources for realistic shadow calculation
#include "RaycastLighting.h"
#include "World/IslandChunkSystem.h"
#include "World/VoxelRaycaster.h"
#include <cmath>
#include <algorithm>
#include <chrono>

RaycastLighting::RaycastLighting(LightingSystem* lightSystem, IslandChunkSystem* worldSystem)
    : m_lightingSystem(lightSystem), m_worldSystem(worldSystem), 
      m_defaultQuality(ReverseRaycastQuality::balanced()), m_currentTimestamp(0)
{
    std::cout << "🔥 RaycastLighting: Reverse raycast system initialized for cube face lighting" << std::endl;
}

// **CORE FACE LIGHTING** - Calculate lighting for a specific cube face
FaceLighting RaycastLighting::calculateFaceLighting(const Vec3& faceCenter, const Vec3& faceNormal, const ReverseRaycastQuality& quality)
{
    auto startTime = std::chrono::high_resolution_clock::now();
    
    FaceLighting result;
    
    // **CHECK CACHE FIRST** - Avoid recalculating if we already know the lighting here
    if (getCachedFaceLighting(faceCenter, CubeFace::POSITIVE_Z, result)) { // TODO: Pass actual face
        m_stats.cacheHits++;
        return result;
    }
    m_stats.cacheMisses++;
    
    // **STEP 1: DIRECT LIGHTING** - Shoot rays from face center to each light source
    Vec3 totalColor(0, 0, 0);
    float totalBrightness = 0.0f;
    float shadowFactor = 1.0f;
    
    if (!m_lightingSystem) {
        result.color = Vec3(0.1f, 0.1f, 0.1f); // Minimal fallback lighting
        return result;
    }
    
    const auto& lightData = m_lightingSystem->getLightData();
    
    for (size_t i = 0; i < lightData.size(); ++i) {
        // Get light properties from SoA arrays
        LightSource light = lightData.getLight(i);
        
        // **REVERSE RAYCAST** - Shoot ray FROM face TO light
        Vec3 lightContribution = calculateDirectLightToFace(light, faceCenter, faceNormal, quality);
        
        totalColor = Vec3(totalColor.x + lightContribution.x, 
                         totalColor.y + lightContribution.y, 
                         totalColor.z + lightContribution.z);
        
        // Track brightness for overall lighting level
        float lightIntensity = lightContribution.x + lightContribution.y + lightContribution.z;
        totalBrightness += lightIntensity;
        
        m_stats.raysShot++;
    }
    
    // **STEP 2: REFLECTED LIGHTING** - Calculate light bouncing off nearby surfaces
    if (quality.enableReflectedLight && quality.bounceRays > 0) {
        Vec3 reflectedLight = calculateReflectedLight(faceCenter, faceNormal, quality);
        result.reflectedColor = reflectedLight;
        
        // Add reflected light to total
        totalColor = Vec3(totalColor.x + reflectedLight.x * 0.3f,  // Reduced intensity for bounced light
                         totalColor.y + reflectedLight.y * 0.3f,
                         totalColor.z + reflectedLight.z * 0.3f);
    }
    
    // **STEP 3: FINALIZE RESULTS**
    result.color = totalColor;
    result.brightness = totalBrightness;
    result.shadowFactor = shadowFactor;
    
    // **CACHE THE RESULT**
    cacheFaceLighting(faceCenter, CubeFace::POSITIVE_Z, result); // TODO: Pass actual face
    
    // **PERFORMANCE TRACKING**
    auto endTime = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime);
    m_stats.totalTime += duration.count() / 1000.0f; // Convert to milliseconds
    m_stats.facesProcessed++;
    
    return result;
}

// **VOXEL FACE LIGHTING** - Calculate lighting for a specific voxel face with LOD
FaceLighting RaycastLighting::calculateVoxelFaceLighting(uint32_t islandID, const Vec3& voxelPos, CubeFace face, const Vec3& cameraPos)
{
    // **DISTANCE-BASED LOD CULLING** for performance
    float distanceToCamera = (voxelPos - cameraPos).length();
    
    // **LOD LEVELS**:
    // - Close (0-500 units): Full raycast lighting
    // - Medium (500-1000 units): Simplified lighting (no shadows)  
    // - Far (1000+ units): Ambient only
    
    if (distanceToCamera > 1000.0f) {
        // **FAR LOD**: Return ambient lighting only
        FaceLighting result;
        result.color = Vec3(0.2f, 0.2f, 0.25f); // Soft blue ambient
        result.reflectedColor = Vec3(0, 0, 0);
        result.brightness = 0.2f; // Low ambient brightness
        result.shadowFactor = 1.0f;   // No shadows
        return result;
    }
    
    Vec3 faceCenter = getFaceCenter(voxelPos, face);
    Vec3 faceNormal = getFaceNormal(face);
    
    if (distanceToCamera > 500.0f) {
        // **MEDIUM LOD**: Simple lighting without expensive raycast shadows
        ReverseRaycastQuality simplifiedQuality = ReverseRaycastQuality::fastest();
        return calculateFaceLighting(faceCenter, faceNormal, simplifiedQuality);
    }
    
    // **CLOSE LOD**: Full quality raycast lighting
    return calculateFaceLighting(faceCenter, faceNormal, m_defaultQuality);
}

// **REVERSE RAYCAST HELPER** - Calculate direct lighting from face to single light source
Vec3 RaycastLighting::calculateDirectLightToFace(const LightSource& light, const Vec3& faceCenter, const Vec3& faceNormal, const ReverseRaycastQuality& quality)
{
    Vec3 lightDirection;
    Vec3 lightPosition;
    float distance = 0.0f;
    
    // **DETERMINE LIGHT DIRECTION AND DISTANCE**
    if (light.type == LightType::DIRECTIONAL) {
        // Directional light (sun) - direction is constant
        lightDirection = Vec3(-light.direction.x, -light.direction.y, -light.direction.z); // Reverse direction
        lightPosition = faceCenter + lightDirection * 1000.0f; // Far away position for raycast
        distance = 1000.0f;
    } else {
        // Point/Spot light - calculate direction and distance
        Vec3 lightDir = light.position - faceCenter;
        distance = std::sqrt(lightDir.x * lightDir.x + lightDir.y * lightDir.y + lightDir.z * lightDir.z);
        
        if (distance < 0.001f) return Vec3(0, 0, 0); // Too close
        if (distance > light.range || distance > quality.maxLightDistance) return Vec3(0, 0, 0); // Too far
        
        lightDirection = Vec3(lightDir.x / distance, lightDir.y / distance, lightDir.z / distance);
        lightPosition = light.position;
    }
    
    // **ANGLE CHECK** - Only light faces that are facing the light
    float angleFactor = std::max(0.0f, faceNormal.x * lightDirection.x + 
                                      faceNormal.y * lightDirection.y + 
                                      faceNormal.z * lightDirection.z);
    if (angleFactor < 0.001f) return Vec3(0, 0, 0); // Face is pointing away from light
    
    // **SHADOW CHECK** - Cast ray from face to light to check for occlusion
    bool inShadow = isInShadow(faceCenter, lightPosition, light);
    if (inShadow) {
        return Vec3(0, 0, 0); // Face is in shadow
    }
    
    // **CALCULATE LIGHT CONTRIBUTION**
    float attenuation = calculateAttenuation(distance, light.range);
    Vec3 lightColor = light.color.getFinalColor();
    
    return Vec3(lightColor.x * angleFactor * attenuation,
                lightColor.y * angleFactor * attenuation,
                lightColor.z * angleFactor * attenuation);
}

// **REFLECTED LIGHT CALCULATION** - Calculate light bouncing off nearby surfaces
Vec3 RaycastLighting::calculateReflectedLight(const Vec3& faceCenter, const Vec3& faceNormal, const ReverseRaycastQuality& quality)
{
    // TODO: Implement bounce lighting calculation
    // For now, return minimal reflected light
    return Vec3(0.02f, 0.02f, 0.03f); // Slight blue ambient
}

// **SHADOW CHECK** - Cast ray from face center to light to check for occlusion
bool RaycastLighting::isInShadow(const Vec3& faceCenter, const Vec3& lightPosition, const LightSource& light)
{
    if (!m_worldSystem) return false;
    
    // **CALCULATE RAY DIRECTION** from face to light
    Vec3 rayDirection = lightPosition - faceCenter;
    float rayLength = std::sqrt(rayDirection.x * rayDirection.x + rayDirection.y * rayDirection.y + rayDirection.z * rayDirection.z);
    
    if (rayLength < 0.001f) return false;
    
    // Normalize direction
    rayDirection = Vec3(rayDirection.x / rayLength, rayDirection.y / rayLength, rayDirection.z / rayLength);
    
    // **CAST SHADOW RAY** using existing raycasting system
    // Add small offset to prevent self-intersection
    Vec3 rayStart = faceCenter + rayDirection * 0.01f;
    
    // Use VoxelRaycaster to check for blocking geometry
    RayHit shadowRay = VoxelRaycaster::raycast(rayStart, rayDirection, rayLength - 0.02f, m_worldSystem);
    
    // If ray hit something before reaching the light, we're in shadow
    return shadowRay.hit;
}

// **FACE NORMAL CALCULATION** - Get normal vector for each cube face
Vec3 RaycastLighting::getFaceNormal(CubeFace face)
{
    switch (face) {
        case CubeFace::POSITIVE_Z: return Vec3(0, 0, 1);   // Front face
        case CubeFace::NEGATIVE_Z: return Vec3(0, 0, -1);  // Back face
        case CubeFace::POSITIVE_Y: return Vec3(0, 1, 0);   // Top face
        case CubeFace::NEGATIVE_Y: return Vec3(0, -1, 0);  // Bottom face
        case CubeFace::POSITIVE_X: return Vec3(1, 0, 0);   // Right face
        case CubeFace::NEGATIVE_X: return Vec3(-1, 0, 0);  // Left face
        default: return Vec3(0, 1, 0); // Default to up
    }
}

// **FACE CENTER CALCULATION** - Get center position of a cube face
Vec3 RaycastLighting::getFaceCenter(const Vec3& voxelPos, CubeFace face)
{
    Vec3 center = voxelPos + Vec3(0.5f, 0.5f, 0.5f); // Voxel center
    
    switch (face) {
        case CubeFace::POSITIVE_Z: return center + Vec3(0, 0, 0.5f);   // Front face center
        case CubeFace::NEGATIVE_Z: return center + Vec3(0, 0, -0.5f);  // Back face center
        case CubeFace::POSITIVE_Y: return center + Vec3(0, 0.5f, 0);   // Top face center
        case CubeFace::NEGATIVE_Y: return center + Vec3(0, -0.5f, 0);  // Bottom face center
        case CubeFace::POSITIVE_X: return center + Vec3(0.5f, 0, 0);   // Right face center
        case CubeFace::NEGATIVE_X: return center + Vec3(-0.5f, 0, 0);  // Left face center
        default: return center; // Default to voxel center
    }
}

// **LIGHT ATTENUATION** - Calculate how light diminishes over distance  
float RaycastLighting::calculateAttenuation(float distance, float lightRange)
{
    if (distance >= lightRange) return 0.0f;
    
    // Inverse square attenuation with linear falloff near the edge
    float attenuation = 1.0f / (1.0f + 0.1f * distance + 0.01f * distance * distance);
    
    // Smooth falloff near range limit
    if (distance > lightRange * 0.8f) {
        float falloffFactor = (lightRange - distance) / (lightRange * 0.2f);
        attenuation *= falloffFactor;
    }
    
    return std::max(0.0f, attenuation);
}

// **CACHE MANAGEMENT FOR FACE LIGHTING**
void RaycastLighting::updateCache(float deltaTime)
{
    m_currentTimestamp++;
    
    // Remove old cache entries to prevent memory growth
    const uint32_t maxAge = 100;
    
    for (int i = static_cast<int>(m_faceCache.size()) - 1; i >= 0; --i) {
        if (m_currentTimestamp - m_faceCache.timestamps[i] > maxAge) {
            // Remove this entry
            m_faceCache.facePositions.erase(m_faceCache.facePositions.begin() + i);
            m_faceCache.faceDirections.erase(m_faceCache.faceDirections.begin() + i);
            m_faceCache.colors.erase(m_faceCache.colors.begin() + i);
            m_faceCache.brightnesses.erase(m_faceCache.brightnesses.begin() + i);
            m_faceCache.timestamps.erase(m_faceCache.timestamps.begin() + i);
        }
    }
}

// **CACHE LOOKUP**
bool RaycastLighting::getCachedFaceLighting(const Vec3& faceCenter, CubeFace face, FaceLighting& lighting)
{
    // Simple cache lookup - could be optimized with spatial hash
    for (size_t i = 0; i < m_faceCache.size(); ++i) {
        Vec3 cachedPos = m_faceCache.facePositions[i];
        float distance = std::sqrt((cachedPos.x - faceCenter.x) * (cachedPos.x - faceCenter.x) +
                                  (cachedPos.y - faceCenter.y) * (cachedPos.y - faceCenter.y) +
                                  (cachedPos.z - faceCenter.z) * (cachedPos.z - faceCenter.z));
        
        if (distance < 0.1f && m_faceCache.faceDirections[i] == static_cast<int>(face)) {
            lighting.color = m_faceCache.colors[i];
            lighting.brightness = m_faceCache.brightnesses[i];
            return true;
        }
    }
    return false;
}

// **CACHE STORAGE**
void RaycastLighting::cacheFaceLighting(const Vec3& faceCenter, CubeFace face, const FaceLighting& lighting)
{
    m_faceCache.addFaceLighting(faceCenter, static_cast<int>(face), lighting.color, lighting.brightness, m_currentTimestamp);
}
