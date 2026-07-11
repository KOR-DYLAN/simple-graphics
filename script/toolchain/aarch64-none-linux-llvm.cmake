# SPDX-License-Identifier: MIT
#
# Copyright (c) 2025 Dylan Hong
#
# This file is released under the MIT License.
# For conditions of distribution and use, see the LICENSE file.

# Cross-compile for Linux/aarch64 with the host Clang driver.
#
# The GNU triple names the target ABI used by the prebuilt sysroot and binary
# utilities.  The trailing dash is convenient for prefixed tools such as
# aarch64-none-linux-gnu-objcopy; TARGET_TRIPLE removes it for Clang's
# --target=<triple> option.
set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR aarch64)

set(TRIPLE "aarch64-none-linux-gnu-")
string(REGEX REPLACE "-$" "" TARGET_TRIPLE "${TRIPLE}")

include(${CMAKE_CURRENT_LIST_DIR}/llvm.cmake)
