function(sgl_apply_install_dir_alias OUTPUT_VAR NEW_ALIAS OLD_ALIAS OLD_HELP)
    # Older builds accepted project-specific install directory variables.  Keep
    # them working, but normalize everything into GNUInstallDirs variables so
    # the rest of the build has one naming convention to read.
    if(DEFINED ${OLD_ALIAS})
        set(${OLD_ALIAS} "${${OLD_ALIAS}}" CACHE PATH "${OLD_HELP}" FORCE)
        set(${OUTPUT_VAR} "${${OLD_ALIAS}}" PARENT_SCOPE)
    elseif(DEFINED ${NEW_ALIAS})
        set(${OUTPUT_VAR} "${${NEW_ALIAS}}" PARENT_SCOPE)
    endif()
endfunction()

sgl_apply_install_dir_alias(CMAKE_INSTALL_BINDIR
    INSTALL_BIN_DIR BIN_INSTALL_DIR
    "Installation directory for executables (Deprecated)")
sgl_apply_install_dir_alias(CMAKE_INSTALL_LIBDIR
    INSTALL_LIB_DIR LIB_INSTALL_DIR
    "Installation directory for libraries (Deprecated)")
sgl_apply_install_dir_alias(CMAKE_INSTALL_INCLUDEDIR
    INSTALL_INC_DIR INC_INSTALL_DIR
    "Installation directory for headers (Deprecated)")

# GNUInstallDirs fills in defaults for any install directory not overridden
# above, and also provides CMAKE_INSTALL_FULL_* variants for package metadata.
include(GNUInstallDirs)

if(DEFINED PKGCONFIG_INSTALL_DIR)
    set(PKGCONFIG_INSTALL_DIR "${PKGCONFIG_INSTALL_DIR}" CACHE PATH "Installation directory for pkgconfig (.pc) files" FORCE)
elseif(DEFINED INSTALL_PKGCONFIG_DIR)
    set(PKGCONFIG_INSTALL_DIR "${INSTALL_PKGCONFIG_DIR}" CACHE PATH "Installation directory for pkgconfig (.pc) files" FORCE)
elseif(DEFINED CMAKE_INSTALL_PKGCONFIGDIR)
    set(PKGCONFIG_INSTALL_DIR "${CMAKE_INSTALL_PKGCONFIGDIR}" CACHE PATH "Installation directory for pkgconfig (.pc) files" FORCE)
elseif(DEFINED CMAKE_INSTALL_FULL_PKGCONFIGDIR)
    set(PKGCONFIG_INSTALL_DIR "${CMAKE_INSTALL_FULL_PKGCONFIGDIR}" CACHE PATH "Installation directory for pkgconfig (.pc) files" FORCE)
else()
    set(PKGCONFIG_INSTALL_DIR "${CMAKE_INSTALL_LIBDIR}/pkgconfig" CACHE PATH "Installation directory for pkgconfig (.pc) files")
endif()

# configure_package_config_file() uses these PATH_VARS names when producing the
# installed CMake package config.
set(INCLUDE_INSTALL_DIR ${CMAKE_INSTALL_INCLUDEDIR})
set(LIB_INSTALL_DIR ${CMAKE_INSTALL_LIBDIR})
