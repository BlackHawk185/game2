#include "TimeEffects.h"
#include <iostream>
#include <algorithm>

// Global time effects instance
TimeEffects* g_timeEffects = nullptr;

TimeEffects::TimeEffects() {
    // Constructor - ready to create effects
}

void TimeEffects::update(float deltaTime) {
    // Update all active effects
    for (auto& effect : m_activeEffects) {
        if (effect->isActive) {
            effect->remainingTime -= deltaTime;
            
            // Call update callback if provided
            if (effect->onUpdate) {
                effect->onUpdate(effect->remainingTime);
            }
            
            // Check if effect has expired
            if (effect->remainingTime <= 0.0f) {
                effect->isActive = false;
                
                // Call end callback
                if (effect->onEnd) {
                    effect->onEnd();
                }
            }
        }
    }
    
    // Remove expired effects
    removeExpiredEffects();
}

void TimeEffects::activateBulletTime(float duration, float intensity) {
    auto effect = createBulletTimeEffect(duration, intensity);
    startEffect(std::move(effect));
}

void TimeEffects::activateSlowMotion(float duration, float intensity) {
    auto effect = createSlowMotionEffect(duration, intensity);
    startEffect(std::move(effect));
}

void TimeEffects::activateTimeFreeze(float duration) {
    auto effect = createTimeFreezeEffect(duration);
    startEffect(std::move(effect));
}

void TimeEffects::activateSpeedBoost(float duration, float intensity) {
    auto effect = createSpeedBoostEffect(duration, intensity);
    startEffect(std::move(effect));
}

void TimeEffects::createTemporalBubble(const std::string& name, float x, float y, float z, 
                                      float radius, float timeScale, float duration) {
    // Stop any existing bubble with this name
    stopEffect(name);
    
    auto effect = std::make_unique<Effect>();
    effect->type = EffectType::TEMPORAL_BUBBLE;
    effect->name = name;
    effect->duration = duration;
    effect->remainingTime = duration;
    effect->intensity = timeScale;
    effect->isActive = false;
    
    effect->onStart = [=]() {
        if (g_timeManager) {
            g_timeManager->createTimeBubble(name, x, y, z, radius, timeScale, duration);
        }
        std::cout << "🕐 Temporal bubble '" << name << "' created at (" << x << "," << y << "," << z 
                  << ") with scale " << timeScale << std::endl;
    };
    
    effect->onEnd = [=]() {
        if (g_timeManager) {
            g_timeManager->removeTimeBubble(name);
        }
        std::cout << "🕐 Temporal bubble '" << name << "' expired" << std::endl;
    };
    
    startEffect(std::move(effect));
}

void TimeEffects::createCustomEffect(const std::string& name, float duration, 
                                    std::function<void()> onStart,
                                    std::function<void()> onEnd,
                                    std::function<void(float)> onUpdate) {
    // Stop any existing effect with this name
    stopEffect(name);
    
    auto effect = std::make_unique<Effect>();
    effect->type = EffectType::CUSTOM;
    effect->name = name;
    effect->duration = duration;
    effect->remainingTime = duration;
    effect->intensity = 1.0f;
    effect->isActive = false;
    effect->onStart = onStart;
    effect->onEnd = onEnd;
    effect->onUpdate = onUpdate;
    
    startEffect(std::move(effect));
}

void TimeEffects::stopEffect(const std::string& name) {
    Effect* effect = findEffect(name);
    if (effect && effect->isActive) {
        effect->isActive = false;
        effect->remainingTime = 0.0f;
        
        if (effect->onEnd) {
            effect->onEnd();
        }
    }
}

void TimeEffects::stopAllEffects() {
    for (auto& effect : m_activeEffects) {
        if (effect->isActive) {
            effect->isActive = false;
            effect->remainingTime = 0.0f;
            
            if (effect->onEnd) {
                effect->onEnd();
            }
        }
    }
}

bool TimeEffects::isEffectActive(const std::string& name) const {
    const Effect* effect = findEffect(name);
    return effect && effect->isActive;
}

float TimeEffects::getEffectRemainingTime(const std::string& name) const {
    const Effect* effect = findEffect(name);
    return effect ? effect->remainingTime : 0.0f;
}

void TimeEffects::onPlayerDeath() {
    // Dramatic slow-motion effect
    activateSlowMotion(2.0f, 0.2f);
    std::cout << "💀 Player death - dramatic slow motion activated" << std::endl;
}

void TimeEffects::onCriticalHit() {
    // Brief time freeze for impact
    activateTimeFreeze(0.15f);
    std::cout << "💥 Critical hit - time freeze for impact" << std::endl;
}

void TimeEffects::onSpecialAbility() {
    // Bullet-time for precision
    activateBulletTime(3.0f, 0.3f);
    std::cout << "✨ Special ability - bullet time activated" << std::endl;
}

void TimeEffects::onLevelComplete() {
    // Speed up for celebration
    activateSpeedBoost(3.0f, 1.5f);
    std::cout << "🎉 Level complete - speed boost activated" << std::endl;
}

void TimeEffects::debugPrintActiveEffects() const {
    std::cout << "=== Active Time Effects ===" << std::endl;
    
    if (m_activeEffects.empty()) {
        std::cout << "No active effects" << std::endl;
        return;
    }
    
    for (const auto& effect : m_activeEffects) {
        if (effect->isActive) {
            const char* typeNames[] = { "BULLET_TIME", "SLOW_MOTION", "TIME_FREEZE", 
                                       "SPEED_BOOST", "TEMPORAL_BUBBLE", "CUSTOM" };
            std::cout << effect->name << " (" << typeNames[(int)effect->type] << "): "
                      << effect->remainingTime << "/" << effect->duration << "s remaining, "
                      << "intensity=" << effect->intensity << std::endl;
        }
    }
}

void TimeEffects::startEffect(std::unique_ptr<Effect> effect) {
    effect->isActive = true;
    
    if (effect->onStart) {
        effect->onStart();
    }
    
    m_activeEffects.push_back(std::move(effect));
}

void TimeEffects::removeExpiredEffects() {
    m_activeEffects.erase(
        std::remove_if(m_activeEffects.begin(), m_activeEffects.end(),
            [](const std::unique_ptr<Effect>& effect) {
                return !effect->isActive;
            }),
        m_activeEffects.end()
    );
}

TimeEffects::Effect* TimeEffects::findEffect(const std::string& name) {
    auto it = std::find_if(m_activeEffects.begin(), m_activeEffects.end(),
        [&name](const std::unique_ptr<Effect>& effect) {
            return effect->name == name;
        });
    
    return (it != m_activeEffects.end()) ? it->get() : nullptr;
}

const TimeEffects::Effect* TimeEffects::findEffect(const std::string& name) const {
    auto it = std::find_if(m_activeEffects.begin(), m_activeEffects.end(),
        [&name](const std::unique_ptr<Effect>& effect) {
            return effect->name == name;
        });
    
    return (it != m_activeEffects.end()) ? it->get() : nullptr;
}

std::unique_ptr<TimeEffects::Effect> TimeEffects::createBulletTimeEffect(float duration, float intensity) {
    auto effect = std::make_unique<Effect>();
    effect->type = EffectType::BULLET_TIME;
    effect->name = "bullet_time";
    effect->duration = duration;
    effect->remainingTime = duration;
    effect->intensity = intensity;
    effect->isActive = false;
    
    effect->onStart = [intensity]() {
        if (g_timeManager) {
            // Slow down everything except UI
            g_timeManager->smoothTransitionToTimeScale(TimeManager::TimeScale::GAMEPLAY, intensity, 0.2f);
            g_timeManager->smoothTransitionToTimeScale(TimeManager::TimeScale::EFFECTS, intensity, 0.2f);
            g_timeManager->smoothTransitionToTimeScale(TimeManager::TimeScale::AUDIO, intensity, 0.2f);
        }
        std::cout << "🎯 Bullet time activated (scale: " << intensity << ")" << std::endl;
    };
    
    effect->onEnd = []() {
        if (g_timeManager) {
            // Return to normal speed
            g_timeManager->smoothTransitionToTimeScale(TimeManager::TimeScale::GAMEPLAY, 1.0f, 0.5f);
            g_timeManager->smoothTransitionToTimeScale(TimeManager::TimeScale::EFFECTS, 1.0f, 0.5f);
            g_timeManager->smoothTransitionToTimeScale(TimeManager::TimeScale::AUDIO, 1.0f, 0.5f);
        }
        std::cout << "🎯 Bullet time ended" << std::endl;
    };
    
    return effect;
}

std::unique_ptr<TimeEffects::Effect> TimeEffects::createSlowMotionEffect(float duration, float intensity) {
    auto effect = std::make_unique<Effect>();
    effect->type = EffectType::SLOW_MOTION;
    effect->name = "slow_motion";
    effect->duration = duration;
    effect->remainingTime = duration;
    effect->intensity = intensity;
    effect->isActive = false;
    
    effect->onStart = [intensity]() {
        if (g_timeManager) {
            g_timeManager->smoothTransitionToTimeScale(TimeManager::TimeScale::GLOBAL, intensity, 0.3f);
        }
        std::cout << "🐌 Slow motion activated (scale: " << intensity << ")" << std::endl;
    };
    
    effect->onEnd = []() {
        if (g_timeManager) {
            g_timeManager->smoothTransitionToTimeScale(TimeManager::TimeScale::GLOBAL, 1.0f, 0.5f);
        }
        std::cout << "🐌 Slow motion ended" << std::endl;
    };
    
    return effect;
}

std::unique_ptr<TimeEffects::Effect> TimeEffects::createTimeFreezeEffect(float duration) {
    auto effect = std::make_unique<Effect>();
    effect->type = EffectType::TIME_FREEZE;
    effect->name = "time_freeze";
    effect->duration = duration;
    effect->remainingTime = duration;
    effect->intensity = 0.0f;
    effect->isActive = false;
    
    effect->onStart = []() {
        if (g_timeManager) {
            // Pause everything except UI
            g_timeManager->pauseTimeScale(TimeManager::TimeScale::GAMEPLAY);
            g_timeManager->pauseTimeScale(TimeManager::TimeScale::EFFECTS);
            g_timeManager->pauseTimeScale(TimeManager::TimeScale::AUDIO);
        }
        std::cout << "❄️ Time freeze activated" << std::endl;
    };
    
    effect->onEnd = []() {
        if (g_timeManager) {
            // Resume everything
            g_timeManager->resumeTimeScale(TimeManager::TimeScale::GAMEPLAY);
            g_timeManager->resumeTimeScale(TimeManager::TimeScale::EFFECTS);
            g_timeManager->resumeTimeScale(TimeManager::TimeScale::AUDIO);
        }
        std::cout << "❄️ Time freeze ended" << std::endl;
    };
    
    return effect;
}

std::unique_ptr<TimeEffects::Effect> TimeEffects::createSpeedBoostEffect(float duration, float intensity) {
    auto effect = std::make_unique<Effect>();
    effect->type = EffectType::SPEED_BOOST;
    effect->name = "speed_boost";
    effect->duration = duration;
    effect->remainingTime = duration;
    effect->intensity = intensity;
    effect->isActive = false;
    
    effect->onStart = [intensity]() {
        if (g_timeManager) {
            g_timeManager->smoothTransitionToTimeScale(TimeManager::TimeScale::GAMEPLAY, intensity, 0.2f);
        }
        std::cout << "⚡ Speed boost activated (scale: " << intensity << ")" << std::endl;
    };
    
    effect->onEnd = []() {
        if (g_timeManager) {
            g_timeManager->smoothTransitionToTimeScale(TimeManager::TimeScale::GAMEPLAY, 1.0f, 0.3f);
        }
        std::cout << "⚡ Speed boost ended" << std::endl;
    };
    
    return effect;
}
