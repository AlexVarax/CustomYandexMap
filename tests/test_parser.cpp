#include <string>

#include <catch2/catch_test_macros.hpp>

#include <catch2/catch_approx.hpp>

#include "parser/json_parser.hpp"

TEST_CASE("Парсер принимает корректный JSON", "[parser]") {
    const std::string payload =
        R"({"lat":56.8389,"lon":60.6057,"ts":1715000000,"flags":{"wifi":true,"mobile":true,"whitelist":true,"non_whitelist":false,"vpn":false,"yt_tg_no_vpn":true,"geolocation":true}})";
    monitor::JsonParser parser;
    monitor::InputRecord record{};
    std::string error;
    REQUIRE(parser.parse(payload, record, error));
    CHECK(record.lat == Catch::Approx(56.8389));
    CHECK(record.lon == Catch::Approx(60.6057));
}

TEST_CASE("Парсер отклоняет некорректные координаты", "[parser]") {
    const std::string payload =
        R"({"lat":91.0,"lon":181.0,"ts":1715000000,"flags":{"wifi":true,"mobile":true,"whitelist":true,"non_whitelist":false,"vpn":false,"yt_tg_no_vpn":true,"geolocation":true}})";
    monitor::JsonParser parser;
    monitor::InputRecord record{};
    std::string error;
    CHECK_FALSE(parser.parse(payload, record, error));
}
