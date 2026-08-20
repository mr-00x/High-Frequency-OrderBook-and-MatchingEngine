# ==========================================
# Build Stage
# ==========================================
FROM gcc:12-bullseye AS builder

WORKDIR /app

# Install CMake
RUN apt-get update && apt-get install -y --no-install-recommends cmake make && rm -rf /var/lib/apt/lists/*

# Copy source code
COPY CMakeLists.txt ./
COPY src/ ./src/
COPY include/ ./include/
COPY tests/ ./tests/

# Build project and run tests
RUN cmake -S . -B build -DCMAKE_BUILD_TYPE=Release \
    && cmake --build build -j$(nproc) \
    && ./build/test_memory_pool \
    && ./build/test_order_book \
    && ./build/test_matching_engine \
    && ./build/benchmark

# ==========================================
# Production Runtime Stage
# ==========================================
FROM debian:bullseye-slim

WORKDIR /app

# Install minimal runtime dependencies
RUN apt-get update && apt-get install -y --no-install-recommends ca-certificates && rm -rf /var/lib/apt/lists/*

# Copy compiled binary from builder stage
COPY --from=builder /app/build/hft_engine /app/hft_engine

# Expose default HTTP port
EXPOSE 8080

ENV PORT=8080

# Run high-frequency matching engine
CMD ["/app/hft_engine"]
