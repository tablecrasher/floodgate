#include "fixed_window.h"

#include <ctime>

namespace limiter {

namespace {

// truncateToWindow rounds down a time point to the nearest multiple of
// window, measured from the Unix epoch (mirrors Go's time.Truncate).
std::chrono::system_clock::time_point truncateToWindow(std::chrono::system_clock::time_point now,
                                                         std::chrono::seconds window) {
    auto epoch = now.time_since_epoch();
    auto windowCount = epoch / window;
    return std::chrono::system_clock::time_point(windowCount * window);
}

}  // namespace

Result FixedWindowLimiter::Allow(const std::string& key) {
    auto now = std::chrono::system_clock::now();
    auto windowStart = truncateToWindow(now, window_);
    auto resetAt = windowStart + window_;

    // Include window timestamp in key to avoid relying on Redis TTL, which can lag.
    long long windowUnix =
        std::chrono::duration_cast<std::chrono::seconds>(windowStart.time_since_epoch()).count();
    std::string windowKey = key + ":" + std::to_string(windowUnix);

    // If INCR is called on a key that doesn't exist yet, it returns 1
    redisReply* reply =
        static_cast<redisReply*>(redisCommand(client_, "INCR %s", windowKey.c_str()));
    if (reply == nullptr) {
        return fallback_->Allow(key);
    }
    long long count = reply->integer;
    freeReplyObject(reply);

    // If first request in that window, set an expiry
    if (count == 1) {
        redisReply* expireReply = static_cast<redisReply*>(
            redisCommand(client_, "EXPIRE %s %lld", windowKey.c_str(), (long long)window_.count()));
        if (expireReply == nullptr) {
            return fallback_->Allow(key);
        }
        freeReplyObject(expireReply);
    }

    if (count <= limit_) {
        return Result{true, limit_ - static_cast<int>(count), resetAt};
    }
    return Result{false, 0, resetAt};
}

}  // namespace limiter