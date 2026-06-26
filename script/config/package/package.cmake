set(SGL_CORE_EXPORT_NAME sgl-core)
set(SGL_CORE_PC sgl-core.pc)
set(SGL_CORE_PACKAGE_CONFIGNAME sgl-core-package)

function(sgl_set_pkgconfig_dir OUTPUT_VAR RELATIVE_PREFIX INSTALL_DIR)
    # .pc files are consumed outside CMake.  Relative install dirs must be
    # expressed through pkg-config variables such as ${prefix}; absolute dirs are
    # kept as-is so explicit non-relocatable installs remain intentional.
    if(IS_ABSOLUTE "${INSTALL_DIR}")
        set(${OUTPUT_VAR} "${INSTALL_DIR}" PARENT_SCOPE)
    else()
        set(${OUTPUT_VAR} "${RELATIVE_PREFIX}/${INSTALL_DIR}" PARENT_SCOPE)
    endif()
endfunction()

sgl_set_pkgconfig_dir(PC_INC_INSTALL_DIR "\${prefix}"
    "${CMAKE_INSTALL_INCLUDEDIR}")
sgl_set_pkgconfig_dir(PC_BIN_INSTALL_DIR "\${exec_prefix}"
    "${CMAKE_INSTALL_BINDIR}")
sgl_set_pkgconfig_dir(PC_LIB_INSTALL_DIR "\${exec_prefix}"
    "${CMAKE_INSTALL_LIBDIR}")

configure_file(
    ${CMAKE_CURRENT_LIST_DIR}/${SGL_CORE_PC}.cmakein
    ${CMAKE_BINARY_DIR}/${SGL_CORE_PC} @ONLY
)
