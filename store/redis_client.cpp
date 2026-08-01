// Package store provides the Redis client connection for the rate limiter.
// It exposes a single constructor that connects and validates the connection
// before returning the client.
#include "redis_client.h"

#include <cstdlib>
#include <stdexcept>

namespace store {

std::unique_ptr<RedisClient> RedisClient::NewRedisClient() {
    const char* addrEnv = std::getenv("REDIS_ADDR");
    std::string addr = (addrEnv != nullptr && addrEnv[0] != '\0') ? addrEnv : "localhost:6379";

    auto sep = addr.find(':');
    std::string host = addr.substr(0, sep);
    int port = std::stoi(addr.substr(sep + 1));

    redisContext* ctx = redisConnect(host.c_str(), port);
    if (ctx == nullptr || ctx->err) {
        std::string msg = ctx ? ctx->errstr : "can't allocate redis context";
        if (ctx) redisFree(ctx);
        throw std::runtime_error("could not connect to redis: " + msg);
    }

    // context.Background() in Go is the root context, no deadline, never
    // cancelled — the ping below is just a synchronous connectivity check.
    redisReply* reply = static_cast<redisReply*>(redisCommand(ctx, "PING"));
    if (reply == nullptr) {
        redisFree(ctx);
        throw std::runtime_error("could not connect to redis: no reply to PING");
    }
    freeReplyObject(reply);

    return std::unique_ptr<RedisClient>(new RedisClient(ctx));
}

RedisClient::~RedisClient() {
    if (ctx_) redisFree(ctx_);
}

}  // namespace store