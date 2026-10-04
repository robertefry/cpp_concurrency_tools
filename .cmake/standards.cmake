
if (NOT CMAKE_BUILD_TYPE)
  message(STATUS "Build type not set. We'll default to Debug.")
  set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
else()
  message(STATUS "Current build type: ${CMAKE_BUILD_TYPE}")
endif()

set(VALID_BUILD_TYPES Debug Release RelWithDebInfo MinSizeRel)
if (NOT CMAKE_BUILD_TYPE IN_LIST VALID_BUILD_TYPES)
  message(FATAL_ERROR "Invalid build type: ${CMAKE_BUILD_TYPE}")
endif()

find_program(CLANG_TIDY "clang-tidy")

function(target_set_standards target)

  get_target_property(_target_type ${target} TYPE)

  if (_target_type STREQUAL "INTERFACE_LIBRARY")
    message(STATUS "${target} is an INTERFACE library; skipping compile settings.")
    return()
  endif()

  set_target_properties(${target} PROPERTIES
    C_STANDARD 23
    C_STANDARD_REQUIRED ON
    C_EXTENSIONS OFF
    CXX_STANDARD 23
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
  )

  if (CLANG_TIDY)
    set_target_properties(${target} PROPERTIES CXX_CLANG_TIDY ${CLANG_TIDY})
  endif()

  if (
    CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU"
  )
    target_compile_options(${target} PRIVATE
      -Wall -Wextra -Wpedantic
      -Wshadow
      -Wundef
      -Wnon-virtual-dtor
      -Woverloaded-virtual
      -Wno-ignored-optimization-argument
      -Wunused
      -Wconversion
      -Wsign-conversion
      -Wold-style-cast
      -Wcast-align
      -Wdouble-promotion
      -Weffc++
      -Wnull-dereference
      -Wimplicit-function-declaration
      -Wredundant-decls
      -Wstrict-prototypes
      -Wmissing-noreturn
    )

    target_compile_options(${target} PRIVATE
      $<$<CONFIG:Debug>:
        -Og -g
      >
      $<$<CONFIG:RelWithDebInfo>:
        -Og -g -DNDEBUG
      >
      $<$<CONFIG:Release>:
        -O3 -DNDEBUG
      >
      $<$<CONFIG:MinSizeRel>:
        -Os -DNDEBUG
      >
    )

  else()
    string(ASCII 27 Esc)
    message(NOTICE "${Esc}[33mWARNING: Unsupported compiler: ${CMAKE_CXX_COMPILER_ID}${Esc}[0m")
  endif()

endfunction()

function(target_add_catch_tests target)

  get_target_property(_type ${target} TYPE)

  if (NOT _type STREQUAL "INTERFACE_LIBRARY")
    message(FATAL_ERROR "target_add_catch_tests: '${target}' must be an INTERFACE library, got ${_type}")
  endif()

  get_target_property(_sources ${target} SOURCES)

  set(_san_names "nosan" "aubsan" "tsan")
  set(_san_flags_nosan "")
  set(_san_flags_aubsan "-fno-omit-frame-pointer" "-fsanitize=address,undefined")
  set(_san_flags_tsan "-fno-omit-frame-pointer" "-fsanitize=thread")

  # This is a hack. The configured CMake compile options are still present, but
  # this hopefully overrides them by appending overriding flags.
  # TODO: We still need correct linking for non-header-only libraries!
  set(_cfg_names "DEBUG" "RELEASE")

  foreach(_san ${_san_names})
    foreach(_cfg ${_cfg_names})

      string(TOLOWER "${_cfg}" _cfg_name)
      set(_variant "${target}-${_san}-${_cfg_name}")

      add_executable(${_variant} ${_sources})
      target_set_standards(${_variant})

      target_link_libraries(${_variant} PRIVATE ${target})

      separate_arguments(_cfg_flags UNIX_COMMAND "${CMAKE_CXX_FLAGS_${_cfg}}")

      target_compile_options(${_variant} PRIVATE ${_san_flags_${_san}} ${_cfg_flags})
      target_link_options(${_variant} PRIVATE ${_san_flags_${_san}})

      catch_discover_tests(${_variant} TEST_PREFIX "${_variant}: " ADD_TAGS_AS_LABELS)

    endforeach()
  endforeach()

endfunction()

function(target_add_catch_benchmarks target)

  get_target_property(_type ${target} TYPE)

  if (NOT _type STREQUAL "EXECUTABLE")
    message(FATAL_ERROR "target_add_catch_benchmarks: '${target}' must be an executable, got ${_type}")
  endif()

  target_set_standards(${target})

  add_test(
    NAME ${target}
    COMMAND ${target} "[!benchmark]"
      --benchmark-samples 100
      --benchmark-confidence-interval 0.95
  )
  set_tests_properties(${target} PROPERTIES LABELS "benchmark")

endfunction()
