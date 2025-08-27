#include "JobSystem.h"

#include <algorithm>
#include <chrono>
#include <iostream>

// Global job system instance
JobSystem g_jobSystem;

JobSystem::JobSystem() = default;

JobSystem::~JobSystem()
{
    shutdown();
}

bool JobSystem::initialize(uint32_t workerCount)
{
    if (m_initialized.load())
    {
        // Removed verbose debug output
        return true;
    }

    // Determine worker count
    if (workerCount == 0)
    {
        uint32_t hwThreads = std::thread::hardware_concurrency();
        workerCount = std::max(1u, hwThreads - 1);  // Leave 1 thread for main
    }

    // Start worker threads
    m_workers.reserve(workerCount);
    for (uint32_t i = 0; i < workerCount; ++i)
    {
        m_workers.emplace_back(&JobSystem::workerThreadMain, this, i);
    }

    m_initialized.store(true);
    resetStats();

    return true;
}

void JobSystem::shutdown()
{
    if (!m_initialized.load())
        return;

    // Signal shutdown and wake all workers
    m_shutdown.store(true);
    m_workCondition.notify_all();

    // Wait for all workers to complete
    for (auto& worker : m_workers)
    {
        if (worker.joinable())
        {
            worker.join();
        }
    }

    m_workers.clear();
    m_initialized.store(false);

    // Clear remaining work
    {
        std::lock_guard<std::mutex> lock(m_workMutex);
        while (!m_workQueue.empty())
        {
            m_workQueue.pop();
        }
    }

    {
        std::lock_guard<std::mutex> lock(m_completedMutex);
        while (!m_completedQueue.empty())
        {
            m_completedQueue.pop();
        }
    }
}

uint32_t JobSystem::submitJob(JobType type, const JobPayload& payload,
                              std::function<JobResult()> work)
{
    if (!m_initialized.load() || m_shutdown.load())
    {
        return 0;  // Invalid job ID
    }

    uint32_t jobID = m_nextJobID.fetch_add(1);

    WorkItem item;
    item.jobID = jobID;
    item.type = type;
    item.payload = payload;
    item.work = std::move(work);

    {
        std::lock_guard<std::mutex> lock(m_workMutex);
        m_workQueue.push(std::move(item));
    }

    m_workCondition.notify_one();
    m_stats.jobsSubmitted.fetch_add(1);
    m_stats.jobsInFlight.fetch_add(1);

    return jobID;
}

bool JobSystem::tryPopCompletedJob(JobResult& outResult)
{
    std::lock_guard<std::mutex> lock(m_completedMutex);

    if (m_completedQueue.empty())
    {
        return false;
    }

    outResult = std::move(m_completedQueue.front());
    m_completedQueue.pop();
    return true;
}

void JobSystem::drainCompletedJobs(std::vector<JobResult>& outResults, size_t maxCount)
{
    outResults.clear();

    std::lock_guard<std::mutex> lock(m_completedMutex);

    size_t count = std::min(maxCount, m_completedQueue.size());
    outResults.reserve(count);

    for (size_t i = 0; i < count; ++i)
    {
        outResults.emplace_back(std::move(m_completedQueue.front()));
        m_completedQueue.pop();
    }
}

void JobSystem::resetStats()
{
    m_stats.jobsSubmitted.store(0);
    m_stats.jobsCompleted.store(0);
    m_stats.jobsInFlight.store(0);
    m_stats.totalWorkerTime.store(0);
    m_stats.activeWorkers.store(0);
}

void JobSystem::workerThreadMain(uint32_t workerID)
{
    m_stats.activeWorkers.fetch_add(1);

    while (!m_shutdown.load())
    {
        WorkItem work;

        // Wait for work or shutdown
        {
            std::unique_lock<std::mutex> lock(m_workMutex);
            m_workCondition.wait(
                lock,
                [this] { return m_shutdown.load() || !m_workQueue.empty() || m_paused.load(); });

            if (m_shutdown.load())
            {
                break;
            }

            if (m_paused.load() || m_workQueue.empty())
            {
                continue;
            }

            // Get work item
            work = std::move(m_workQueue.front());
            m_workQueue.pop();
        }

        // Execute work (outside of lock)
        auto startTime = std::chrono::high_resolution_clock::now();

        JobResult result = work.work();
        result.jobID = work.jobID;
        result.type = work.type;

        auto endTime = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime);

        // Add completed result
        {
            std::lock_guard<std::mutex> lock(m_completedMutex);
            m_completedQueue.push(std::move(result));
        }

        // Update stats
        m_stats.jobsCompleted.fetch_add(1);
        m_stats.jobsInFlight.fetch_sub(1);
        m_stats.totalWorkerTime.fetch_add(duration.count());
    }

    m_stats.activeWorkers.fetch_sub(1);
}

// Convenience job functions
namespace Jobs
{
uint32_t submitChunkMesh(uint32_t chunkID, uint32_t islandID, const void* voxelData,
                         size_t dataSize)
{
    JobPayload payload;
    payload.chunkID = chunkID;
    payload.islandID = islandID;
    payload.data = const_cast<void*>(voxelData);
    payload.dataSize = dataSize;

    return g_jobSystem.submitJob(JobType::CHUNK_MESHING, payload,
                                 [=]() -> JobResult
                                 {
                                     JobResult result;
                                     result.chunkID = chunkID;
                                     result.success = true;

                                     // Chunk meshing implementation will connect to VoxelChunk
                                     // generation For now, just simulate work
                                     std::this_thread::sleep_for(std::chrono::milliseconds(1));

                                     return result;
                                 });
}

uint32_t submitPhysicsCook(uint32_t chunkID, const void* meshData, size_t meshSize)
{
    JobPayload payload;
    payload.chunkID = chunkID;
    payload.data = const_cast<void*>(meshData);
    payload.dataSize = meshSize;

    return g_jobSystem.submitJob(JobType::PHYSICS_COOKING, payload,
                                 [=]() -> JobResult
                                 {
                                     JobResult result;
                                     result.chunkID = chunkID;
                                     result.success = true;

                                     // Physics cooking will generate collision meshes
                                     std::this_thread::sleep_for(std::chrono::milliseconds(2));

                                     return result;
                                 });
}

uint32_t submitLODGeneration(uint32_t chunkID, int lodLevel, const void* sourceData)
{
    JobPayload payload;
    payload.chunkID = chunkID;
    payload.data = const_cast<void*>(sourceData);

    return g_jobSystem.submitJob(JobType::LOD_GENERATION, payload,
                                 [=]() -> JobResult
                                 {
                                     JobResult result;
                                     result.chunkID = chunkID;
                                     result.success = true;

                                     // LOD generation will create lower detail versions of chunks
                                     std::this_thread::sleep_for(std::chrono::microseconds(500));

                                     return result;
                                 });
}

uint32_t submitWorldGeneration(uint32_t seed, int chunkX, int chunkY, int chunkZ)
{
    JobPayload payload;
    payload.chunkID = (chunkX << 16) | (chunkY << 8) | chunkZ;  // Pack coordinates

    return g_jobSystem.submitJob(JobType::WORLD_GENERATION, payload,
                                 [=]() -> JobResult
                                 {
                                     JobResult result;
                                     result.chunkID = payload.chunkID;
                                     result.success = true;

                                     // World generation will use Perlin noise for terrain creation
                                     std::this_thread::sleep_for(std::chrono::milliseconds(5));

                                     return result;
                                 });
}
}  // namespace Jobs
