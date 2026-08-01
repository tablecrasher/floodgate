#pragma once

#include <hiredis/hiredis.h>

#include <chrono>
#include <memory>
#include <string>

#include "fallback.h"
#include "limiter.h"

namespace limiter {

// FixedWindowLimiter divides time into fixed intervals and counts requests
// in each interval. When the count exceeds the limit, requests are denied
// until the next window starts.
class FixedWindowLimiter : public Limiter {
public:
    FixedWindowLimiter(redisContext* client, int limit, std::chrono::seconds window)
        : client_(client),
          limit_(limit),
          window_(window),
          fallback_(std::make_unique<Fallback>(limit / 2, window)) {}

    Result Allow(const std::string& key) override;

private:
    redisContext* client_;
    int limit_;
    std::chrono::seconds window_;
    // fallback uses half the normal limit as a reasonable default when Redis is unavailable
    std::unique_ptr<Fallback> fallback_;
};

}  // namespace limiter