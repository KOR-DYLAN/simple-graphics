# Set Toolchin
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

if(TRIPLE)
    # Get sysroot
    execute_process(
        COMMAND ${CMAKE_C_COMPILER} -print-sysroot
        OUTPUT_VARIABLE GCC_SYSROOT
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )

    # Apply sysroot
    if(GCC_SYSROOT)
        set(CMAKE_SYSROOT "${GCC_SYSROOT}" CACHE PATH "Sysroot path" FORCE)
        file(WRITE ${CMAKE_BINARY_DIR}/sysroot.txt "${GCC_SYSROOT}\n")
        message(STATUS "Sysroot detected: ${CMAKE_SYSROOT}")
    else()
        message(WARNING "Failed to detect sysroot from ${CMAKE_C_COMPILER}")
    endif()
endif()

# Debug
set(CMAKE_ASM_FLAGS_DEBUG           "-O0 -g")
set(CMAKE_C_FLAGS_DEBUG             "-O0 -g")
set(CMAKE_CXX_FLAGS_DEBUG           "-O0 -g")

# MinSizeRel
set(CMAKE_ASM_FLAGS_MINSIZEREL      "-Os -DNDEBUG")
set(CMAKE_C_FLAGS_MINSIZEREL        "-Os -DNDEBUG")
set(CMAKE_CXX_FLAGS_MINSIZEREL      "-Os -DNDEBUG")

# Release
set(CMAKE_ASM_FLAGS_RELEASE         "-O2 -DNDEBUG")
set(CMAKE_C_FLAGS_RELEASE           "-O2 -DNDEBUG")
set(CMAKE_CXX_FLAGS_RELEASE         "-O2 -DNDEBUG")

# RelWithDebInfo
set(CMAKE_ASM_FLAGS_RELWITHDEBINFO  "-O2 -g -DNDEBUG")
set(CMAKE_C_FLAGS_RELWITHDEBINFO    "-O2 -g -DNDEBUG")
set(CMAKE_CXX_FLAGS_RELWITHDEBINFO  "-O2 -g -DNDEBUG")

if(TRIPLE)
    list(APPEND CMAKE_FIND_ROOT_PATH "${CMAKE_SOURCE_DIR}/prebuild/libpng")
    list(APPEND CMAKE_FIND_ROOT_PATH "${CMAKE_SOURCE_DIR}/prebuild/zlib")
    set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
    set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
endif()
