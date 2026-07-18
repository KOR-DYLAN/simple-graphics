# SPDX-License-Identifier: MIT
#
# Copyright (c) 2025 Dylan Hong
#
# This file is released under the MIT License.
# For conditions of distribution and use, see the LICENSE file.

ifneq ($(TOOLCHAIN),)
    CMAKE_FLAGS	+=-DCMAKE_TOOLCHAIN_FILE=$(TOPDIR)/script/toolchain/$(TOOLCHAIN).cmake
endif

ifneq ($(INSTALL_PREFIX),)
    CMAKE_FLAGS	+=-DCMAKE_INSTALL_PREFIX=$(INSTALL_PREFIX)
endif

# Runtime test preset for sanitizer/stack-protector builds.  The preset writes
# to sanitizer-specific build directories because ASan/UBSan/LSan and TSan must
# be configured as separate CMake trees.
#
# Usage:
#   make test WITH_RUNTIME_TEST=ON
#   make test WITH_RUNTIME_TEST=ON SANITIZERS=thread
ifneq ($(filter 1 ON TRUE YES on true yes,$(WITH_RUNTIME_TEST)),)
ifeq ($(origin BUILD_TYPE),file)
    # Runtime reports are easier to read from Debug binaries, but command-line
    # BUILD_TYPE still wins when a caller wants RelWithDebInfo or another mode.
    BUILD_TYPE	:=Debug
endif
    WITH_TEST_APP			        ?=ON
    WITH_SANITIZER			        ?=ON
    SANITIZERS				        ?=address,undefined
    WITH_STACK_PROTECTOR		    ?=ON
    STACK_PROTECTOR_MODE		    ?=all
    SGL_TEST_RESIZE_REPEAT_COUNT	?=1
    SGL_TEST_RESIZE_WARMUP_COUNT	?=0
    comma				            :=,
    SGL_RUNTIME_TEST_SANITIZER_DIR	:=$(subst $(comma),-,$(SANITIZERS))
ifeq ($(origin BUILD),file)
    # Keep the normal build/llvm cache clean and avoid reusing an ASan cache for
    # a later TSan run.
    BUILD	:=$(BUILD_ROOT)/runtime-$(TOOLCHAIN)-$(SGL_RUNTIME_TEST_SANITIZER_DIR)
endif
endif

define add_cmake_cache_var
ifneq ($($(1)),)
    CMAKE_FLAGS	+=-D$(1)=$($(1))
endif
endef

CMAKE_CACHE_VARS	:= \
    WITH_COMPILER_WARNINGS \
    WITH_CPPCHECK \
    WITH_CPPCHECK_MISRA \
    WITH_CPPCHECK_WARNINGS_AS_ERRORS \
    WITH_SANITIZER \
    SANITIZERS \
    WITH_STACK_PROTECTOR \
    STACK_PROTECTOR_MODE \
    WITH_LTTNG \
    LTTNG_UST_ROOT \
    CPPCHECK_MISRA_RULE_TEXTS \
    WITH_TEST_APP \
    SGL_TEST_RESIZE_REPEAT_COUNT \
    SGL_TEST_RESIZE_WARMUP_COUNT \
    WITH_BENCHMARK_COMPARE \
    WITH_SIMD \
    WITH_THREAD

$(foreach var,$(CMAKE_CACHE_VARS),$(eval $(call add_cmake_cache_var,$(var))))

ifneq ($(V),0)
    VERBOSE	:=-v
endif
