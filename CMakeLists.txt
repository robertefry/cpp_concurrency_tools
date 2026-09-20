
cmake_minimum_required(VERSION 3.25)

project(cpp_thread_tools)

include(.cmake/standards.cmake)
include(.cmake/dependencies.cmake)

add_library(thread_tools INTERFACE)
target_set_standards(thread_tools)

target_include_directories(thread_tools
  INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

enable_testing()

add_library(thread_tools-tests INTERFACE
  tests/channel_tests.cc
)
target_add_catch_tests(thread_tools-tests)

target_link_libraries(thread_tools-tests INTERFACE
  thread_tools
  Catch2::Catch2WithMain
)

add_executable(thread_tools-benchmarks
  tests/channel_benchmarks.cc
)
target_add_catch_benchmarks(thread_tools-benchmarks)

target_link_libraries(thread_tools-benchmarks
  thread_tools
  Catch2::Catch2WithMain
)
