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

# Keep the normal benchmark repeat stable, while allowing runtime-check builds
# to lower it and still execute the full channel/method/thread matrix.
set(SGL_TEST_RESIZE_REPEAT_COUNT
    "10"
    CACHE STRING "Resize benchmark repeat count used by the resize test app")
if(NOT SGL_TEST_RESIZE_REPEAT_COUNT MATCHES "^[1-9][0-9]*$")
    message(FATAL_ERROR
        "SGL_TEST_RESIZE_REPEAT_COUNT must be a positive integer")
endif()

# Warm-up calls initialize dependency state, caches, and worker scheduling but
# are deliberately excluded from the reported latency samples.
set(SGL_TEST_RESIZE_WARMUP_COUNT
    "3"
    CACHE STRING "Resize benchmark warm-up count used by the resize test app")
if(NOT SGL_TEST_RESIZE_WARMUP_COUNT MATCHES "^[0-9]+$")
    message(FATAL_ERROR
        "SGL_TEST_RESIZE_WARMUP_COUNT must be a non-negative integer")
endif()

set(SGL_TEST_DEFAULT_INPUT "${CMAKE_SOURCE_DIR}/resource/sample.png")
set(SGL_TEST_DEFAULT_OUTPUT_DIR "${CMAKE_SOURCE_DIR}/build/output")

# Resize benchmark executable; runtime arguments can override both paths.
add_executable(${PROJECT_NAME} ${SRC})
target_compile_definitions(${PROJECT_NAME} PRIVATE
    "SGL_TEST_BUILD_DIR=\"${CMAKE_BINARY_DIR}\""
    "SGL_TEST_OUTPUT_DIR=\"${SGL_TEST_DEFAULT_OUTPUT_DIR}\""
    "SGL_TEST_REPEAT_COUNT=${SGL_TEST_RESIZE_REPEAT_COUNT}"
    "SGL_TEST_WARMUP_COUNT=${SGL_TEST_RESIZE_WARMUP_COUNT}"
)
target_link_libraries(${PROJECT_NAME} PRIVATE test-util sgl-core)

if(NOT WITH_BENCHMARK_COMPARE)
    message(FATAL_ERROR
        "The resize benchmark requires Cairo and NE10 comparison rows. "
        "Configure with WITH_BENCHMARK_COMPARE=ON, or disable test apps with "
        "WITH_TEST_APP=OFF.")
endif()

if(TARGET SGLTest::Cairo)
    # Every published resize benchmark includes the Cairo comparison rows.
    target_compile_definitions(${PROJECT_NAME} PRIVATE SGL_TEST_HAS_CAIRO)
    target_link_libraries(${PROJECT_NAME} PRIVATE SGLTest::Cairo)
    add_dependencies(${PROJECT_NAME} sgl-test-cairo)
    message(STATUS "resize benchmark cairo comparison: enabled")
else()
    message(FATAL_ERROR "Cairo is required by the resize benchmark")
endif()

if(TARGET SGLTest::NE10)
    # Every published resize benchmark includes the NE10 C and NEON rows.
    target_compile_definitions(${PROJECT_NAME} PRIVATE SGL_TEST_HAS_NE10)
    target_link_libraries(${PROJECT_NAME} PRIVATE SGLTest::NE10)
    add_dependencies(${PROJECT_NAME} sgl-test-ne10)
    message(STATUS "resize benchmark NE10 comparison: enabled")
else()
    message(FATAL_ERROR "NE10 is required by the resize benchmark")
endif()

find_package(Python3 COMPONENTS Interpreter REQUIRED)

set(SGL_TEST_BENCHMARK_CSV
    "${CMAKE_BINARY_DIR}/benchmark/resize-benchmark.csv")
set(SGL_TEST_BENCHMARK_SNAPSHOT_DIR
    "${CMAKE_SOURCE_DIR}/benchmark/resize")
set(SGL_TEST_BENCHMARK_SNAPSHOT_CSV
    "${SGL_TEST_BENCHMARK_SNAPSHOT_DIR}/benchmark.csv")

if(SGL_QEMU_IS_CROSS_BUILD AND NOT SGL_QEMU_AVAILABLE)
    add_custom_target(benchmark-update
        COMMAND ${CMAKE_COMMAND} -E echo
                "Cannot update benchmark snapshots: QEMU ARM64 is unavailable"
        COMMAND ${CMAKE_COMMAND} -E false
        USES_TERMINAL
    )
else()
    set(SGL_TEST_BENCHMARK_RUNNER)
    if(SGL_QEMU_IS_CROSS_BUILD)
        set(SGL_TEST_BENCHMARK_RUNNER ${SGL_QEMU_RUNNER})
    endif()

    # Run from the build tree because the benchmark writes its CSV relative to
    # the process working directory, then publish the validated repo snapshot.
    add_custom_target(benchmark-update
        COMMAND ${CMAKE_COMMAND} -E make_directory
                "${SGL_TEST_BENCHMARK_SNAPSHOT_DIR}"
        COMMAND ${CMAKE_COMMAND} -E chdir "${CMAKE_BINARY_DIR}"
                ${SGL_TEST_BENCHMARK_RUNNER}
                $<TARGET_FILE:${PROJECT_NAME}>
                "${SGL_TEST_DEFAULT_INPUT}"
                "${SGL_TEST_DEFAULT_OUTPUT_DIR}"
        COMMAND "${Python3_EXECUTABLE}"
                "${CMAKE_SOURCE_DIR}/tools/resize-benchmark-summary-svg.py"
                --csv "${SGL_TEST_BENCHMARK_CSV}"
                --validate-only
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${SGL_TEST_BENCHMARK_CSV}"
                "${SGL_TEST_BENCHMARK_SNAPSHOT_CSV}"
        COMMAND "${Python3_EXECUTABLE}"
                "${CMAKE_SOURCE_DIR}/tools/resize-benchmark-summary-svg.py"
                --csv "benchmark/resize/benchmark.csv"
                --output-dir "benchmark/resize"
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
        DEPENDS ${PROJECT_NAME}
        COMMENT "Updating representative and channel-specific benchmark SVGs"
        USES_TERMINAL
        VERBATIM
    )
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
