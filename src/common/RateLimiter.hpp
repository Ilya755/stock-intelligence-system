#pragma once

#include <chrono>
#include <mutex>

#include "Config.hpp"

class RateLimiter {
public:
    explicit RateLimiter(const Limits& limits);

    bool TryAcquire();

private:
    int max_per_minute_;
    int max_per_day_;

    int current_minute_count_ = 0;
    int current_day_count_ = 0;

    std::chrono::steady_clock::time_point last_minute_start_;
    std::chrono::sys_days last_day_start_;

    std::mutex mutex_;

    void UpdateWindows();
};