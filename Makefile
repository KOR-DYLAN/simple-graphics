TOPDIR			:=$(dir $(abspath $(firstword $(MAKEFILE_LIST))))
WORKSPACE		:=$(TOPDIR)
BUILD			:=$(TOPDIR)/build
CMAKE			:=cmake
NPROC			?=$(shell nproc)
V				?=0
HOSTENV			:=TRUE

# qemu-aarch64
QEMU			:=qemu-aarch64

# aarch64-none-linux-llvm | aarch64-none-linux-gnu |
# llvm | gnu
# TOOLCHAIN		:=aarch64-none-linux-llvm
TOOLCHAIN		:=llvm

# Debug | Release | RelWithDebInfo | MinSizeRel
BUILD_TYPE		:=Debug

# Target Name
TARGET			:=queue
ARGS			:=$(TOPDIR)/resource/sample.png

ifneq ($(TOOLCHAIN),)
    CMAKE_FLAGS	+=-DCMAKE_TOOLCHAIN_FILE=$(TOPDIR)/script/toolchain/$(TOOLCHAIN).cmake
endif

ifneq ($(V),0)
    VERBOSE	:=-v
endif

ifeq ($(TOOLCHAIN),gnu)
    HOSTENV=TRUE
endif

ifeq ($(TOOLCHAIN),llvm)
    HOSTENV=TRUE
endif

all: build

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
ifeq ($(HOSTENV),TRUE)
	$(BUILD)/$(BUILD_TYPE)/bin/$(TARGET) $(ARGS)
else
	$(eval SYSROOT := $(shell cat $(BUILD)/sysroot.txt))
	$(QEMU) -L $(SYSROOT) $(BUILD)/$(BUILD_TYPE)/bin/$(TARGET) $(ARGS)
endif

.PHONY: $(phony)
