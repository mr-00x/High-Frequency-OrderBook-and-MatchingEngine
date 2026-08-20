#pragma once

#include <cstdint>
#include <string>

namespace hft {

// ---------------------------------------------------------------------------
// Enumerations
// ---------------------------------------------------------------------------

enum class Side : uint8_t {
    BUY  = 0,
    SELL = 1
};

enum class OrderStatus : uint8_t {
    OPEN        = 0,
    FILLED      = 1,
    PARTIALLY_FILLED = 2,
    CANCELLED   = 3
};

enum class OrderType : uint8_t {
    LIMIT  = 0,
    MARKET = 1
};

// ---------------------------------------------------------------------------
// Order
//
// Price is stored as a scaled integer: multiply by 100 to avoid floating-point
// precision issues. e.g. $100.50 → price = 10050.
// Quantity represents number of shares / contracts.
// ---------------------------------------------------------------------------

struct Order {
    uint64_t    id;
    Side        side;
    OrderType   type;
    OrderStatus status;
    uint64_t    price;       // scaled by 100 (0 for market orders)
    uint64_t    quantity;    // total quantity
    uint64_t    filled;      // quantity already matched
    uint64_t    timestamp;   // nanoseconds since epoch (logical clock)

    // Intrusive doubly-linked list pointers for O(1) removal within a price level
    Order*      prev{nullptr};
    Order*      next{nullptr};

    // ---------------------------------------------------------------------------
    // Convenience
    // ---------------------------------------------------------------------------

    [[nodiscard]] uint64_t remaining() const noexcept {
        return quantity - filled;
    }

    [[nodiscard]] bool is_active() const noexcept {
        return status == OrderStatus::OPEN || status == OrderStatus::PARTIALLY_FILLED;
    }

    [[nodiscard]] double price_as_double() const noexcept {
        return static_cast<double>(price) / 100.0;
    }
};

// ---------------------------------------------------------------------------
// Trade — produced when two orders match
// ---------------------------------------------------------------------------

struct Trade {
    uint64_t trade_id;
    uint64_t buy_order_id;
    uint64_t sell_order_id;
    uint64_t price;        // execution price (scaled by 100)
    uint64_t quantity;     // matched quantity
    uint64_t timestamp;

    [[nodiscard]] double price_as_double() const noexcept {
        return static_cast<double>(price) / 100.0;
    }
};

} // namespace hft
