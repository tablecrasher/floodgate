# Floodgate

Floodgate is a distributed rate limiter, ported to C++, that controls how many requests a client can make to an API. It uses Redis to share counters across multiple servers, so limits are enforced consistently no matter which server handles the request.

---

## Request Flow

<img width="1440" height="1040" alt="image" src="https://github.com/user-attachments/assets/fc03dad7-4c0a-497b-85ea-9ef42dc61ed6" />

---

## Middleware

Every request hits the middleware first. It extracts the client's IP, calls the limiter, and either passes the request through or returns a `429 Too Many Requests` response immediately.

---

## Limiter

The limiter checks the current request count against the configured limit. Three algorithms are supported:

**Fixed Window** — Counts requests in fixed time intervals (e.g. 10:00–10:01). Simple and fast, but a client can game the boundary by sending requests at the very end and start of two windows back to back, effectively getting double the limit.

**Token Bucket** — Imagine a bucket that slowly refills with tokens. Each request uses one token. You can burst through requests quickly, but once the bucket is empty you have to wait for it to refill. The refill rate controls how fast access is restored.

**Sliding Window** — Rather than resetting a counter on a hard boundary, it looks back a rolling 60 seconds from right now. It tracks two sub-buckets — the previous and the current — and blends them together based on how much of the previous bucket still falls within the last 60 seconds. This smooths out the boundary problem without storing every single request timestamp.

---

## Redis

Counters are stored in Redis so every server reads and writes to the same data. A client can't bypass the limit by hitting a different server. All updates use atomic Lua scripts to prevent race conditions across servers.

---

## Fallback

If Redis goes down, each limiter switches to an in-memory counter with half the normal limit. The API stays protected without going down completely. Since each server has its own fallback counter during an outage, limits aren't coordinated across servers — an accepted tradeoff to keep things running.

---

## Project Structure

```
floodgate/
  cmd/
    main.cpp            (starts the server)
  store/
    redis_client.h
    redis_client.cpp     (connects to Redis)
  limiter/
    limiter.h            (shared interface used by all algorithms)
    fixed_window.h
    fixed_window.cpp
    token_bucket.h
    token_bucket.cpp
    sliding_window.h
    sliding_window.cpp
    fallback.h
    fallback.cpp         (backup limiter if Redis goes down)
  middleware/
    middleware.h
    middleware.cpp       (intercepts requests and checks the limit)
  docker-compose.yml     (spins up 3 app instances, Redis, and Nginx)
  Dockerfile             (builds the C++ app image)
  nginx.conf             (load balancer config)
  Makefile
```

## Getting Started

**Requirements:** a C++17 compiler, Make, libhiredis, Docker

**1. Clone the repo and start everything (requires Docker):**
```bash
docker compose up --build
```

This spins up 3 C++ app instances, a Redis container, and an Nginx load balancer. Requests are distributed across all three instances, all sharing the same Redis counters.

**2. Test it:**
```bash
curl -i http://localhost:80
```

**3. Verify the rate limit is enforced across all instances:**
```bash
for i in $(seq 1 110); do curl -s -o /dev/null -w "%{http_code}\n" http://localhost:80; done
```

You'll see 100 `200` responses followed by `429`s — proving all three instances share the same counter through Redis.

---

## Response Headers

Every response includes headers so clients know where they stand:

| Header | Description |
|---|---|
| `X-RateLimit-Remaining` | Requests left in the current window |
| `X-RateLimit-Reset` | When the window resets (Unix timestamp) |
| `Retry-After` | How long to wait before retrying (only on 429) |

---

## Switching Algorithms

To switch algorithms, update `cmd/main.cpp`:

```cpp
// Fixed Window — 100 requests per minute
limiter::FixedWindowLimiter l(client->raw(), 100, std::chrono::minutes(1));

// Token Bucket — capacity 100, refills 10 tokens per second
limiter::TokenBucketLimiter l(client->raw(), 100, 10.0);

// Sliding Window — 100 requests per minute, 6 sub-windows
limiter::SlidingWindowLimiter l(client->raw(), 100, std::chrono::minutes(1), 6);
```
