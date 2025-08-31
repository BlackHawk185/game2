#pragma once

#include <functional>
#include "../Math/Vec3.h"

/**
 * @brief Day/Night cycle system for the MMORPG engine
 * 
 * Manages time of day progression, lighting transitions, and celestial bodies.
 * Integrates with the existing TimeManager for smooth time scaling effects.
 */
class DayNightCycle {
public:
    enum class TimeOfDay {
        DAWN,        // 5:00 - 7:00   - Sunrise period
        MORNING,     // 7:00 - 11:00  - Bright daylight
        MIDDAY,      // 11:00 - 13:00 - Peak sunlight
        AFTERNOON,   // 13:00 - 17:00 - Still bright
        DUSK,        // 17:00 - 19:00 - Sunset period
        EVENING,     // 19:00 - 21:00 - Twilight
        NIGHT,       // 21:00 - 5:00  - Darkness
    };

    struct LightingParams {
        float sunIntensity;     // 0.0 (night) to 1.0 (day)
        float ambientLevel;     // Base ambient lighting
        float sunAngle;         // Sun position in sky (0-360 degrees)
        float moonPhase;        // 0.0 (new moon) to 1.0 (full moon)
        
        // Color temperatures
        float sunColorR, sunColorG, sunColorB;
        float ambientColorR, ambientColorG, ambientColorB;
        float fogColorR, fogColorG, fogColorB;
        
        // Atmospheric effects
        float fogDensity;
        float skyBrightness;
        float starVisibility;
    };

    struct SkyColors {
        float horizonR, horizonG, horizonB;
        float zenithR, zenithG, zenithB;
        float sunR, sunG, sunB;
        float cloudR, cloudG, cloudB;
    };

    DayNightCycle();
    ~DayNightCycle() = default;

    // Core cycle management
    void update(float deltaTime);
    void reset();

    // Time control
    void setTimeOfDay(float hours);        // 0.0-24.0
    void setTimeSpeed(float multiplier);   // How fast time passes (1.0 = real time)
    void pauseTime();
    void resumeTime();
    
    float getTimeOfDay() const;            // Current time in hours (0.0-24.0)
    float getTimeSpeed() const;
    bool isTimePaused() const;
    
    // Convenience methods
    void setTime(int hours, int minutes);
    void getTime(int& hours, int& minutes) const;
    TimeOfDay getCurrentPeriod() const;
    const char* getCurrentPeriodName() const;

    // Lighting system
    const LightingParams& getLightingParams() const;
    const SkyColors& getSkyColors() const;
    
    // Shadow mapping support
    Vec3 getSunDirection() const;          // Get sun direction for shadow mapping
    Vec3 getLightColor() const;            // Get current light color
    float getLightIntensity() const;       // Get current light intensity
    
    // Smooth transitions
    void setTransitionDuration(float seconds);  // How long day/night transitions take
    float getTransitionDuration() const;
    
    // Events - register callbacks for time changes
    void onTimeChanged(std::function<void(float)> callback);
    void onPeriodChanged(std::function<void(TimeOfDay)> callback);
    void onSunrise(std::function<void()> callback);
    void onSunset(std::function<void()> callback);

    // Debug and control
    void debugPrintTime() const;
    void setDebugMode(bool enabled);
    
    // Seasonal effects (future expansion)
    void setSeasonalFactor(float factor);   // 0.0-1.0, affects day length
    float getSeasonalFactor() const;

    // Moon and celestial bodies
    void setMoonPhase(float phase);         // 0.0-1.0
    float getMoonPhase() const;
    float getMoonAngle() const;             // Moon position in sky
    
    // Weather interaction
    void setWeatherInfluence(float cloudCover, float precipitation);
    
private:
    // Core time state
    float m_currentTimeHours;      // 0.0-24.0
    float m_timeSpeed;             // Time multiplier
    bool m_isPaused;
    
    // Cycle parameters
    float m_transitionDuration;    // Smooth transition time in seconds
    float m_seasonalFactor;        // Affects day/night length
    float m_moonPhase;             // Current moon phase
    
    // Weather influence
    float m_cloudCover;            // 0.0-1.0
    float m_precipitation;         // 0.0-1.0
    
    // Cached lighting state
    mutable LightingParams m_lightingParams;
    mutable SkyColors m_skyColors;
    mutable bool m_lightingDirty;
    
    // Event callbacks
    std::function<void(float)> m_onTimeChanged;
    std::function<void(TimeOfDay)> m_onPeriodChanged;
    std::function<void()> m_onSunrise;
    std::function<void()> m_onSunset;
    
    // Previous state for event detection
    TimeOfDay m_lastPeriod;
    bool m_lastWasDay;
    
    // Debug
    bool m_debugMode;
    
    // Internal calculations
    void updateLighting();
    void updateSkyColors();
    void checkTimeEvents();
    float calculateSunAngle() const;
    float calculateMoonAngle() const;
    float calculateSunIntensity() const;
    void calculateSkyColors() const;
    
    // Smooth interpolation helpers
    float smoothstep(float edge0, float edge1, float x) const;
    void lerpColor(float t, float r1, float g1, float b1, float r2, float g2, float b2, 
                   float& outR, float& outG, float& outB) const;
};

// Global day/night cycle instance
extern DayNightCycle* g_dayNightCycle;

// Convenience macros
#define CURRENT_TIME_HOURS() (g_dayNightCycle->getTimeOfDay())
#define CURRENT_PERIOD() (g_dayNightCycle->getCurrentPeriod())
#define IS_DAY() (g_dayNightCycle->getCurrentPeriod() != DayNightCycle::TimeOfDay::NIGHT && \
                  g_dayNightCycle->getCurrentPeriod() != DayNightCycle::TimeOfDay::EVENING)
#define IS_NIGHT() (g_dayNightCycle->getCurrentPeriod() == DayNightCycle::TimeOfDay::NIGHT)
