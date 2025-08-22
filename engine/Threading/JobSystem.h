// JobSystem.h - Scalable threading foundation for MMORPG engine
#pragma once
#include <thread>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <atomic>
#include <future>
#include <vector>

// Job types for categorization and priority
enum class JobType {
    CHUNK_MESHING = 0,
    PHYSICS_COOKING,
    WORLD_GENERATION, 
    LOD_GENERATION,
    ASSET_LOADING,
    AI_PROCESSING,
    COUNT
};

// Basic job payload - can be extended per job type
struct JobPayload {
    uint32_t chunkID = 0;
    uint32_t islandID = 0;
    void* data = nullptr;
    size_t dataSize = 0;
};

// Job result for main thread consumption
struct JobResult {
    JobType type;
    uint32_t jobID;
    uint32_t chunkID;
    bool success = false;
    void* resultData = nullptr;
    size_t resultSize = 0;
    
    ~JobResult() {
        if (resultData) {
            free(resultData);
            resultData = nullptr;
        }
    }
};

// Work item internal structure
struct WorkItem {
    uint32_t jobID;
    JobType type;
    JobPayload payload;
    std::function<JobResult()> work;
};

class JobSystem {
public:
    JobSystem();
    ~JobSystem();
    
    // Core system lifecycle
    bool initialize(uint32_t workerCount = 0); // 0 = hardware_concurrency - 1
    void shutdown();
    bool isInitialized() const { return m_initialized.load(); }
    
    // Job submission (thread-safe)
    uint32_t submitJob(JobType type, const JobPayload& payload, std::function<JobResult()> work);
    
    // Main thread result collection (non-blocking)
    bool tryPopCompletedJob(JobResult& outResult);
    void drainCompletedJobs(std::vector<JobResult>& outResults, size_t maxCount = 100);
    
    // Statistics and debugging
    struct Stats {
        std::atomic<uint64_t> jobsSubmitted{0};
        std::atomic<uint64_t> jobsCompleted{0};
        std::atomic<uint64_t> jobsInFlight{0};
        std::atomic<uint64_t> totalWorkerTime{0}; // microseconds
        std::atomic<uint32_t> activeWorkers{0};
    };
    
    const Stats& getStats() const { return m_stats; }
    void resetStats();
    
    // Configuration
    void setPaused(bool paused) { m_paused.store(paused); }
    bool isPaused() const { return m_paused.load(); }
    
    // Threading info
    uint32_t getWorkerCount() const { return static_cast<uint32_t>(m_workers.size()); }
    uint32_t getHardwareConcurrency() const { return std::thread::hardware_concurrency(); }
    
private:
    // Worker thread function
    void workerThreadMain(uint32_t workerID);
    
    // Thread-safe queues
    std::queue<WorkItem> m_workQueue;
    std::queue<JobResult> m_completedQueue;
    
    // Synchronization
    std::mutex m_workMutex;
    std::mutex m_completedMutex;
    std::condition_variable m_workCondition;
    
    // Worker management
    std::vector<std::thread> m_workers;
    std::atomic<bool> m_initialized{false};
    std::atomic<bool> m_shutdown{false};
    std::atomic<bool> m_paused{false};
    
    // Job ID generation
    std::atomic<uint32_t> m_nextJobID{1};
    
    // Statistics
    mutable Stats m_stats;
};

// Global job system instance
extern JobSystem g_jobSystem;

// Convenience functions for common job types
namespace Jobs {
    // Chunk meshing job
    uint32_t submitChunkMesh(uint32_t chunkID, uint32_t islandID, const void* voxelData, size_t dataSize);
    
    // Physics cooking job  
    uint32_t submitPhysicsCook(uint32_t chunkID, const void* meshData, size_t meshSize);
    
    // LOD generation job
    uint32_t submitLODGeneration(uint32_t chunkID, int lodLevel, const void* sourceData);
    
    // World generation job
    uint32_t submitWorldGeneration(uint32_t seed, int chunkX, int chunkY, int chunkZ);
}
