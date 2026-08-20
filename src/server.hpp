#pragma once

// ---------------------------------------------------------------------------
// server.hpp — HTTP REST API layer
//
// Wraps cpp-httplib to expose the matching engine over HTTP.
// All JSON is produced/consumed via nlohmann/json.
//
// Endpoints:
//   POST   /orders          — submit a new order
//   DELETE /orders/{id}     — cancel a resting order
//   GET    /book            — order book snapshot
//   GET    /trades          — recent trade history
//   GET    /stats           — engine performance stats
//   GET    /health          — liveness probe
// ---------------------------------------------------------------------------

// httplib must be included before windows.h on MSVC; fine for MinGW too.
#include "../include/httplib.h"
#include "../include/json.hpp"
#include "matching_engine.hpp"
#include <string>
#include <functional>

namespace hft {

using json = nlohmann::json;

// ---------------------------------------------------------------------------
// Helper: add CORS headers to every response so the React dev server can talk
// to us during local development (and Vercel → Railway in production).
// ---------------------------------------------------------------------------

static void add_cors(httplib::Response& res) {
    res.set_header("Access-Control-Allow-Origin",  "*");
    res.set_header("Access-Control-Allow-Methods", "GET, POST, DELETE, OPTIONS");
    res.set_header("Access-Control-Allow-Headers", "Content-Type");
}

// ---------------------------------------------------------------------------
// parse_price_string — convert "100.50" → 10050 (uint64_t, scaled ×100)
// Returns 0 on failure.
// ---------------------------------------------------------------------------

static uint64_t parse_price(const json& j, const std::string& key) {
    if (j.contains(key)) {
        if (j[key].is_number()) {
            double v = j[key].get<double>();
            return static_cast<uint64_t>(v * 100.0 + 0.5);
        }
        if (j[key].is_string()) {
            double v = std::stod(j[key].get<std::string>());
            return static_cast<uint64_t>(v * 100.0 + 0.5);
        }
    }
    return 0;
}

// ---------------------------------------------------------------------------
// Server
// ---------------------------------------------------------------------------

class Server {
public:
    Server(MatchingEngine& engine, int port = 8080)
        : engine_(engine)
        , port_(port)
    {
        register_routes();
    }

    // Block until the server is stopped.
    void run() {
        svr_.listen("0.0.0.0", port_);
    }

    void stop() {
        svr_.stop();
    }

private:
    void register_routes() {
        // ------------------------------------------------------------------
        // OPTIONS — pre-flight CORS
        // ------------------------------------------------------------------
        svr_.Options(".*", [](const httplib::Request&, httplib::Response& res) {
            add_cors(res);
            res.status = 204;
        });

        // ------------------------------------------------------------------
        // GET /health
        // ------------------------------------------------------------------
        svr_.Get("/health", [](const httplib::Request&, httplib::Response& res) {
            add_cors(res);
            res.set_content(R"({"status":"ok"})", "application/json");
        });

        // ------------------------------------------------------------------
        // POST /orders
        // Body: { "side": "buy"|"sell",
        //         "type": "limit"|"market",
        //         "price": 100.50,     (omit for market orders)
        //         "quantity": 10 }
        // ------------------------------------------------------------------
        svr_.Post("/orders", [this](const httplib::Request& req,
                                    httplib::Response& res) {
            add_cors(res);
            try {
                auto body = json::parse(req.body);

                std::string side_str = body.value("side", "buy");
                std::string type_str = body.value("type", "limit");

                Side side = (side_str == "sell") ? Side::SELL : Side::BUY;
                OrderType otype = (type_str == "market")
                                  ? OrderType::MARKET : OrderType::LIMIT;

                uint64_t price    = parse_price(body, "price");
                uint64_t quantity = body.value("quantity", uint64_t{0});

                auto result = engine_.submit(side, otype, price, quantity);

                json resp;
                resp["ok"]       = result.ok;
                resp["order_id"] = result.order_id;
                resp["message"]  = result.message;

                res.status = result.ok ? 201 : 400;
                res.set_content(resp.dump(), "application/json");
            } catch (const std::exception& e) {
                json err;
                err["ok"]      = false;
                err["message"] = e.what();
                res.status = 400;
                res.set_content(err.dump(), "application/json");
            }
        });

        // ------------------------------------------------------------------
        // DELETE /orders/{id}
        // ------------------------------------------------------------------
        svr_.Delete(R"(/orders/(\d+))",
            [this](const httplib::Request& req, httplib::Response& res) {
                add_cors(res);
                try {
                    uint64_t id = std::stoull(req.matches[1].str());
                    bool ok = engine_.cancel(id);

                    json resp;
                    resp["ok"]       = ok;
                    resp["order_id"] = id;
                    resp["message"]  = ok ? "cancelled" : "order not found";

                    res.status = ok ? 200 : 404;
                    res.set_content(resp.dump(), "application/json");
                } catch (const std::exception& e) {
                    json err;
                    err["ok"]      = false;
                    err["message"] = e.what();
                    res.status = 400;
                    res.set_content(err.dump(), "application/json");
                }
            });

        // ------------------------------------------------------------------
        // GET /book?depth=10
        // ------------------------------------------------------------------
        svr_.Get("/book", [this](const httplib::Request& req,
                                  httplib::Response& res) {
            add_cors(res);
            std::size_t depth = 10;
            if (req.has_param("depth")) {
                try { depth = std::stoul(req.get_param_value("depth")); }
                catch (...) {}
            }

            auto snap = engine_.book_snapshot(depth);

            json resp;
            resp["symbol"] = engine_.symbol();

            json bids = json::array();
            for (const auto& lvl : snap.bids) {
                json l;
                l["price"]    = lvl.price;
                l["quantity"] = lvl.quantity;
                bids.push_back(l);
            }
            json asks = json::array();
            for (const auto& lvl : snap.asks) {
                json l;
                l["price"]    = lvl.price;
                l["quantity"] = lvl.quantity;
                asks.push_back(l);
            }
            resp["bids"] = bids;
            resp["asks"] = asks;

            res.set_content(resp.dump(), "application/json");
        });

        // ------------------------------------------------------------------
        // GET /trades?limit=50
        // ------------------------------------------------------------------
        svr_.Get("/trades", [this](const httplib::Request& req,
                                    httplib::Response& res) {
            add_cors(res);
            std::size_t limit = 50;
            if (req.has_param("limit")) {
                try { limit = std::stoul(req.get_param_value("limit")); }
                catch (...) {}
            }

            auto trades = engine_.recent_trades(limit);

            json arr = json::array();
            for (const auto& t : trades) {
                json j;
                j["trade_id"]      = t.trade_id;
                j["buy_order_id"]  = t.buy_order_id;
                j["sell_order_id"] = t.sell_order_id;
                j["price"]         = t.price_as_double();
                j["quantity"]      = t.quantity;
                j["timestamp"]     = t.timestamp;
                arr.push_back(j);
            }
            json resp;
            resp["trades"] = arr;
            res.set_content(resp.dump(), "application/json");
        });

        // ------------------------------------------------------------------
        // GET /stats
        // ------------------------------------------------------------------
        svr_.Get("/stats", [this](const httplib::Request&,
                                   httplib::Response& res) {
            add_cors(res);
            auto s = engine_.stats();

            json resp;
            resp["orders_submitted"]     = s.orders_submitted;
            resp["orders_cancelled"]     = s.orders_cancelled;
            resp["orders_filled"]        = s.orders_filled;
            resp["trades_executed"]      = s.trades_executed;
            resp["total_volume"]         = s.total_volume;
            resp["avg_match_latency_us"] = s.avg_match_latency_us;

            res.set_content(resp.dump(), "application/json");
        });
    }

    MatchingEngine& engine_;
    int             port_;
    httplib::Server svr_;
};

} // namespace hft
