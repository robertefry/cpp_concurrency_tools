
set(CPM_SOURCE_CACHE "${CMAKE_BINARY_DIR}/../.cache/cpm" CACHE PATH "CPM.cmake source cache")
include(${CMAKE_CURRENT_LIST_DIR}/get_cpm.cmake)

include(CheckCXXCompilerFlag)
check_cxx_compiler_flag(-Wc2y-extensions _has_wc2y_extensions)

CPMAddPackage(
  NAME Catch2
  GITHUB_REPOSITORY catchorg/Catch2
  VERSION 3.16.0
)

# Suppress -Wc2y-extensions warnings caused by Catch2's use of '__COUNTER__'.
if (_has_wc2y_extensions)
  get_target_property(_catch2 Catch2::Catch2 ALIASED_TARGET)
  target_compile_options(${_catch2} INTERFACE -Wno-c2y-extensions)
endif()

list(APPEND CMAKE_MODULE_PATH ${Catch2_SOURCE_DIR}/extras)
include(CTest)
include(Catch)
