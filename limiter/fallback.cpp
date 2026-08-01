#include "fallback.h"

namespace limiter {

namespace {

std::chrono::system_clock::time_point truncateToWindow(std::chrono::system_clock::time_point now,
                                                         std::chrono::seconds window) {
    auto epoch = now.time_since_epoch();
    auto windowCount = epoch / window;
    return std::chrono::system_clock::time_point(windowCount * window);
}

}  // namespace

Result Fallback::Allow(const std::string& key) {
    auto now = std::chrono::system_clock::now();
    auto windowStart = truncateToWindow(now, window_);
    long long windowUnix =
        std::chrono::duration_cast<std::chrono::seconds>(windowStart.time_since_epoch()).count();
    std::string windowKey = key + ":" + std::to_string(windowUnix);

    // Lock protects the counters map from concurrent writes across threads.
    std::lock_guard<std::mutex> lock(mu_);
    int count = ++counters_[windowKey];

    if (count <= limit_) {
        return Result{true, limit_ - count, windowStart + window_};
    }
    return Result{false, 0, windowStart + window_};
}

}  // namespace limiter