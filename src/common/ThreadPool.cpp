#include "ThreadPool.hpp"

ThreadPool::ThreadPool(size_t threadCount) {
    threads.reserve(threadCount);
    for (size_t i = 0; i < threadCount; ++i) {
        threads.emplace_back([this](){
            while (true) {
                std::function<void()> task;
                {
                    std::unique_lock<std::mutex> lock(mtx);
                    
                    cv_get.wait(lock, [this] {
                        return (exit.load() && tasks.empty()) || term.load() || !tasks.empty();
                    });

                    if (term.load()) {
                        return;
                    }

                    if (exit.load() && tasks.empty()) {
                        cv_empty.notify_all();
                        return;
                    }

                    task = std::move(tasks.front());
                    tasks.pop();
                } 
                task();
            }
        });
    }
}

ThreadPool::~ThreadPool() {
    if (!exit.load() && !term.load()) {
        Terminate(true);
    }
}

void ThreadPool::PushTask(const std::function<void()>& task) {
    std::lock_guard<std::mutex> lock(mtx);
    if (exit.load()) {
        throw std::runtime_error("Cannot push tasks after termination");
    }
    tasks.push(task);
    cv_get.notify_one();
}

void ThreadPool::Terminate(bool wait) {
    {
        std::unique_lock<std::mutex> lock(mtx);
        if (stopped) {
            return;
        }

        exit.store(true);
        if (!wait) {
            term.store(true);
        }
    }
    
    cv_get.notify_all();

    {
        std::unique_lock<std::mutex> lock(mtx);
        if (wait) {
            cv_empty.wait(lock, [this]() { return tasks.empty(); });
        }
        stopped = true;
    }

    for (auto& thread : threads) {
        if (thread.joinable()) {
            thread.join();
        }
    }
}

bool ThreadPool::IsActive() const {
    return !exit.load();
}

size_t ThreadPool::QueueSize() const {
    std::lock_guard<std::mutex> lock(mtx);
    return tasks.size();
}