cmake_minimum_required(VERSION 4.3.2)
project(co_mcp VERSION 1.0.0 LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Threads REQUIRED)
find_package(CURL REQUIRED)
find_package(nlohmann_json REQUIRED)
find_package(spdlog REQUIRED)
find_package(GTest REQUIRED)

enable_testing()

add_subdirectory(src)
add_subdirectory(tests)
add_subdirectory(lib)
