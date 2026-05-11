#include "grid/cell_grid.hpp"

#include <cstdint>

namespace monitor {

std::uint64_t CellGrid::cell_id(double lat, double lon) const noexcept {
#if HAS_S2
    const auto ll = S2LatLng::FromDegrees(lat, lon);
    const S2CellId id = S2CellId::FromLatLng(ll).parent(13);
    return id.id();
#else
    const std::int64_t lat_bin = static_cast<std::int64_t>((lat + 90.0) * 600.0);
    const std::int64_t lon_bin = static_cast<std::int64_t>((lon + 180.0) * 600.0);
    return (static_cast<std::uint64_t>(lat_bin) << 32U) ^ static_cast<std::uint64_t>(lon_bin);
#endif
}

}  // namespace monitor
