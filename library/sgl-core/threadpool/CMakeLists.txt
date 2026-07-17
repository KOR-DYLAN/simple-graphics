# SPDX-License-Identifier: MIT
#
# Copyright (c) 2025 Dylan Hong
#
# This file is released under the MIT License.
# For conditions of distribution and use, see the LICENSE file.

if(SGL_CFG_HAS_THREAD)
    # Use the real worker-thread implementation when threading is available.
    target_sources(${PROJECT_NAME} PRIVATE threadpool.c)
else()
    # Keep the API linkable on platforms or builds where threading is disabled.
    target_sources(${PROJECT_NAME} PRIVATE dummy_threadpool.c)
endif()
