# SPDX-License-Identifier: MIT
#
# Copyright (c) 2025 Dylan Hong
#
# This file is released under the MIT License.
# For conditions of distribution and use, see the LICENSE file.

project(crop
    VERSION 1.0
    LANGUAGES C
)

set(SGL_TEST_CROP_REPEAT_COUNT
    "10"
    CACHE STRING "Crop benchmark repeat count")
if(NOT SGL_TEST_CROP_REPEAT_COUNT MATCHES "^[1-9][0-9]*$")
    message(FATAL_ERROR
        "SGL_TEST_CROP_REPEAT_COUNT must be a positive integer")
endif()

set(SGL_TEST_CROP_WARMUP_COUNT
    "3"
    CACHE STRING "Crop benchmark warm-up count")
if(NOT SGL_TEST_CROP_WARMUP_COUNT MATCHES "^[0-9]+$")
    message(FATAL_ERROR
        "SGL_TEST_CROP_WARMUP_COUNT must be a non-negative integer")
endif()

set(SGL_TEST_CROP_INPUT "${CMAKE_SOURCE_DIR}/resource/sample.png")
set(SGL_TEST_CROP_BENCHMARK_CSV
    "${CMAKE_BINARY_DIR}/benchmark/crop-benchmark.csv")
set(SGL_TEST_CROP_OUTPUT_DIR "${CMAKE_BINARY_DIR}/crop-output")
file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/benchmark")
file(MAKE_DIRECTORY "${SGL_TEST_CROP_OUTPUT_DIR}")

add_executable(${PROJECT_NAME}
    benchmark.c
    main.c
)
target_compile_definitions(${PROJECT_NAME} PRIVATE
    "SGL_TEST_BENCHMARK_CSV=\"${SGL_TEST_CROP_BENCHMARK_CSV}\""
    "SGL_TEST_OUTPUT_DIR=\"${SGL_TEST_CROP_OUTPUT_DIR}\""
    "SGL_TEST_REPEAT_COUNT=${SGL_TEST_CROP_REPEAT_COUNT}"
    "SGL_TEST_WARMUP_COUNT=${SGL_TEST_CROP_WARMUP_COUNT}"
)
target_link_libraries(${PROJECT_NAME} PRIVATE test-util sgl-core)
set_property(TARGET ${PROJECT_NAME} PROPERTY C_STANDARD 11)

if(WITH_BENCHMARK_COMPARE)
    if(TARGET SGLTest::Cairo)
        target_sources(${PROJECT_NAME} PRIVATE cairo_benchmark.c)
        target_compile_definitions(${PROJECT_NAME} PRIVATE SGL_TEST_HAS_CAIRO)
        target_link_libraries(${PROJECT_NAME} PRIVATE SGLTest::Cairo)
        add_dependencies(${PROJECT_NAME} sgl-test-cairo)
        message(STATUS "crop benchmark Cairo comparison: enabled")
    else()
        message(FATAL_ERROR
            "Cairo is required by the crop benchmark comparison")
    endif()
else()
    message(STATUS "crop benchmark external comparison: disabled")
endif()

if(SGL_QEMU_AVAILABLE)
    add_test(
        NAME ${PROJECT_NAME}
        COMMAND ${SGL_QEMU_RUNNER}
                $<TARGET_FILE:${PROJECT_NAME}>
                ${SGL_TEST_CROP_INPUT}
    )
else()
    add_test(
        NAME ${PROJECT_NAME}
        COMMAND $<TARGET_FILE:${PROJECT_NAME}>
                ${SGL_TEST_CROP_INPUT}
    )
endif()

sgl_add_benchmark_report_test(
    ${PROJECT_NAME}
    "${SGL_TEST_CROP_BENCHMARK_CSV}"
    "${SGL_TEST_CROP_INPUT}"
    "${SGL_TEST_CROP_WARMUP_COUNT}"
    "${SGL_TEST_CROP_REPEAT_COUNT}"
)
