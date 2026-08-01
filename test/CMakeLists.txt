# SPDX-License-Identifier: MIT
#
# Copyright (c) 2025 Dylan Hong
#
# This file is released under the MIT License.
# For conditions of distribution and use, see the LICENSE file.

include(${CMAKE_SOURCE_DIR}/script/config/test-dependencies.cmake)

# Keep direct configuration of the test subtree usable, even though the root
# project also enables testing for normal builds.
enable_testing()

include_directories(util)

find_package(Python3 3.10 COMPONENTS Interpreter REQUIRED)
find_program(SGL_GNUPLOT_EXECUTABLE NAMES gnuplot)
if(NOT SGL_GNUPLOT_EXECUTABLE)
    message(FATAL_ERROR
        "gnuplot is required for benchmark reports. "
        "Ubuntu/WSL: sudo apt install gnuplot-nox")
endif()

set(SGL_BENCHMARK_REPORT_SCRIPT
    "${CMAKE_SOURCE_DIR}/tools/benchmark-report.py")
set(SGL_BENCHMARK_METADATA_FILE
    "${CMAKE_BINARY_DIR}/benchmark/build-metadata.txt")
file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/benchmark")

get_filename_component(SGL_BENCHMARK_TOOLCHAIN
    "${CMAKE_TOOLCHAIN_FILE}" NAME_WE)
string(TOUPPER "${CMAKE_BUILD_TYPE}" SGL_BENCHMARK_BUILD_TYPE_UPPER)
set(SGL_BENCHMARK_EFFECTIVE_C_FLAGS
    "${CMAKE_C_FLAGS} ${CMAKE_C_FLAGS_${SGL_BENCHMARK_BUILD_TYPE_UPPER}}")
string(STRIP "${SGL_BENCHMARK_EFFECTIVE_C_FLAGS}"
    SGL_BENCHMARK_EFFECTIVE_C_FLAGS)

set(SGL_BENCHMARK_COMPILER_WARNINGS "OFF")
if(WITH_COMPILER_WARNINGS)
    set(SGL_BENCHMARK_COMPILER_WARNINGS "ON (-Wall, -Wextra)")
endif()

set(SGL_BENCHMARK_SIMD "OFF")
if(SGL_CFG_HAS_NEON)
    set(SGL_BENCHMARK_SIMD "ON (NEON)")
elseif(SGL_CFG_HAS_AVX2)
    set(SGL_BENCHMARK_SIMD "ON (AVX2)")
elseif(SGL_CFG_HAS_SSE42)
    set(SGL_BENCHMARK_SIMD "ON (SSE4.2)")
endif()

set(SGL_BENCHMARK_THREADING "OFF")
if(SGL_CFG_HAS_PTHREAD)
    set(SGL_BENCHMARK_THREADING "ON (pthread)")
elseif(SGL_CFG_HAS_WINTHREAD)
    set(SGL_BENCHMARK_THREADING "ON (Win32)")
endif()

set(SGL_BENCHMARK_COMPARE "OFF")
if(WITH_BENCHMARK_COMPARE)
    set(SGL_BENCHMARK_COMPARE "ON (Cairo/NE10 where supported)")
endif()

set(SGL_BENCHMARK_EXECUTION "native")
if(SGL_QEMU_IS_CROSS_BUILD)
    set(SGL_BENCHMARK_EXECUTION "QEMU (${SGL_QEMU_EXECUTABLE})")
endif()

configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/benchmark-build-metadata.txt.in"
    "${SGL_BENCHMARK_METADATA_FILE}"
    @ONLY
)

function(sgl_add_benchmark_report_test
         application benchmark_csv input_path warmup_count repeat_count)
    set(report_args)
    if(SGL_CFG_HAS_SIMD)
        list(APPEND report_args --require-simd)
    endif()
    if(WITH_BENCHMARK_COMPARE AND
       (application STREQUAL "resize" OR
        application STREQUAL "crop" OR
        application STREQUAL "convert" OR
        application STREQUAL "rotate"))
        if(TARGET SGLTest::Cairo)
            list(APPEND report_args --require-cairo)
        endif()
    endif()
    if(application STREQUAL "resize" AND WITH_BENCHMARK_COMPARE)
        if(TARGET SGLTest::NE10)
            list(APPEND report_args --require-ne10)
        endif()
    endif()
    if(application STREQUAL "rotate" AND WITH_BENCHMARK_COMPARE AND
       TARGET SGLTest::NE10)
        list(APPEND report_args --require-ne10)
    endif()

    add_test(
        NAME ${application}-report
        COMMAND "${Python3_EXECUTABLE}"
                "${SGL_BENCHMARK_REPORT_SCRIPT}"
                --benchmark "${application}"
                --csv "${benchmark_csv}"
                --output-dir
                    "${CMAKE_BINARY_DIR}/benchmark/${application}"
                --metadata "${SGL_BENCHMARK_METADATA_FILE}"
                --compile-commands
                    "${CMAKE_BINARY_DIR}/compile_commands.json"
                --source-root "${CMAKE_SOURCE_DIR}"
                --input "${input_path}"
                --warmup "${warmup_count}"
                --repeat "${repeat_count}"
                --gnuplot "${SGL_GNUPLOT_EXECUTABLE}"
                ${report_args}
    )
    set_tests_properties(${application}-report PROPERTIES
        DEPENDS ${application}
        FIXTURES_REQUIRED "${application}-benchmark-data"
        LABELS "benchmark-report"
    )
    set_tests_properties(${application} PROPERTIES
        FIXTURES_SETUP "${application}-benchmark-data"
    )
endfunction()

# Build user-facing sample/benchmark/regression applications.
add_subdirectory(sample)
add_subdirectory(resize)
add_subdirectory(queue)
add_subdirectory(memory)
add_subdirectory(crop)
add_subdirectory(convert)
add_subdirectory(rotate)
if(SGL_CFG_HAS_PTHREAD)
    add_subdirectory(threadpool)
endif()

# Build the shared PNG helper library after dependency targets are declared.
add_subdirectory(util)

function(sgl_add_operation_benchmark_target
         target_name application input_path snapshot_dir
         warmup_count repeat_count)
    set(benchmark_csv
        "${CMAKE_BINARY_DIR}/benchmark/${application}-benchmark.csv")
    set(snapshot_csv "${snapshot_dir}/benchmark.csv")
    set(report_args)
    if(SGL_CFG_HAS_SIMD)
        list(APPEND report_args --require-simd)
    endif()
    if(WITH_BENCHMARK_COMPARE AND TARGET SGLTest::Cairo)
        list(APPEND report_args --require-cairo)
    endif()
    if(application STREQUAL "rotate" AND WITH_BENCHMARK_COMPARE AND
       TARGET SGLTest::NE10)
        list(APPEND report_args --require-ne10)
    endif()

    if(SGL_QEMU_IS_CROSS_BUILD AND NOT SGL_QEMU_AVAILABLE)
        add_custom_target(${target_name}
            COMMAND ${CMAKE_COMMAND} -E echo
                    "Cannot update ${application} benchmark: QEMU is unavailable"
            COMMAND ${CMAKE_COMMAND} -E false
            USES_TERMINAL
        )
    else()
        set(benchmark_runner)
        if(SGL_QEMU_IS_CROSS_BUILD)
            set(benchmark_runner ${SGL_QEMU_RUNNER})
        endif()

        add_custom_target(${target_name}
            COMMAND ${CMAKE_COMMAND} -E make_directory "${snapshot_dir}"
            COMMAND ${CMAKE_COMMAND} -E chdir "${CMAKE_BINARY_DIR}"
                    ${benchmark_runner}
                    $<TARGET_FILE:${application}>
                    "${input_path}"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "${benchmark_csv}"
                    "${snapshot_csv}"
            COMMAND "${Python3_EXECUTABLE}"
                    "${SGL_BENCHMARK_REPORT_SCRIPT}"
                    --benchmark "${application}"
                    --csv "${snapshot_csv}"
                    --output-dir "${snapshot_dir}"
                    --metadata "${SGL_BENCHMARK_METADATA_FILE}"
                    --compile-commands
                        "${CMAKE_BINARY_DIR}/compile_commands.json"
                    --source-root "${CMAKE_SOURCE_DIR}"
                    --input "${input_path}"
                    --warmup "${warmup_count}"
                    --repeat "${repeat_count}"
                    --gnuplot "${SGL_GNUPLOT_EXECUTABLE}"
                    ${report_args}
            DEPENDS ${application}
            COMMENT "Updating ${application} benchmark report"
            USES_TERMINAL
            VERBATIM
        )
    endif()
endfunction()

sgl_add_operation_benchmark_target(
    benchmark-crop-update
    crop
    "${CMAKE_SOURCE_DIR}/resource/sample.png"
    "${CMAKE_SOURCE_DIR}/benchmark/crop"
    "${SGL_TEST_CROP_WARMUP_COUNT}"
    "${SGL_TEST_CROP_REPEAT_COUNT}"
)
sgl_add_operation_benchmark_target(
    benchmark-convert-update
    convert
    "${CMAKE_SOURCE_DIR}/resource/sample.png"
    "${CMAKE_SOURCE_DIR}/benchmark/convert"
    "${SGL_TEST_CONVERT_WARMUP_COUNT}"
    "${SGL_TEST_CONVERT_REPEAT_COUNT}"
)
sgl_add_operation_benchmark_target(
    benchmark-rotate-update
    rotate
    "${CMAKE_SOURCE_DIR}/resource/sample.png"
    "${CMAKE_SOURCE_DIR}/benchmark/rotate"
    "${SGL_TEST_ROTATE_WARMUP_COUNT}"
    "${SGL_TEST_ROTATE_REPEAT_COUNT}"
)

add_dependencies(
    benchmark-convert-update
    benchmark-crop-update
)
add_dependencies(
    benchmark-rotate-update
    benchmark-convert-update
)

if(TARGET benchmark-update)
    add_dependencies(
        benchmark-update
        benchmark-rotate-update
    )
endif()

# Validate the non-interactive configuration backend used by the curses menus.
add_test(
    NAME menuconfig
    COMMAND ${Python3_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/menuconfig/test_menuconfig.py
)

add_test(
    NAME benchmark-report-tool
    COMMAND ${Python3_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/benchmark-report/test_benchmark_report.py
            --gnuplot ${SGL_GNUPLOT_EXECUTABLE}
)
set_tests_properties(benchmark-report-tool PROPERTIES
    LABELS "benchmark-report"
)
