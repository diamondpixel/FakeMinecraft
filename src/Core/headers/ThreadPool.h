/**
 * @file ThreadPool.h
 * @brief High-performance multithreaded task execution system.
 */

#pragma once

#include <vector>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <atomic>
#include <memory>
#include <type_traits>

/**
 * @name CPU Cache Constants
 * @{
 */
#ifdef __cpp_lib_hardware_interference_size
    constexpr std::size_t CACHE_LINE_SIZE = std::hardware_destructive_interference_size;
#else
    constexpr std::size_t CACHE_LINE_SIZE = 64; ///< Industry standard for modern x86/ARM CPUs.
#endif
/** @} */

/**
 * @class ThreadPool
 * @brief A standard executor that manages a fixed set of worker threads and a shared task queue.
 * 
 * This class is designed for low latency and high throughput. It employs several 
 * optimizations:
 * 1. **False Sharing Prevention**: Critical atomic counters are aligned to separate cache lines.
 * 2. **Lock-Free Fast Paths**: Idle checks and pending counts use relaxed atomic operations.
 * 3. **Perfect Forwarding**: Tasks are forwarded into the queue to avoid redundant copies.
 * 4. **Batch Submission**: Multiple tasks can be added in a single lock acquisition to reduce contention.
 */
class ThreadPool {
public:
    /**
     * @brief Initializes the pool and spawns worker threads.
     * @param numThreads Number of threads to create. If 0, defaults to hardware_concurrency - 1.
     */
    explicit ThreadPool(size_t numThreads = 0)
        : activeTasks(0), stopping(false), taskCount(0) {

        if (numThreads == 0) {
            numThreads = std::max(1u, std::thread::hardware_concurrency() - 1);
        }

        workers.reserve(numThreads);

        for (size_t i = 0; i < numThreads; ++i) {
            workers.emplace_back(&ThreadPool::workerThread, this);
        }
    }

    /**
     * @brief Destructor. Forces a shutdown of all threads.
     */
    ~ThreadPool() {
        shutdown();
    }

    // Disable copying and moving to ensure thread safety
    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;
    ThreadPool(ThreadPool&&) = delete;
    ThreadPool& operator=(ThreadPool&&) = delete;

    /**
     * @brief Submits a singular task to the global queue.
     * @tparam F Callable type.
     * @param task The function to execute.
     */
    template<typename F>
    void submit(F&& task) {
        static_assert(std::is_invocable_v<F>, "Task must be callable with no arguments");

        {
            std::unique_lock<std::mutex> lock(queueMutex);

            if (stopping) [[unlikely]] {
                return;
            }

            tasks.emplace(std::forward<F>(task));
            taskCount.fetch_add(1, std::memory_order_relaxed);
        }

        condition.notify_one();
    }

    /**
     * @brief Submits a range of tasks in a single batch.
     * 
     * This method is significantly faster than calling submit() in a loop because it 
     * only acquires the queue mutex once for the entire range.
     * 
     * @tparam Iterator Iterator type for a container of callables.
     * @param begin Start of range.
     * @param end End of range (exclusive).
     */
    template<typename Iterator>
    void submitBatch(Iterator begin, Iterator end) {
        if (begin == end) [[unlikely]] return;

        size_t count = 0;
        {
            std::unique_lock<std::mutex> lock(queueMutex);

            if (stopping) [[unlikely]] {
                return;
            }

            for (auto it = begin; it != end; ++it) {
                tasks.emplace(*it);
                ++count;
            }

            taskCount.fetch_add(count, std::memory_order_relaxed);
        }

        // Adaptive Notification: Single notify_all is better if many tasks added
        const size_t notifyCount = std::min(count, workers.size());
        if (notifyCount >= workers.size() / 2) {
            condition.notify_all();
        } else {
            for (size_t i = 0; i < notifyCount; ++i) {
                condition.notify_one();
            }
        }
    }

    /**
     * @brief Blocks the calling thread until all currently queued tasks are finished.
     */
    void waitAll() {
        if (isIdleFast()) [[likely]] {
            return;
        }

        std::unique_lock<std::mutex> lock(queueMutex);
        completionCondition.wait(lock, [this] {
            return tasks.empty() && activeTasks.load(std::memory_order_acquire) == 0;
        });
    }

    /**
     * @brief Blocks until either all tasks finish or the timeout is reached.
     * @return true if all tasks finished, false if timed out.
     */
    template<typename Rep, typename Period>
    bool waitFor(const std::chrono::duration<Rep, Period>& timeout) {
        if (isIdleFast()) [[likely]] {
            return true;
        }

        std::unique_lock<std::mutex> lock(queueMutex);
        return completionCondition.wait_for(lock, timeout, [this] {
            return tasks.empty() && activeTasks.load(std::memory_order_acquire) == 0;
        });
    }

    /// @return Number of tasks currently in the queue but not yet being executed.
    [[nodiscard]] size_t pendingTasks() const noexcept {
        return taskCount.load(std::memory_order_relaxed);
    }

    /// @return The total number of worker threads in the pool.
    [[nodiscard]] size_t workerCount() const noexcept {
        return workers.size();
    }

    /// @return true if no tasks are queued and no threads are busy.
    [[nodiscard]] bool isIdle() const noexcept {
        return isIdleFast();
    }

    /**
     * @brief Discards all tasks currently waiting in the queue.
     * Does NOT stop tasks already in progress.
     */
    void clearPending() {
        std::unique_lock<std::mutex> lock(queueMutex);

        const size_t cleared = tasks.size();

        std::queue<std::function<void()>> empty;
        std::swap(tasks, empty);

        taskCount.fetch_sub(cleared, std::memory_order_relaxed);
    }

    /**
     * @brief Submits a task to the front of the queue, bypassing other pending tasks.
     */
    template<typename F>
    void submitPriority(F&& task) {
        static_assert(std::is_invocable_v<F>, "Task must be callable with no arguments");

        {
            std::unique_lock<std::mutex> lock(queueMutex);

            if (stopping) [[unlikely]] {
                return;
            }

            std::queue<std::function<void()>> temp;
            temp.emplace(std::forward<F>(task));

            while (!tasks.empty()) {
                temp.push(std::move(tasks.front()));
                tasks.pop();
            }

            std::swap(tasks, temp);
            taskCount.fetch_add(1, std::memory_order_relaxed);
        }

        condition.notify_one();
    }

private:
    /**
     * @brief A lock-free inspection of pool activity.
     */
    [[nodiscard]] bool isIdleFast() const noexcept {
        return activeTasks.load(std::memory_order_relaxed) == 0 &&
               taskCount.load(std::memory_order_relaxed) == 0;
    }

    /**
     * @brief Internal loop executed by each worker thread.
     */
    void workerThread() {
        while (true) {
            std::function<void()> task;

            {
                std::unique_lock<std::mutex> lock(queueMutex);

                condition.wait(lock, [this] {
                    return stopping || !tasks.empty();
                });

                if (stopping && tasks.empty()) [[unlikely]] {
                    return;
                }

                task = std::move(tasks.front());
                tasks.pop();

                activeTasks.fetch_add(1, std::memory_order_relaxed);
                taskCount.fetch_sub(1, std::memory_order_relaxed);
            }

            try {
                task();
            } catch (...) {
                // Future: add logging or error reporter integration
            }

            // Signal task completion
            const size_t activeCount = activeTasks.fetch_sub(1, std::memory_order_release);

            if (activeCount == 1) [[unlikely]] {
                std::unique_lock<std::mutex> lock(queueMutex);
                if (tasks.empty() && activeTasks.load(std::memory_order_acquire) == 0) {
                    completionCondition.notify_all();
                }
            }
        }
    }

    /**
     * @brief Stops all threads. Blocks until they finish their CURRENT task.
     */
    void shutdown() {
        {
            std::unique_lock<std::mutex> lock(queueMutex);
            if (stopping) [[unlikely]] {
                return;
            }
            stopping = true;
        }

        condition.notify_all();

        for (std::thread& worker : workers) {
            if (worker.joinable()) {
                worker.join();
            }
        }
    }

    /** @name Thread-Safe Counters
     * Counters are aligned to CACHE_LINE_SIZE to separate them into different 
     * L1/L2 cache lines, preventing "False Sharing" performance degradation 
     * on multi-socket or high-core-count systems.
     * @{
     */
    alignas(CACHE_LINE_SIZE) std::atomic<size_t> activeTasks; ///< Number of tasks being executed right now.
    alignas(CACHE_LINE_SIZE) std::atomic<size_t> taskCount;   ///< Total tasks queued + active.
    /** @} */

    /** @name Synchronization @{ */
    std::queue<std::function<void()>> tasks; ///< FIFO task queue.
    mutable std::mutex queueMutex; ///< Protects the tasks queue and stopping flag.
    std::condition_variable condition; ///< Signals workers to wake up.
    std::condition_variable completionCondition; ///< Signals when all work is drained (waitAll).
    /** @} */

    /** @name Thread Management @{ */
    std::vector<std::thread> workers; ///< Set of durable consumer threads.
    bool stopping; ///< Lifecycle flag.
    /** @} */
};

/**
 * @class WorkStealingThreadPool
 * @brief An advanced thread pool where each worker has its own local queue.
 * 
 * Provides significantly better load balancing for heterogeneous workloads (tasks 
 * with widely varying durations). When a thread finishes its local queue, it 
 * "steals" work from another worker's queue.
 * 
 * Reduces synchronization contention vs. the global queue model.
 */
class WorkStealingThreadPool {
public:
    explicit WorkStealingThreadPool(size_t numThreads = 0);
    ~WorkStealingThreadPool();

    /**
     * @brief Submits a task to a worker's local queue.
     */
    template<typename F>
    void submit(F&& task);

    /**
     * @brief Blocks until all local and global queues are drained.
     */
    void waitAll();

private:
    /**
     * @struct WorkerData
     * @brief Container for per-thread local task storage.
     */
    struct alignas(CACHE_LINE_SIZE) WorkerData {
        std::queue<std::function<void()>> localQueue;
        std::mutex localMutex;
        std::condition_variable localCondition;
    };

    std::vector<std::unique_ptr<WorkerData>> workerQueues;
    std::vector<std::thread> workers;

    std::atomic<size_t> nextWorker{0};
    std::atomic<bool> stopping{false};

    void workerThread(size_t workerId);
    bool tryStealWork(size_t thiefId);
};
