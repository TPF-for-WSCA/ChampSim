cmake_minimum_required(VERSION 3.24)
project(ChampSimDRAMPower LANGUAGES CXX)

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")
set(BUILD_SHARED_LIBS OFF)
find_package(nlohmann_json 3.11 REQUIRED)

set(FETCHCONTENT_SOURCE_DIR_DRAMUTILS "${CMAKE_CURRENT_SOURCE_DIR}/../../third_party/DRAMUtils")
if(NOT EXISTS "${FETCHCONTENT_SOURCE_DIR_DRAMUTILS}/CMakeLists.txt")
    message(FATAL_ERROR "Run git submodule update --init third_party/DRAMUtils")
endif()

set(DRAMUTILS_USE_FETCH_CONTENT OFF CACHE BOOL "" FORCE)
set(DRAMUTILS_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(DRAMPOWER_USE_FETCH_CONTENT OFF CACHE BOOL "" FORCE)
set(DRAMPOWER_BUILD_CLI OFF CACHE BOOL "" FORCE)
set(DRAMPOWER_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(DRAMPOWER_BUILD_BENCHMARKS OFF CACHE BOOL "" FORCE)

include(FetchContent)
# Redirect DRAMPower's find_package(DRAMUtils) to the local target.
FetchContent_Declare(DRAMUtils OVERRIDE_FIND_PACKAGE)
FetchContent_MakeAvailable(DRAMUtils)

add_subdirectory("../../third_party/DRAMPower" drampower)

add_library(champsim_drampower STATIC adapter.cc)
target_compile_features(champsim_drampower PRIVATE cxx_std_17)
target_include_directories(champsim_drampower PRIVATE "../../inc")
target_link_libraries(champsim_drampower PRIVATE DRAMPower::DRAMPower)
