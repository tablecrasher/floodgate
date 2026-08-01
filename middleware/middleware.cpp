// This file implements the HTTP middleware that intercepts requests,
// checks the rate limit, and either passes the request through or
// rejects it with a 429 response.
#include "middleware.h"

#include <chrono>

namespace middleware {

// extractIP pulls the client IP from the request. httplib already gives us
// the bare IP (no port) via remote_addr.
static std::string extractIP(const httplib::Request& req) { return req.remote_addr; }

Handler RateLimitMiddleware(limiter::Limiter* l, Handler next) {
    return [l, next](const httplib::Request& req, httplib::Response& res) {
        std::string host = extractIP(req);

        limiter::Result result;
        try {
            result = l->Allow(host);
        } catch (const std::exception&) {
            res.status = 500;
            res.set_content("internal server error", "text/plain");
            return;
        }

        // Set headers on every response so clients know their current rate limit status.
        long long resetUnix = std::chrono::duration_cast<std::chrono::seconds>(
                                   result.reset_at.time_since_epoch())
                                   .count();
        res.set_header("X-RateLimit-Remaining", std::to_string(result.remaining));
        res.set_header("X-RateLimit-Reset", std::to_string(resetUnix));

        if (!result.allowed) {
            // Retry-After tells the client how many seconds to wait before retrying.
            // Only sent on 429 responses.
            auto retryAfter = std::chrono::duration_cast<std::chrono::seconds>(
                                   result.reset_at - std::chrono::system_clock::now())
                                   .count();
            res.set_header("Retry-After", std::to_string(retryAfter));
            res.status = 429;
            res.set_content("rate limit exceeded", "text/plain");
            return;
        }

        next(req, res);
    };
}

}  // namespace middleware