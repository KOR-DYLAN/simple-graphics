# SPDX-License-Identifier: MIT
#
# Copyright (c) 2025 Dylan Hong
#
# This file is released under the MIT License.
# For conditions of distribution and use, see the LICENSE file.

# minimum cmake version
cmake_minimum_required(VERSION 3.16)

# project name
project(simple-graphics-library)

include(script/config/sgl-config.cmake)

# Register CTest at the root so tests added in subdirectories are visible to
# `ctest --test-dir build` and the run-qemu aggregate target.
if(WITH_TEST_APP)
    enable_testing()
endif()

# Place executables under build-type-specific bin directories for single-config generators.
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY                  ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG            ${CMAKE_BINARY_DIR}/Debug/bin)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE          ${CMAKE_BINARY_DIR}/Release/bin)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO   ${CMAKE_BINARY_DIR}/RelWithDebInfo/bin)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL       ${CMAKE_BINARY_DIR}/MinSizeRel/bin)

# Place shared libraries next to the selected build type's outputs.
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_DEBUG            ${CMAKE_BINARY_DIR}/Debug/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_RELEASE          ${CMAKE_BINARY_DIR}/Release/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_RELWITHDEBINFO   ${CMAKE_BINARY_DIR}/RelWithDebInfo/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_MINSIZEREL       ${CMAKE_BINARY_DIR}/MinSizeRel/lib)

# Place static libraries next to the selected build type's outputs.
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_DEBUG            ${CMAKE_BINARY_DIR}/Debug/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_RELEASE          ${CMAKE_BINARY_DIR}/Release/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_RELWITHDEBINFO   ${CMAKE_BINARY_DIR}/RelWithDebInfo/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_MINSIZEREL       ${CMAKE_BINARY_DIR}/MinSizeRel/lib)

# Expose public headers and the generated config header to all project targets.
include_directories(${CMAKE_SOURCE_DIR}/include)

# Build the library first, then optional test applications that link against it.
add_subdirectory(library)
if (WITH_TEST_APP)
    add_subdirectory(test)
endif()

# Create a wrapper script for the `run` target so make environment variables
# like RUN_TARGET and ARGS can be passed through cleanly without fragile shell
# quoting in generated make recipes.
set(SGL_RUN_SCRIPT "${CMAKE_BINARY_DIR}/run-test.sh")
if(CMAKE_BUILD_TYPE)
    set(SGL_RUN_BINARY_DIR "${CMAKE_BINARY_DIR}/${CMAKE_BUILD_TYPE}/bin")
else()
    set(SGL_RUN_BINARY_DIR "${CMAKE_BINARY_DIR}/bin")
endif()

if(SGL_QEMU_IS_CROSS_BUILD AND SGL_QEMU_AVAILABLE)
    # Cross-built binaries need QEMU plus the target sysroot for the dynamic loader.
    if(CMAKE_SYSROOT)
        set(SGL_RUN_EXEC "\"${SGL_QEMU_EXECUTABLE}\" -L \"${CMAKE_SYSROOT}\"")
    else()
        set(SGL_RUN_EXEC "\"${SGL_QEMU_EXECUTABLE}\"")
    endif()
else()
    set(SGL_RUN_EXEC "")
endif()

file(WRITE "${SGL_RUN_SCRIPT}"
"#!/bin/sh\n"
"set -e\n"
"if [ -z \"\$RUN_TARGET\" ]; then\n"
"  echo \"RUN_TARGET is required\" >&2\n"
"  exit 1\n"
"fi\n"
"set -- \$ARGS\n"
"exec ${SGL_RUN_EXEC} \"${SGL_RUN_BINARY_DIR}/\$RUN_TARGET\" \"\$@\"\n"
)
execute_process(COMMAND chmod +x "${SGL_RUN_SCRIPT}")

#============================================================================
# QEMU ARM64 Run Target
#============================================================================
if(SGL_QEMU_IS_CROSS_BUILD AND SGL_QEMU_AVAILABLE)
    # Run every registered CTest entry through the QEMU-aware test commands.
    add_custom_target(run-qemu
        COMMENT "Running tests on QEMU ARM64..."
        COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        DEPENDS memory queue resize sample
    )
elseif(SGL_QEMU_IS_CROSS_BUILD AND NOT SGL_QEMU_AVAILABLE)
    # Fail loudly instead of silently trying to execute target binaries on host.
    add_custom_target(run-qemu
        COMMENT "ERROR: QEMU ARM64 not available for cross-compiled tests"
        COMMAND ${CMAKE_COMMAND} -E echo "Install qemu-user-static package to run tests"
        COMMAND false
    )
endif()

#============================================================================
# Test application list target
#============================================================================
add_custom_target(list-test-apps
    COMMENT "List available test applications and usage"
    COMMAND ${CMAKE_COMMAND} -E echo "Available test applications:"
    COMMAND ${CMAKE_COMMAND} -E echo "  memory  - Memory regression test"
    COMMAND ${CMAKE_COMMAND} -E echo "  queue   - Queue regression test"
    COMMAND ${CMAKE_COMMAND} -E echo "  resize  - Resize benchmark test"
    COMMAND ${CMAKE_COMMAND} -E echo "  sample  - Sample application"
    COMMAND ${CMAKE_COMMAND} -E echo ""
    COMMAND ${CMAKE_COMMAND} -E echo "Usage: make run TARGET=name ARGS=..."
    COMMAND ${CMAKE_COMMAND} -E echo "  memory  : no extra args"
    COMMAND ${CMAKE_COMMAND} -E echo "  queue   : no extra args"
    COMMAND ${CMAKE_COMMAND} -E echo "  resize  : ARGS=<input.png> [<output-dir>]"
    COMMAND ${CMAKE_COMMAND} -E echo "  sample  : ARGS=path/to/image.png"
    COMMAND ${CMAKE_COMMAND} -E echo ""
    COMMAND ${CMAKE_COMMAND} -E echo "Example: make run TARGET=resize"
)

#============================================================================
# Generic run target for Makefile usage
#============================================================================
if(SGL_QEMU_IS_CROSS_BUILD AND NOT SGL_QEMU_AVAILABLE)
    # The generic `make run` target also needs QEMU when the binary is not native.
    add_custom_target(run
        COMMENT "ERROR: QEMU ARM64 not available for cross-compiled tests"
        COMMAND ${CMAKE_COMMAND} -E echo "Install qemu-user-static package to run tests"
        COMMAND false
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    )
else()
    # Native builds and QEMU-capable cross builds both go through run-test.sh.
    add_custom_target(run
        COMMAND "${SGL_RUN_SCRIPT}"
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        USES_TERMINAL
    )
endif()

#============================================================================
# LTTng profiling target
#============================================================================
if(SGL_CFG_HAS_LTTNG AND WITH_TEST_APP AND NOT CMAKE_CROSSCOMPILING)
    if(SGL_LTTNG_EXECUTABLE)
        set(SGL_LTTNG_CAPTURE_COMMAND "${SGL_LTTNG_EXECUTABLE}")
    else()
        set(SGL_LTTNG_CAPTURE_COMMAND "lttng")
    endif()
    if(SGL_LTTNG_SESSIOND_EXECUTABLE)
        set(SGL_LTTNG_SESSIOND_COMMAND "${SGL_LTTNG_SESSIOND_EXECUTABLE}")
    else()
        set(SGL_LTTNG_SESSIOND_COMMAND "lttng-sessiond")
    endif()

    add_custom_target(profile-lttng
        COMMAND ${CMAKE_COMMAND} -E env
            "SGL_LTTNG_COMMAND=${SGL_LTTNG_CAPTURE_COMMAND}"
            "SGL_LTTNG_SESSIOND_COMMAND=${SGL_LTTNG_SESSIOND_COMMAND}"
            "SGL_LTTNG_LIBRARY_DIR=${SGL_LTTNG_UST_LIBRARY_DIR}"
            "SGL_LTTNG_REPORT=${CMAKE_SOURCE_DIR}/tools/lttng-resize-report.py"
            "${CMAKE_SOURCE_DIR}/script/profile-lttng.sh"
            "${CMAKE_BINARY_DIR}/profile/lttng"
            "$<TARGET_FILE:resize>"
            "${CMAKE_SOURCE_DIR}/resource/sample.png"
            "${CMAKE_BINARY_DIR}/profile/output"
        COMMENT "Recording SGL resize LTTng-UST trace"
        DEPENDS resize
        USES_TERMINAL
    )
endif()

#============================================================================
# CPack Target
#============================================================================
set(CPACK_GENERATOR
    ZIP
    TGZ
)
set(CPACK_SOURCE_GENERATOR
    ZIP
    TGZ
)
set(CPACK_SOURCE_IGNORE_FILES .git/ _CPack_Packages/ package .vscode .gitignore downloads "${CMAKE_BINARY_DIR}/")

set(CPACK_PACKAGE_NAME "sgl")
set(CPACK_PACKAGE_VERSION ${SGL_FULL_VERSION})
set(CPACK_PACKAGE_DIRECTORY "${CMAKE_SOURCE_DIR}/package")

if("${PROJECT_BINARY_DIR}" STREQUAL "${PROJECT_SOURCE_DIR}")
    message(WARNING "Building to source folder is not recommended. Cpack will be unable to generate source package.")
endif()

# Create package and source-package targets from the metadata above.
include(CPack)
