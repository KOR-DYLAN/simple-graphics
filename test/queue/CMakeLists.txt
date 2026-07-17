# SPDX-License-Identifier: MIT
#
# Copyright (c) 2025 Dylan Hong
#
# This file is released under the MIT License.
# For conditions of distribution and use, see the LICENSE file.

project(queue
    VERSION 1.0
    LANGUAGES C CXX
)

list(APPEND SRC main.c)

# Queue regression executable linked against the shared test utilities and core library.
add_executable(${PROJECT_NAME} ${SRC})
target_link_libraries(${PROJECT_NAME} PRIVATE test-util sgl-core)

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
