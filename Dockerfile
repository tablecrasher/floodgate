# syntax=docker/dockerfile:1

FROM debian:bookworm-slim AS build

WORKDIR /app

RUN apt-get update && apt-get install -y --no-install-recommends \
    g++ make libhiredis-dev ca-certificates \
    && rm -rf /var/lib/apt/lists/*

COPY . .

RUN make build

FROM debian:bookworm-slim

RUN apt-get update && apt-get install -y --no-install-recommends \
    libhiredis-dev \
    && rm -rf /var/lib/apt/lists/*

COPY --from=build /app/bin/floodgate /floodgate

EXPOSE 8080

CMD ["/floodgate"]