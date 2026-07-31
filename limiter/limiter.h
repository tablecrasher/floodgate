#pragma once

#include <chrono>
#include <string>

namespace limiter {

// Result holds the outcome of a rate limit check.
struct Result {
    bool allowed = false;
    int remaining = 0;
    std::chrono::system_clock::time_point reset_at;
};

// Limiter is implemented by all rate limiting algorithms.
class Limiter {
public:
    virtual ~Limiter() = default;

    virtual Result Allow(const std::string& key) = 0;
};

}  // namespace limiter