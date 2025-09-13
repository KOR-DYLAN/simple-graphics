TOPDIR			:=$(dir $(abspath $(firstword $(MAKEFILE_LIST))))
WORKSPACE		:=$(TOPDIR)
BUILD			:=$(TOPDIR)/build
CMAKE			:=cmake
NPROC			?=$(shell nproc)
V				?=0

# qemu-aarch64
QEMU			:=qemu-aarch64

# aarch64-none-linux-llvm | aarch64-none-linux-gnu
TOOLCHAIN		:=aarch64-none-linux-llvm

# Debug | Release | RelWithDebInfo | MinSizeRel
BUILD_TYPE		:=Debug

# Target Name
TARGET			:=sample

ifneq ($(TOOLCHAIN),)
    CMAKE_FLAGS	+=-DCMAKE_TOOLCHAIN_FILE=$(TOPDIR)/script/toolchain/$(TOOLCHAIN).cmake
endif

ifneq ($(V),0)
    VERBOSE	:=-v
endif

phony+=config
config:
	$(CMAKE) $(CMAKE_FLAGS) -DCMAKE_BUILD_TYPE=$(BUILD_TYPE) -S$(WORKSPACE) -B$(BUILD)

phony+=build
build:
	$(CMAKE) --build $(BUILD) -j $(NPROC) $(VERBOSE)

phony+=clean
clean:
	$(CMAKE) --build $(BUILD) --target clean $(VERBOSE)

phony+=distclean
distclean:
	rm -rf $(BUILD)

phony+=run
run: build
	$(eval SYSROOT := $(shell cat $(BUILD)/sysroot.txt))
	$(QEMU) -L $(SYSROOT) $(BUILD)/$(BUILD_TYPE)/bin/$(TARGET)

.PHONY: $(phony)
