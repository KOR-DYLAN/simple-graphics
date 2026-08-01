# SPDX-License-Identifier: MIT
#
# Copyright (c) 2025 Dylan Hong
#
# This file is released under the MIT License.
# For conditions of distribution and use, see the LICENSE file.

set(SGL_AUTO_VECTORIZATION_STATUS "ON (compiler default)")
set(SGL_AUTO_VECTORIZATION_DISABLE_OPTIONS)

if(NOT WITH_AUTO_VECTORIZATION)
    if(CMAKE_C_COMPILER_ID STREQUAL "GNU")
        # GCC's umbrella switch disables both loop and SLP vectorization.
        list(APPEND SGL_AUTO_VECTORIZATION_DISABLE_OPTIONS
            -fno-tree-vectorize)
    elseif(CMAKE_C_COMPILER_ID MATCHES "Clang")
        # Clang controls loop and SLP vectorization with separate switches.
        list(APPEND SGL_AUTO_VECTORIZATION_DISABLE_OPTIONS
            -fno-vectorize
            -fno-slp-vectorize)
    else()
        message(FATAL_ERROR
            "WITH_AUTO_VECTORIZATION=OFF is unsupported for "
            "${CMAKE_C_COMPILER_ID}")
    endif()

    foreach(SGL_OPTION IN LISTS SGL_AUTO_VECTORIZATION_DISABLE_OPTIONS)
        add_compile_options(
            "$<$<COMPILE_LANGUAGE:C>:${SGL_OPTION}>"
            "$<$<COMPILE_LANGUAGE:CXX>:${SGL_OPTION}>"
        )
    endforeach()

    string(JOIN ", " SGL_AUTO_VECTORIZATION_FLAGS
        ${SGL_AUTO_VECTORIZATION_DISABLE_OPTIONS})
    set(SGL_AUTO_VECTORIZATION_STATUS
        "OFF (${SGL_AUTO_VECTORIZATION_FLAGS})")
endif()
