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

const char* kSlidingWindowScript = R"(
	local prevKey = KEYS[1]
	local curKey = KEYS[2]
	local limit = tonumber(ARGV[1])
	local remainingRatio = tonumber(ARGV[2])
	local window = tonumber(ARGV[3])

	local result = redis.call("MGET", prevKey, curKey)
	local prevCount = result[1]
	local currCount = result[2]

	if prevCount == false then prevCount = 0 else prevCount = tonumber(prevCount) end
	if currCount == false then currCount = 0 else currCount = tonumber(currCount) end

	local estimate = (prevCount * remainingRatio) + currCount
	if estimate < limit then
		local newCount = redis.call("INCR", curKey)
		if newCount == 1 then
			redis.call("EXPIRE", curKey, window)
		end
		local remaining = limit - estimate - 1
		return {1, remaining}
	else
		return {0, 0}
	end
)";

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

    // Calculate what fraction of the previous sub-window still falls inside the
    // current sliding window. Capped at 1.0 for when it's fully within range.
    auto swStart = now - window_;
    double remainingRatio =
        std::min(1.0, std::chrono::duration<double>((curSW + sub_window_) - swStart).count() /
                           std::chrono::duration<double>(sub_window_).count());

    // Pass remainingRatio from C++ rather than calculating it in Lua — time math
    // is simpler here and keeps the script focused on Redis operations.
    redisReply* reply = static_cast<redisReply*>(
        redisCommand(client_, "EVAL %s 2 %s %s %d %f %lld", kSlidingWindowScript,
                     prevKey.c_str(), curKey.c_str(), limit_, remainingRatio,
                     (long long)window_.count()));
    if (reply == nullptr || reply->type != REDIS_REPLY_ARRAY || reply->elements != 2) {
        if (reply) freeReplyObject(reply);
        return fallback_->Allow(key);
    }

    long long allowed = reply->element[0]->integer;
    long long remaining = reply->element[1]->integer;
    freeReplyObject(reply);

    return Result{allowed == 1, static_cast<int>(remaining), now + window_};
}

}  // namespace limiter