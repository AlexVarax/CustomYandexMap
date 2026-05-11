#include "aggregator/aggregator.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>

namespace monitor {

Aggregator::Aggregator(std::size_t queue_capacity)
    : queue_(queue_capacity) {}

Aggregator::~Aggregator() {
    stop();
}

bool Aggregator::enqueue(const Sample& sample) noexcept {
    if (!queue_.try_enqueue(sample)) {
        dropped_.fetch_add(1, std::memory_order_relaxed);
        return false;
    }
    queue_depth_.fetch_add(1, std::memory_order_relaxed);
    return true;
}

void Aggregator::start() {
    bool expected = false;
    if (!running_.compare_exchange_strong(expected, true, std::memory_order_acq_rel)) {
        return;
    }
    worker_ = std::thread(&Aggregator::run_loop, this);
}

void Aggregator::stop() {
    bool expected = true;
    if (!running_.compare_exchange_strong(expected, false, std::memory_order_acq_rel)) {
        return;
    }
    if (worker_.joinable()) {
        worker_.join();
    }
}

void Aggregator::run_loop() {
    using namespace std::chrono_literals;
    while (running_.load(std::memory_order_acquire)) {
        Sample sample{};
        std::size_t consumed = 0;
        while (queue_.try_dequeue(sample)) {
            add_sample(sample);
            ++consumed;
        }
        if (consumed > 0) {
            queue_depth_.fetch_sub(consumed, std::memory_order_relaxed);
        }

        const auto now = std::chrono::system_clock::now();
        const auto sec = std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count();
        if (sec > last_emit_sec_) {
            last_emit_sec_ = sec;
            deltas_.clear();
            snapshot_deltas(deltas_);
        }
        std::this_thread::sleep_for(20ms);
    }
}

void Aggregator::rotate_cell(CellWindow& cell, std::int64_t ts_sec) {
    if (cell.last_ts < 0) {
        cell.last_ts = ts_sec;
        return;
    }
    const auto diff = ts_sec - cell.last_ts;
    if (diff <= 0) {
        return;
    }
    const std::int64_t steps = std::min<std::int64_t>(30, diff);
    for (std::int64_t i = 1; i <= steps; ++i) {
        const std::size_t idx = static_cast<std::size_t>((cell.last_ts + i) % 30);
        cell.seconds[idx] = Bucket{};
    }
    cell.last_ts = ts_sec;
}

void Aggregator::add_sample(const Sample& sample) {
    const std::uint64_t id = grid_.cell_id(sample.lat, sample.lon);
    auto& window = cells_[id];
    rotate_cell(window, sample.ts);
    Bucket& bucket = window.seconds[static_cast<std::size_t>(sample.ts % 30)];
    ++bucket.total;
    bucket.positive[0] += sample.flags.wifi ? 1U : 0U;
    bucket.positive[1] += sample.flags.mobile ? 1U : 0U;
    bucket.positive[2] += sample.flags.whitelist ? 1U : 0U;
    bucket.positive[3] += sample.flags.non_whitelist ? 1U : 0U;
    bucket.positive[4] += sample.flags.vpn ? 1U : 0U;
    bucket.positive[5] += sample.flags.yt_tg_no_vpn ? 1U : 0U;
    bucket.positive[6] += sample.flags.geolocation ? 1U : 0U;
}

void Aggregator::snapshot_deltas(std::vector<CellDelta>& out) {
    for (auto& [id, window] : cells_) {
        std::array<std::uint64_t, 7> sums{};
        std::uint64_t total = 0;
        for (const auto& sec : window.seconds) {
            total += sec.total;
            for (std::size_t i = 0; i < sums.size(); ++i) {
                sums[i] += sec.positive[i];
            }
        }
        if (total == 0) {
            continue;
        }

        CellDelta delta{};
        delta.cell_id = id;
        delta.total = total;
        bool changed = false;
        for (std::size_t i = 0; i < sums.size(); ++i) {
            const double value = static_cast<double>(sums[i]) / static_cast<double>(total);
            delta.availability[i] = value;
            if (std::abs(value - window.last_sent[i]) > 0.01) {
                changed = true;
            }
            window.last_sent[i] = value;
        }
        if (changed) {
            out.push_back(delta);
        }
    }
}

std::vector<CellDelta> Aggregator::consume_deltas() {
    return deltas_;
}

std::uint64_t Aggregator::queue_depth() const noexcept {
    return queue_depth_.load(std::memory_order_relaxed);
}

std::uint64_t Aggregator::dropped() const noexcept {
    return dropped_.load(std::memory_order_relaxed);
}

}  // namespace monitor
