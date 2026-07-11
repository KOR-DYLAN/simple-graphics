# SPDX-License-Identifier: MIT
#
# Copyright (c) 2025 Dylan Hong
#
# This file is released under the MIT License.
# For conditions of distribution and use, see the LICENSE file.

option(WITH_COMPILER_WARNINGS           "Enable common compiler warnings"                       ON)
option(WITH_CPPCHECK                    "Enable cppcheck static analysis"                       ON)
option(WITH_CPPCHECK_MISRA              "Enable cppcheck MISRA C:2012 addon"                    ON)
option(WITH_CPPCHECK_WARNINGS_AS_ERRORS "Treat cppcheck findings as build errors"               ON)
option(WITH_TEST_APP                    "Enable Test Application Feature"                       ON)
option(WITH_BENCHMARK_COMPARE           "Enable optional resize benchmark comparison backends"  OFF)
option(WITH_SIMD                        "Enable SIMD Feature"                                   ON)
option(WITH_THREAD                      "Enable Thread Feature"                                 ON)
set(CPPCHECK_MISRA_RULE_TEXTS           "" CACHE FILEPATH "Optional path to the licensed MISRA C rule headlines file")
