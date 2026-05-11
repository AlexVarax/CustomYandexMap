#pragma once

#include <atomic>
#include <cstdint>
#include <string>

namespace monitor {

struct LoadgenConfig final {
    std::string host = "127.0.0.1";
    unsigned short port = 8080;
    std::uint64_t total_messages = 1000000;
    std::size_t batch_size = 20000;
    std::size_t threads = 4;
};

class LoadGenerator final {
public:
    explicit LoadGenerator(LoadgenConfig config);
    int run();

private:
    LoadgenConfig config_;
};

}  // namespace monitor
