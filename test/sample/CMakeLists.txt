# SPDX-License-Identifier: MIT
#
# Copyright (c) 2025 Dylan Hong
#
# This file is released under the MIT License.
# For conditions of distribution and use, see the LICENSE file.

project(sample
    VERSION 1.0
    LANGUAGES C CXX
)

list(APPEND SRC main.c)

set(SGL_TEST_SAMPLE_INPUT "${CMAKE_SOURCE_DIR}/resource/sample.png")

# Minimal PNG load/save sample application.
add_executable(${PROJECT_NAME} ${SRC})
target_compile_definitions(${PROJECT_NAME} PRIVATE
    "SGL_TEST_BUILD_DIR=\"${CMAKE_BINARY_DIR}\""
)
target_link_libraries(${PROJECT_NAME} PRIVATE test-util sgl-core)

# Register the sample binary as a PNG load/save smoke test.
if(SGL_QEMU_AVAILABLE)
    add_test(
        NAME ${PROJECT_NAME}
        COMMAND ${SGL_QEMU_RUNNER}
                $<TARGET_FILE:${PROJECT_NAME}>
                ${SGL_TEST_SAMPLE_INPUT}
    )
else()
    add_test(
        NAME ${PROJECT_NAME}
        COMMAND $<TARGET_FILE:${PROJECT_NAME}>
                ${SGL_TEST_SAMPLE_INPUT}
    )
endif()
