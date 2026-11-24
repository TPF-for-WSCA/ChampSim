cmake_minimum_required(VERSION 3.24)
project(ChampSimDRAMPower LANGUAGES CXX)

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")
set(BUILD_SHARED_LIBS OFF)
find_package(nlohmann_json 3.11 REQUIRED)
include(FetchContent)
FetchContent_Declare(DRAMUtils
    URL https://codeload.github.com/tukl-msd/DRAMUtils/tar.gz/refs/tags/v1.12.1
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    OVERRIDE_FIND_PACKAGE)
FetchContent_MakeAvailable(DRAMUtils)

set(DRAMPOWER_BUILD_CLI OFF CACHE BOOL "" FORCE)
set(DRAMPOWER_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(DRAMPOWER_BUILD_BENCHMARKS OFF CACHE BOOL "" FORCE)
add_subdirectory("../../third_party/DRAMPower" drampower)

add_library(champsim_drampower STATIC adapter.cc)
target_compile_features(champsim_drampower PRIVATE cxx_std_17)
target_include_directories(champsim_drampower PRIVATE "../../inc")
target_link_libraries(champsim_drampower PRIVATE DRAMPower::DRAMPower)
