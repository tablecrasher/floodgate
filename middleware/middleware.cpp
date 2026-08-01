// This file implements the HTTP middleware that intercepts requests,
// checks the rate limit, and either passes the request through or
// rejects it with a 429 response.
#include "middleware.h"

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

        if (!result.allowed) {
            res.status = 429;
            res.set_content("rate limit exceeded", "text/plain");
            return;
        }

        res.set_header("X-RateLimit-Remaining", std::to_string(result.remaining));
        next(req, res);
    };
}

}  // namespace middleware