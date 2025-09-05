// Profiler.cpp - Basic performance profiler implementation (moved to Profiling)
#include "Profiler.h"
#include <iostream>
#include <iomanip>
#include <algorithm>

// Global profiler instance: provide a never-destroyed singleton to avoid
// shutdown order issues with STL internals during process teardown.
static Profiler* GetProfilerSingleton()
{
    static Profiler* s_instance = new Profiler();
    return s_instance;
}

Profiler& g_profiler = *GetProfilerSingleton();

Profiler::Profiler()
{
    m_lastReportTime = std::chrono::high_resolution_clock::now();
}

Profiler::~Profiler()
{
    // Disable profiler immediately to prevent any race conditions during shutdown
    m_enabled = false;
    
    // Skip final report during global destruction to avoid iterator issues
    // The profiler will have already reported during normal operation
}

void Profiler::recordTime(const std::string& name, double timeMs)
{
    if (!m_enabled)
        return;

    std::lock_guard<std::mutex> lock(m_profileMutex);
    
    ProfileData& data = m_profiles[name];
    data.name = name;
    data.totalTime += timeMs;
    data.sampleCount++;
    
    if (timeMs < data.minTime)
        data.minTime = timeMs;
    if (timeMs > data.maxTime)
        data.maxTime = timeMs;
}

void Profiler::updateAndReport()
{
    if (!m_enabled)
        return;

    double timeSinceReport = getTimeSinceLastReport();
    if (timeSinceReport >= m_reportInterval)
    {
        reportToConsole();
        
        // Reset for next interval
        std::lock_guard<std::mutex> lock(m_profileMutex);
        for (auto& pair : m_profiles)
        {
            pair.second.reset();
        }
        
        m_lastReportTime = std::chrono::high_resolution_clock::now();
    }
}

void Profiler::forceReport()
{
    if (!m_enabled)
        return;

    reportToConsole();
    
    // Reset profiles
    std::lock_guard<std::mutex> lock(m_profileMutex);
    for (auto& pair : m_profiles)
    {
        pair.second.reset();
    }
    
    m_lastReportTime = std::chrono::high_resolution_clock::now();
}

const Profiler::ProfileData* Profiler::getProfileData(const std::string& name) const
{
    std::lock_guard<std::mutex> lock(m_profileMutex);
    auto it = m_profiles.find(name);
    return (it != m_profiles.end()) ? &it->second : nullptr;
}

void Profiler::clearAll()
{
    std::lock_guard<std::mutex> lock(m_profileMutex);
    m_profiles.clear();
}

void Profiler::reportToConsole()
{
    // Early exit if profiler is disabled (e.g., during shutdown)
    if (!m_enabled)
        return;
        
    std::lock_guard<std::mutex> lock(m_profileMutex);
    
    if (m_profiles.empty())
        return;

    // Sort profiles by total time (descending)
    std::vector<std::pair<std::string, ProfileData*>> sortedProfiles;
    
    // Use try-catch to handle potential iterator issues during shutdown
    try
    {
        for (auto& pair : m_profiles)
        {
            if (pair.second.sampleCount > 0)
            {
                sortedProfiles.push_back({pair.first, &pair.second});
            }
        }
    }
    catch (...)
    {
        // If we get an iterator exception (e.g., during shutdown), just return
        return;
    }
    
    std::sort(sortedProfiles.begin(), sortedProfiles.end(),
        [](const auto& a, const auto& b) {
            return a.second->totalTime > b.second->totalTime;
        });

    // Report header
    double reportTime = getTimeSinceLastReport();
    std::cout << "\n=== PROFILER REPORT (" << std::fixed << std::setprecision(1) 
              << reportTime << "s) ===" << std::endl;
    std::cout << std::left << std::setw(25) << "Function"
              << std::right << std::setw(8) << "Samples"
              << std::setw(10) << "Total(ms)"
              << std::setw(10) << "Avg(ms)"
              << std::setw(10) << "Min(ms)"
              << std::setw(10) << "Max(ms)"
              << std::setw(8) << "FPS*" << std::endl;
    std::cout << std::string(81, '-') << std::endl;

    // Report each profile
    for (const auto& entry : sortedProfiles)
    {
        const ProfileData& data = *entry.second;
        double fps = data.getAverageTime() > 0.0 ? 1000.0 / data.getAverageTime() : 0.0;
        
        std::cout << std::left << std::setw(25) << data.name.substr(0, 24)
                  << std::right << std::setw(8) << data.sampleCount
                  << std::setw(10) << std::fixed << std::setprecision(2) << data.totalTime
                  << std::setw(10) << std::fixed << std::setprecision(2) << data.getAverageTime()
                  << std::setw(10) << std::fixed << std::setprecision(2) << data.minTime
                  << std::setw(10) << std::fixed << std::setprecision(2) << data.maxTime
                  << std::setw(8) << std::fixed << std::setprecision(0) << fps
                  << std::endl;
    }
    
    std::cout << "* FPS calculated from average frame time" << std::endl;
    std::cout << std::endl;
}

double Profiler::getTimeSinceLastReport() const
{
    auto currentTime = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration<double>(currentTime - m_lastReportTime);
    return duration.count();
}

// ProfileScope implementation
ProfileScope::ProfileScope(const std::string& name)
    : m_name(name), m_active(true)
{
    m_startTime = std::chrono::high_resolution_clock::now();
}

ProfileScope::~ProfileScope()
{
    stop();
}

void ProfileScope::stop()
{
    if (!m_active)
        return;

    // Check if profiler is still enabled (avoid recording during shutdown)
    if (!g_profiler.isEnabled())
    {
        m_active = false;
        return;
    }

    auto endTime = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration<double, std::milli>(endTime - m_startTime);
    
    g_profiler.recordTime(m_name, duration.count());
    m_active = false;
}
