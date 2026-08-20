// test_matching_engine.cpp
//
// Integration tests for the MatchingEngine: full match cycles, partial fills,
// cancellation, market orders, stats, and edge cases.

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"
#include "matching_engine.hpp"

using namespace hft;

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

// Price as scaled integer (e.g. $100.50 → 10050)
static constexpr uint64_t px(double d) {
    return static_cast<uint64_t>(d * 100.0 + 0.5);
}

// ---------------------------------------------------------------------------

TEST_CASE("MatchingEngine: submit limit order with no counterpart rests in book") {
    MatchingEngine engine;

    auto result = engine.submit(Side::BUY, OrderType::LIMIT, px(100.0), 10);
    CHECK(result.ok);
    CHECK(result.order_id > 0);

    auto snap = engine.book_snapshot();
    REQUIRE(snap.bids.size() == 1);
    CHECK(snap.bids[0].price    == doctest::Approx(100.0));
    CHECK(snap.bids[0].quantity == 10);
    CHECK(snap.asks.empty());

    auto stats = engine.stats();
    CHECK(stats.orders_submitted == 1);
    CHECK(stats.trades_executed  == 0);
}

TEST_CASE("MatchingEngine: full match between buy and sell") {
    MatchingEngine engine;

    auto buy  = engine.submit(Side::BUY,  OrderType::LIMIT, px(100.0), 10);
    auto sell = engine.submit(Side::SELL, OrderType::LIMIT, px(100.0), 10);

    CHECK(buy.ok);
    CHECK(sell.ok);

    // Both sides fully filled — book should be empty
    auto snap = engine.book_snapshot();
    CHECK(snap.bids.empty());
    CHECK(snap.asks.empty());

    // One trade recorded
    auto trades = engine.recent_trades();
    REQUIRE(trades.size() == 1);
    CHECK(trades[0].price    == doctest::Approx(100.0));
    CHECK(trades[0].quantity == 10);

    auto stats = engine.stats();
    CHECK(stats.trades_executed == 1);
    CHECK(stats.total_volume    == 10);
}

TEST_CASE("MatchingEngine: partial fill — resting order quantity reduced") {
    MatchingEngine engine;

    // Resting buy: 20 shares at 100
    engine.submit(Side::BUY, OrderType::LIMIT, px(100.0), 20);

    // Aggressive sell: 5 shares at 100 — only partial of the resting buy
    engine.submit(Side::SELL, OrderType::LIMIT, px(100.0), 5);

    auto snap = engine.book_snapshot();
    REQUIRE(snap.bids.size() == 1);
    CHECK(snap.bids[0].quantity == 15); // 20 - 5
    CHECK(snap.asks.empty());

    auto trades = engine.recent_trades();
    REQUIRE(trades.size() == 1);
    CHECK(trades[0].quantity == 5);
}

TEST_CASE("MatchingEngine: price-time priority — multiple resting orders") {
    MatchingEngine engine;

    // Two resting buys at 100, FIFO
    auto b1 = engine.submit(Side::BUY, OrderType::LIMIT, px(100.0), 10);
    auto b2 = engine.submit(Side::BUY, OrderType::LIMIT, px(100.0), 10);

    // Sell 15 — should fill all of b1 and 5 of b2
    engine.submit(Side::SELL, OrderType::LIMIT, px(100.0), 15);

    auto snap = engine.book_snapshot();
    REQUIRE(snap.bids.size() == 1);
    CHECK(snap.bids[0].quantity == 5); // 10 - 5 remaining from b2

    auto trades = engine.recent_trades();
    CHECK(trades.size() == 2); // two fill events
}

TEST_CASE("MatchingEngine: no match when prices don't cross") {
    MatchingEngine engine;

    engine.submit(Side::BUY,  OrderType::LIMIT, px(99.0),  10);
    engine.submit(Side::SELL, OrderType::LIMIT, px(101.0), 10);

    // No trade — spread not crossed
    auto snap = engine.book_snapshot();
    CHECK(snap.bids.size() == 1);
    CHECK(snap.asks.size() == 1);
    CHECK(engine.recent_trades().empty());
}

TEST_CASE("MatchingEngine: price improvement — buy at higher price than resting ask") {
    MatchingEngine engine;

    // Resting sell at 100
    engine.submit(Side::SELL, OrderType::LIMIT, px(100.0), 10);

    // Buy limit at 101 — crosses spread, matches at resting price (100)
    engine.submit(Side::BUY, OrderType::LIMIT, px(101.0), 10);

    auto trades = engine.recent_trades();
    REQUIRE(trades.size() == 1);
    CHECK(trades[0].price == doctest::Approx(100.0)); // exec at resting price
    CHECK(trades[0].quantity == 10);

    auto snap = engine.book_snapshot();
    CHECK(snap.bids.empty());
    CHECK(snap.asks.empty());
}

TEST_CASE("MatchingEngine: cancel a resting order") {
    MatchingEngine engine;

    auto result = engine.submit(Side::BUY, OrderType::LIMIT, px(100.0), 10);
    REQUIRE(result.ok);

    bool cancelled = engine.cancel(result.order_id);
    CHECK(cancelled);

    auto snap = engine.book_snapshot();
    CHECK(snap.bids.empty());

    // Cancelling again should return false
    CHECK(engine.cancel(result.order_id) == false);

    auto stats = engine.stats();
    CHECK(stats.orders_cancelled == 1);
}

TEST_CASE("MatchingEngine: cancel non-existent order") {
    MatchingEngine engine;
    CHECK(engine.cancel(999999) == false);
}

TEST_CASE("MatchingEngine: market buy fills against resting asks") {
    MatchingEngine engine;

    // Resting asks at different prices
    engine.submit(Side::SELL, OrderType::LIMIT, px(100.0), 5);
    engine.submit(Side::SELL, OrderType::LIMIT, px(101.0), 5);

    // Market buy for 8 — should eat 5 @ 100 and 3 @ 101
    auto result = engine.submit(Side::BUY, OrderType::MARKET, 0, 8);
    CHECK(result.ok);

    auto snap = engine.book_snapshot();
    REQUIRE(snap.asks.size() == 1);
    CHECK(snap.asks[0].price    == doctest::Approx(101.0));
    CHECK(snap.asks[0].quantity == 2); // 5 - 3

    auto trades = engine.recent_trades();
    CHECK(trades.size() == 2);
}

TEST_CASE("MatchingEngine: market order with no liquidity is cancelled") {
    MatchingEngine engine;

    // Empty book — market buy should cancel unfilled remainder
    auto result = engine.submit(Side::BUY, OrderType::MARKET, 0, 100);
    CHECK(result.ok); // submit itself is valid

    // Nothing in book
    auto snap = engine.book_snapshot();
    CHECK(snap.bids.empty());
    CHECK(snap.asks.empty());
    CHECK(engine.recent_trades().empty());
}

TEST_CASE("MatchingEngine: reject order with quantity 0") {
    MatchingEngine engine;
    auto result = engine.submit(Side::BUY, OrderType::LIMIT, px(100.0), 0);
    CHECK(!result.ok);
}

TEST_CASE("MatchingEngine: reject limit order with price 0") {
    MatchingEngine engine;
    auto result = engine.submit(Side::BUY, OrderType::LIMIT, 0, 10);
    CHECK(!result.ok);
}

TEST_CASE("MatchingEngine: stats accumulate correctly") {
    MatchingEngine engine;

    engine.submit(Side::BUY,  OrderType::LIMIT, px(100.0), 10);
    engine.submit(Side::SELL, OrderType::LIMIT, px(100.0), 10);

    auto s = engine.stats();
    CHECK(s.orders_submitted >= 2);
    CHECK(s.trades_executed  == 1);
    CHECK(s.total_volume     == 10);
    CHECK(s.avg_match_latency_us >= 0.0);
}

TEST_CASE("MatchingEngine: order IDs are unique and increasing") {
    MatchingEngine engine;
    auto r1 = engine.submit(Side::BUY, OrderType::LIMIT, px(100.0), 1);
    auto r2 = engine.submit(Side::BUY, OrderType::LIMIT, px(100.0), 1);
    auto r3 = engine.submit(Side::BUY, OrderType::LIMIT, px(100.0), 1);

    CHECK(r1.order_id < r2.order_id);
    CHECK(r2.order_id < r3.order_id);
}

TEST_CASE("MatchingEngine: sweep multiple price levels") {
    MatchingEngine engine;

    // Three ask levels
    engine.submit(Side::SELL, OrderType::LIMIT, px(100.0), 5);
    engine.submit(Side::SELL, OrderType::LIMIT, px(101.0), 5);
    engine.submit(Side::SELL, OrderType::LIMIT, px(102.0), 5);

    // Limit buy at 102 — sweeps all three
    engine.submit(Side::BUY, OrderType::LIMIT, px(102.0), 15);

    auto snap = engine.book_snapshot();
    CHECK(snap.bids.empty());
    CHECK(snap.asks.empty());

    auto trades = engine.recent_trades();
    CHECK(trades.size() == 3);
}
