#pragma once

#include <atomic>
#include <array>
#include <cstdint>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

#include "grid/cell_grid.hpp"
#include "parser/json_parser.hpp"
#include "readerwriterqueue.h"

namespace monitor {

struct Sample final {
    double lat;
    double lon;
    std::int64_t ts;
    Flags flags;
};

struct CellDelta final {
    std::uint64_t cell_id;
    std::array<double, 7> availability;
    std::uint64_t total;
};

class Aggregator final {
public:
    explicit Aggregator(std::size_t queue_capacity);
    ~Aggregator();

    bool enqueue(const Sample& sample) noexcept;
    void start();
    void stop();

    std::vector<CellDelta> consume_deltas();
    std::uint64_t queue_depth() const noexcept;
    std::uint64_t dropped() const noexcept;

private:
    struct Bucket final {
        std::array<std::uint64_t, 7> positive{};
        std::uint64_t total = 0;
    };
    struct CellWindow final {
        std::array<Bucket, 30> seconds{};
        std::int64_t last_ts = -1;
        std::array<double, 7> last_sent{};
    };

    void run_loop();
    void add_sample(const Sample& sample);
    void rotate_cell(CellWindow& cell, std::int64_t ts_sec);
    void snapshot_deltas(std::vector<CellDelta>& out);

    CellGrid grid_;
    moodycamel::ReaderWriterQueue<Sample> queue_;
    std::atomic<bool> running_{false};
    std::atomic<std::uint64_t> queue_depth_{0};
    std::atomic<std::uint64_t> dropped_{0};
    std::thread worker_;
    std::unordered_map<std::uint64_t, CellWindow> cells_;
    std::vector<CellDelta> deltas_;
    std::int64_t last_emit_sec_ = 0;
};

}  // namespace monitor
