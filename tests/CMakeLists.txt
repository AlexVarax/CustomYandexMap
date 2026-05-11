FetchContent_Declare(
    catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG v3.7.1
)
FetchContent_MakeAvailable(catch2)

add_executable(test_parser test_parser.cpp)
target_link_libraries(test_parser PRIVATE core Catch2::Catch2WithMain)
target_compile_options(test_parser PRIVATE ${MONITOR_COMMON_FLAGS})
add_test(NAME test_parser COMMAND test_parser)

add_executable(test_geo test_geo.cpp)
target_link_libraries(test_geo PRIVATE core Catch2::Catch2WithMain)
target_compile_options(test_geo PRIVATE ${MONITOR_COMMON_FLAGS})
add_test(NAME test_geo COMMAND test_geo)
