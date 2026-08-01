#pragma once

#include <hiredis/hiredis.h>

#include <chrono>
#include <string>

#include "limiter.h"

namespace limiter {

// SlidingWindowLimiter approximates a true sliding window by dividing time into
// sub-windows and tracking two counters: the previous and current sub-window.
// This reduces the boundary problem where fixed windows can allow 2x the limit
// at window edges.
class SlidingWindowLimiter : public Limiter {
public:
    SlidingWindowLimiter(redisContext* client, int limit, std::chrono::seconds window,
                          int num_sub_windows)
        : client_(client), limit_(limit), window_(window), sub_window_(window / num_sub_windows) {}

    Result Allow(const std::string& key) override;

private:
    redisContext* client_;
    int limit_;
    std::chrono::seconds window_;
    std::chrono::seconds sub_window_;
};

}  // namespace limiter