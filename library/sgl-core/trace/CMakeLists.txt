# SPDX-License-Identifier: MIT
#
# Copyright (c) 2025 Dylan Hong
#
# This file is released under the MIT License.
# For conditions of distribution and use, see the LICENSE file.

# Every core module sees the zero-cost trace wrappers.  The provider source and
# LTTng dependency are added only to explicitly instrumented Linux builds.
target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

if(SGL_CFG_HAS_LTTNG)
    target_sources(${PROJECT_NAME} PRIVATE
        sgl_lttng_tracepoints.c
    )
    target_include_directories(${PROJECT_NAME} PRIVATE
        ${SGL_LTTNG_UST_INCLUDE_DIR}
    )
    target_link_libraries(${PROJECT_NAME} PRIVATE
        ${SGL_LTTNG_UST_LIBRARY}
        ${CMAKE_DL_LIBS}
    )
    if(CMAKE_C_COMPILER_ID MATCHES "Clang")
        set_source_files_properties(
            "${CMAKE_CURRENT_SOURCE_DIR}/sgl_lttng_tracepoints.c"
            TARGET_DIRECTORY ${PROJECT_NAME}
            PROPERTIES
            COMPILE_OPTIONS "-Wno-gnu-folding-constant")
    endif()
endif()
