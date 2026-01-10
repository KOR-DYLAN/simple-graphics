ifneq ($(TOOLCHAIN),)
    CMAKE_FLAGS	+=-DCMAKE_TOOLCHAIN_FILE=$(TOPDIR)/script/toolchain/$(TOOLCHAIN).cmake
endif

ifneq ($(INSTALL_PREFIX),)
    CMAKE_FLAGS	+=-DCMAKE_INSTALL_PREFIX=$(INSTALL_PREFIX)
endif

ifneq ($(WITH_CLANG_TIDY),)
    CMAKE_FLAGS	+=-DWITH_CLANG_TIDY=$(WITH_CLANG_TIDY)
endif

ifneq ($(WITH_SIMD),)
    CMAKE_FLAGS	+=-DWITH_SIMD=$(WITH_SIMD)
endif

ifneq ($(WITH_THREAD),)
    CMAKE_FLAGS	+=-DWITH_THREAD=$(WITH_THREAD)
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
