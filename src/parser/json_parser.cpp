#include "parser/json_parser.hpp"

#include <cmath>
#include <limits>
#include <string>

#include <simdjson.h>

namespace monitor {

namespace {
bool is_valid_coord(double v) noexcept {
    return std::isfinite(v);
}
}  // namespace

bool JsonParser::parse(std::string_view body, InputRecord& out, std::string& error) const noexcept {
    simdjson::ondemand::parser parser;
    simdjson::padded_string padded(body);
    auto doc_result = parser.iterate(padded);
    if (doc_result.error() != simdjson::SUCCESS) {
        error = "json_parse_failed";
        return false;
    }

    simdjson::ondemand::object root;
    if (doc_result.get(root) != simdjson::SUCCESS) {
        error = "json_object_expected";
        return false;
    }

    auto lat_result = root["lat"].get_double();
    auto lon_result = root["lon"].get_double();
    auto ts_result = root["ts"].get_int64();
    auto flags_result = root["flags"].get_object();

    if (lat_result.error() != simdjson::SUCCESS || lon_result.error() != simdjson::SUCCESS ||
        ts_result.error() != simdjson::SUCCESS || flags_result.error() != simdjson::SUCCESS) {
        error = "missing_required_fields";
        return false;
    }

    const double lat = lat_result.value_unsafe();
    const double lon = lon_result.value_unsafe();
    if (!is_valid_coord(lat) || !is_valid_coord(lon) || lat < -90.0 || lat > 90.0 || lon < -180.0 || lon > 180.0) {
        error = "invalid_coordinates";
        return false;
    }

    simdjson::ondemand::object flags_obj = flags_result.value_unsafe();
    auto wifi = flags_obj["wifi"].get_bool();
    auto mobile = flags_obj["mobile"].get_bool();
    auto whitelist = flags_obj["whitelist"].get_bool();
    auto non_whitelist = flags_obj["non_whitelist"].get_bool();
    auto vpn = flags_obj["vpn"].get_bool();
    auto yt_tg_no_vpn = flags_obj["yt_tg_no_vpn"].get_bool();
    auto geolocation = flags_obj["geolocation"].get_bool();

    if (wifi.error() != simdjson::SUCCESS || mobile.error() != simdjson::SUCCESS ||
        whitelist.error() != simdjson::SUCCESS || non_whitelist.error() != simdjson::SUCCESS ||
        vpn.error() != simdjson::SUCCESS || yt_tg_no_vpn.error() != simdjson::SUCCESS ||
        geolocation.error() != simdjson::SUCCESS) {
        error = "invalid_flags";
        return false;
    }

    out.lat = lat;
    out.lon = lon;
    out.ts = ts_result.value_unsafe();
    out.flags = Flags{
        wifi.value_unsafe(),
        mobile.value_unsafe(),
        whitelist.value_unsafe(),
        non_whitelist.value_unsafe(),
        vpn.value_unsafe(),
        yt_tg_no_vpn.value_unsafe(),
        geolocation.value_unsafe(),
    };
    return true;
}

}  // namespace monitor
