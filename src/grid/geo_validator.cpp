#include "grid/geo_validator.hpp"

#include <cmath>
#include <iomanip>
#include <limits>
#include <sstream>

namespace monitor {

namespace {
constexpr double kEarthRadiusKm = 6371.0088;
constexpr double kPi = 3.14159265358979323846;
double to_rad(double deg) noexcept {
    return deg * kPi / 180.0;
}
}  // namespace

bool GeoValidator::in_radius(double lat, double lon, double& distance_km) const noexcept {
    if (!std::isfinite(lat) || !std::isfinite(lon) || lat < -90.0 || lat > 90.0 || lon < -180.0 || lon > 180.0) {
        distance_km = std::numeric_limits<double>::infinity();
        return false;
    }
    const double d_lat = to_rad(lat - kCenterLat);
    const double d_lon = to_rad(lon - kCenterLon);
    const double a = std::sin(d_lat / 2.0) * std::sin(d_lat / 2.0) +
                     std::cos(to_rad(kCenterLat)) * std::cos(to_rad(lat)) *
                         std::sin(d_lon / 2.0) * std::sin(d_lon / 2.0);
    const double c = 2.0 * std::atan2(std::sqrt(a), std::sqrt(1.0 - a));
    distance_km = kEarthRadiusKm * c;
    return distance_km <= kRadiusKm;
}

bool GeoValidator::validate_or_reject(double lat, double lon, std::string& log_line) noexcept {
    double distance_km = 0.0;
    const bool accepted = in_radius(lat, lon, distance_km);
    if (!accepted) {
        rejected_out_of_radius_.fetch_add(1, std::memory_order_relaxed);
        std::ostringstream oss;
        oss << "[VALIDATE] Rejected: (" << std::fixed << std::setprecision(6) << lat << "," << lon << ") "
            << std::setprecision(2) << distance_km << " km > 1000km";
        log_line = oss.str();
    }
    return accepted;
}

std::uint64_t GeoValidator::rejected_out_of_radius() const noexcept {
    return rejected_out_of_radius_.load(std::memory_order_relaxed);
}

}  // namespace monitor
