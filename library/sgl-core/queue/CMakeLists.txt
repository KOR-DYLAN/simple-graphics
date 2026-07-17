# SPDX-License-Identifier: MIT
#
# Copyright (c) 2025 Dylan Hong
#
# This file is released under the MIT License.
# For conditions of distribution and use, see the LICENSE file.

# Add queue support used by the threadpool and queue regression test.
target_sources(${PROJECT_NAME} PRIVATE queue.c)
