include(CheckCXXSourceCompiles)
include(CheckCXXCompilerFlag)

set(SGL_CFG_HAS_SIMD    FALSE)
set(SGL_CFG_HAS_NEON    FALSE)
set(SGL_CFG_HAS_SSE42   FALSE)
set(SGL_CFG_HAS_AVX2    FALSE)

function(sgl_check_neon SGL_NEON_FLAGS)
    # ARMv7 needs -mfpu=neon for both the feature probe and normal compilation.
    # AArch64 has NEON as part of the baseline architecture, so the flag is empty
    # there and the same probe source can still validate the compiler headers.
    message(STATUS "Checking NEON support...")

    set(CMAKE_REQUIRED_FLAGS "${SGL_NEON_FLAGS}")
    file(READ "${CMAKE_CURRENT_LIST_DIR}/sgl-test-neon.c" TEST_NEON_CODE)
    check_cxx_source_compiles("${TEST_NEON_CODE}" SGL_CHECK_HAS_NEON)
    set(CMAKE_REQUIRED_FLAGS "")

    if(SGL_CHECK_HAS_NEON)
        message(STATUS "NEON supported: YES")
        set(SGL_CFG_HAS_NEON TRUE PARENT_SCOPE)
        set(SGL_CFG_HAS_SIMD TRUE PARENT_SCOPE)
        if(NOT "${SGL_NEON_FLAGS}" STREQUAL "")
            add_compile_options(${SGL_NEON_FLAGS})
        endif()
    else()
        message(STATUS "NEON supported: NO")
        set(SGL_CFG_HAS_NEON FALSE PARENT_SCOPE)
    endif()
endfunction()

function(sgl_check_x86_simd)
    # These flags are detected independently because SSE4.2 and AVX2 can be
    # enabled by different compiler defaults.  The result variables feed both
    # sgl-config.h and the configuration summary.
    message(STATUS "Checking x86 SIMD support...")

    check_cxx_compiler_flag("-msse4.2" SGL_CHECK_HAS_SSE42)
    check_cxx_compiler_flag("-mavx2" SGL_CHECK_HAS_AVX2)

    if(SGL_CHECK_HAS_SSE42)
        message(STATUS "SSE4.2 supported: YES")
        add_compile_options(-msse4.2)
        set(SGL_CFG_HAS_SSE42 TRUE PARENT_SCOPE)
        set(SGL_CFG_HAS_SIMD TRUE PARENT_SCOPE)
    endif()

    if(SGL_CHECK_HAS_AVX2)
        message(STATUS "AVX2 supported: YES")
        add_compile_options(-mavx2)
        set(SGL_CFG_HAS_AVX2 TRUE PARENT_SCOPE)
        set(SGL_CFG_HAS_SIMD TRUE PARENT_SCOPE)
    endif()
endfunction()

if(NOT WITH_SIMD)
    message(STATUS "SIMD is force disabled")
elseif(SGL_CFG_IS_ARM)
    sgl_check_neon("-mfpu=neon")
elseif(SGL_CFG_IS_ARM64)
    sgl_check_neon("")
elseif(SGL_CFG_IS_X86 OR SGL_CFG_IS_X86_64)
    sgl_check_x86_simd()
endif()
