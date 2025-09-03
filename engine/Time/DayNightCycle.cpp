#include "DayNightCycle.h"
#include <cmath>
#include <algorithm>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// Global day/night cycle instance pointer
DayNightCycle* g_dayNightCycle = nullptr;

DayNightCycle::DayNightCycle()
    : m_currentTimeHours(12.0f)         // Start at noon
    , m_timeSpeed(1.0f)                 // Real time speed
    , m_isPaused(false)
{
}

void DayNightCycle::update(float deltaTime) {
    if (m_isPaused) return;
    
    // Update time
    float timeIncrement = deltaTime * m_timeSpeed / 3600.0f; // Convert seconds to hours
    m_currentTimeHours += timeIncrement;
    
    // Wrap around 24 hours
    while (m_currentTimeHours >= 24.0f) {
        m_currentTimeHours -= 24.0f;
    }
}

float DayNightCycle::getTimeOfDay() const {
    return m_currentTimeHours;
}

Vec3 DayNightCycle::getSunDirection() const {
    // Calculate sun direction based on time of day
    // Sun rises at 6:00 AM (90 degrees) and sets at 6:00 PM (270 degrees)
    float sunAngle = (m_currentTimeHours - 6.0f) * 15.0f; // 15 degrees per hour
    float sunAngleRad = sunAngle * M_PI / 180.0f;
    
    // Sun path: high at noon, low at sunrise/sunset
    float sunElevation = sin(sunAngleRad);
    sunElevation = std::max(sunElevation, -0.2f); // Don't go too far below horizon
    
    // Sun azimuth: from east to west
    float sunAzimuth = cos(sunAngleRad);
    
    return Vec3(sunAzimuth * 0.7f, sunElevation, 0.3f).normalized();
}

void DayNightCycle::setTimeSpeed(float multiplier) {
    m_timeSpeed = std::max(0.0f, multiplier);
}

void DayNightCycle::pauseTime() {
    m_isPaused = true;
}

void DayNightCycle::resumeTime() {
    m_isPaused = false;
}
