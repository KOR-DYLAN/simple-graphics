option(WITH_CLANG_TIDY  "Enable clang-tidy Feature"         OFF)
option(WITH_TEST_APP    "Enable Test Application Feature"   ON)
option(WITH_SIMD        "Enable SIMD Feature"               ON)
option(WITH_THREAD      "Enable Thread Feature"             ON)

include(${CMAKE_CURRENT_LIST_DIR}/load-version.cmake)
include(${CMAKE_CURRENT_LIST_DIR}/arch/detect-arch.cmake)
include(${CMAKE_CURRENT_LIST_DIR}/simd/detect-simd.cmake)
include(${CMAKE_CURRENT_LIST_DIR}/thread/detect-thread.cmake)
include(${CMAKE_CURRENT_LIST_DIR}/detect-install-dirs.cmake)

# The user is allowed (but discouraged) to set absolute CMAKE_INSTALL_*DIR paths.
# If they do, we copy these non-relocatable paths into the pkg-config file.
if(IS_ABSOLUTE "${CMAKE_INSTALL_INCLUDEDIR}")
    set(PC_INC_INSTALL_DIR "${CMAKE_INSTALL_INCLUDEDIR}")
else()
    set(PC_INC_INSTALL_DIR "\${prefix}/${CMAKE_INSTALL_INCLUDEDIR}")
endif()

if(IS_ABSOLUTE "${CMAKE_INSTALL_BINDIR}")
    set(PC_BIN_INSTALL_DIR "${CMAKE_INSTALL_BINDIR}")
else()
    set(PC_BIN_INSTALL_DIR "\${exec_prefix}/${CMAKE_INSTALL_BINDIR}")
endif()

if(IS_ABSOLUTE "${CMAKE_INSTALL_LIBDIR}")
    set(PC_LIB_INSTALL_DIR "${CMAKE_INSTALL_LIBDIR}")
else()
    set(PC_LIB_INSTALL_DIR "\${exec_prefix}/${CMAKE_INSTALL_LIBDIR}")
endif()

set(SGL_CORE_EXPORT_NAME sgl-core)
set(SGL_CORE_PC sgl-core.pc)
set(SGL_CORE_PACKAGE_CONFIGNAME sgl-core-package)
configure_file(
    ${CMAKE_CURRENT_LIST_DIR}/${SGL_CORE_PC}.cmakein
    ${CMAKE_BINARY_DIR}/${SGL_CORE_PC} @ONLY
)

configure_file(
    ${CMAKE_CURRENT_LIST_DIR}/sgl-config.h.in
    ${CMAKE_BINARY_DIR}/sgl-config.h
)

include_directories(${CMAKE_BINARY_DIR})
