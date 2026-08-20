#pragma once
// ---------------------------------------------------------------------------
// server.hpp — Minimal HTTP/1.1 REST server over Winsock2
//
// Why Winsock2 directly instead of cpp-httplib?
//   MinGW.org GCC 6.3 does not ship a working <thread> / <mutex>
//   implementation. Rather than pulling in a heavyweight dependency, this
//   server is implemented directly on top of Windows Sockets, which are
//   always available on Windows. On deployment (Linux Docker), the
//   Dockerfile uses a modern GCC with cpp-httplib, so this file is only
//   compiled locally on Windows for development / testing.
//
// Design:
//   - Single-threaded, synchronous request/response loop (one client at a time)
//   - Parses the request line + headers, reads an optional body, dispatches
//     to a route handler, and writes the response back.
//   - Routes are registered as std::function<std::string(Request&)> lambdas.
//   - Response body is always JSON; Content-Type header is fixed.
//
// Endpoints:
//   POST   /orders          — submit a new order
//   DELETE /orders/{id}     — cancel a resting order
//   GET    /book            — order book snapshot
//   GET    /trades          — recent trade history
//   GET    /stats           — engine performance stats
//   GET    /health          — liveness probe
// ---------------------------------------------------------------------------

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <ws2tcpip.h>

#include "../include/json.hpp"
#include "matching_engine.hpp"

#include <string>
#include <sstream>
#include <functional>
#include <map>
#include <vector>
#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstring>

#pragma comment(lib, "ws2_32.lib")

namespace hft {

using json = nlohmann::json;

// ---------------------------------------------------------------------------
// Request — parsed HTTP request
// ---------------------------------------------------------------------------

struct Request {
    std::string method;
    std::string path;
    std::string body;
    std::map<std::string, std::string> headers;

    // Extract a named path segment (e.g. for /orders/42, segment(1) == "42")
    std::string segment(std::size_t idx) const {
        std::vector<std::string> parts;
        std::istringstream ss(path);
        std::string tok;
        while (std::getline(ss, tok, '/')) {
            if (!tok.empty()) parts.push_back(tok);
        }
        return (idx < parts.size()) ? parts[idx] : "";
    }

    // Query string extraction (e.g. /book?depth=5)
    std::string query_param(const std::string& key) const {
        auto pos = path.find('?');
        if (pos == std::string::npos) return "";
        std::string qs = path.substr(pos + 1);
        std::istringstream ss(qs);
        std::string pair;
        while (std::getline(ss, pair, '&')) {
            auto eq = pair.find('=');
            if (eq != std::string::npos && pair.substr(0, eq) == key) {
                return pair.substr(eq + 1);
            }
        }
        return "";
    }

    std::string clean_path() const {
        auto pos = path.find('?');
        return (pos == std::string::npos) ? path : path.substr(0, pos);
    }
};

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static std::string http_response(int status, const std::string& body,
                                  const std::string& content_type = "application/json") {
    const char* status_str = "200 OK";
    if      (status == 201) status_str = "201 Created";
    else if (status == 204) status_str = "204 No Content";
    else if (status == 400) status_str = "400 Bad Request";
    else if (status == 404) status_str = "404 Not Found";

    std::ostringstream ss;
    ss << "HTTP/1.1 " << status_str << "\r\n"
       << "Content-Type: " << content_type << "\r\n"
       << "Content-Length: " << body.size() << "\r\n"
       << "Access-Control-Allow-Origin: *\r\n"
       << "Access-Control-Allow-Methods: GET, POST, DELETE, OPTIONS\r\n"
       << "Access-Control-Allow-Headers: Content-Type\r\n"
       << "Connection: close\r\n"
       << "\r\n"
       << body;
    return ss.str();
}

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
    explicit Server(MatchingEngine& engine, int port = 8080)
        : engine_(engine)
        , port_(port)
        , listen_sock_(INVALID_SOCKET)
    {}

    ~Server() {
        if (listen_sock_ != INVALID_SOCKET) {
            closesocket(listen_sock_);
        }
        WSACleanup();
    }

    void run() {
        WSADATA wsa;
        if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
            fprintf(stderr, "[error] WSAStartup failed\n");
            return;
        }

        listen_sock_ = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (listen_sock_ == INVALID_SOCKET) {
            fprintf(stderr, "[error] socket() failed\n");
            WSACleanup();
            return;
        }

        // Allow immediate port reuse after restart
        int yes = 1;
        setsockopt(listen_sock_, SOL_SOCKET, SO_REUSEADDR,
                   reinterpret_cast<char*>(&yes), sizeof(yes));

        sockaddr_in addr{};
        addr.sin_family      = AF_INET;
        addr.sin_addr.s_addr = INADDR_ANY;
        addr.sin_port        = htons(static_cast<u_short>(port_));

        if (bind(listen_sock_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == SOCKET_ERROR) {
            fprintf(stderr, "[error] bind() failed on port %d\n", port_);
            closesocket(listen_sock_);
            WSACleanup();
            return;
        }

        listen(listen_sock_, SOMAXCONN);
        fprintf(stdout, "[info] Listening on port %d\n", port_);

        while (true) {
            SOCKET client = accept(listen_sock_, nullptr, nullptr);
            if (client == INVALID_SOCKET) continue;
            handle_client(client);
            closesocket(client);
        }
    }

    void stop() {
        if (listen_sock_ != INVALID_SOCKET) {
            closesocket(listen_sock_);
            listen_sock_ = INVALID_SOCKET;
        }
    }

private:
    // ------------------------------------------------------------------
    // Read a complete HTTP request from the socket
    // ------------------------------------------------------------------
    bool read_request(SOCKET sock, Request& req) {
        // Read raw bytes into a string, stopping at end of headers
        std::string raw;
        char buf[4096];
        int  received;

        // Read until we have the full header block (\r\n\r\n)
        while (true) {
            received = recv(sock, buf, sizeof(buf) - 1, 0);
            if (received <= 0) return false;
            buf[received] = '\0';
            raw += buf;
            if (raw.find("\r\n\r\n") != std::string::npos) break;
            if (raw.size() > 65536) return false; // guard
        }

        // Split into header block and partial body
        auto header_end = raw.find("\r\n\r\n");
        std::string header_block = raw.substr(0, header_end);
        std::string body_so_far  = raw.substr(header_end + 4);

        // Parse request line
        std::istringstream ss(header_block);
        std::string request_line;
        std::getline(ss, request_line);
        if (!request_line.empty() && request_line.back() == '\r')
            request_line.pop_back();

        std::istringstream rl(request_line);
        rl >> req.method >> req.path;

        // Parse headers
        std::string line;
        std::size_t content_length = 0;
        while (std::getline(ss, line)) {
            if (!line.empty() && line.back() == '\r') line.pop_back();
            if (line.empty()) break;
            auto colon = line.find(':');
            if (colon != std::string::npos) {
                std::string key = line.substr(0, colon);
                std::string val = line.substr(colon + 1);
                // ltrim val
                val.erase(val.begin(),
                          std::find_if(val.begin(), val.end(),
                                       [](int c){ return !std::isspace(c); }));
                // lowercase key for lookup
                std::string lk = key;
                std::transform(lk.begin(), lk.end(), lk.begin(), ::tolower);
                req.headers[lk] = val;
                if (lk == "content-length") {
                    content_length = static_cast<std::size_t>(std::stoul(val));
                }
            }
        }

        // Read remaining body bytes
        req.body = body_so_far;
        while (req.body.size() < content_length) {
            std::size_t need = content_length - req.body.size();
            std::size_t chunk = need < sizeof(buf) ? need : sizeof(buf) - 1;
            received = recv(sock, buf, static_cast<int>(chunk), 0);
            if (received <= 0) break;
            buf[received] = '\0';
            req.body += buf;
        }

        return true;
    }

    // ------------------------------------------------------------------
    // Dispatch and write response
    // ------------------------------------------------------------------
    void handle_client(SOCKET sock) {
        Request req;
        if (!read_request(sock, req)) return;

        std::string response;

        // OPTIONS preflight
        if (req.method == "OPTIONS") {
            response = http_response(204, "");
        }
        // GET /health
        else if (req.method == "GET" && req.clean_path() == "/health") {
            response = http_response(200, R"({"status":"ok"})");
        }
        // POST /orders
        else if (req.method == "POST" && req.clean_path() == "/orders") {
            response = handle_post_order(req);
        }
        // DELETE /orders/{id}
        else if (req.method == "DELETE" &&
                 req.clean_path().substr(0, 8) == "/orders/") {
            response = handle_delete_order(req);
        }
        // GET /book
        else if (req.method == "GET" && req.clean_path() == "/book") {
            response = handle_get_book(req);
        }
        // GET /trades
        else if (req.method == "GET" && req.clean_path() == "/trades") {
            response = handle_get_trades(req);
        }
        // GET /stats
        else if (req.method == "GET" && req.clean_path() == "/stats") {
            response = handle_get_stats(req);
        }
        else {
            response = http_response(404, R"({"ok":false,"message":"not found"})");
        }

        send(sock, response.c_str(), static_cast<int>(response.size()), 0);
    }

    // ------------------------------------------------------------------
    // Route handlers
    // ------------------------------------------------------------------

    std::string handle_post_order(const Request& req) {
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

            return http_response(result.ok ? 201 : 400, resp.dump());
        } catch (const std::exception& e) {
            json err;
            err["ok"]      = false;
            err["message"] = e.what();
            return http_response(400, err.dump());
        }
    }

    std::string handle_delete_order(const Request& req) {
        try {
            std::string id_str = req.segment(1);
            if (id_str.empty()) {
                return http_response(400, R"({"ok":false,"message":"missing order id"})");
            }
            uint64_t id = std::stoull(id_str);
            bool ok = engine_.cancel(id);

            json resp;
            resp["ok"]       = ok;
            resp["order_id"] = id;
            resp["message"]  = ok ? "cancelled" : "order not found";

            return http_response(ok ? 200 : 404, resp.dump());
        } catch (const std::exception& e) {
            json err;
            err["ok"]      = false;
            err["message"] = e.what();
            return http_response(400, err.dump());
        }
    }

    std::string handle_get_book(const Request& req) {
        std::size_t depth = 10;
        std::string d = req.query_param("depth");
        if (!d.empty()) {
            try { depth = std::stoul(d); } catch (...) {}
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

        return http_response(200, resp.dump());
    }

    std::string handle_get_trades(const Request& req) {
        std::size_t limit = 50;
        std::string l = req.query_param("limit");
        if (!l.empty()) {
            try { limit = std::stoul(l); } catch (...) {}
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
        return http_response(200, resp.dump());
    }

    std::string handle_get_stats(const Request& /*req*/) {
        auto s = engine_.stats();

        json resp;
        resp["orders_submitted"]     = s.orders_submitted;
        resp["orders_cancelled"]     = s.orders_cancelled;
        resp["orders_filled"]        = s.orders_filled;
        resp["trades_executed"]      = s.trades_executed;
        resp["total_volume"]         = s.total_volume;
        resp["avg_match_latency_us"] = s.avg_match_latency_us;

        return http_response(200, resp.dump());
    }

    MatchingEngine& engine_;
    int             port_;
    SOCKET          listen_sock_;
};

} // namespace hft
