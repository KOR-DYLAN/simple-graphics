# SPDX-License-Identifier: MIT
#
# Copyright (c) 2025 Dylan Hong
#
# This file is released under the MIT License.
# For conditions of distribution and use, see the LICENSE file.

# Normalize CMake's target processor string into the architecture name used by
# feature detection and binary package filenames.  CMAKE_SYSTEM_PROCESSOR is
# intentionally used instead of CMAKE_HOST_SYSTEM_PROCESSOR so cross-compiled
# packages are labelled with the architecture they run on.
string(TOLOWER "${CMAKE_SYSTEM_PROCESSOR}" ARCH_LOWER)
string(STRIP "${ARCH_LOWER}" ARCH_LOWER)

set(SGL_CFG_IS_ARM FALSE)
set(SGL_CFG_IS_ARM64 FALSE)
set(SGL_CFG_IS_X86 FALSE)
set(SGL_CFG_IS_X86_64 FALSE)
set(SGL_TARGET_ARCH "${ARCH_LOWER}")

if(ARCH_LOWER MATCHES "aarch64" OR ARCH_LOWER MATCHES "arm64")
    set(SGL_CFG_IS_ARM64 TRUE)
    set(SGL_TARGET_ARCH "aarch64")
elseif(ARCH_LOWER MATCHES "^armv7")
    set(SGL_CFG_IS_ARM TRUE)
    set(SGL_TARGET_ARCH "armv7")
elseif(ARCH_LOWER MATCHES "^armv6")
    set(SGL_CFG_IS_ARM TRUE)
    set(SGL_TARGET_ARCH "armv6")
elseif(ARCH_LOWER MATCHES "^arm")
    set(SGL_CFG_IS_ARM TRUE)
    set(SGL_TARGET_ARCH "arm")
elseif(ARCH_LOWER MATCHES "x86_64" OR ARCH_LOWER MATCHES "amd64" OR
       ARCH_LOWER MATCHES "^x64$")
    set(SGL_CFG_IS_X86_64 TRUE)
    set(SGL_TARGET_ARCH "x86_64")
elseif(ARCH_LOWER MATCHES "i[3-6]86" OR ARCH_LOWER MATCHES "^x86$")
    set(SGL_CFG_IS_X86 TRUE)
    set(SGL_TARGET_ARCH "x86")
elseif(ARCH_LOWER MATCHES "riscv64")
    set(SGL_TARGET_ARCH "riscv64")
elseif(ARCH_LOWER MATCHES "ppc64le" OR ARCH_LOWER MATCHES "powerpc64le")
    set(SGL_TARGET_ARCH "ppc64le")
elseif(ARCH_LOWER MATCHES "ppc64" OR ARCH_LOWER MATCHES "powerpc64")
    set(SGL_TARGET_ARCH "ppc64")
endif()

# Keep unknown processor names usable as filename components.  An empty target
# processor cannot identify package compatibility, so make that state explicit.
string(REGEX REPLACE "[^a-z0-9_+-]+" "-" SGL_TARGET_ARCH
    "${SGL_TARGET_ARCH}")
if(SGL_TARGET_ARCH STREQUAL "")
    set(SGL_TARGET_ARCH "unknown")
    message(WARNING
        "CMAKE_SYSTEM_PROCESSOR is empty; package architecture is unknown")
endif()

message(STATUS
    "Detected target architecture: ${SGL_TARGET_ARCH} (${CMAKE_SYSTEM_PROCESSOR})")
