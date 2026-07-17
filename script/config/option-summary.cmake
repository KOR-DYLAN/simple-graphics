# SPDX-License-Identifier: MIT
#
# Copyright (c) 2025 Dylan Hong
#
# This file is released under the MIT License.
# For conditions of distribution and use, see the LICENSE file.

# Convert a simple boolean option into the summary text printed at configure time.
function(sgl_set_enabled_status OUTPUT_VAR ENABLED_TEXT DISABLED_TEXT ENABLED)
    if(${ENABLED})
        set(${OUTPUT_VAR} "${ENABLED_TEXT}" PARENT_SCOPE)
    else()
        set(${OUTPUT_VAR} "${DISABLED_TEXT}" PARENT_SCOPE)
    endif()
endfunction()

# Report whether warning flags were actually applied for the active compiler.
function(sgl_set_compiler_warnings_status OUTPUT_VAR)
    set(SGL_STATUS "OFF")

    if(WITH_COMPILER_WARNINGS)
        if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
            set(SGL_STATUS "ON (-Wall, -Wextra)")
        else()
            set(SGL_STATUS
                "OFF (unsupported compiler: ${CMAKE_C_COMPILER_ID})")
        endif()
    endif()

    set(${OUTPUT_VAR} "${SGL_STATUS}" PARENT_SCOPE)
endfunction()

# Report MISRA addon state, including optional rule text metadata.
function(sgl_set_cppcheck_misra_status OUTPUT_VAR)
    set(SGL_STATUS "OFF")

    if(WITH_CPPCHECK AND WITH_CPPCHECK_MISRA)
        set(SGL_STATUS "ON")
        if(CPPCHECK_MISRA_RULE_TEXTS)
            string(APPEND SGL_STATUS
                " (rule texts: ${CPPCHECK_MISRA_RULE_TEXTS})")
        else()
            string(APPEND SGL_STATUS " (without licensed rule texts)")
        endif()
    elseif(WITH_CPPCHECK_MISRA)
        set(SGL_STATUS "OFF (cppcheck is disabled)")
    endif()

    set(${OUTPUT_VAR} "${SGL_STATUS}" PARENT_SCOPE)
endfunction()

# Report the SIMD feature selected by architecture and compiler probes.
function(sgl_set_simd_status OUTPUT_VAR)
    set(SGL_STATUS "OFF")

    if(SGL_CFG_HAS_SIMD)
        set(SGL_STATUS "ON")
        if(SGL_CFG_HAS_NEON)
            string(APPEND SGL_STATUS " (NEON)")
        elseif(SGL_CFG_HAS_AVX2)
            string(APPEND SGL_STATUS " (AVX2)")
        elseif(SGL_CFG_HAS_SSE42)
            string(APPEND SGL_STATUS " (SSE4.2)")
        endif()
    elseif(WITH_SIMD)
        set(SGL_STATUS "OFF (requested, but not detected)")
    endif()

    set(${OUTPUT_VAR} "${SGL_STATUS}" PARENT_SCOPE)
endfunction()

# Report the thread backend selected by platform detection.
function(sgl_set_thread_status OUTPUT_VAR)
    set(SGL_STATUS "OFF")

    if(SGL_CFG_HAS_THREAD)
        set(SGL_STATUS "ON")
        if(SGL_CFG_HAS_PTHREAD)
            set(SGL_STATUS "ON (pthread)")
        elseif(SGL_CFG_HAS_WINTHREAD)
            set(SGL_STATUS "ON (Win32)")
        endif()
    elseif(WITH_THREAD)
        set(SGL_STATUS "OFF (requested, but not detected)")
    endif()

    set(${OUTPUT_VAR} "${SGL_STATUS}" PARENT_SCOPE)
endfunction()

# Report whether cross-built AArch64 test binaries can run through QEMU.
function(sgl_set_qemu_status OUTPUT_VAR)
    set(SGL_STATUS "N/A")

    if(SGL_QEMU_IS_CROSS_BUILD)
        if(SGL_QEMU_AVAILABLE)
            set(SGL_STATUS "ON (${SGL_QEMU_EXECUTABLE})")
        else()
            set(SGL_STATUS "OFF (cross-compile detected, but QEMU not found)")
        endif()
    endif()

    set(${OUTPUT_VAR} "${SGL_STATUS}" PARENT_SCOPE)
endfunction()

# Print the consolidated configure summary after all detection scripts run.
function(sgl_print_configuration_summary)
    # The detection scripts run before this summary, so this function only
    # translates final boolean/config values into one-line user-facing text.
    sgl_set_compiler_warnings_status(COMPILER_WARNINGS_STATUS)
    sgl_set_enabled_status(CPPCHECK_STATUS
        "ON (${CPPCHECK_EXECUTABLE})" "OFF" WITH_CPPCHECK)
    sgl_set_cppcheck_misra_status(CPPCHECK_MISRA_STATUS)
    sgl_set_enabled_status(CPPCHECK_ERRORS_STATUS
        "ON" "OFF" WITH_CPPCHECK_WARNINGS_AS_ERRORS)
    sgl_set_enabled_status(TEST_APP_STATUS "ON" "OFF" WITH_TEST_APP)
    sgl_set_enabled_status(BENCHMARK_COMPARE_STATUS
        "ON" "OFF" WITH_BENCHMARK_COMPARE)
    sgl_set_simd_status(SIMD_STATUS)
    sgl_set_thread_status(THREAD_STATUS)
    sgl_set_qemu_status(QEMU_STATUS)

    if(WITH_CPPCHECK_WARNINGS_AS_ERRORS AND NOT WITH_CPPCHECK)
        set(CPPCHECK_ERRORS_STATUS "OFF (cppcheck is disabled)")
    endif()

    message(STATUS "")
    message(STATUS "simple-graphics configuration summary")
    message(STATUS "  Compiler warnings ............ ${COMPILER_WARNINGS_STATUS}")
    message(STATUS "  cppcheck ..................... ${CPPCHECK_STATUS}")
    message(STATUS "  cppcheck MISRA C:2012 ........ ${CPPCHECK_MISRA_STATUS}")
    message(STATUS "  cppcheck findings as errors .. ${CPPCHECK_ERRORS_STATUS}")
    message(STATUS "  Test applications ............ ${TEST_APP_STATUS}")
    message(STATUS "  Benchmark comparison ......... ${BENCHMARK_COMPARE_STATUS}")
    message(STATUS "  SIMD ......................... ${SIMD_STATUS}")
    message(STATUS "  Threading .................... ${THREAD_STATUS}")
    message(STATUS "  QEMU ARM64 (test runner) ..... ${QEMU_STATUS}")
    message(STATUS "")
endfunction()
