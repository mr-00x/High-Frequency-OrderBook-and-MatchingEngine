// ---------------------------------------------------------------------------
// main.cpp
//
// Entry point for the HFT Order Book & Matching Engine server.
// Starts the matching engine and HTTP server on port 8080 (or $PORT env var).
// ---------------------------------------------------------------------------

#include "matching_engine.hpp"
#include "server.hpp"
#include <cstdlib>
#include <iostream>
#include <string>

int main() {
    // Read port from environment (needed for Railway / Render deployment)
    int port = 8080;
    if (const char* port_env = std::getenv("PORT")) {
        try {
            port = std::stoi(port_env);
        } catch (...) {
            std::cerr << "[warn] Invalid PORT env var, defaulting to 8080\n";
        }
    }

    std::cout << "=================================================\n";
    std::cout << "  HFT Limit Order Book & Matching Engine\n";
    std::cout << "  Listening on http://0.0.0.0:" << port << "\n";
    std::cout << "=================================================\n";

    hft::MatchingEngine engine("STOCK");
    
    // Pre-seed realistic market depth and initial trade history
    engine.seed_market(100.0, 10);
    std::cout << "[info] Preloaded realistic market liquidity (10 Bid & 10 Ask levels + baseline trades).\n";

    hft::Server server(engine, port);

    std::cout << "[info] Engine ready. Waiting for orders...\n";
    server.run();

    return 0;
}
