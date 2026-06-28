ifneq ($(TOOLCHAIN),)
    CMAKE_FLAGS	+=-DCMAKE_TOOLCHAIN_FILE=$(TOPDIR)/script/toolchain/$(TOOLCHAIN).cmake
endif

ifneq ($(INSTALL_PREFIX),)
    CMAKE_FLAGS	+=-DCMAKE_INSTALL_PREFIX=$(INSTALL_PREFIX)
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
    CPPCHECK_MISRA_RULE_TEXTS \
    WITH_TEST_APP \
    WITH_BENCHMARK_COMPARE \
    WITH_SIMD \
    WITH_THREAD

$(foreach var,$(CMAKE_CACHE_VARS),$(eval $(call add_cmake_cache_var,$(var))))

ifneq ($(V),0)
    VERBOSE	:=-v
endif
