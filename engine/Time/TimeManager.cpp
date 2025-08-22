#include "TimeManager.h"
#include <iostream>
#include <cmath>
#include <algorithm>

// Global time manager instance
TimeManager* g_timeManager = nullptr;

TimeManager::TimeManager() 
    : m_realTotalTime(0.0f)
    , m_lastRealDeltaTime(0.0f)
    , m_lastFrameTime(std::chrono::high_resolution_clock::now())
{
    initializeTimeScales();
}

void TimeManager::initializeTimeScales() {
    // Initialize all time scales with default values
    TimeInfo defaultInfo = { 0.0f, 0.0f, 1.0f, false };
    
    m_timeScales[TimeScale::GLOBAL] = defaultInfo;
    m_timeScales[TimeScale::GAMEPLAY] = defaultInfo;
    m_timeScales[TimeScale::EFFECTS] = defaultInfo;
    m_timeScales[TimeScale::UI] = defaultInfo;
    m_timeScales[TimeScale::AUDIO] = defaultInfo;
    m_timeScales[TimeScale::NETWORK] = defaultInfo;
}

void TimeManager::update(float realDeltaTime) {
    // Update real time tracking
    m_lastRealDeltaTime = realDeltaTime;
    m_realTotalTime += realDeltaTime;

    // Update time bubbles (reduce remaining time)
    for (auto& [name, bubble] : m_timeBubbles) {
        if (bubble.duration > 0.0f) {
            bubble.remainingTime -= realDeltaTime;
        }
    }

    // Remove expired bubbles
    for (auto it = m_timeBubbles.begin(); it != m_timeBubbles.end();) {
        if (it->second.duration > 0.0f && it->second.remainingTime <= 0.0f) {
            it = m_timeBubbles.erase(it);
        } else {
            ++it;
        }
    }

    // Update smooth transitions
    updateTransitions(realDeltaTime);

    // Update each time scale
    for (auto& [scale, timeInfo] : m_timeScales) {
        if (!timeInfo.isPaused) {
            // Calculate effective time scale (includes transitions and global effects)
            float effectiveScale = calculateEffectiveTimeScale(scale);
            
            // Update delta time and total time
            timeInfo.deltaTime = realDeltaTime * effectiveScale;
            timeInfo.totalTime += timeInfo.deltaTime;
        } else {
            timeInfo.deltaTime = 0.0f;
        }
    }
}

void TimeManager::updateTransitions(float realDeltaTime) {
    for (auto it = m_transitions.begin(); it != m_transitions.end();) {
        auto& [scale, transition] = *it;
        
        transition.elapsed += realDeltaTime;
        float t = std::min(transition.elapsed / transition.duration, 1.0f);
        
        // Smooth interpolation (ease-in-out)
        t = t * t * (3.0f - 2.0f * t);
        
        // Update the time scale
        float currentScale = transition.startScale + (transition.targetScale - transition.startScale) * t;
        m_timeScales[scale].timeScale = currentScale;
        
        // Remove completed transitions
        if (t >= 1.0f) {
            it = m_transitions.erase(it);
        } else {
            ++it;
        }
    }
}

float TimeManager::calculateEffectiveTimeScale(TimeScale scale, float x, float y, float z) const {
    float baseScale = m_timeScales.at(scale).timeScale;
    float globalScale = m_timeScales.at(TimeScale::GLOBAL).timeScale;
    
    // Apply global time scale (except for UI which should be independent)
    float effectiveScale = (scale == TimeScale::UI) ? baseScale : baseScale * globalScale;
    
    // Apply time bubble effects if position is provided
    if (x != 0.0f || y != 0.0f || z != 0.0f) {
        float bubbleEffect = getTimeBubbleEffect(x, y, z);
        effectiveScale *= bubbleEffect;
    }
    
    return effectiveScale;
}

void TimeManager::reset() {
    m_realTotalTime = 0.0f;
    m_lastRealDeltaTime = 0.0f;
    m_lastFrameTime = std::chrono::high_resolution_clock::now();
    
    for (auto& [scale, timeInfo] : m_timeScales) {
        timeInfo.deltaTime = 0.0f;
        timeInfo.totalTime = 0.0f;
        timeInfo.timeScale = 1.0f;
        timeInfo.isPaused = false;
    }
    
    m_timeBubbles.clear();
    m_transitions.clear();
}

void TimeManager::setTimeScale(TimeScale scale, float multiplier) {
    if (m_timeScales.find(scale) != m_timeScales.end()) {
        m_timeScales[scale].timeScale = std::max(0.0f, multiplier);
    }
}

float TimeManager::getTimeScale(TimeScale scale) const {
    auto it = m_timeScales.find(scale);
    return (it != m_timeScales.end()) ? it->second.timeScale : 1.0f;
}

void TimeManager::pauseTimeScale(TimeScale scale) {
    if (m_timeScales.find(scale) != m_timeScales.end()) {
        m_timeScales[scale].isPaused = true;
    }
}

void TimeManager::resumeTimeScale(TimeScale scale) {
    if (m_timeScales.find(scale) != m_timeScales.end()) {
        m_timeScales[scale].isPaused = false;
    }
}

bool TimeManager::isTimeScalePaused(TimeScale scale) const {
    auto it = m_timeScales.find(scale);
    return (it != m_timeScales.end()) ? it->second.isPaused : false;
}

const TimeManager::TimeInfo& TimeManager::getTimeInfo(TimeScale scale) const {
    static TimeInfo defaultInfo = { 0.0f, 0.0f, 1.0f, false };
    auto it = m_timeScales.find(scale);
    return (it != m_timeScales.end()) ? it->second : defaultInfo;
}

float TimeManager::getDeltaTime(TimeScale scale) const {
    return getTimeInfo(scale).deltaTime;
}

float TimeManager::getTotalTime(TimeScale scale) const {
    return getTimeInfo(scale).totalTime;
}

void TimeManager::setGlobalTimeScale(float scale) {
    setTimeScale(TimeScale::GLOBAL, scale);
}

float TimeManager::getGlobalTimeScale() const {
    return getTimeScale(TimeScale::GLOBAL);
}

void TimeManager::pauseGlobalTime() {
    pauseTimeScale(TimeScale::GLOBAL);
}

void TimeManager::resumeGlobalTime() {
    resumeTimeScale(TimeScale::GLOBAL);
}

bool TimeManager::isGlobalTimePaused() const {
    return isTimeScalePaused(TimeScale::GLOBAL);
}

void TimeManager::createTimeBubble(const std::string& name, float x, float y, float z, 
                                  float radius, float timeScale, float duration) {
    TimeBubble bubble;
    bubble.x = x;
    bubble.y = y;
    bubble.z = z;
    bubble.radius = radius;
    bubble.timeScale = timeScale;
    bubble.duration = duration;
    bubble.remainingTime = duration;
    bubble.name = name;
    
    m_timeBubbles[name] = bubble;
}

void TimeManager::removeTimeBubble(const std::string& name) {
    m_timeBubbles.erase(name);
}

void TimeManager::clearAllTimeBubbles() {
    m_timeBubbles.clear();
}

float TimeManager::getTimeBubbleEffect(float x, float y, float z) const {
    float strongest = 1.0f;
    
    for (const auto& [name, bubble] : m_timeBubbles) {
        // Calculate distance from bubble center
        float dx = x - bubble.x;
        float dy = y - bubble.y;
        float dz = z - bubble.z;
        float distance = std::sqrt(dx * dx + dy * dy + dz * dz);
        
        // If within bubble radius, apply effect
        if (distance <= bubble.radius) {
            // Linear falloff from center to edge
            float falloff = 1.0f - (distance / bubble.radius);
            float bubbleEffect = 1.0f + (bubble.timeScale - 1.0f) * falloff;
            
            // Take the strongest effect (furthest from 1.0)
            if (std::abs(bubbleEffect - 1.0f) > std::abs(strongest - 1.0f)) {
                strongest = bubbleEffect;
            }
        }
    }
    
    return strongest;
}

void TimeManager::smoothTransitionToTimeScale(TimeScale scale, float targetScale, float duration) {
    if (m_timeScales.find(scale) == m_timeScales.end()) {
        return;
    }
    
    TimeTransition transition;
    transition.startScale = m_timeScales[scale].timeScale;
    transition.targetScale = targetScale;
    transition.duration = duration;
    transition.elapsed = 0.0f;
    
    m_transitions[scale] = transition;
}

bool TimeManager::isTransitioning(TimeScale scale) const {
    return m_transitions.find(scale) != m_transitions.end();
}

void TimeManager::debugPrintTimeInfo() const {
    std::cout << "=== Time Manager Debug Info ===" << std::endl;
    std::cout << "Real Time: " << m_realTotalTime << "s, FPS: " << getFrameRate() << std::endl;
    
    const char* scaleNames[] = { "GLOBAL", "GAMEPLAY", "EFFECTS", "UI", "AUDIO", "NETWORK" };
    int i = 0;
    
    for (const auto& [scale, timeInfo] : m_timeScales) {
        std::cout << scaleNames[i++] << ": scale=" << timeInfo.timeScale 
                  << ", time=" << timeInfo.totalTime << "s"
                  << ", dt=" << timeInfo.deltaTime << "s"
                  << (timeInfo.isPaused ? " [PAUSED]" : "") << std::endl;
    }
    
    // Time bubbles are active
    
    if (!m_transitions.empty()) {
        std::cout << "Active Transitions:" << std::endl;
        for (const auto& [scale, transition] : m_transitions) {
            std::cout << "  Scale " << (int)scale << ": " << transition.startScale 
                      << " -> " << transition.targetScale 
                      << " (" << transition.elapsed << "/" << transition.duration << "s)" << std::endl;
        }
    }
}
