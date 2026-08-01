#include "token_bucket.h"

#include <stdexcept>

namespace limiter {

Result TokenBucketLimiter::Allow(const std::string& key) {
    // HGETALL returns all fields and values of the hash stored at key.
    redisReply* reply =
        static_cast<redisReply*>(redisCommand(client_, "HGETALL %s", key.c_str()));
    if (reply == nullptr || reply->type != REDIS_REPLY_ARRAY) {
        if (reply) freeReplyObject(reply);
        throw std::runtime_error("error: HGETALL command not working");
    }

    // Default to a full bucket if this key has never been seen
    double tokens = static_cast<double>(capacity_);
    double lastRefill = static_cast<double>(time(nullptr));

    // reply->element alternates field, value, field, value, ...
    for (size_t i = 0; i + 1 < reply->elements; i += 2) {
        std::string field = reply->element[i]->str;
        std::string value = reply->element[i + 1]->str;
        if (field == "tokens") {
            tokens = std::stod(value);
        } else if (field == "lastRefill") {
            lastRefill = std::stod(value);
        }
    }
    freeReplyObject(reply);

    double timeNow = static_cast<double>(time(nullptr));
    double elapsedTime = timeNow - lastRefill;
    double tokensToAdd = elapsedTime * refill_rate_;
    tokens = std::min(static_cast<double>(capacity_), tokens + tokensToAdd);

    bool allowed = tokens >= 1;
    if (allowed) {
        tokens--;
    }

    redisReply* setReply = static_cast<redisReply*>(redisCommand(
        client_, "HSET %s tokens %f lastRefill %f", key.c_str(), tokens, lastRefill));
    if (setReply == nullptr) {
        throw std::runtime_error("error: HSET command not working");
    }
    freeReplyObject(setReply);

    if (allowed) {
        return Result{true, static_cast<int>(tokens), std::chrono::system_clock::time_point{}};
    }
    return Result{false, 0, std::chrono::system_clock::time_point{}};
}

}  // namespace limiter