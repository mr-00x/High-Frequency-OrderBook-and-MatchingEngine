#pragma once

#include "order.hpp"
#include <map>
#include <unordered_map>
#include <vector>
#include <cstdint>
#include <functional>

namespace hft {

// ---------------------------------------------------------------------------
// PriceLevel
//
// Holds all resting orders at a single price, in FIFO (time-priority) order,
// using an intrusive doubly-linked list of Order pointers.
// Insertion and removal are O(1).
// ---------------------------------------------------------------------------

struct PriceLevel {
    uint64_t price;
    uint64_t total_quantity{0};  // sum of remaining quantities at this level
    Order*   head{nullptr};       // oldest order (matched first)
    Order*   tail{nullptr};       // newest order (matched last)

    void push_back(Order* order) {
        order->prev = tail;
        order->next = nullptr;
        if (tail) {
            tail->next = order;
        } else {
            head = order;
        }
        tail = order;
        total_quantity += order->remaining();
    }

    void remove(Order* order) {
        total_quantity -= order->remaining();
        if (order->prev) order->prev->next = order->next;
        else             head = order->next;
        if (order->next) order->next->prev = order->prev;
        else             tail = order->prev;
        order->prev = order->next = nullptr;
    }

    [[nodiscard]] bool empty() const noexcept { return head == nullptr; }
};

// ---------------------------------------------------------------------------
// OrderBookSnapshot — a lightweight view of top N levels for the GUI
// ---------------------------------------------------------------------------

struct LevelSnapshot {
    double   price;
    uint64_t quantity;
};

struct OrderBookSnapshot {
    std::vector<LevelSnapshot> bids; // sorted best bid first (highest)
    std::vector<LevelSnapshot> asks; // sorted best ask first (lowest)
};

// ---------------------------------------------------------------------------
// OrderBook
//
// Maintains two sorted maps of price levels (bids and asks) and an
// unordered_map for O(1) order lookup/cancel.
//
// Bid map: std::map with std::greater comparator → highest price at begin()
// Ask map: std::map with default comparator  → lowest price at begin()
// ---------------------------------------------------------------------------

class OrderBook {
public:
    explicit OrderBook(std::size_t max_orders = 65536);

    // Insert a resting order into the book.
    // Caller must have already matched what they could.
    void insert(Order* order);

    // Remove an order from the book (cancel or after fill).
    // Returns false if the order is not found.
    bool remove(Order* order);

    // Look up a resting order by ID. Returns nullptr if not found.
    [[nodiscard]] Order* find(uint64_t order_id) const noexcept;

    // Returns the best bid price level (highest bid), or nullptr if empty.
    [[nodiscard]] PriceLevel* best_bid() noexcept;

    // Returns the best ask price level (lowest ask), or nullptr if empty.
    [[nodiscard]] PriceLevel* best_ask() noexcept;

    // Build a snapshot of the top `depth` levels on each side.
    [[nodiscard]] OrderBookSnapshot snapshot(std::size_t depth = 10) const;

    // Total resting orders on each side
    [[nodiscard]] std::size_t bid_count() const noexcept { return bid_levels_.size(); }
    [[nodiscard]] std::size_t ask_count() const noexcept { return ask_levels_.size(); }

private:
    // Bid side: highest price first
    std::map<uint64_t, PriceLevel, std::greater<uint64_t>> bid_levels_;

    // Ask side: lowest price first
    std::map<uint64_t, PriceLevel>                          ask_levels_;

    // Order ID → pointer lookup (for O(1) cancel)
    std::unordered_map<uint64_t, Order*> order_index_;
};

} // namespace hft
