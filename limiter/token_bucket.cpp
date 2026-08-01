#include "token_bucket.h"

#include <stdexcept>

namespace limiter {

namespace {

// tokenBucketScript is defined once to avoid recompiling the script on every request.
const char* kTokenBucketScript = R"(
	local keys = KEYS[1]
	local capacity = tonumber(ARGV[1])
	local refill_rate = tonumber(ARGV[2])
	local now = tonumber(ARGV[3])

	local result = redis.call("HMGET", keys, "tokens", "lastRefill")
	local tokens = result[1] -- Lua arrays start at 1, not 0
	local lastRefill = result[2]

	if tokens == false then
		tokens = capacity
	end
	if lastRefill == false then
		lastRefill = now
	end

	local elapsedTime = now - lastRefill
	local tokensToAdd = elapsedTime * refill_rate

	tokens = math.min(capacity, tokens+tokensToAdd)

	local allowed = 0
	if tokens >= 1 then
		tokens = tokens - 1
		allowed = 1
	else
		allowed = 0
	end

	local remaining = tokens
	redis.call("HSET", keys, "tokens", tokens, "lastRefill", now)
	redis.call("EXPIRE", keys, 3600)

	return {allowed, remaining}
)";

}  // namespace

Result TokenBucketLimiter::Allow(const std::string& key) {
    long long now = static_cast<long long>(time(nullptr));

    redisReply* reply = static_cast<redisReply*>(redisCommand(
        client_, "EVAL %s 1 %s %d %f %lld", kTokenBucketScript, key.c_str(), capacity_,
        refill_rate_, now));
    if (reply == nullptr || reply->type != REDIS_REPLY_ARRAY || reply->elements != 2) {
        if (reply) freeReplyObject(reply);
        return fallback_->Allow(key);
    }

    long long allowed = reply->element[0]->integer;
    long long remaining = reply->element[1]->integer;
    freeReplyObject(reply);

    return Result{
        allowed == 1,
        static_cast<int>(remaining),
        std::chrono::system_clock::time_point{},
    };
}

}  // namespace limiter