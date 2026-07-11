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

add_executable(${PROJECT_NAME} ${SRC})
target_link_libraries(${PROJECT_NAME} PRIVATE test-util sgl-core)

if(TARGET SGLTest::Cairo)
    target_compile_definitions(${PROJECT_NAME} PRIVATE SGL_TEST_HAS_CAIRO)
    target_link_libraries(${PROJECT_NAME} PRIVATE SGLTest::Cairo)
    message(STATUS "resize benchmark cairo comparison: enabled")
else()
    message(STATUS "resize benchmark cairo comparison: disabled")
endif()

if(TARGET SGLTest::NE10)
    target_compile_definitions(${PROJECT_NAME} PRIVATE SGL_TEST_HAS_NE10)
    target_link_libraries(${PROJECT_NAME} PRIVATE SGLTest::NE10)
    message(STATUS "resize benchmark NE10 comparison: enabled")
else()
    message(STATUS "resize benchmark NE10 comparison: disabled")
endif()
