# SPDX-License-Identifier: MIT
#
# Copyright (c) 2025 Dylan Hong
#
# This file is released under the MIT License.
# For conditions of distribution and use, see the LICENSE file.

# Add generic color-space conversion implementations to the core library.
target_sources(${PROJECT_NAME} PRIVATE
    generic_common.c
    generic_rgb2rgb.c
    generic_rgb2yuv.c
    generic_yuv2rgb.c
    generic_yuv2yuv.c
)

if(SGL_CFG_HAS_NEON)
    # Add real NEON color kernels only when the target compiler supports NEON.
    target_sources(${PROJECT_NAME} PRIVATE
        neon_common.c
        neon_rgb2rgb.c
        neon_rgb2yuv.c
        neon_yuv2rgb.c
        neon_yuv2yuv.c
    )
endif()
