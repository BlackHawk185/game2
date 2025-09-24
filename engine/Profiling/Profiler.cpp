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

    // Calculate total frame time for percentage calculations
    double totalFrameTime = 0.0;
    for (const auto& entry : sortedProfiles) {
        if (entry.first.find("GameClient::update") != std::string::npos ||
            entry.first.find("GameClient::render") != std::string::npos) {
            totalFrameTime = std::max(totalFrameTime, entry.second->getAverageTime());
        }
    }
    
    // If no main frame function found, use the highest time consumer
    if (totalFrameTime == 0.0 && !sortedProfiles.empty()) {
        totalFrameTime = sortedProfiles[0].second->getAverageTime();
    }

    // Report header
    double reportTime = getTimeSinceLastReport();
    std::cout << "\n=== PERFORMANCE ANALYSIS (" << std::fixed << std::setprecision(1) 
              << reportTime << "s) ===" << std::endl;
              
    // Only show major performance consumers (>1ms average or >5% of frame time)
    std::cout << std::left << std::setw(35) << "System"
              << std::right << std::setw(8) << "Avg(ms)"
              << std::setw(8) << "% Frame"
              << std::setw(8) << "Calls"
              << std::setw(10) << "Total(ms)"
              << std::setw(8) << "FPS" << std::endl;
    std::cout << std::string(77, '-') << std::endl;

    // Report major systems only
    for (const auto& entry : sortedProfiles)
    {
        const ProfileData& data = *entry.second;
        double avgTime = data.getAverageTime();
        double framePercent = totalFrameTime > 0.0 ? (avgTime / totalFrameTime) * 100.0 : 0.0;
        
        // Only show significant performance consumers
        if (avgTime < 1.0 && framePercent < 5.0) continue;
        
        double fps = avgTime > 0.0 ? 1000.0 / avgTime : 0.0;
        
        // Simplify function names for clarity
        std::string displayName = data.name;
        
        // Clean up common prefixes and focus on the main function
        size_t lastScope = displayName.rfind("::");
        if (lastScope != std::string::npos) {
            displayName = displayName.substr(lastScope + 2);
        }
        
        // Truncate if still too long
        if (displayName.length() > 34) {
            displayName = displayName.substr(0, 31) + "...";
        }
        
        std::cout << std::left << std::setw(35) << displayName
                  << std::right << std::setw(8) << std::fixed << std::setprecision(2) << avgTime
                  << std::setw(7) << std::fixed << std::setprecision(1) << framePercent << "%"
                  << std::setw(8) << data.sampleCount
                  << std::setw(10) << std::fixed << std::setprecision(1) << data.totalTime
                  << std::setw(8) << std::fixed << std::setprecision(0) << fps
                  << std::endl;
    }
    
    // Show frame time summary
    if (totalFrameTime > 0.0) {
        double actualFPS = 1000.0 / totalFrameTime;
        std::cout << std::string(77, '-') << std::endl;
        std::cout << "FRAME SUMMARY: " << std::fixed << std::setprecision(2) << totalFrameTime 
                  << "ms (" << std::setprecision(1) << actualFPS << " FPS)" << std::endl;
    }
    
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
