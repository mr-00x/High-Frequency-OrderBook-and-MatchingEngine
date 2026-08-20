#pragma once

#include "order.hpp"
#include "order_book.hpp"
#include "memory_pool.hpp"
#include <atomic>
#include <mutex>
#include <vector>
#include <deque>
#include <chrono>
#include <cstdint>
#include <optional>
#include <string>

namespace hft {

// ---------------------------------------------------------------------------
// Stats — throughput & latency counters exposed on /stats
// ---------------------------------------------------------------------------

struct EngineStats {
    uint64_t orders_submitted{0};
    uint64_t orders_cancelled{0};
    uint64_t orders_filled{0};
    uint64_t trades_executed{0};
    uint64_t total_volume{0};      // cumulative matched quantity
    double   avg_match_latency_us{0.0}; // microseconds
};

// ---------------------------------------------------------------------------
// SubmitResult — returned to the HTTP layer after an order submission
// ---------------------------------------------------------------------------

struct SubmitResult {
    bool     ok{false};
    uint64_t order_id{0};
    std::string message;
};

// ---------------------------------------------------------------------------
// MatchingEngine
//
// Thread-safe matching engine for a single instrument.
//
// Match algorithm (price-time priority):
//   - For a new BUY  order: walk ask levels from lowest ask upward
//   - For a new SELL order: walk bid levels from highest bid downward
//   - Within a price level, match FIFO (oldest resting order first)
//   - After matching, any remaining quantity rests in the book
//
// All public methods acquire a std::mutex — simple, correct, and explainable.
// ---------------------------------------------------------------------------

class MatchingEngine {
public:
    static constexpr std::size_t POOL_CAPACITY    = 65536; // max live orders
    static constexpr std::size_t MAX_TRADE_HISTORY = 500;  // trade log size

    explicit MatchingEngine(std::string symbol = "STOCK");

    // Non-copyable
    MatchingEngine(const MatchingEngine&)            = delete;
    MatchingEngine& operator=(const MatchingEngine&) = delete;

    // Submit a new order. Returns the assigned order ID (or 0 on failure).
    SubmitResult submit(Side side, OrderType type, uint64_t price, uint64_t quantity);

    // Cancel a resting order by ID. Returns true if found and cancelled.
    bool cancel(uint64_t order_id);

    // Snapshot of the order book for the GUI.
    OrderBookSnapshot book_snapshot(std::size_t depth = 10);

    // Recent trades (up to MAX_TRADE_HISTORY entries, newest first).
    std::vector<Trade> recent_trades(std::size_t n = 50);

    // Current engine statistics.
    EngineStats stats();

    [[nodiscard]] const std::string& symbol() const noexcept { return symbol_; }

private:
    // Match a newly submitted order against the opposite side of the book.
    // Fills `order` in-place, records trades, and updates stats.
    void match(Order* order, std::vector<Trade>& new_trades);

    // Allocate an Order from the pool.
    Order* alloc_order(Side side, OrderType type, uint64_t price, uint64_t quantity);

    // Return an Order to the pool.
    void free_order(Order* order);

    // Generate a monotonically increasing nanosecond-resolution timestamp
    // (using a logical counter based on std::chrono for determinism in tests).
    uint64_t next_timestamp() noexcept;

    std::string             symbol_;
    std::mutex              mutex_;
    OrderBook               book_;
    MemoryPool<Order>       pool_;
    std::atomic<uint64_t>   next_order_id_{1};
    std::atomic<uint64_t>   next_trade_id_{1};
    std::deque<Trade>       trade_log_;    // bounded ring (MAX_TRADE_HISTORY)
    EngineStats             stats_;
};

} // namespace hft
