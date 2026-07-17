# SPDX-License-Identifier: MIT
#
# Copyright (c) 2025 Dylan Hong
#
# This file is released under the MIT License.
# For conditions of distribution and use, see the LICENSE file.

project(memory
    VERSION 1.0
    LANGUAGES C
)

# Standalone memory allocator regression and stress-test executable.
add_executable(${PROJECT_NAME} main.c)
target_link_libraries(${PROJECT_NAME} PRIVATE sgl-core)
set_property(TARGET ${PROJECT_NAME} PROPERTY C_STANDARD 11)

# Register the executable as a CTest entry, using QEMU for cross-built binaries.
if(SGL_QEMU_AVAILABLE)
    add_test(
        NAME ${PROJECT_NAME}
        COMMAND ${SGL_QEMU_RUNNER} $<TARGET_FILE:${PROJECT_NAME}>
    )
else()
    add_test(
        NAME ${PROJECT_NAME}
        COMMAND $<TARGET_FILE:${PROJECT_NAME}>
    )
endif()
