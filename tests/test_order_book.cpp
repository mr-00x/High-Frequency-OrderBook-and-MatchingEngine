// test_order_book.cpp
//
// Unit tests for OrderBook: insert, remove, find, snapshot, price ordering.

#define TEST_MAIN
#include "test_framework.hpp"
#include "order_book.hpp"
#include "order.hpp"
#include <memory>
#include <vector>

using namespace hft;

// Helper: build a heap-allocated Order for testing (bypasses pool)
static Order* make_order(uint64_t id, Side side, uint64_t price, uint64_t qty) {
    auto* o = new Order{};
    o->id       = id;
    o->side     = side;
    o->price    = price;
    o->quantity = qty;
    o->filled   = 0;
    o->status   = OrderStatus::OPEN;
    o->type     = OrderType::LIMIT;
    return o;
}

// Cleanup helper so tests don't leak
struct OrderGuard {
    std::vector<Order*> orders;
    ~OrderGuard() { for (auto* o : orders) delete o; }
    Order* add(Order* o) { orders.push_back(o); return o; }
};

// ---------------------------------------------------------------------------

TEST_CASE("OrderBook: insert and find") {
    OrderGuard g;
    OrderBook book;

    Order* b1 = g.add(make_order(1, Side::BUY,  10000, 100));
    Order* s1 = g.add(make_order(2, Side::SELL, 10100, 50));

    book.insert(b1);
    book.insert(s1);

    CHECK(book.find(1) == b1);
    CHECK(book.find(2) == s1);
    CHECK(book.find(99) == nullptr);
    CHECK(book.bid_count() == 1);
    CHECK(book.ask_count() == 1);
}

TEST_CASE("OrderBook: best bid is highest bid, best ask is lowest ask") {
    OrderGuard g;
    OrderBook book;

    // Insert bids at multiple prices
    Order* b1 = g.add(make_order(1, Side::BUY, 9900, 10));
    Order* b2 = g.add(make_order(2, Side::BUY, 10000, 20));  // best bid
    Order* b3 = g.add(make_order(3, Side::BUY, 9800, 5));

    // Insert asks at multiple prices
    Order* s1 = g.add(make_order(4, Side::SELL, 10100, 15));  // best ask
    Order* s2 = g.add(make_order(5, Side::SELL, 10200, 30));

    book.insert(b1); book.insert(b2); book.insert(b3);
    book.insert(s1); book.insert(s2);

    REQUIRE(book.best_bid() != nullptr);
    REQUIRE(book.best_ask() != nullptr);
    CHECK(book.best_bid()->price == 10000);
    CHECK(book.best_ask()->price == 10100);
}

TEST_CASE("OrderBook: remove cleans up empty price levels") {
    OrderGuard g;
    OrderBook book;

    Order* b1 = g.add(make_order(1, Side::BUY, 10000, 100));
    book.insert(b1);
    CHECK(book.bid_count() == 1);

    bool removed = book.remove(b1);
    CHECK(removed);
    CHECK(book.bid_count() == 0);         // level pruned
    CHECK(book.best_bid() == nullptr);
    CHECK(book.find(1) == nullptr);
}

TEST_CASE("OrderBook: remove returns false for unknown order") {
    OrderGuard g;
    OrderBook book;

    Order* b1 = g.add(make_order(42, Side::BUY, 10000, 100));
    // NOT inserted
    CHECK(book.remove(b1) == false);
}

TEST_CASE("OrderBook: FIFO order preserved within a price level") {
    OrderGuard g;
    OrderBook book;

    // Three buy orders at the same price — should queue FIFO
    Order* b1 = g.add(make_order(1, Side::BUY, 10000, 10));
    Order* b2 = g.add(make_order(2, Side::BUY, 10000, 20));
    Order* b3 = g.add(make_order(3, Side::BUY, 10000, 30));

    book.insert(b1); book.insert(b2); book.insert(b3);

    PriceLevel* level = book.best_bid();
    REQUIRE(level != nullptr);
    CHECK(level->total_quantity == 60);

    // Walk the linked list — must be in insertion order
    Order* cur = level->head;
    REQUIRE(cur != nullptr); CHECK(cur->id == 1);
    cur = cur->next;
    REQUIRE(cur != nullptr); CHECK(cur->id == 2);
    cur = cur->next;
    REQUIRE(cur != nullptr); CHECK(cur->id == 3);
    CHECK(cur->next == nullptr);
}

TEST_CASE("OrderBook: snapshot returns correct depth and ordering") {
    OrderGuard g;
    OrderBook book;

    // Bids: 100.00, 99.50, 99.00
    book.insert(g.add(make_order(1, Side::BUY, 10000, 5)));
    book.insert(g.add(make_order(2, Side::BUY,  9950, 8)));
    book.insert(g.add(make_order(3, Side::BUY,  9900, 3)));

    // Asks: 100.50, 101.00, 101.50
    book.insert(g.add(make_order(4, Side::SELL, 10050, 4)));
    book.insert(g.add(make_order(5, Side::SELL, 10100, 6)));
    book.insert(g.add(make_order(6, Side::SELL, 10150, 2)));

    auto snap = book.snapshot(2); // request top 2 only

    REQUIRE(snap.bids.size() == 2);
    REQUIRE(snap.asks.size() == 2);

    // Bids descending
    CHECK(snap.bids[0].price == doctest::Approx(100.00));
    CHECK(snap.bids[1].price == doctest::Approx(99.50));

    // Asks ascending
    CHECK(snap.asks[0].price == doctest::Approx(100.50));
    CHECK(snap.asks[1].price == doctest::Approx(101.00));
}

TEST_CASE("OrderBook: multiple orders per level — partial removal") {
    OrderGuard g;
    OrderBook book;

    Order* b1 = g.add(make_order(1, Side::BUY, 10000, 10));
    Order* b2 = g.add(make_order(2, Side::BUY, 10000, 20));
    book.insert(b1); book.insert(b2);

    // Remove b1; level should still exist with b2
    book.remove(b1);
    REQUIRE(book.bid_count() == 1); // level still exists
    PriceLevel* level = book.best_bid();
    REQUIRE(level != nullptr);
    CHECK(level->head == b2);
    CHECK(level->tail == b2);
    CHECK(level->total_quantity == 20);
}
