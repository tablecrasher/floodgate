#include "sliding_window.h"

#include <algorithm>
#include <stdexcept>

namespace limiter {

namespace {

std::chrono::system_clock::time_point truncateTo(std::chrono::system_clock::time_point now,
                                                   std::chrono::seconds unit) {
    auto epoch = now.time_since_epoch();
    auto count = epoch / unit;
    return std::chrono::system_clock::time_point(count * unit);
}

}  // namespace

Result SlidingWindowLimiter::Allow(const std::string& key) {
    auto now = std::chrono::system_clock::now();
    auto curSW = truncateTo(now, sub_window_);
    auto prevSW = curSW - sub_window_;

    long long curUnix =
        std::chrono::duration_cast<std::chrono::seconds>(curSW.time_since_epoch()).count();
    long long prevUnix =
        std::chrono::duration_cast<std::chrono::seconds>(prevSW.time_since_epoch()).count();
    std::string curKey = key + ":" + std::to_string(curUnix);
    std::string prevKey = key + ":" + std::to_string(prevUnix);

    redisReply* reply = static_cast<redisReply*>(
        redisCommand(client_, "MGET %s %s", prevKey.c_str(), curKey.c_str()));
    if (reply == nullptr || reply->type != REDIS_REPLY_ARRAY || reply->elements != 2) {
        if (reply) freeReplyObject(reply);
        throw std::runtime_error("error: could not retrieve values of specified keys");
    }

    auto prevEnd = prevSW + sub_window_;
    auto swStart = now - window_;
    auto swSize = sub_window_;

    double remainingRatio =
        std::min(1.0, std::chrono::duration<double>(prevEnd - swStart).count() /
                           std::chrono::duration<double>(swSize).count());

    double prevCount = 0;
    if (reply->element[0]->type == REDIS_REPLY_STRING) {
        prevCount = std::stod(reply->element[0]->str);
    }
    double currCount = 0;
    if (reply->element[1]->type == REDIS_REPLY_STRING) {
        currCount = std::stod(reply->element[1]->str);
    }
    freeReplyObject(reply);

    double estimate = (prevCount * remainingRatio) + currCount;

    if (estimate < limit_) {
        redisReply* incrReply =
            static_cast<redisReply*>(redisCommand(client_, "INCR %s", curKey.c_str()));
        if (incrReply == nullptr) {
            throw std::runtime_error("error: could not increment current sub window counter");
        }
        long long newCount = incrReply->integer;
        freeReplyObject(incrReply);

        if (newCount == 1) {
            redisReply* expireReply = static_cast<redisReply*>(redisCommand(
                client_, "EXPIRE %s %lld", curKey.c_str(), (long long)window_.count()));
            if (expireReply) freeReplyObject(expireReply);
        }

        return Result{true, limit_ - static_cast<int>(estimate) - 1, now + window_};
    }
    return Result{false, 0, now + window_};
}

}  // namespace limiter