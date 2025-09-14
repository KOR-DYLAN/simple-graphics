include(${CMAKE_CURRENT_LIST_DIR}/../misra/llvm.cmake)

# Set Toolchin
set(CMAKE_ASM_COMPILER  clang)
set(CMAKE_C_COMPILER    clang)
set(CMAKE_CXX_COMPILER  clang++)
set(CMAKE_LINKER        clang)
set(CMAKE_PP            ${TRIPLE}cpp)
set(CMAKE_AR            llvm-ar)
set(CMAKE_OBJCOPY       ${TRIPLE}objcopy)
set(CMAKE_OBJDUMP       ${TRIPLE}objdump)
set(CMAKE_RANLIB        llvm-ranlib)
set(CMAKE_STRIP         llvm-strip)
set(CMAKE_SIZE          llvm-size)
set(CMAKE_C_COMPILER_TARGET ${TARGET_TRIPLE})
set(CMAKE_CXX_COMPILER_TARGET ${TARGET_TRIPLE})
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Get And Apply sysroot
execute_process(
    COMMAND ${TRIPLE}gcc -print-sysroot
    OUTPUT_VARIABLE GCC_SYSROOT
    OUTPUT_STRIP_TRAILING_WHITESPACE
)
if(GCC_SYSROOT)
    set(CMAKE_SYSROOT "${GCC_SYSROOT}" CACHE PATH "Sysroot path" FORCE)
    file(WRITE ${CMAKE_BINARY_DIR}/sysroot.txt "${GCC_SYSROOT}\n")
    message(STATUS "Sysroot detected: ${CMAKE_SYSROOT}")
else()
    message(WARNING "Failed to detect sysroot from ${CMAKE_C_COMPILER}")
endif()

# Find And Apply gcc-toolchain
execute_process(
    COMMAND which ${TRIPLE}gcc
    OUTPUT_VARIABLE GCC_TOOLCHAIN
    OUTPUT_STRIP_TRAILING_WHITESPACE
)
get_filename_component(GCC_TOOLCHAIN_BIN "${GCC_TOOLCHAIN}" DIRECTORY)
get_filename_component(GCC_TOOLCHAIN_ROOT "${GCC_TOOLCHAIN_BIN}" DIRECTORY)

set(CMAKE_C_FLAGS_INIT              "--gcc-toolchain=${GCC_TOOLCHAIN_ROOT}")
set(CMAKE_CXX_FLAGS_INIT            "--gcc-toolchain=${GCC_TOOLCHAIN_ROOT}")
set(CMAKE_ASM_FLAGS_INIT            "--gcc-toolchain=${GCC_TOOLCHAIN_ROOT}")
set(CMAKE_EXE_LINKER_FLAGS_INIT     "--gcc-toolchain=${GCC_TOOLCHAIN_ROOT} -fuse-ld=bfd")
set(CMAKE_SHARED_LINKER_FLAGS_INIT  "--gcc-toolchain=${GCC_TOOLCHAIN_ROOT} -fuse-ld=bfd")
set(CMAKE_MODULE_LINKER_FLAGS_INIT  "--gcc-toolchain=${GCC_TOOLCHAIN_ROOT} -fuse-ld=bfd")

# Debug
set(CMAKE_ASM_FLAGS_DEBUG           "-O0 -g")
set(CMAKE_C_FLAGS_DEBUG             "-O0 -g ${MISRA_C_WARN}")
set(CMAKE_CXX_FLAGS_DEBUG           "-O0 -g ${MISRA_CXX_WARN}")

# MinSizeRel
set(CMAKE_ASM_FLAGS_MINSIZEREL      "-Os -DNDEBUG")
set(CMAKE_C_FLAGS_MINSIZEREL        "-Os -DNDEBUG ${MISRA_C_WARN}")
set(CMAKE_CXX_FLAGS_MINSIZEREL      "-Os -DNDEBUG ${MISRA_CXX_WARN}")

# Release
set(CMAKE_ASM_FLAGS_RELEASE         "-O2 -DNDEBUG")
set(CMAKE_C_FLAGS_RELEASE           "-O2 -DNDEBUG ${MISRA_C_WARN}")
set(CMAKE_CXX_FLAGS_RELEASE         "-O2 -DNDEBUG ${MISRA_CXX_WARN}")

# RelWithDebInfo
set(CMAKE_ASM_FLAGS_RELWITHDEBINFO  "-O2 -g -DNDEBUG")
set(CMAKE_C_FLAGS_RELWITHDEBINFO    "-O2 -g -DNDEBUG ${MISRA_C_WARN}")
set(CMAKE_CXX_FLAGS_RELWITHDEBINFO  "-O2 -g -DNDEBUG ${MISRA_CXX_WARN}")

list(APPEND CMAKE_FIND_ROOT_PATH "${CMAKE_SYSROOT}")
list(APPEND CMAKE_FIND_ROOT_PATH "${CMAKE_SOURCE_DIR}/prebuild/libpng")
list(APPEND CMAKE_FIND_ROOT_PATH "${CMAKE_SOURCE_DIR}/prebuild/zlib")
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
