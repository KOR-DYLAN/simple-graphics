# SPDX-License-Identifier: MIT
#
# Copyright (c) 2025 Dylan Hong
#
# This file is released under the MIT License.
# For conditions of distribution and use, see the LICENSE file.

set(SGL_CFG_HAS_LTTNG OFF)
set(SGL_LTTNG_STATUS "OFF")

if(WITH_LTTNG)
    if(NOT CMAKE_SYSTEM_NAME STREQUAL "Linux")
        message(FATAL_ERROR
            "WITH_LTTNG is supported only for Linux targets")
    endif()

    set(SGL_LTTNG_PREFIX_HINTS "${LTTNG_UST_ROOT}")
    if(LTTNG_UST_ROOT)
        list(APPEND SGL_LTTNG_PREFIX_HINTS "${LTTNG_UST_ROOT}/usr")
    endif()

    find_path(SGL_LTTNG_UST_INCLUDE_DIR
        NAMES lttng/tracepoint.h
        HINTS ${SGL_LTTNG_PREFIX_HINTS}
        PATH_SUFFIXES
            include
            include/${CMAKE_LIBRARY_ARCHITECTURE}
    )
    find_library(SGL_LTTNG_UST_LIBRARY
        NAMES lttng-ust
        HINTS ${SGL_LTTNG_PREFIX_HINTS}
        PATH_SUFFIXES
            lib
            lib64
            lib/${CMAKE_LIBRARY_ARCHITECTURE}
    )
    find_program(SGL_LTTNG_EXECUTABLE
        NAMES lttng
        HINTS ${SGL_LTTNG_PREFIX_HINTS}
        PATH_SUFFIXES bin
    )
    find_program(SGL_LTTNG_SESSIOND_EXECUTABLE
        NAMES lttng-sessiond
        HINTS ${SGL_LTTNG_PREFIX_HINTS}
        PATH_SUFFIXES bin sbin
    )
    find_program(SGL_BABELTRACE2_EXECUTABLE
        NAMES babeltrace2
        HINTS ${SGL_LTTNG_PREFIX_HINTS}
        PATH_SUFFIXES bin
    )

    if(NOT SGL_LTTNG_UST_INCLUDE_DIR OR NOT SGL_LTTNG_UST_LIBRARY)
        message(FATAL_ERROR
            "WITH_LTTNG=ON requires LTTng-UST development files. "
            "On Ubuntu install: sudo apt install lttng-tools "
            "liblttng-ust-dev babeltrace2")
    endif()

    set(SGL_CFG_HAS_LTTNG ON)
    get_filename_component(SGL_LTTNG_UST_LIBRARY_DIR
        "${SGL_LTTNG_UST_LIBRARY}" DIRECTORY)
    set(SGL_LTTNG_STATUS
        "ON (${SGL_LTTNG_UST_LIBRARY})")
    if(NOT SGL_LTTNG_EXECUTABLE)
        string(APPEND SGL_LTTNG_STATUS ", capture CLI unavailable")
    endif()
    if(NOT SGL_LTTNG_SESSIOND_EXECUTABLE)
        string(APPEND SGL_LTTNG_STATUS ", session daemon unavailable")
    endif()

    unset(SGL_LTTNG_PREFIX_HINTS)
endif()
