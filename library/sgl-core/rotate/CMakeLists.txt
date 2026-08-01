# SPDX-License-Identifier: MIT
#
# Copyright (c) 2025 Dylan Hong
#
# This file is released under the MIT License.
# For conditions of distribution and use, see the LICENSE file.

# Add shared fixed-point rotation math and the generic implementation.
target_sources(${PROJECT_NAME} PRIVATE
    rotate_common.c
    generic_rotate.c
)

if(SGL_CFG_HAS_NEON)
    # Add the NEON backend only when the target compiler supports NEON.
    target_sources(${PROJECT_NAME} PRIVATE neon_rotate.c)
endif()
