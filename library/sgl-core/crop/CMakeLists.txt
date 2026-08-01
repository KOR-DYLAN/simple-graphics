# SPDX-License-Identifier: MIT
#
# Copyright (c) 2025 Dylan Hong
#
# This file is released under the MIT License.
# For conditions of distribution and use, see the LICENSE file.

# Add the shared validation and generic crop implementation.
target_sources(${PROJECT_NAME} PRIVATE
    crop_common.c
    generic_crop.c
)

if(SGL_CFG_HAS_NEON)
    # Add the NEON row-copy backend only when NEON is available.
    target_sources(${PROJECT_NAME} PRIVATE neon_crop.c)
endif()
