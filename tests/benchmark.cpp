// benchmark.cpp
//
// High-throughput performance benchmark for the MatchingEngine.
// Simulates 100,000+ order operations to measure ops/sec and latency.

#include "matching_engine.hpp"
#include <iostream>
#include <vector>
#include <chrono>
#include <random>
#include <iomanip>

using namespace hft;

int main() {
    std::cout << "=================================================\n";
    std::cout << " Running Matching Engine Benchmark (100,000 Orders)\n";
    std::cout << "=================================================\n";

    MatchingEngine engine("BENCH");

    constexpr int NUM_ORDERS = 100000;
    std::mt19937_64 rng(42);
    std::uniform_int_distribution<uint64_t> side_dist(0, 1);
    std::uniform_int_distribution<uint64_t> price_dist(9500, 10500); // $95.00 to $105.00
    std::uniform_int_distribution<uint64_t> qty_dist(1, 50);

    // Pre-generate order parameters to avoid timing the random generator
    struct OrderParam {
        Side side;
        OrderType type;
        uint64_t price;
        uint64_t qty;
    };

    std::vector<OrderParam> params;
    params.reserve(NUM_ORDERS);
    for (int i = 0; i < NUM_ORDERS; ++i) {
        Side side = (side_dist(rng) == 0) ? Side::BUY : Side::SELL;
        uint64_t price = price_dist(rng);
        uint64_t qty = qty_dist(rng);
        params.push_back({side, OrderType::LIMIT, price, qty});
    }

    // Warm up
    for (int i = 0; i < 1000; ++i) {
        engine.submit(params[i].side, params[i].type, params[i].price, params[i].qty);
    }

    // Benchmark Run
    auto start_time = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < NUM_ORDERS; ++i) {
        engine.submit(params[i].side, params[i].type, params[i].price, params[i].qty);
    }

    auto end_time = std::chrono::high_resolution_clock::now();
    double elapsed_sec = std::chrono::duration<double>(end_time - start_time).count();
    double ops_per_sec = static_cast<double>(NUM_ORDERS) / elapsed_sec;

    auto stats = engine.stats();

    std::cout << std::fixed << std::setprecision(2);
    std::cout << " Orders Processed:     " << NUM_ORDERS << "\n";
    std::cout << " Total Time:           " << elapsed_sec * 1000.0 << " ms\n";
    std::cout << " Throughput:           " << ops_per_sec << " ops/sec (" 
              << ops_per_sec / 1000.0 << "k ops/sec)\n";
    std::cout << " Average Match Latency:" << stats.avg_match_latency_us << " us (" 
              << stats.avg_match_latency_us * 1000.0 << " ns)\n";
    std::cout << " Trades Executed:      " << stats.trades_executed << "\n";
    std::cout << " Total Volume Matched: " << stats.total_volume << "\n";
    std::cout << "=================================================\n";

    if (ops_per_sec >= 100000.0) {
        std::cout << " [SUCCESS] Benchmark goal (>100k ops/sec) MET!\n";
    } else {
        std::cout << " [INFO] Benchmark completed.\n";
    }
    std::cout << "=================================================\n";

    return 0;
}
