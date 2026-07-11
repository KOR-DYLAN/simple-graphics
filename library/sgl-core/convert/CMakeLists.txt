# SPDX-License-Identifier: MIT
#
# Copyright (c) 2025 Dylan Hong
#
# This file is released under the MIT License.
# For conditions of distribution and use, see the LICENSE file.

target_sources(${PROJECT_NAME} PRIVATE generic_rgb2rgb.c)
target_sources(${PROJECT_NAME} PRIVATE generic_rgb2yuv.c)
target_sources(${PROJECT_NAME} PRIVATE generic_yuv2rgb.c)
target_sources(${PROJECT_NAME} PRIVATE generic_yuv2yuv.c)
