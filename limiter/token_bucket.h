#pragma once

#include <hiredis/hiredis.h>

#include <string>

#include "limiter.h"

namespace limiter {

// TokenBucketLimiter maintains a bucket of tokens that refills at a constant
// rate. Each request consumes one token. Bursts are allowed up to the bucket
// capacity, and requests are denied when the bucket is empty.
class TokenBucketLimiter : public Limiter {
public:
    TokenBucketLimiter(redisContext* client, int capacity, double refill_rate)
        : client_(client), capacity_(capacity), refill_rate_(refill_rate) {}

    Result Allow(const std::string& key) override;

private:
    redisContext* client_;
    int capacity_;
    double refill_rate_;
};

}  // namespace limiter