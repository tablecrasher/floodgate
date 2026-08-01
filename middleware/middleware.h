#pragma once

#include <functional>

#include "../limiter/limiter.h"
#include "../third_party/httplib.h"

namespace middleware {

using Handler = std::function<void(const httplib::Request&, httplib::Response&)>;

// RateLimitMiddleware wraps a handler and rejects requests that exceed the
// rate limit.
Handler RateLimitMiddleware(limiter::Limiter* l, Handler next);

}  // namespace middleware