#pragma once

#include "TimeManager.h"
#include <vector>
#include <memory>
#include <functional>

/**
 * @brief High-level time effect system for gameplay mechanics
 * 
 * Provides pre-built time manipulation effects like bullet-time, slow-motion,
 * time freeze, and temporal abilities. Built on top of TimeManager.
 */
class TimeEffects {
public:
    enum class EffectType {
        BULLET_TIME,        // Slow down everything except player
        SLOW_MOTION,        // Global slow motion
        TIME_FREEZE,        // Pause everything except UI
        SPEED_BOOST,        // Speed up gameplay
        TEMPORAL_BUBBLE,    // Localized time effect
        CUSTOM              // User-defined effect
    };

    struct Effect {
        EffectType type;
        std::string name;
        float duration;
        float remainingTime;
        float intensity;        // 0.0 to 1.0
        bool isActive;
        std::function<void()> onStart;
        std::function<void()> onEnd;
        std::function<void(float)> onUpdate; // Parameter is remaining time
    };

    TimeEffects();
    ~TimeEffects() = default;

    void update(float deltaTime);

    // Predefined effects
    void activateBulletTime(float duration = 3.0f, float intensity = 0.3f);
    void activateSlowMotion(float duration = 2.0f, float intensity = 0.5f);
    void activateTimeFreeze(float duration = 1.0f);
    void activateSpeedBoost(float duration = 5.0f, float intensity = 2.0f);
    
    // Temporal bubble (localized time effect)
    void createTemporalBubble(const std::string& name, float x, float y, float z, 
                             float radius, float timeScale, float duration = 5.0f);

    // Custom effect creation
    void createCustomEffect(const std::string& name, float duration, 
                           std::function<void()> onStart,
                           std::function<void()> onEnd = nullptr,
                           std::function<void(float)> onUpdate = nullptr);

    // Effect management
    void stopEffect(const std::string& name);
    void stopAllEffects();
    bool isEffectActive(const std::string& name) const;
    float getEffectRemainingTime(const std::string& name) const;

    // Utility functions for common gameplay scenarios
    void onPlayerDeath();       // Dramatic slow-motion effect
    void onCriticalHit();       // Brief time freeze for impact
    void onSpecialAbility();    // Bullet-time for precision
    void onLevelComplete();     // Speed up for celebration

    // Debug
    void debugPrintActiveEffects() const;

private:
    std::vector<std::unique_ptr<Effect>> m_activeEffects;

    // Internal helpers
    void startEffect(std::unique_ptr<Effect> effect);
    void removeExpiredEffects();
    Effect* findEffect(const std::string& name);
    const Effect* findEffect(const std::string& name) const;

    // Predefined effect implementations
    std::unique_ptr<Effect> createBulletTimeEffect(float duration, float intensity);
    std::unique_ptr<Effect> createSlowMotionEffect(float duration, float intensity);
    std::unique_ptr<Effect> createTimeFreezeEffect(float duration);
    std::unique_ptr<Effect> createSpeedBoostEffect(float duration, float intensity);
};

// Global time effects instance
extern TimeEffects* g_timeEffects;
