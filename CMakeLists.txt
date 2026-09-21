
cmake_minimum_required(VERSION 3.25)

project(cpp_concurrency_tools)

include(.cmake/standards.cmake)
include(.cmake/dependencies.cmake)

add_library(concurrency_tools INTERFACE)
target_set_standards(concurrency_tools)

target_include_directories(concurrency_tools
  INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

enable_testing()

add_library(concurrency_tools-tests INTERFACE
  tests/channel_tests.cc
)
target_add_catch_tests(concurrency_tools-tests)

target_link_libraries(concurrency_tools-tests INTERFACE
  concurrency_tools
  Catch2::Catch2WithMain
)

add_executable(concurrency_tools-benchmarks
  tests/channel_benchmarks.cc
)
target_add_catch_benchmarks(concurrency_tools-benchmarks)

target_link_libraries(concurrency_tools-benchmarks
  concurrency_tools
  Catch2::Catch2WithMain
)
