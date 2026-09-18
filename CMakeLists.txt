
cmake_minimum_required(VERSION 3.12)

project(cpp_thread_tools)

include(cmake/standards.cmake)
include(cmake/dependencies.cmake)

add_library(thread_tools INTERFACE)
target_set_standards(thread_tools)

target_include_directories(thread_tools
  INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

add_library(thread_tools-tests INTERFACE
  tests/test_channel.cc
)
target_set_standards(thread_tools-tests)

target_link_libraries(thread_tools-tests INTERFACE
  thread_tools
  Catch2::Catch2WithMain
)

# Suppress annoying c2y extension warnings for Catch2
target_compile_options(thread_tools-tests INTERFACE -Wno-c2y-extensions)

enable_testing()
target_add_sanitized_catch_tests(thread_tools-tests)
