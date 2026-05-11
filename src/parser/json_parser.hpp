#pragma once

#include <cstdint>
#include <string_view>

namespace monitor {

struct Flags final {
    bool wifi;
    bool mobile;
    bool whitelist;
    bool non_whitelist;
    bool vpn;
    bool yt_tg_no_vpn;
    bool geolocation;
};

struct InputRecord final {
    double lat;
    double lon;
    std::int64_t ts;
    Flags flags;
};

class JsonParser final {
public:
    bool parse(std::string_view body, InputRecord& out, std::string& error) const noexcept;
};

}  // namespace monitor
