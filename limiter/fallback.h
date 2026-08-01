#pragma once

#include <chrono>
#include <mutex>
#include <string>
#include <unordered_map>

#include "limiter.h"

namespace limiter {

// Fallback is an in-memory rate limiter used when Redis is unavailable.
// It uses a fixed window counter protected by a mutex for concurrent access.
// Limits are set lower than the main limiter since counters are not shared
// across servers.
class Fallback {
public:
    Fallback(int limit, std::chrono::seconds window) : limit_(limit), window_(window) {}

    Result Allow(const std::string& key);

private:
    std::mutex mu_;
    std::unordered_map<std::string, int> counters_;  // old keys are never deleted - acceptable for short fallback usage
    int limit_;
    std::chrono::seconds window_;
};

}  // namespace limiter