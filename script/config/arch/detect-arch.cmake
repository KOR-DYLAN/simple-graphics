# ---------------------------------------------------------------------
# 1. Detect CPU architecture
# ---------------------------------------------------------------------
string(TOLOWER "${CMAKE_SYSTEM_PROCESSOR}" ARCH_LOWER)

set(SGL_CFG_IS_ARM FALSE)
set(SGL_CFG_IS_ARM64 FALSE)
set(SGL_CFG_IS_X86 FALSE)
set(SGL_CFG_IS_X86_64 FALSE)

if (ARCH_LOWER MATCHES "armv7" OR ARCH_LOWER MATCHES "^arm")
    set(SGL_CFG_IS_ARM TRUE)
elseif (ARCH_LOWER MATCHES "aarch64" OR ARCH_LOWER MATCHES "arm64")
    set(SGL_CFG_IS_ARM64 TRUE)
elseif (ARCH_LOWER MATCHES "x86_64" OR ARCH_LOWER MATCHES "amd64")
    set(SGL_CFG_IS_X86_64 TRUE)
elseif (ARCH_LOWER MATCHES "i386" OR ARCH_LOWER MATCHES "i686" OR ARCH_LOWER MATCHES "x86")
    set(SGL_CFG_IS_X86 TRUE)
endif()

message(STATUS "Detected architecture: ${ARCH_LOWER}")
