#include "order_book.hpp"
#include <stdexcept>
#include <algorithm>

namespace hft {

OrderBook::OrderBook(std::size_t max_orders) {
    order_index_.reserve(max_orders);
}

void OrderBook::insert(Order* order) {
    if (!order || !order->is_active()) return;

    order_index_[order->id] = order;

    if (order->side == Side::BUY) {
        auto& level = bid_levels_[order->price];
        level.price = order->price;
        level.push_back(order);
    } else {
        auto& level = ask_levels_[order->price];
        level.price = order->price;
        level.push_back(order);
    }
}

bool OrderBook::remove(Order* order) {
    if (!order) return false;

    auto it = order_index_.find(order->id);
    if (it == order_index_.end()) return false;

    order_index_.erase(it);

    if (order->side == Side::BUY) {
        auto level_it = bid_levels_.find(order->price);
        if (level_it != bid_levels_.end()) {
            level_it->second.remove(order);
            if (level_it->second.empty()) {
                bid_levels_.erase(level_it);
            }
        }
    } else {
        auto level_it = ask_levels_.find(order->price);
        if (level_it != ask_levels_.end()) {
            level_it->second.remove(order);
            if (level_it->second.empty()) {
                ask_levels_.erase(level_it);
            }
        }
    }

    return true;
}

Order* OrderBook::find(uint64_t order_id) const noexcept {
    auto it = order_index_.find(order_id);
    return (it != order_index_.end()) ? it->second : nullptr;
}

PriceLevel* OrderBook::best_bid() noexcept {
    if (bid_levels_.empty()) return nullptr;
    return &bid_levels_.begin()->second;
}

PriceLevel* OrderBook::best_ask() noexcept {
    if (ask_levels_.empty()) return nullptr;
    return &ask_levels_.begin()->second;
}

OrderBookSnapshot OrderBook::snapshot(std::size_t depth) const {
    OrderBookSnapshot snap;
    snap.bids.reserve(depth);
    snap.asks.reserve(depth);

    std::size_t count = 0;
    for (const auto& kv : bid_levels_) {
        if (count++ >= depth) break;
        snap.bids.push_back({kv.second.price / 100.0, kv.second.total_quantity});
    }

    count = 0;
    for (const auto& kv : ask_levels_) {
        if (count++ >= depth) break;
        snap.asks.push_back({kv.second.price / 100.0, kv.second.total_quantity});
    }

    return snap;
}

} // namespace hft
