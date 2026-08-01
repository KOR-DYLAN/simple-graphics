# SPDX-License-Identifier: MIT

find_package(Doxygen QUIET)

set(SGL_DOXYGEN_CONFIG "${CMAKE_CURRENT_BINARY_DIR}/Doxyfile")
set(SGL_DOXYGEN_HTML_DIR "${CMAKE_BINARY_DIR}/docs/html")
set(SGL_DOXYGEN_OUTPUT "${SGL_DOXYGEN_HTML_DIR}/index.html")
set(SGL_DOCS_PACKAGE_BASENAME "sgl-docs-${SGL_FULL_VERSION}")
set(SGL_DOCS_PACKAGE_DIR "${CMAKE_SOURCE_DIR}/package")
set(SGL_DOXYGEN_SOURCES
    "${CMAKE_SOURCE_DIR}/include/sgl-compiler.h"
    "${CMAKE_SOURCE_DIR}/include/sgl-core.h"
    "${CMAKE_SOURCE_DIR}/include/sgl-fixed_point.h"
    "${CMAKE_SOURCE_DIR}/include/sgl-fourcc.h"
    "${CMAKE_SOURCE_DIR}/include/sgl-type.h"
    "${CMAKE_SOURCE_DIR}/include/fixed_point/generic.h"
    "${CMAKE_SOURCE_DIR}/include/fixed_point/neon.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/api/mainpage.dox"
    "${CMAKE_CURRENT_SOURCE_DIR}/api/getting-started.dox"
    "${CMAKE_CURRENT_SOURCE_DIR}/api/image-operations.dox"
    "${CMAKE_CURRENT_SOURCE_DIR}/api/pixel-formats.dox"
    "${CMAKE_CURRENT_SOURCE_DIR}/api/runtime-services.dox"
)

if(DOXYGEN_FOUND)
    configure_file(Doxyfile.in "${SGL_DOXYGEN_CONFIG}" @ONLY)
    add_custom_command(
        OUTPUT "${SGL_DOXYGEN_OUTPUT}"
        COMMAND "${DOXYGEN_EXECUTABLE}" "${SGL_DOXYGEN_CONFIG}"
        DEPENDS ${SGL_DOXYGEN_SOURCES} "${SGL_DOXYGEN_CONFIG}"
        COMMENT "Generating SGL API reference and user manual"
        VERBATIM
    )
    add_custom_target(docs DEPENDS "${SGL_DOXYGEN_OUTPUT}")

    # Keep the generated manual optional for developer installs. Release flows
    # that build `docs` before install or CPack receive the complete HTML tree.
    install(
        DIRECTORY "${SGL_DOXYGEN_HTML_DIR}/"
        DESTINATION "${CMAKE_INSTALL_DATAROOTDIR}/doc/sgl/html"
        COMPONENT Documentation
        OPTIONAL
    )

    add_custom_target(package-docs
        COMMAND ${CMAKE_COMMAND}
            "-DSGL_DOCS_HTML_DIR=${SGL_DOXYGEN_HTML_DIR}"
            "-DSGL_DOCS_PACKAGE_DIR=${SGL_DOCS_PACKAGE_DIR}"
            "-DSGL_DOCS_PACKAGE_BASENAME=${SGL_DOCS_PACKAGE_BASENAME}"
            "-DSGL_DOCS_STAGE_ROOT=${CMAKE_CURRENT_BINARY_DIR}/release-stage"
            "-DSGL_DOCS_VERSION=${SGL_FULL_VERSION}"
            "-DSGL_DOCS_LICENSE=${CMAKE_SOURCE_DIR}/LICENSE"
            "-DSGL_DOCS_README_TEMPLATE=${CMAKE_CURRENT_SOURCE_DIR}/release-README.md.in"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/package-docs.cmake"
        DEPENDS docs
        COMMENT "Creating versioned SGL documentation release archives"
        USES_TERMINAL
        VERBATIM
    )
else()
    add_custom_target(docs
        COMMAND ${CMAKE_COMMAND} -E echo
            "Doxygen is required: install the 'doxygen' package and rerun make docs."
        COMMAND false
        COMMENT "Doxygen is not installed"
        VERBATIM
    )
    add_custom_target(package-docs
        COMMAND ${CMAKE_COMMAND} -E echo
            "Doxygen is required: install the 'doxygen' package and rerun make package-docs."
        COMMAND false
        COMMENT "Doxygen is not installed"
        VERBATIM
    )
endif()
