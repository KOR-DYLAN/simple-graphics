include(CMakePackageConfigHelpers)

project(sgl-core
    VERSION ${SGL_FULL_VERSION}
    LANGUAGES ASM C
)

add_library(${PROJECT_NAME})
add_subdirectory(convert)
add_subdirectory(crop)
add_subdirectory(resize)
add_subdirectory(rotate)
add_subdirectory(queue)
add_subdirectory(threadpool)

# Apply C99
set_property(TARGET ${PROJECT_NAME} PROPERTY C_STANDARD 99)
set_property(TARGET ${PROJECT_NAME} PROPERTY C_STANDARD_REQUIRED ON)
set_property(TARGET ${PROJECT_NAME} PROPERTY C_EXTENSIONS ON)

# Apply clang-tidy
if (WITH_CLANG_TIDY)
    set_target_properties(${PROJECT_NAME} PROPERTIES
        C_CLANG_TIDY "clang-tidy;-checks=misra-c2012-*"
    )
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
    FILES ${CMAKE_SOURCE_DIR}/include/sgl-core.h
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
    ${CMAKE_SOURCE_DIR}/script/config/${SGL_CORE_PACKAGE_CONFIGNAME}-config.cmake.in
    ${CMAKE_BINARY_DIR}/${SGL_CORE_PACKAGE_CONFIGNAME}-config.cmake
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/${SGL_CORE_EXPORT_NAME}
    PATH_VARS INCLUDE_INSTALL_DIR LIB_INSTALL_DIR
)
write_basic_package_version_file(
    ${CMAKE_BINARY_DIR}/${SGL_CORE_PACKAGE_CONFIGNAME}-config-version.cmake
    VERSION ${PACKAGE_VERSION}
    COMPATIBILITY AnyNewerVersion
)
install(
    FILES ${CMAKE_BINARY_DIR}/${SGL_CORE_PACKAGE_CONFIGNAME}-config.cmake ${CMAKE_BINARY_DIR}/${SGL_CORE_PACKAGE_CONFIGNAME}-config-version.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/${SGL_CORE_EXPORT_NAME}
)
