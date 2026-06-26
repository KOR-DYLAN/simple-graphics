include(${CMAKE_CURRENT_LIST_DIR}/common.cmake)

# Native LLVM builds use the host Clang driver.  Cross LLVM builds keep the same
# driver but add CMAKE_*_COMPILER_TARGET below so Clang emits aarch64 code.
set(CMAKE_ASM_COMPILER  clang)
set(CMAKE_C_COMPILER    clang)
set(CMAKE_CXX_COMPILER  clang++)

set(SGL_TOOLCHAIN_IS_CROSS FALSE)
if(DEFINED TRIPLE AND NOT "${TRIPLE}" STREQUAL "")
    set(SGL_TOOLCHAIN_IS_CROSS TRUE)
endif()

set(CMAKE_PP            ${TRIPLE}cpp)
set(CMAKE_AR            llvm-ar)
set(CMAKE_OBJCOPY       ${TRIPLE}objcopy)
set(CMAKE_OBJDUMP       ${TRIPLE}objdump)
set(CMAKE_RANLIB        llvm-ranlib)
set(CMAKE_STRIP         llvm-strip)
set(CMAKE_SIZE          llvm-size)

if(SGL_TOOLCHAIN_IS_CROSS)
    # CMake passes this value to Clang as --target=<triple>.  The executable
    # remains plain clang, which keeps host and cross LLVM files shareable.
    set(CMAKE_LINKER clang)
    set(CMAKE_C_COMPILER_TARGET ${TARGET_TRIPLE})
    set(CMAKE_CXX_COMPILER_TARGET ${TARGET_TRIPLE})
endif()

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

sgl_toolchain_set_standard_build_flags()

if(SGL_TOOLCHAIN_IS_CROSS)
    sgl_toolchain_detect_sysroot(${TRIPLE}gcc)
    sgl_toolchain_find_gcc_root(GCC_TOOLCHAIN_ROOT ${TRIPLE}gcc)

    # Clang's aarch64 driver delegates runtime pieces to the GCC toolchain.  The
    # flag is applied to compile, assemble, and link phases so every compiler
    # probe observes the same cross environment.
    set(CMAKE_C_FLAGS_INIT              "--gcc-toolchain=${GCC_TOOLCHAIN_ROOT}")
    set(CMAKE_CXX_FLAGS_INIT            "--gcc-toolchain=${GCC_TOOLCHAIN_ROOT}")
    set(CMAKE_ASM_FLAGS_INIT            "--gcc-toolchain=${GCC_TOOLCHAIN_ROOT}")
    set(CMAKE_EXE_LINKER_FLAGS_INIT     "--gcc-toolchain=${GCC_TOOLCHAIN_ROOT} -fuse-ld=bfd")
    set(CMAKE_SHARED_LINKER_FLAGS_INIT  "--gcc-toolchain=${GCC_TOOLCHAIN_ROOT} -fuse-ld=bfd")
    set(CMAKE_MODULE_LINKER_FLAGS_INIT  "--gcc-toolchain=${GCC_TOOLCHAIN_ROOT} -fuse-ld=bfd")

    sgl_toolchain_append_project_find_roots("${CMAKE_SYSROOT}" NEVER)
endif()
