cmake_minimum_required(VERSION 3.16)
project(hft_engine CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ---------------------------------------------------------------------------
# Compiler flags
# ---------------------------------------------------------------------------
if(MSVC)
    add_compile_options(/W4 /O2)
else()
    add_compile_options(
        -Wall -Wextra
        -O2
    )
endif()

# cpp-httplib requires Windows 10 (0x0A00) or later
if(WIN32)
    add_compile_definitions(_WIN32_WINNT=0x0A00)
endif()



# ---------------------------------------------------------------------------
# Include paths
# ---------------------------------------------------------------------------
include_directories(
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
)

# ---------------------------------------------------------------------------
# Core engine library (shared between main + tests)
# ---------------------------------------------------------------------------
add_library(hft_core STATIC
    src/order_book.cpp
    src/matching_engine.cpp
)

target_include_directories(hft_core PUBLIC
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
)

# Link pthreads on Linux (needed by httplib)
if(UNIX)
    target_link_libraries(hft_core pthread)
endif()

# ---------------------------------------------------------------------------
# Main executable
# ---------------------------------------------------------------------------
add_executable(hft_engine src/main.cpp)
target_link_libraries(hft_engine hft_core)

if(WIN32)
    target_link_libraries(hft_engine ws2_32)
endif()

if(UNIX)
    target_link_libraries(hft_engine pthread)
endif()

# ---------------------------------------------------------------------------
# Tests (doctest)
# ---------------------------------------------------------------------------
enable_testing()

set(TEST_SOURCES
    tests/test_order_book.cpp
    tests/test_matching_engine.cpp
    tests/test_memory_pool.cpp
)

foreach(test_src ${TEST_SOURCES})
    get_filename_component(test_name ${test_src} NAME_WE)
    add_executable(${test_name} ${test_src})
    target_include_directories(${test_name} PRIVATE
        ${CMAKE_SOURCE_DIR}/tests
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
    )
    target_link_libraries(${test_name} hft_core)
    if(UNIX)
        target_link_libraries(${test_name} pthread)
    endif()
    add_test(NAME ${test_name} COMMAND ${test_name})
endforeach()

# ---------------------------------------------------------------------------
# Benchmark executable
# ---------------------------------------------------------------------------
add_executable(benchmark tests/benchmark.cpp)
target_include_directories(benchmark PRIVATE
    ${CMAKE_SOURCE_DIR}/tests
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
)
target_link_libraries(benchmark hft_core)
if(UNIX)
    target_link_libraries(benchmark pthread)
endif()
