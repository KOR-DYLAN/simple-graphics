# SPDX-License-Identifier: MIT
#
# Copyright (c) 2025 Dylan Hong
#
# This file is released under the MIT License.
# For conditions of distribution and use, see the LICENSE file.

if(NOT WITH_COMPILER_WARNINGS)
    message(STATUS "Compiler warnings: disabled")
    return()
endif()

set(SGL_COMPILER_WARNING_OPTIONS
    -Wall
    -Wextra
)

if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    foreach(WARNING_OPTION IN LISTS SGL_COMPILER_WARNING_OPTIONS)
        add_compile_options(
            "$<$<COMPILE_LANGUAGE:C>:${WARNING_OPTION}>"
            "$<$<COMPILE_LANGUAGE:CXX>:${WARNING_OPTION}>"
        )
    endforeach()
    message(STATUS "Compiler warnings: ${SGL_COMPILER_WARNING_OPTIONS}")
else()
    message(STATUS "Compiler warnings: unavailable for ${CMAKE_C_COMPILER_ID}")
endif()
