# SPDX-License-Identifier: MIT
#
# Copyright (c) 2025 Dylan Hong
#
# This file is released under the MIT License.
# For conditions of distribution and use, see the LICENSE file.

target_sources(${PROJECT_NAME} PRIVATE
    generic_memory.c
    memory.c
    memory_operation.c
)

if(SGL_CFG_HAS_NEON)
    target_sources(${PROJECT_NAME} PRIVATE neon_memory.c)
endif()
