#include "loadgen/generator.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <random>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#include <boost/asio/connect.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>

namespace monitor {

namespace beast = boost::beast;
namespace http = beast::http;
using tcp = boost::asio::ip::tcp;

namespace {
constexpr double kPi = 3.14159265358979323846;
constexpr double kEarthRadiusKm = 6371.0088;
constexpr double kCenterLat = 56.8389;
constexpr double kCenterLon = 60.6057;

double to_deg(double r) noexcept { return r * 180.0 / kPi; }
double to_rad(double d) noexcept { return d * kPi / 180.0; }

std::pair<double, double> random_point_in_radius(std::mt19937_64& rng, double radius_km) {
    std::uniform_real_distribution<double> unif(0.0, 1.0);
    std::uniform_real_distribution<double> az(0.0, 2.0 * kPi);
    const double d = radius_km * std::sqrt(unif(rng));
    const double brng = az(rng);
    const double lat1 = to_rad(kCenterLat);
    const double lon1 = to_rad(kCenterLon);
    const double dr = d / kEarthRadiusKm;

    const double lat2 = std::asin(std::sin(lat1) * std::cos(dr) + std::cos(lat1) * std::sin(dr) * std::cos(brng));
    const double lon2 = lon1 + std::atan2(std::sin(brng) * std::sin(dr) * std::cos(lat1),
                                          std::cos(dr) - std::sin(lat1) * std::sin(lat2));
    return {to_deg(lat2), to_deg(lon2)};
}

std::string payload(double lat, double lon, std::int64_t ts, std::mt19937_64& rng) {
    std::bernoulli_distribution b(0.85);
    std::ostringstream ss;
    ss << "{\"lat\":" << lat << ",\"lon\":" << lon << ",\"ts\":" << ts
       << ",\"flags\":{\"wifi\":" << (b(rng) ? "true" : "false")
       << ",\"mobile\":" << (b(rng) ? "true" : "false")
       << ",\"whitelist\":" << (b(rng) ? "true" : "false")
       << ",\"non_whitelist\":" << (b(rng) ? "true" : "false")
       << ",\"vpn\":" << (b(rng) ? "true" : "false")
       << ",\"yt_tg_no_vpn\":" << (b(rng) ? "true" : "false")
       << ",\"geolocation\":" << (b(rng) ? "true" : "false")
       << "}}";
    return ss.str();
}
}  // namespace

LoadGenerator::LoadGenerator(LoadgenConfig config) : config_(std::move(config)) {}

int LoadGenerator::run() {
    const auto started = std::chrono::steady_clock::now();
    std::atomic<std::uint64_t> sent{0};
    std::atomic<std::uint64_t> errors{0};

    auto worker = [&](std::size_t tid) {
        boost::asio::io_context ioc;
        tcp::resolver resolver(ioc);
        beast::tcp_stream stream(ioc);
        auto const results = resolver.resolve(config_.host, std::to_string(config_.port));
        stream.connect(results);

        std::mt19937_64 rng(static_cast<std::uint64_t>(std::random_device{}()) + tid);
        const std::uint64_t per_thread = config_.total_messages / config_.threads;
        std::uint64_t local_sent = 0;
        std::int64_t virtual_ts = 1715000000 + static_cast<std::int64_t>(tid) * 1000;

        while (local_sent < per_thread) {
            const std::size_t to_send = static_cast<std::size_t>(std::min<std::uint64_t>(
                static_cast<std::uint64_t>(config_.batch_size), per_thread - local_sent));
            for (std::size_t i = 0; i < to_send; ++i) {
                auto p = random_point_in_radius(rng, 1000.0);
                auto body = payload(p.first, p.second, virtual_ts++, rng);
                http::request<http::string_body> req{http::verb::post, "/ingest", 11};
                req.set(http::field::host, config_.host);
                req.set(http::field::content_type, "application/json");
                req.keep_alive(true);
                req.body() = std::move(body);
                req.prepare_payload();

                beast::error_code ec;
                http::write(stream, req, ec);
                if (ec) {
                    errors.fetch_add(1, std::memory_order_relaxed);
                    continue;
                }
                beast::flat_buffer buffer;
                http::response<http::string_body> res;
                http::read(stream, buffer, res, ec);
                if (ec) {
                    errors.fetch_add(1, std::memory_order_relaxed);
                    continue;
                }
                sent.fetch_add(1, std::memory_order_relaxed);
            }
            local_sent += to_send;
            const std::uint64_t global = sent.load(std::memory_order_relaxed);
            if (global > 0 && global % 50000000ULL == 0ULL) {
                std::cout << "[LOADGEN] progress=" << global << std::endl;
            }
        }
        beast::error_code ec;
        stream.socket().shutdown(tcp::socket::shutdown_both, ec);
    };

    std::vector<std::thread> pool;
    pool.reserve(config_.threads);
    for (std::size_t i = 0; i < config_.threads; ++i) {
        pool.emplace_back(worker, i);
    }
    for (auto& t : pool) {
        t.join();
    }

    const auto ended = std::chrono::steady_clock::now();
    const double sec = std::chrono::duration<double>(ended - started).count();
    const double rps = sec > 0.0 ? static_cast<double>(sent.load()) / sec : 0.0;
    std::cout << "[LOADGEN] sent=" << sent.load() << " errors=" << errors.load() << " elapsed_sec=" << sec
              << " rps=" << rps << " p99_latency_ms=na queue_depth=na drop_rate=na cpu=na ram=na rejected_out_of_radius=na"
              << std::endl;
    return errors.load() > 0 ? 1 : 0;
}

}  // namespace monitor

int main(int argc, char** argv) {
    monitor::LoadgenConfig cfg;
    if (argc > 1) {
        cfg.total_messages = static_cast<std::uint64_t>(std::strtoull(argv[1], nullptr, 10));
    }
    if (argc > 2) {
        cfg.threads = static_cast<std::size_t>(std::strtoull(argv[2], nullptr, 10));
    }
    monitor::LoadGenerator gen(cfg);
    return gen.run();
}
