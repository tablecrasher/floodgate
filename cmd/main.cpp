#include <iostream>

#include "../limiter/fixed_window.h"
#include "../middleware/middleware.h"
#include "../store/redis_client.h"
#include "../third_party/httplib.h"

int main() {
    std::unique_ptr<store::RedisClient> client;
    try {
        client = store::RedisClient::NewRedisClient();
    } catch (const std::exception& e) {
        std::cerr << e.what() << std::endl;
        return 1;
    }

    std::cout << "connected to redis!" << std::endl;

    limiter::FixedWindowLimiter l(client->raw(), 100, std::chrono::minutes(1));

    middleware::Handler handler = [](const httplib::Request&, httplib::Response& res) {
        res.set_content("ok", "text/plain");
    };

    middleware::Handler wrapped = middleware::RateLimitMiddleware(&l, handler);

    httplib::Server svr;
    svr.Get("/", wrapped);

    if (!svr.listen("0.0.0.0", 8080)) {
        std::cerr << "failed to start server" << std::endl;
        return 1;
    }
}