TOPDIR			:=$(dir $(abspath $(firstword $(MAKEFILE_LIST))))
WORKSPACE		:=$(TOPDIR)
BUILD			:=$(TOPDIR)/build
CMAKE			:=cmake
CPACK			:=cpack
NPROC			?=$(shell nproc)
V				?=0
HOSTENV			:=TRUE

# qemu-aarch64
QEMU			:=qemu-aarch64

# Options(ON,OFF)
# WITH_CLANG_TIDY	?=ON
# WITH_SIMD		?=OFF
# WITH_THREAD		?=OFF

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
build:
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

.PHONY: $(phony)
