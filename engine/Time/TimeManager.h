#pragma once

#include <chrono>
#include <unordered_map>
#include <string>
#include <functional>

/**
 * @brief Core time management system for the MMORPG engine
 * 
 * Provides multiple time scales, time manipulation effects, and frame-rate independent timing.
 * Supports gameplay mechanics like slow-motion, bullet-time, time bubbles, and temporal effects.
 */
class TimeManager {
public:
    enum class TimeScale {
        GLOBAL,     // Master time scale affects everything
        GAMEPLAY,   // Player movement, physics, animations
        EFFECTS,    // Particle effects, visual effects
        UI,         // UI animations, menus (usually unaffected by time manipulation)
        AUDIO,      // Audio playback speed
        NETWORK     // Network update timing
    };

    struct TimeInfo {
        float deltaTime;        // Frame delta time for this scale
        float totalTime;        // Total accumulated time for this scale
        float timeScale;        // Current time multiplier (1.0 = normal, 0.5 = half speed, 2.0 = double speed)
        bool isPaused;          // Whether this time scale is paused
    };

    struct TimeBubble {
        float radius;           // Affected radius
        float timeScale;        // Time multiplier within bubble
        float x, y, z;          // Center position
        float duration;         // How long the bubble lasts (-1 for infinite)
        float remainingTime;    // Time left for this bubble
        std::string name;       // Identifier for the bubble
    };

    TimeManager();
    ~TimeManager() = default;

    // Core time management
    void update(float realDeltaTime);
    void reset();

    // Time scale management
    void setTimeScale(TimeScale scale, float multiplier);
    float getTimeScale(TimeScale scale) const;
    void pauseTimeScale(TimeScale scale);
    void resumeTimeScale(TimeScale scale);
    bool isTimeScalePaused(TimeScale scale) const;

    // Get timing information
    const TimeInfo& getTimeInfo(TimeScale scale) const;
    float getDeltaTime(TimeScale scale) const;
    float getTotalTime(TimeScale scale) const;

    // Global time manipulation
    void setGlobalTimeScale(float scale);
    float getGlobalTimeScale() const;
    void pauseGlobalTime();
    void resumeGlobalTime();
    bool isGlobalTimePaused() const;

    // Time bubble system for localized time effects
    void createTimeBubble(const std::string& name, float x, float y, float z, 
                         float radius, float timeScale, float duration = -1.0f);
    void removeTimeBubble(const std::string& name);
    void clearAllTimeBubbles();
    float getTimeBubbleEffect(float x, float y, float z) const;

    // Smooth time transitions
    void smoothTransitionToTimeScale(TimeScale scale, float targetScale, float duration);
    bool isTransitioning(TimeScale scale) const;

    // Utility functions
    float getRealTime() const { return m_realTotalTime; }
    float getFrameRate() const { return 1.0f / m_lastRealDeltaTime; }

    // Debug information
    void debugPrintTimeInfo() const;

private:
    // Time scale data
    std::unordered_map<TimeScale, TimeInfo> m_timeScales;
    
    // Time bubbles for localized effects
    std::unordered_map<std::string, TimeBubble> m_timeBubbles;

    // Smooth transitions
    struct TimeTransition {
        float startScale;
        float targetScale;
        float duration;
        float elapsed;
    };
    std::unordered_map<TimeScale, TimeTransition> m_transitions;

    // Real time tracking
    float m_realTotalTime;
    float m_lastRealDeltaTime;
    std::chrono::high_resolution_clock::time_point m_lastFrameTime;

    // Internal helpers
    void initializeTimeScales();
    void updateTransitions(float realDeltaTime);
    float calculateEffectiveTimeScale(TimeScale scale, float x = 0, float y = 0, float z = 0) const;
};

// Global time manager instance
extern TimeManager* g_timeManager;

// Convenience macros for common time operations
#define DELTA_TIME(scale) (g_timeManager->getDeltaTime(TimeManager::TimeScale::scale))
#define TOTAL_TIME(scale) (g_timeManager->getTotalTime(TimeManager::TimeScale::scale))
#define REAL_DELTA_TIME() (g_timeManager->getDeltaTime(TimeManager::TimeScale::GLOBAL))
