# SPDX-License-Identifier: MIT
#
# Copyright (c) 2025 Dylan Hong
#
# This file is released under the MIT License.
# For conditions of distribution and use, see the LICENSE file.

TOPDIR			:=$(dir $(abspath $(firstword $(MAKEFILE_LIST))))
WORKSPACE		:=$(TOPDIR)
BUILD			:=$(TOPDIR)/build
CMAKE			:=cmake
CPACK			:=cpack
NPROC			?=$(shell nproc)
V				?=0
HOSTENV			:=TRUE
CPPCHECK_REPORT	?=$(TOPDIR)/script/cppcheck-report.sh

# qemu-aarch64
QEMU			:=qemu-aarch64

# aarch64-none-linux-llvm | aarch64-none-linux-gnu |
# llvm | gnu
# TOOLCHAIN		:=aarch64-none-linux-llvm
TOOLCHAIN		?=llvm

# Debug | Release | RelWithDebInfo | MinSizeRel
BUILD_TYPE		?=Release

# Install Prefix
# INSTALL_PREFIX	:=$(BUILD)/install

# Target Name
TARGET			?=resize
ARGS			?=$(TOPDIR)/resource/sample.png

include $(TOPDIR)/config.mk

all: build

phony+=config
config:
	$(CMAKE) $(CMAKE_FLAGS) -DCMAKE_BUILD_TYPE=$(BUILD_TYPE) -S$(WORKSPACE) -B$(BUILD)

phony+=build
build: config
	$(CMAKE) --build $(BUILD) -j $(NPROC) $(VERBOSE)

phony+=install
install:
	$(CMAKE) --install $(BUILD) $(VERBOSE)

phony+=package
package:
	$(CPACK) --config $(BUILD)/CPackConfig.cmake $(VERBOSE)

phony+=package-source
package-source:
	$(CPACK) --config $(BUILD)/CPackSourceConfig.cmake $(VERBOSE)

phony+=clean
clean:
	$(CMAKE) --build $(BUILD) --target clean $(VERBOSE)

phony+=distclean
distclean:
	rm -rf $(BUILD)

phony+=run
run: build
ifeq ($(HOSTENV),TRUE)
	$(BUILD)/$(BUILD_TYPE)/bin/$(TARGET) $(ARGS)
else
	$(eval SYSROOT := $(shell cat $(BUILD)/sysroot.txt))
	$(QEMU) -L $(SYSROOT) $(BUILD)/$(BUILD_TYPE)/bin/$(TARGET) $(ARGS)
endif

phony+=cppcheck-report
cppcheck-report: config
	BUILD_DIR=$(BUILD) REPORT_ROOT=$(TOPDIR)/report WITH_CPPCHECK_MISRA=$(WITH_CPPCHECK_MISRA) CPPCHECK_MISRA_RULE_TEXTS="$(CPPCHECK_MISRA_RULE_TEXTS)" CPPCHECK_MAX_CTU_DEPTH=$(CPPCHECK_MAX_CTU_DEPTH) CPPCHECK_JOBS=$(NPROC) $(CPPCHECK_REPORT)

.PHONY: $(phony)
