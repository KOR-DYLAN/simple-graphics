# SPDX-License-Identifier: MIT
#
# Copyright (c) 2025 Dylan Hong
#
# This file is released under the MIT License.
# For conditions of distribution and use, see the LICENSE file.

# Add scalar resize entry points and reusable interpolation implementations.
target_sources(${PROJECT_NAME} PRIVATE nearest_neighbor.c)
target_sources(${PROJECT_NAME} PRIVATE bilinear.c)
target_sources(${PROJECT_NAME} PRIVATE bicubic.c)
target_sources(${PROJECT_NAME} PRIVATE generic_resize_nearest_neighbor.c)
target_sources(${PROJECT_NAME} PRIVATE generic_resize_bilinear.c)
target_sources(${PROJECT_NAME} PRIVATE generic_resize_bicubic.c)

if(SGL_CFG_HAS_NEON)
    # Add NEON resize backends only when the target compiler supports NEON.
    target_sources(${PROJECT_NAME} PRIVATE neon_resize_nearest_neighbor.c)
    target_sources(${PROJECT_NAME} PRIVATE neon_resize_bilinear.c)
    target_sources(${PROJECT_NAME} PRIVATE neon_resize_bicubic.c)
endif()
