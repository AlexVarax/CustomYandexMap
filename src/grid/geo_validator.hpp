#pragma once

#include <atomic>
#include <cstdint>
#include <string>

namespace monitor {

class GeoValidator final {
public:
    static constexpr double kCenterLat = 56.8389;
    static constexpr double kCenterLon = 60.6057;
    static constexpr double kRadiusKm = 1000.0;

    bool in_radius(double lat, double lon, double& distance_km) const noexcept;
    bool validate_or_reject(double lat, double lon, std::string& log_line) noexcept;
    std::uint64_t rejected_out_of_radius() const noexcept;

private:
    std::atomic<std::uint64_t> rejected_out_of_radius_{0};
};

}  // namespace monitor
