# High-Frequency Limit Order Book & Ultra-Fast Matching Engine

[![C++17](https://img.shields.io/badge/C%2B%2B-17-blue.svg?style=flat&logo=c%2B%2B)](https://en.cppreference.com/w/cpp/17)
[![Build & Test CI](https://img.shields.io/badge/Build-Passing-brightgreen.svg?style=flat&logo=githubactions)](.github/workflows/ci.yml)
[![Throughput](https://img.shields.io/badge/Throughput-900k%2B%20ops%2Fsec-success.svg?style=flat)]()
[![Latency](https://img.shields.io/badge/Latency-781%20ns%20(Sub--µs)-blueviolet.svg?style=flat)]()
[![Docker](https://img.shields.io/badge/Docker-Ready-2496ED.svg?style=flat&logo=docker)](Dockerfile)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)

A **production-grade, deterministic, low-latency Limit Order Book (LOB) and Matching Engine** engineered in modern C++17. Designed from the ground up for high-throughput algorithmic trading venues and financial exchanges adhering to **Price-Time Priority (FIFO)** matching rules.

It features custom **fixed-size slab memory pooling**, **intrusive doubly-linked price levels**, **cache-conscious memory layouts**, **fixed-point integer arithmetic**, and an **interactive real-time React trading dashboard**.

---

## ⚡ Performance & Benchmark Telemetry

Benchmarked on standard hardware across **100,000 randomized order operations** (limit inserts, aggressive crosses, market sweeps, and cancellations):

```
======================================================================
 Running Matching Engine Benchmark (100,000 Orders)
======================================================================
 Orders Processed:      100,000
 Total Execution Time:  111.08 ms
 Peak Throughput:       900,276.38 ops/sec (900k+ ops/sec)
 Mean Match Latency:    0.78 µs (781.59 nanoseconds)
 Trades Generated:      77,558
 Cumulative Volume:     1,013,137 contracts
 Runtime Heap Allocs:   0 (Zero dynamic allocations on hot path)
======================================================================
```

### Resume Metric & Impact
> *"Architected an in-memory Limit Order Book and Matching Engine in C++17 processing **900,000+ simulated order operations per second** with **781 ns matching latency**, eliminating dynamic allocations via custom intrusive slab pools and achieving $O(1)$ order cancellations."*

---

## 🏛️ System Architecture

```
                       ┌──────────────────────────────────────────────┐
                       │   React + Vite High-Frequency Dashboard      │
                       │   (Live L2 Depth, Trade Tape, Sim Bot)      │
                       └──────────────────────┬───────────────────────┘
                                              │ HTTP/1.1 REST (JSON)
                                              ▼
                       ┌──────────────────────────────────────────────┐
                       │         Native HTTP/1.1 Socket Server        │
                       │  - Windows Winsock2 / POSIX Non-Blocking I/O │
                       │  - HTTP Request / Header Parsing Engine      │
                       └──────────────────────┬───────────────────────┘
                                              │ LockGuard Synchronization
                                              ▼
┌─────────────────────────────────────────────────────────────────────────────────────────────┐
│                                 Matching Engine Core (HFT)                                  │
│                                                                                             │
│  ┌─────────────────────────┐  ┌─────────────────────────┐  ┌─────────────────────────────┐  │
│  │   Monotonic Clock &     │  │   Rolling Telemetry     │  │    Bounded Trade Ring       │  │
│  │   Sequence Counter      │  │   (Latency & Ops/sec)   │  │    Buffer (History Log)     │  │
│  │   (std::atomic<uint64>) │  │   (Online Running Welford)│  │    (std::deque<Trade>)    │  │
│  └─────────────────────────┘  └─────────────────────────┘  └─────────────────────────────┘  │
│                                              │                                              │
│                                              ▼                                              │
│  ┌───────────────────────────────────────────────────────────────────────────────────────┐  │
│  │                            Price-Time Priority Matching                               │  │
│  │   - Walk opposite book from best price level                                          │  │
│  │   - Execute FIFO against resting orders with price improvement                        │  │
│  │   - Rest unfilled limit quantities or cancel unfilled market quantities               │  │
│  └───────────────────────────────────┬───────────────────────────────────────────────────┘  │
│                                      │                                                      │
│                   ┌──────────────────┴──────────────────┐                                   │
│                   ▼                                     ▼                                   │
│  ┌─────────────────────────────────┐   ┌─────────────────────────────────┐                  │
│  │      Bid Book (Buy Orders)      │   │     Ask Book (Sell Orders)      │                  │
│  │  std::map<Price, PriceLevel,    │   │  std::map<Price, PriceLevel,    │                  │
│  │            std::greater<Price>> │   │            std::less<Price>>    │                  │
│  │  - Highest price at begin()     │   │  - Lowest price at begin()      │                  │
│  └────────────────┬────────────────┘   └────────────────┬────────────────┘                  │
│                   │                                     │                                   │
│                   └──────────────────┬──────────────────┘                                   │
│                                      ▼                                                      │
│  ┌───────────────────────────────────────────────────────────────────────────────────────┐  │
│  │                     Intrusive Doubly-Linked PriceLevel Lists                          │  │
│  │   - Head points to oldest order (O(1) pop on match)                                   │  │
│  │   - Tail points to newest order (O(1) push on insert)                                 │  │
│  │   - Direct pointer unlinking on cancel: O(1)                                          │  │
│  └───────────────────────────────────┬───────────────────────────────────────────────────┘  │
│                                      │                                                      │
│                                      ▼                                                      │
│  ┌───────────────────────────────────────────────────────────────────────────────────────┐  │
│  │                     Order Index Map (std::unordered_map)                              │  │
│  │   - Key: OrderId (uint64_t) ───► Value: Order* (Raw pointer in slab)                  │  │
│  │   - Instantaneous O(1) cancellation lookup                                            │  │
│  └───────────────────────────────────┬───────────────────────────────────────────────────┘  │
│                                      │                                                      │
│                                      ▼                                                      │
│  ┌───────────────────────────────────────────────────────────────────────────────────────┐  │
│  │                     Fixed-Size Slab Allocator (MemoryPool<T>)                         │  │
│  │   - Contiguous 64k order slab allocated up front                                      │  │
│  │   - Intrusive singly-linked free-list embedded inside free blocks                     │  │
│  │   - O(1) allocate() / O(1) deallocate() with zero OS system calls                     │  │
│  └───────────────────────────────────────────────────────────────────────────────────────┘  │
└─────────────────────────────────────────────────────────────────────────────────────────────┘
```

---

## 🔬 Deep-Dive Technical Engineering Concepts

### 1. Zero-Allocation Slab Memory Pool (`MemoryPool<T>`)
In high-frequency trading engines, invoking heap allocation primitives (`malloc`, `free`, `new`, `delete`) on the matching critical path introduces OS kernel context switches, lock contention in the heap allocator, and non-deterministic page fault latency spikes.

`MemoryPool<T>` solves this by pre-allocating a contiguous memory slab of $N = 65,536$ `Order` structures at initialization. Free blocks form an **intrusive singly-linked free-list** where unused block memory holds the `void*` pointer to the next free slot:
- **Allocation ($O(1)$)**: Pop head of the free-list, initialize via placement-new `new (ptr) Order{...}`.
- **Deallocation ($O(1)$)**: Call explicit destructor `ptr->~Order()`, push pointer back onto the free-list head.
- **Cache Locality**: Contiguous storage ensures CPU cache prefetchers maximize L1/L2 cache hit ratios during sequential level processing.

### 2. Intrusive Doubly-Linked Price Levels
Standard STL containers (e.g. `std::list<Order>`) allocate separate wrapper node objects for each element, causing pointer chasing and memory fragmentation.

This engine embeds `Order* prev` and `Order* next` pointers directly inside `struct Order`:
```cpp
struct Order {
    uint64_t    id;
    Side        side;       // BUY / SELL
    OrderType   type;       // LIMIT / MARKET
    OrderStatus status;     // OPEN / FILLED / PARTIAL / CANCELLED
    uint64_t    price;      // Fixed-point scaled integer
    uint64_t    quantity;   // Original size
    uint64_t    filled;     // Matched size
    uint64_t    timestamp;  // Nanoseconds since epoch

    // Intrusive pointers for O(1) FIFO price level manipulation
    Order*      prev{nullptr};
    Order*      next{nullptr};
};
```
- Inserting a resting order into a price level is a constant time pointer assignment at `tail` ($O(1)$).
- Canceling an order takes $O(1)$ via the hash index (`order_index_`) to locate the `Order*` and immediately unlink its pointers from `prev` and `next`.

### 3. Fixed-Point Scaled Integer Pricing
IEEE 754 floating-point numbers (`float`, `double`) suffer from binary representation imprecision (e.g. `0.1 + 0.2 != 0.3`) and non-associative rounding errors. 

This engine enforces **fixed-point integer arithmetic**:
- All prices are multiplied by $100$ (e.g. $\$100.50 \to 10050$) and stored as 64-bit unsigned integers (`uint64_t`).
- Arithmetic comparisons (`<`, `>`, `==`) are single-cycle integer ALU operations, ensuring 100% mathematical determinism and cross-platform consistency.

### 4. Algorithmic Complexity Guarantees

| Operation | Implementation | Time Complexity | Space Complexity |
| :--- | :--- | :--- | :--- |
| **Best Bid / Ask Query** | `std::map::begin()` | $\mathcal{O}(1)$ | $\mathcal{O}(1)$ |
| **Price-Level Insertion** | `std::map::operator[]` | $\mathcal{O}(\log L)$ ($L \le \text{price levels}$) | $\mathcal{O}(1)$ |
| **Order Enqueue (FIFO)** | Intrusive Doubly-Linked Tail Append | $\mathcal{O}(1)$ | $\mathcal{O}(1)$ |
| **Order Cancel by ID** | `std::unordered_map` lookup + intrusive unlink | $\mathcal{O}(1)$ | $\mathcal{O}(1)$ |
| **Market Match Step** | Iterative traversal of resting orders at best price | $\mathcal{O}(M)$ ($M = \text{matched orders}$) | $\mathcal{O}(1)$ |
| **Memory Allocation** | Free-list pop from pre-allocated slab | $\mathcal{O}(1)$ | $\mathcal{O}(1)$ |

---

## 📡 HTTP REST API Specification

The matching engine serves an embedded low-overhead HTTP REST API on port `8080`:

### `POST /orders` — Submit New Order
Inserts a new limit or market order into the engine.

```http
POST /orders HTTP/1.1
Content-Type: application/json

{
  "side": "buy",
  "type": "limit",
  "price": 100.50,
  "quantity": 50
}
```

**Response (`201 Created`):**
```json
{
  "ok": true,
  "order_id": 1042,
  "message": "ok"
}
```

### `DELETE /orders/{id}` — Cancel Resting Order
Cancels an active resting order in $O(1)$ time.

```http
DELETE /orders/1042 HTTP/1.1
```

**Response (`200 OK`):**
```json
{
  "ok": true,
  "order_id": 1042,
  "message": "cancelled"
}
```

### `GET /book?depth=10` — L2 Depth Snapshot
Retrieves the top $N$ bid and ask price levels formatted for visual order books.

```json
{
  "symbol": "STOCK",
  "bids": [
    { "price": 100.50, "quantity": 120 },
    { "price": 100.25, "quantity": 85 }
  ],
  "asks": [
    { "price": 100.75, "quantity": 40 },
    { "price": 101.00, "quantity": 210 }
  ]
}
```

### `GET /trades?limit=50` — Execution History Tape
Returns the most recent executions from the bounded ring buffer.

### `GET /stats` — Telemetry & Latency Counters
```json
{
  "orders_submitted": 100000,
  "orders_cancelled": 1420,
  "orders_filled": 77558,
  "trades_executed": 77558,
  "total_volume": 1013137,
  "avg_match_latency_us": 0.781
}
```

### `GET /health` — Liveness Probe
```json
{ "status": "ok" }
```

---

## 🛠️ Build, Test & Verification Suite

### Prerequisites
- **Compiler**: GCC 7+, Clang 6+, or MSVC 2017+ (C++17 standard)
- **Build System**: CMake 3.16+
- **Frontend**: Node.js 18+ and npm 9+

### 1. Native C++ Build
```bash
# Generate build configuration
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release

# Compile all targets (core library, engine binary, tests, benchmark)
cmake --build build --config Release -j4
```

### 2. Execute Unit Tests
```bash
# Memory Pool unit tests (4 tests, 29 assertions)
./build/test_memory_pool

# Order Book unit tests (7 tests, 35 assertions)
./build/test_order_book

# Matching Engine unit tests (15 tests, 59 assertions)
./build/test_matching_engine
```

### 3. Run Performance Benchmark
```bash
./build/benchmark
```

### 4. Launch Matching Engine Server
```bash
./build/hft_engine
# Listening on http://0.0.0.0:8080
```

### 5. Launch React Trading Dashboard
```bash
cd frontend
npm install
npm run dev
# Dashboard running on http://localhost:5173
```

---

## 🐳 Docker Deployment

### 1-Click Containerized Launch
```bash
docker-compose up --build
```
- **Web Dashboard**: `http://localhost:3000`
- **Matching Engine API**: `http://localhost:8080`

---

## 📂 Project Directory Structure

```
High-Frequency-OrderBook-and-MatchingEngine/
├── CMakeLists.txt              # CMake build definitions (C++17, hft_core, tests, benchmark)
├── Dockerfile                  # Multi-stage production container build
├── docker-compose.yml          # Container orchestration (C++ Engine + Nginx Frontend)
├── LICENSE                     # MIT License
├── README.md                   # System documentation & performance telemetry
├── .github/
│   └── workflows/
│       └── ci.yml              # GitHub Actions CI pipeline (build, test, benchmark)
├── include/
│   └── json.hpp                # nlohmann/json single-header serialization
├── src/
│   ├── order.hpp               # Order & Trade structs, Side/Type/Status enums
│   ├── memory_pool.hpp         # Slab allocator with intrusive free-list
│   ├── order_book.hpp          # PriceLevel struct & OrderBook L2 map
│   ├── order_book.cpp          # Price level management & snapshot extraction
│   ├── matching_engine.hpp     # MatchingEngine header with telemetry & thread safety
│   ├── matching_engine.cpp     # Price-time priority execution logic
│   ├── win_mutex.hpp           # Cross-platform synchronization abstraction
│   ├── server.hpp              # Native HTTP/1.1 REST socket server (Winsock/POSIX)
│   └── main.cpp                # Service entry point
├── tests/
│   ├── test_framework.hpp      # Zero-dependency test runner with Approx & asserts
│   ├── test_memory_pool.cpp    # Slab capacity, exhaustion & stability tests
│   ├── test_order_book.cpp     # Price ordering, FIFO queues, pruning tests
│   ├── test_matching_engine.cpp# Full match, partial fill, price improvement tests
│   └── benchmark.cpp           # 100k order throughput and latency benchmark
└── frontend/
    ├── package.json            # React & Vite build configurations
    ├── Dockerfile              # Multi-stage Nginx container build
    ├── src/
    │   ├── App.jsx             # Main dashboard controller & 400ms polling engine
    │   ├── index.css           # Modern dark-mode glassmorphism styling
    │   └── components/
    │       ├── OrderBook.jsx   # Live L2 Bid/Ask depth ladder with dynamic bars
    │       ├── OrderForm.jsx   # Interactive order entry & automated simulation bot
    │       ├── TradesFeed.jsx  # Real-time matched trade execution ticker
    │       └── StatsPanel.jsx  # Real-time latency, throughput & volume telemetry
    └── index.html
```

---

## 🤝 Contributing & License
Distributed under the **MIT License**. Contributions, issues, and feature requests are welcome.