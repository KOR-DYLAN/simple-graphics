include(CheckCXXSourceCompiles)
include(CheckCXXCompilerFlag)

set(SGL_CFG_HAS_SIMD    FALSE)

if (NOT WITH_SIMD)
    message(STATUS "SIMD is force disabled!")
    set(SGL_CFG_HAS_NEON FALSE)
    set(SGL_CFG_HAS_SSE42 FALSE)
    set(SGL_CFG_HAS_AVX2 FALSE)
else()
    # ---------------------------------------------------------------------
    # 1. Check NEON support for ARM
    # ---------------------------------------------------------------------
    if (SGL_CFG_IS_ARM)
        message(STATUS "Checking NEON support...")

        set(CMAKE_REQUIRED_FLAGS "-mfpu=neon")

        file(READ "${CMAKE_CURRENT_LIST_DIR}/sgl-test-neon.c" TEST_NEON_CODE)
        check_cxx_source_compiles("${TEST_NEON_CODE}" SGL_CFG_HAS_NEON)

        if (SGL_CFG_HAS_NEON)
            message(STATUS "NEON supported: YES")
            add_compile_options(-mfpu=neon)
            set(SGL_CFG_HAS_SIMD TRUE)
        else()
            message(STATUS "NEON supported: NO")
        endif()
    endif()

    # ---------------------------------------------------------------------
    # 2. Check NEON support for ARM64
    # ---------------------------------------------------------------------
    if (SGL_CFG_IS_ARM64)
        message(STATUS "Checking NEON support...")

        file(READ "${CMAKE_CURRENT_LIST_DIR}/sgl-test-neon.c" TEST_NEON_CODE)
        check_cxx_source_compiles("${TEST_NEON_CODE}" SGL_CFG_HAS_NEON)

        if (SGL_CFG_HAS_NEON)
            message(STATUS "NEON supported: YES")
            set(SGL_CFG_HAS_SIMD TRUE)
        else()
            message(STATUS "NEON supported: NO")
        endif()
    endif()

    # ---------------------------------------------------------------------
    # 3. Check SSE/AVX support for x86/x86_64
    # ---------------------------------------------------------------------
    if (SGL_CFG_IS_X86 OR SGL_CFG_IS_X86_64)
        message(STATUS "Checking x86 SIMD support...")

        check_cxx_compiler_flag("-msse4.2" HAS_SSE42)
        check_cxx_compiler_flag("-mavx2"   HAS_AVX2)

        if (SGL_CFG_HAS_SSE42)
            message(STATUS "SSE4.2 supported: YES")
            add_compile_options(-msse4.2)
            # set(SGL_CFG_HAS_SIMD TRUE)
        endif()

        if (SGL_CFG_HAS_AVX2)
            message(STATUS "AVX2 supported: YES")
            add_compile_options(-mavx2)
            # set(SGL_CFG_HAS_SIMD TRUE)
        endif()
    endif()
endif()
