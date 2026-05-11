#pragma once

#include <array>
#include <cstdint>

#if HAS_S2
#include <s2/s2cell_id.h>
#include <s2/s2latlng.h>
#endif

namespace monitor {

struct CellStats final {
    std::uint64_t total = 0;
    std::array<std::uint64_t, 7> positive{};
};

class CellGrid final {
public:
    std::uint64_t cell_id(double lat, double lon) const noexcept;
};

}  // namespace monitor
