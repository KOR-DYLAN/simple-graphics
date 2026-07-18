# SPDX-License-Identifier: MIT
#
# Copyright (c) 2025 Dylan Hong
#
# This file is released under the MIT License.
# For conditions of distribution and use, see the LICENSE file.

# Build hygiene toggles exposed as CMake cache options.
option(WITH_COMPILER_WARNINGS           "Enable common compiler warnings"                       ON)
option(WITH_CPPCHECK                    "Enable cppcheck static analysis"                       ON)
option(WITH_CPPCHECK_MISRA              "Enable cppcheck MISRA C:2012 addon"                    ON)
option(WITH_CPPCHECK_WARNINGS_AS_ERRORS "Treat cppcheck findings as build errors"               ON)
option(WITH_SANITIZER                   "Enable compiler sanitizer instrumentation"             OFF)
option(WITH_STACK_PROTECTOR             "Enable compiler stack protector instrumentation"       OFF)
option(WITH_LTTNG                       "Enable Linux LTTng-UST profiling instrumentation"        OFF)

# Feature toggles that decide which project targets and optimized code paths are built.
option(WITH_TEST_APP                    "Enable Test Application Feature"                       ON)
option(WITH_BENCHMARK_COMPARE           "Enable required Cairo and NE10 resize benchmark backends" ON)
option(WITH_SIMD                        "Enable SIMD Feature"                                   ON)
option(WITH_THREAD                      "Enable Thread Feature"                                 ON)
set(CPPCHECK_MISRA_RULE_TEXTS           "" CACHE FILEPATH "Optional path to the licensed MISRA C rule headlines file")
set(SANITIZERS                          "address,undefined" CACHE STRING "Comma-separated sanitizer list, for example: address,undefined")
set(STACK_PROTECTOR_MODE                "strong" CACHE STRING "Stack protector mode: basic, strong, or all")
set(LTTNG_UST_ROOT                      "" CACHE PATH "Optional LTTng-UST installation prefix")
set_property(CACHE STACK_PROTECTOR_MODE PROPERTY STRINGS basic strong all)

# Keep local installs under the build tree unless the user provides a prefix.
if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
    set(CMAKE_INSTALL_PREFIX "${CMAKE_BINARY_DIR}/install" CACHE PATH "Install prefix" FORCE)
endif()
