#pragma once

#include <hiredis/hiredis.h>

#include <chrono>
#include <stdexcept>
#include <string>

#include "limiter.h"

namespace limiter {

// FixedWindowLimiter divides time into fixed intervals and counts requests
// in each interval. When the count exceeds the limit, requests are denied
// until the next window starts.
class FixedWindowLimiter : public Limiter {
public:
    FixedWindowLimiter(redisContext* client, int limit, std::chrono::seconds window)
        : client_(client), limit_(limit), window_(window) {}

    Result Allow(const std::string& key) override;

private:
    redisContext* client_;
    int limit_;
    std::chrono::seconds window_;
};

}  // namespace limiter