#pragma once

#include <thread>
#include <mutex>
#include <vector>
#include <queue>
#include <functional>
#include <condition_variable>
#include <atomic>
#include <exception>
#include <future>

class ThreadPool {
public:
    explicit ThreadPool(size_t threadCount);

    ~ThreadPool();

    void PushTask(const std::function<void()>& task);

    void Terminate(bool wait);

    bool IsActive() const;

    size_t QueueSize() const;

private:
    std::vector<std::thread> threads;
    std::queue<std::function<void()>> tasks;
    
    mutable std::mutex mtx;
    std::condition_variable cv_get;
    std::condition_variable cv_empty;

    std::atomic<bool> exit = false;
    std::atomic<bool> term = false;   
    bool stopped = false; 
};