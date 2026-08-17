#include "RateLimiter.hpp"

#include "Logger.hpp"

RateLimiter::RateLimiter(const Limits& limits)
    : max_per_minute_(limits.requests_per_minute)
    , max_per_day_(limits.requests_per_day) {
        last_minute_start_ = std::chrono::steady_clock::now();
        last_day_start_ = std::chrono::floor<std::chrono::days>(std::chrono::system_clock::now());
    }

bool RateLimiter::TryAcquire() {
    std::lock_guard<std::mutex> lock(mutex_);

    if (max_per_minute_ == 0 || max_per_day_ == 0) {
        return false;
    }

    UpdateWindows();

    if (max_per_minute_ > 0 && current_minute_count_ == max_per_minute_) {
        Logger::Warn(
            "[RateLimiter] Minute limit hit: {}/{}", 
            current_minute_count_, max_per_minute_);
        return false;
    }
    if (max_per_day_ > 0 && current_day_count_ == max_per_day_) {
        Logger::Warn(
            "[RateLimiter] Daily limit hit: {}/{}", 
            current_day_count_, max_per_day_);
        return false;
    }

    ++current_minute_count_;
    ++current_day_count_;
    return true;
}

void RateLimiter::UpdateWindows() {
    auto now_steady = std::chrono::steady_clock::now();
    auto elapsed_seconds = std::chrono::duration_cast<std::chrono::seconds>(now_steady - last_minute_start_).count();
    
    if (elapsed_seconds >= 60) {
        current_minute_count_ = 0;
        last_minute_start_ = now_steady;
    }

    auto today = std::chrono::floor<std::chrono::days>(std::chrono::system_clock::now());

    if (today != last_day_start_) {
        Logger::Info(
            "[RateLimiter] New UTC day detected. Resetting daily counters.");
        current_day_count_ = 0;
        last_day_start_ = today;
    }
}