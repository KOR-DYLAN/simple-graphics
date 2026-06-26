include(${CMAKE_CURRENT_LIST_DIR}/common.cmake)

# GCC toolchains are selected by prefixing the GNU binary names.  Native builds
# leave TRIPLE empty and therefore resolve gcc/g++ from PATH; cross builds use
# names such as aarch64-none-linux-gnu-gcc.
set(CMAKE_ASM_COMPILER  ${TRIPLE}gcc)
set(CMAKE_C_COMPILER    ${TRIPLE}gcc)
set(CMAKE_CXX_COMPILER  ${TRIPLE}g++)
set(CMAKE_LINKER        ${TRIPLE}ld)
set(CMAKE_PP            ${TRIPLE}cpp)
set(CMAKE_AR            ${TRIPLE}ar)
set(CMAKE_OBJCOPY       ${TRIPLE}objcopy)
set(CMAKE_OBJDUMP       ${TRIPLE}objdump)
set(CMAKE_RANLIB        ${TRIPLE}ranlib)
set(CMAKE_STRIP         ${TRIPLE}strip)
set(CMAKE_SIZE          ${TRIPLE}size)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(SGL_TOOLCHAIN_IS_CROSS FALSE)
if(DEFINED TRIPLE AND NOT "${TRIPLE}" STREQUAL "")
    set(SGL_TOOLCHAIN_IS_CROSS TRUE)
endif()

sgl_toolchain_set_standard_build_flags()

if(SGL_TOOLCHAIN_IS_CROSS)
    sgl_toolchain_detect_sysroot(${CMAKE_C_COMPILER})

    # GCC already understands its own sysroot.  Only the project's target-side
    # prebuilt libraries need to be appended to CMake's package search roots.
    sgl_toolchain_append_project_find_roots("" "")
endif()
