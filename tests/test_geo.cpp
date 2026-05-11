#include <catch2/catch_test_macros.hpp>

#include "grid/geo_validator.hpp"

TEST_CASE("Гео-валидатор принимает Екатеринбург", "[geo]") {
    monitor::GeoValidator validator;
    double km = 0.0;
    CHECK(validator.in_radius(56.84, 60.61, km));
}

TEST_CASE("Точка 999.9 км принимается", "[geo]") {
    monitor::GeoValidator validator;
    double km = 0.0;
    CHECK(validator.in_radius(65.83, 60.6057, km));
}

TEST_CASE("Москва отклоняется и счётчик растёт", "[geo]") {
    monitor::GeoValidator validator;
    std::string log_line;
    CHECK_FALSE(validator.validate_or_reject(55.75, 37.62, log_line));
    CHECK(validator.rejected_out_of_radius() == 1);
}

TEST_CASE("Точка за 1000 км отклоняется", "[geo]") {
    monitor::GeoValidator validator;
    std::string log_line;
    CHECK_FALSE(validator.validate_or_reject(66.0, 60.6057, log_line));
}
