#pragma once

#include <vector>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <atomic>

class ThreadPool {
public:
    explicit ThreadPool(size_t numThreads = 0)
        : activeTasks(0), stopping(false) {
        // Default to hardware concurrency minus 1 (leave one for main thread)
        if (numThreads == 0) {
            numThreads = std::max(1u, std::thread::hardware_concurrency() - 1);
        }

        workers.reserve(numThreads);

        for (size_t i = 0; i < numThreads; ++i) {
            workers.emplace_back(&ThreadPool::workerThread, this);
        }
    }

    ~ThreadPool() {
        shutdown();
    }

    // Delete copy and move operations
    ThreadPool(const ThreadPool &) = delete;

    ThreadPool &operator=(const ThreadPool &) = delete;

    ThreadPool(ThreadPool &&) = delete;

    ThreadPool &operator=(ThreadPool &&) = delete;

    // Submit a task to the pool
    template<class F>
    void submit(F &&task) {
        {
            std::unique_lock<std::mutex> lock(queueMutex);

            // Don't accept new tasks if stopping
            if (stopping) {
                return;
            }

            tasks.emplace(std::forward<F>(task));
        }
        condition.notify_one();
    }

    // Batch submit multiple tasks (more efficient than individual submits)
    template<class Iterator>
    void submitBatch(Iterator begin, Iterator end) {
        {
            std::unique_lock<std::mutex> lock(queueMutex);

            if (stopping) {
                return;
            }

            for (auto it = begin; it != end; ++it) {
                tasks.emplace(*it);
            }
        }

        // Notify all workers for batch processing
        condition.notify_all();
    }

    // Wait for all pending tasks to complete
    void waitAll() {
        std::unique_lock<std::mutex> lock(queueMutex);
        completionCondition.wait(lock, [this] {
            return tasks.empty() && activeTasks.load(std::memory_order_acquire) == 0;
        });
    }

    // Wait with timeout (returns true if all tasks completed, false if timed out)
    template<class Rep, class Period>
    bool waitFor(const std::chrono::duration<Rep, Period> &timeout) {
        std::unique_lock<std::mutex> lock(queueMutex);
        return completionCondition.wait_for(lock, timeout, [this] {
            return tasks.empty() && activeTasks.load(std::memory_order_acquire) == 0;
        });
    }

    // Get number of pending tasks
    size_t pendingTasks() const {
        std::unique_lock<std::mutex> lock(queueMutex);
        return tasks.size() + activeTasks.load(std::memory_order_relaxed);
    }

    // Get number of worker threads
    size_t workerCount() const {
        return workers.size();
    }

    // Check if pool is idle (no pending or active tasks)
    bool isIdle() const {
        std::unique_lock<std::mutex> lock(queueMutex);
        return tasks.empty() && activeTasks.load(std::memory_order_relaxed) == 0;
    }

    // Clear all pending tasks (does not affect currently executing tasks)
    void clearPending() {
        std::unique_lock<std::mutex> lock(queueMutex);

        // Efficiently clear the queue
        std::queue<std::function<void()> > empty;
        std::swap(tasks, empty);
    }

private:
    // Worker thread function
    void workerThread() {
        while (true) {
            std::function<void()> task;

            {
                std::unique_lock<std::mutex> lock(queueMutex);

                // Wait for task or stop signal
                condition.wait(lock, [this] {
                    return stopping || !tasks.empty();
                });

                // Exit if stopping and no more tasks
                if (stopping && tasks.empty()) {
                    return;
                }

                // Get task from queue
                task = std::move(tasks.front());
                tasks.pop();
            }

            // Execute task outside the lock
            activeTasks.fetch_add(1, std::memory_order_release);

            try {
                task();
            } catch (...) {
                // Catch exceptions to prevent thread termination
                // In production, you might want to log these
            }

            activeTasks.fetch_sub(1, std::memory_order_release);

            // Notify completion (check if we're now idle)
            if (activeTasks.load(std::memory_order_acquire) == 0) {
                std::unique_lock<std::mutex> lock(queueMutex);
                if (tasks.empty()) {
                    completionCondition.notify_all();
                }
            }
        }
    }

    // Shutdown the thread pool
    void shutdown() {
        {
            std::unique_lock<std::mutex> lock(queueMutex);
            if (stopping) {
                return; // Already stopped
            }
            stopping = true;
        }

        condition.notify_all();

        for (std::thread &worker: workers) {
            if (worker.joinable()) {
                worker.join();
            }
        }
    }

    std::vector<std::thread> workers;
    std::queue<std::function<void()> > tasks;

    mutable std::mutex queueMutex;
    std::condition_variable condition;
    std::condition_variable completionCondition;

    std::atomic<size_t> activeTasks;
    bool stopping;
};