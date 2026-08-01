# SPDX-License-Identifier: MIT
#
# Copyright (c) 2025 Dylan Hong
#
# This file is released under the MIT License.
# For conditions of distribution and use, see the LICENSE file.

project(convert
    VERSION 1.0
    LANGUAGES C
)

set(SGL_TEST_CONVERT_REPEAT_COUNT
    "10"
    CACHE STRING "Convert benchmark repeat count")
if(NOT SGL_TEST_CONVERT_REPEAT_COUNT MATCHES "^[1-9][0-9]*$")
    message(FATAL_ERROR
        "SGL_TEST_CONVERT_REPEAT_COUNT must be a positive integer")
endif()

set(SGL_TEST_CONVERT_WARMUP_COUNT
    "3"
    CACHE STRING "Convert benchmark warm-up count")
if(NOT SGL_TEST_CONVERT_WARMUP_COUNT MATCHES "^[0-9]+$")
    message(FATAL_ERROR
        "SGL_TEST_CONVERT_WARMUP_COUNT must be a non-negative integer")
endif()

set(SGL_TEST_CONVERT_MATRIX_WIDTH
    "1280"
    CACHE STRING "Convert benchmark and full-matrix width")
if(NOT SGL_TEST_CONVERT_MATRIX_WIDTH MATCHES "^([2-9]|[1-9][0-9]+)$")
    message(FATAL_ERROR
        "SGL_TEST_CONVERT_MATRIX_WIDTH must be an integer of at least 2")
endif()

set(SGL_TEST_CONVERT_MATRIX_HEIGHT
    "720"
    CACHE STRING "Convert benchmark and full-matrix height")
if(NOT SGL_TEST_CONVERT_MATRIX_HEIGHT MATCHES "^([2-9]|[1-9][0-9]+)$")
    message(FATAL_ERROR
        "SGL_TEST_CONVERT_MATRIX_HEIGHT must be an integer of at least 2")
endif()

set(SGL_TEST_CONVERT_OUTPUT_DIR "${CMAKE_BINARY_DIR}/convert-output")
set(SGL_TEST_CONVERT_INPUT
    "${CMAKE_SOURCE_DIR}/resource/sample.png")
set(SGL_TEST_CONVERT_BENCHMARK_CSV
    "${CMAKE_BINARY_DIR}/benchmark/convert-benchmark.csv")
file(MAKE_DIRECTORY "${SGL_TEST_CONVERT_OUTPUT_DIR}")
file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/benchmark")

add_executable(${PROJECT_NAME}
    benchmark.c
    main.c
    raw_output.c
    source.c
)
target_link_libraries(${PROJECT_NAME} PRIVATE test-util sgl-core)
target_compile_definitions(${PROJECT_NAME} PRIVATE
    SGL_TEST_CONVERT_OUTPUT_DIR="${SGL_TEST_CONVERT_OUTPUT_DIR}"
    "SGL_TEST_BENCHMARK_CSV=\"${SGL_TEST_CONVERT_BENCHMARK_CSV}\""
    "SGL_TEST_REPEAT_COUNT=${SGL_TEST_CONVERT_REPEAT_COUNT}"
    "SGL_TEST_WARMUP_COUNT=${SGL_TEST_CONVERT_WARMUP_COUNT}"
    "SGL_TEST_CONVERT_MATRIX_WIDTH=${SGL_TEST_CONVERT_MATRIX_WIDTH}"
    "SGL_TEST_CONVERT_MATRIX_HEIGHT=${SGL_TEST_CONVERT_MATRIX_HEIGHT}"
)
set_property(TARGET ${PROJECT_NAME} PROPERTY C_STANDARD 11)

if(WITH_BENCHMARK_COMPARE)
    if(TARGET SGLTest::Cairo)
        target_sources(${PROJECT_NAME} PRIVATE cairo_benchmark.c)
        target_compile_definitions(${PROJECT_NAME} PRIVATE SGL_TEST_HAS_CAIRO)
        target_link_libraries(${PROJECT_NAME} PRIVATE SGLTest::Cairo)
        add_dependencies(${PROJECT_NAME} sgl-test-cairo)
        message(STATUS
            "convert benchmark Cairo packed-copy comparison: enabled")
    else()
        message(FATAL_ERROR
            "Cairo is required by the convert benchmark comparison")
    endif()
else()
    message(STATUS "convert benchmark external comparison: disabled")
endif()

if(SGL_QEMU_AVAILABLE)
    add_test(
        NAME ${PROJECT_NAME}
        COMMAND ${SGL_QEMU_RUNNER}
                $<TARGET_FILE:${PROJECT_NAME}>
                ${SGL_TEST_CONVERT_INPUT}
    )
else()
    add_test(
        NAME ${PROJECT_NAME}
        COMMAND $<TARGET_FILE:${PROJECT_NAME}>
                ${SGL_TEST_CONVERT_INPUT}
    )
endif()

sgl_add_benchmark_report_test(
    ${PROJECT_NAME}
    "${SGL_TEST_CONVERT_BENCHMARK_CSV}"
    "${SGL_TEST_CONVERT_INPUT}"
    "${SGL_TEST_CONVERT_WARMUP_COUNT}"
    "${SGL_TEST_CONVERT_REPEAT_COUNT}"
)
