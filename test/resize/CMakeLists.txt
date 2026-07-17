# SPDX-License-Identifier: MIT
#
# Copyright (c) 2025 Dylan Hong
#
# This file is released under the MIT License.
# For conditions of distribution and use, see the LICENSE file.

project(resize
    VERSION 1.0
    LANGUAGES C CXX
)

list(APPEND SRC main.c)

set(SGL_TEST_DEFAULT_INPUT "${CMAKE_SOURCE_DIR}/resource/sample.png")
set(SGL_TEST_DEFAULT_OUTPUT_DIR "${CMAKE_SOURCE_DIR}/build/output")

# Resize benchmark executable; runtime arguments can override both paths.
add_executable(${PROJECT_NAME} ${SRC})
target_compile_definitions(${PROJECT_NAME} PRIVATE
    "SGL_TEST_BUILD_DIR=\"${CMAKE_BINARY_DIR}\""
    "SGL_TEST_OUTPUT_DIR=\"${SGL_TEST_DEFAULT_OUTPUT_DIR}\""
)
target_link_libraries(${PROJECT_NAME} PRIVATE test-util sgl-core)

if(TARGET SGLTest::Cairo)
    # Compile Cairo comparison rows only when the optional dependency target exists.
    target_compile_definitions(${PROJECT_NAME} PRIVATE SGL_TEST_HAS_CAIRO)
    target_link_libraries(${PROJECT_NAME} PRIVATE SGLTest::Cairo)
    message(STATUS "resize benchmark cairo comparison: enabled")
else()
    message(STATUS "resize benchmark cairo comparison: disabled")
endif()

if(TARGET SGLTest::NE10)
    # Compile NE10 comparison rows only when the optional dependency target exists.
    target_compile_definitions(${PROJECT_NAME} PRIVATE SGL_TEST_HAS_NE10)
    target_link_libraries(${PROJECT_NAME} PRIVATE SGLTest::NE10)
    message(STATUS "resize benchmark NE10 comparison: enabled")
else()
    message(STATUS "resize benchmark NE10 comparison: disabled")
endif()

# Register the default benchmark smoke run as a CTest entry.
if(SGL_QEMU_AVAILABLE)
    add_test(
        NAME ${PROJECT_NAME}
        COMMAND ${SGL_QEMU_RUNNER}
                $<TARGET_FILE:${PROJECT_NAME}>
                ${SGL_TEST_DEFAULT_INPUT}
                ${SGL_TEST_DEFAULT_OUTPUT_DIR}
    )
else()
    add_test(
        NAME ${PROJECT_NAME}
        COMMAND $<TARGET_FILE:${PROJECT_NAME}>
                ${SGL_TEST_DEFAULT_INPUT}
                ${SGL_TEST_DEFAULT_OUTPUT_DIR}
    )
endif()
