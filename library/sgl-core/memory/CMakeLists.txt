# SPDX-License-Identifier: MIT
#
# Copyright (c) 2025 Dylan Hong
#
# This file is released under the MIT License.
# For conditions of distribution and use, see the LICENSE file.

# Add the portable memory allocator and memory operation implementations.
target_sources(${PROJECT_NAME} PRIVATE
    generic_memory.c
    memory.c
    memory_operation.c
)

if(SGL_CFG_HAS_NEON)
    # Add NEON memory routines only for ARM targets where the probe succeeded.
    target_sources(${PROJECT_NAME} PRIVATE neon_memory.c)
endif()
