# SPDX-License-Identifier: MIT
#
# Copyright (c) 2025 Dylan Hong
#
# This file is released under the MIT License.
# For conditions of distribution and use, see the LICENSE file.

include(CMakePackageConfigHelpers)

project(sgl-core
    VERSION ${SGL_FULL_VERSION}
    LANGUAGES ASM C
)

# Main library target.  Module subdirectories attach their sources to this target.
add_library(${PROJECT_NAME})

# Attach implementation modules while keeping each domain's source list local.
add_subdirectory(trace)
add_subdirectory(memory)
add_subdirectory(convert)
add_subdirectory(crop)
add_subdirectory(resize)
add_subdirectory(rotate)
add_subdirectory(queue)
add_subdirectory(threadpool)

if(SGL_CFG_HAS_PTHREAD)
    # Link pthread only when the platform detection selected the pthread backend.
    find_package(Threads REQUIRED)
    target_link_libraries(${PROJECT_NAME} PUBLIC Threads::Threads)
endif()

# The public C API is C99-oriented; extensions stay enabled for compiler intrinsics.
set_property(TARGET ${PROJECT_NAME} PROPERTY C_STANDARD 99)
set_property(TARGET ${PROJECT_NAME} PROPERTY C_STANDARD_REQUIRED ON)
set_property(TARGET ${PROJECT_NAME} PROPERTY C_EXTENSIONS ON)

if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    # SGL provides memory primitives, so avoid compiler builtin substitutions in the core library.
    target_compile_options(${PROJECT_NAME} PRIVATE
        -fno-builtin-memcpy
        -fno-builtin-memset
    )
endif()

if(WITH_CPPCHECK)
    # Run cppcheck only on the core library; test applications remain fast to iterate.
    set_property(TARGET ${PROJECT_NAME} PROPERTY C_CPPCHECK "${SGL_C_CPPCHECK}")
    set_property(TARGET ${PROJECT_NAME} PROPERTY CXX_CPPCHECK "${SGL_CXX_CPPCHECK}")
endif()

#============================================================================
# Install Target
#============================================================================
install(
    TARGETS ${PROJECT_NAME}
    EXPORT ${SGL_CORE_EXPORT_NAME}-targets
    RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
    ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
    LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
)

install(
    FILES ${CMAKE_BINARY_DIR}/sgl-config.h
    DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
)
install(
    FILES
        ${CMAKE_SOURCE_DIR}/include/sgl-compiler.h
        ${CMAKE_SOURCE_DIR}/include/sgl-core.h
        ${CMAKE_SOURCE_DIR}/include/sgl-type.h
    DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
)

install(
    FILES ${CMAKE_BINARY_DIR}/${SGL_CORE_PC}
    DESTINATION "${PKGCONFIG_INSTALL_DIR}"
)
install(
    EXPORT ${SGL_CORE_EXPORT_NAME}-targets
    DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/${SGL_CORE_EXPORT_NAME}"
    NAMESPACE ${SGL_CORE_EXPORT_NAME}::)

configure_package_config_file(
    ${CMAKE_SOURCE_DIR}/script/config/package/${SGL_CORE_PACKAGE_CONFIGNAME}-config.cmake.in
    ${CMAKE_BINARY_DIR}/${SGL_CORE_PACKAGE_CONFIGNAME}-config.cmake
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/${SGL_CORE_EXPORT_NAME}
    PATH_VARS INCLUDE_INSTALL_DIR LIB_INSTALL_DIR
)
write_basic_package_version_file(
    ${CMAKE_BINARY_DIR}/${SGL_CORE_PACKAGE_CONFIGNAME}-config-version.cmake
    VERSION ${SGL_FULL_VERSION}
    COMPATIBILITY AnyNewerVersion
)
install(
    FILES ${CMAKE_BINARY_DIR}/${SGL_CORE_PACKAGE_CONFIGNAME}-config.cmake ${CMAKE_BINARY_DIR}/${SGL_CORE_PACKAGE_CONFIGNAME}-config-version.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/${SGL_CORE_EXPORT_NAME}
)
