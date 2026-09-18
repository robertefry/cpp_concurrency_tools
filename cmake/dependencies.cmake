
set(CPM_SOURCE_CACHE "${CMAKE_BINARY_DIR}/../.cache/cpm" CACHE PATH "CPM.cmake source cache")
include(${CMAKE_CURRENT_LIST_DIR}/get_cpm.cmake)

CPMAddPackage(
  NAME Catch2
  GITHUB_REPOSITORY catchorg/Catch2
  VERSION 3.16.0
)

list(APPEND CMAKE_MODULE_PATH ${Catch2_SOURCE_DIR}/extras)
include(CTest)
include(Catch)
