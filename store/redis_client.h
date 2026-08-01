#pragma once

#include <hiredis/hiredis.h>

#include <memory>
#include <string>

namespace store {

// Thin RAII wrapper around a hiredis connection.
class RedisClient {
public:
    static std::unique_ptr<RedisClient> NewRedisClient();

    ~RedisClient();

    redisContext* raw() const { return ctx_; }

private:
    explicit RedisClient(redisContext* ctx) : ctx_(ctx) {}

    redisContext* ctx_;
};

}  // namespace store