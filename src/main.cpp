#include <atomic>
#include <algorithm>
#include <chrono>
#include <csignal>
#include <iostream>
#include <thread>
#include <vector>

#include <boost/asio/io_context.hpp>

#include "aggregator/aggregator.hpp"
#include "grid/geo_validator.hpp"
#include "server/http_ws.hpp"

namespace {
std::atomic<bool> g_running{true};
void on_signal(int) {
    g_running.store(false, std::memory_order_release);
}
}  // namespace

int main() {
    std::signal(SIGINT, on_signal);
    std::signal(SIGTERM, on_signal);

    boost::asio::io_context ioc;
    monitor::Aggregator aggregator(1U << 20U);
    monitor::GeoValidator validator;
    monitor::HttpWsServer server(ioc, 8080, aggregator, validator, "frontend");

    aggregator.start();
    server.run();

    std::thread ws_broadcast([&]() { server.broadcast_loop(g_running); });

    const auto workers_count = std::max(1U, std::thread::hardware_concurrency());
    std::vector<std::thread> workers;
    workers.reserve(workers_count);
    for (unsigned i = 0; i < workers_count; ++i) {
        workers.emplace_back([&ioc]() { ioc.run(); });
    }

    while (g_running.load(std::memory_order_acquire)) {
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }

    server.stop();
    ioc.stop();
    aggregator.stop();

    if (ws_broadcast.joinable()) {
        ws_broadcast.join();
    }
    for (auto& t : workers) {
        if (t.joinable()) {
            t.join();
        }
    }

    std::cout << "[SHUTDOWN] queue_depth=" << aggregator.queue_depth()
              << " dropped=" << aggregator.dropped()
              << " rejected_out_of_radius=" << validator.rejected_out_of_radius()
              << std::endl;
    return 0;
}
