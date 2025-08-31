// Profiler.h - Basic performance profiler for MMORPG Engine
#pragma once
#include <chrono>
#include <string>
#include <unordered_map>
#include <vector>
#include <mutex>

class Profiler
{
public:
    struct ProfileData
    {
        std::string name;
        double totalTime = 0.0;         // Total accumulated time in milliseconds
        double minTime = 99999.0;       // Minimum time recorded
        double maxTime = 0.0;           // Maximum time recorded
        uint32_t sampleCount = 0;       // Number of samples
        
        double getAverageTime() const
        {
            return sampleCount > 0 ? totalTime / sampleCount : 0.0;
        }
        
        void reset()
        {
            totalTime = 0.0;
            minTime = 99999.0;
            maxTime = 0.0;
            sampleCount = 0;
        }
    };

private:
    std::unordered_map<std::string, ProfileData> m_profiles;
    std::chrono::high_resolution_clock::time_point m_lastReportTime;
    mutable std::mutex m_profileMutex;
    bool m_enabled = true;
    double m_reportInterval = 1.0; // Report every 1 second

public:
    Profiler();
    ~Profiler();

    // Enable/disable profiling
    void setEnabled(bool enabled) { m_enabled = enabled; }
    bool isEnabled() const { return m_enabled; }

    // Set report interval in seconds
    void setReportInterval(double interval) { m_reportInterval = interval; }

    // Record a time sample
    void recordTime(const std::string& name, double timeMs);

    // Check if it's time to report and do so if needed
    void updateAndReport();

    // Force a report right now
    void forceReport();

    // Get specific profile data
    const ProfileData* getProfileData(const std::string& name) const;

    // Clear all profile data
    void clearAll();

private:
    void reportToConsole();
    double getTimeSinceLastReport() const;
};

// RAII profiler scope for automatic timing
class ProfileScope
{
private:
    std::string m_name;
    std::chrono::high_resolution_clock::time_point m_startTime;
    bool m_active;

public:
    ProfileScope(const std::string& name);
    ~ProfileScope();

    // Manual stop (optional - destructor will call this)
    void stop();
};

// Global profiler instance (never destroyed; avoids shutdown-order issues)
extern Profiler& g_profiler;

// Convenient macros for profiling
#define PROFILE_SCOPE(name) ProfileScope _prof_scope(name)
#define PROFILE_FUNCTION() ProfileScope _prof_scope(__FUNCTION__)
