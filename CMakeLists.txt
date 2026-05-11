cmake_minimum_required(VERSION 3.22)
project(internet_monitor LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(BUILD_LOADGEN "Собирать генератор нагрузки" ON)
option(BUILD_TESTING "Собирать тесты" ON)

include(FetchContent)

# 🔧 Убрали -fno-exceptions и -fno-rtti — Boost.Beast требует исключений
set(MONITOR_COMMON_FLAGS
    -O3
    -march=native
    -flto
    -Wall
    -Wextra
    -Wpedantic
    -Werror
    # -fno-exceptions  # ← убрано
    # -fno-rtti        # ← убрано
)

find_package(Threads REQUIRED)
find_package(Boost 1.74 REQUIRED COMPONENTS system)
find_package(OpenSSL REQUIRED)
find_package(S2 QUIET)

FetchContent_Declare(
    simdjson
    GIT_REPOSITORY https://github.com/simdjson/simdjson.git
    GIT_TAG v3.9.4
)
FetchContent_MakeAvailable(simdjson)

FetchContent_Declare(
    readerwriterqueue
    GIT_REPOSITORY https://github.com/cameron314/readerwriterqueue.git
    GIT_TAG v1.0.6
)
FetchContent_MakeAvailable(readerwriterqueue)

add_library(rwq INTERFACE)
target_include_directories(rwq INTERFACE ${readerwriterqueue_SOURCE_DIR})

add_library(core
    src/server/http_ws.cpp
    src/parser/json_parser.cpp
    src/grid/geo_validator.cpp
    src/grid/cell_grid.cpp
    src/aggregator/aggregator.cpp
)

target_include_directories(core PUBLIC src)
target_link_libraries(core
    PUBLIC
        Threads::Threads
        Boost::system
        OpenSSL::SSL
        OpenSSL::Crypto
        simdjson
        rwq
)

if(S2_FOUND)
    target_link_libraries(core PUBLIC S2::s2)
    target_compile_definitions(core PUBLIC HAS_S2=1)
else()
    target_compile_definitions(core PUBLIC HAS_S2=0)
endif()

target_compile_options(core PRIVATE ${MONITOR_COMMON_FLAGS})
target_link_options(core PRIVATE -flto)

# monitor_server
add_executable(monitor_server src/main.cpp)
target_link_libraries(monitor_server PRIVATE core)
target_compile_options(monitor_server PRIVATE ${MONITOR_COMMON_FLAGS})
target_link_options(monitor_server PRIVATE -flto)

# loadgen
if(BUILD_LOADGEN)
    add_executable(loadgen src/loadgen/generator.cpp)
    target_link_libraries(loadgen PRIVATE core)
    target_compile_options(loadgen PRIVATE ${MONITOR_COMMON_FLAGS})
    target_link_options(loadgen PRIVATE -flto)
endif()

# Тесты
if(BUILD_TESTING)
    include(CTest)
    add_subdirectory(tests)
endif()