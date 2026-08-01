# SPDX-License-Identifier: MIT

foreach(required_variable IN ITEMS
        SGL_DOCS_HTML_DIR
        SGL_DOCS_PACKAGE_DIR
        SGL_DOCS_PACKAGE_BASENAME
        SGL_DOCS_STAGE_ROOT
        SGL_DOCS_VERSION
        SGL_DOCS_LICENSE
        SGL_DOCS_README_TEMPLATE)
    if(NOT DEFINED ${required_variable} OR "${${required_variable}}" STREQUAL "")
        message(FATAL_ERROR "Missing required variable: ${required_variable}")
    endif()
endforeach()

foreach(path_variable IN ITEMS
        SGL_DOCS_HTML_DIR
        SGL_DOCS_PACKAGE_DIR
        SGL_DOCS_STAGE_ROOT
        SGL_DOCS_LICENSE
        SGL_DOCS_README_TEMPLATE)
    get_filename_component(
        normalized_path
        "${${path_variable}}"
        ABSOLUTE
        BASE_DIR "${CMAKE_CURRENT_BINARY_DIR}"
    )
    set(${path_variable} "${normalized_path}")
endforeach()

if(NOT EXISTS "${SGL_DOCS_HTML_DIR}/index.html")
    message(FATAL_ERROR
        "Doxygen entry point does not exist: ${SGL_DOCS_HTML_DIR}/index.html")
endif()
if(NOT EXISTS "${SGL_DOCS_LICENSE}")
    message(FATAL_ERROR "License file does not exist: ${SGL_DOCS_LICENSE}")
endif()
if(NOT EXISTS "${SGL_DOCS_README_TEMPLATE}")
    message(FATAL_ERROR
        "Documentation README template does not exist: ${SGL_DOCS_README_TEMPLATE}")
endif()

set(SGL_DOCS_STAGE_DIR
    "${SGL_DOCS_STAGE_ROOT}/${SGL_DOCS_PACKAGE_BASENAME}")
set(SGL_DOCS_TGZ
    "${SGL_DOCS_PACKAGE_DIR}/${SGL_DOCS_PACKAGE_BASENAME}.tar.gz")
set(SGL_DOCS_ZIP
    "${SGL_DOCS_PACKAGE_DIR}/${SGL_DOCS_PACKAGE_BASENAME}.zip")
set(SGL_DOCS_CHECKSUMS
    "${SGL_DOCS_PACKAGE_DIR}/${SGL_DOCS_PACKAGE_BASENAME}-SHA256SUMS.txt")

file(REMOVE_RECURSE "${SGL_DOCS_STAGE_ROOT}")
file(MAKE_DIRECTORY "${SGL_DOCS_STAGE_DIR}/html")
file(MAKE_DIRECTORY "${SGL_DOCS_PACKAGE_DIR}")
file(COPY "${SGL_DOCS_HTML_DIR}/"
    DESTINATION "${SGL_DOCS_STAGE_DIR}/html")
file(COPY "${SGL_DOCS_LICENSE}"
    DESTINATION "${SGL_DOCS_STAGE_DIR}")
configure_file(
    "${SGL_DOCS_README_TEMPLATE}"
    "${SGL_DOCS_STAGE_DIR}/README.md"
    @ONLY
)

file(REMOVE "${SGL_DOCS_TGZ}" "${SGL_DOCS_ZIP}" "${SGL_DOCS_CHECKSUMS}")
execute_process(
    COMMAND "${CMAKE_COMMAND}" -E tar cfz
        "${SGL_DOCS_TGZ}" "${SGL_DOCS_PACKAGE_BASENAME}"
    WORKING_DIRECTORY "${SGL_DOCS_STAGE_ROOT}"
    RESULT_VARIABLE archive_tgz_result
)
if(NOT archive_tgz_result EQUAL 0)
    message(FATAL_ERROR "Failed to create ${SGL_DOCS_TGZ}")
endif()

execute_process(
    COMMAND "${CMAKE_COMMAND}" -E tar cf
        "${SGL_DOCS_ZIP}" --format=zip "${SGL_DOCS_PACKAGE_BASENAME}"
    WORKING_DIRECTORY "${SGL_DOCS_STAGE_ROOT}"
    RESULT_VARIABLE archive_zip_result
)
if(NOT archive_zip_result EQUAL 0)
    message(FATAL_ERROR "Failed to create ${SGL_DOCS_ZIP}")
endif()

file(SHA256 "${SGL_DOCS_TGZ}" docs_tgz_sha256)
file(SHA256 "${SGL_DOCS_ZIP}" docs_zip_sha256)
get_filename_component(docs_tgz_name "${SGL_DOCS_TGZ}" NAME)
get_filename_component(docs_zip_name "${SGL_DOCS_ZIP}" NAME)
file(WRITE "${SGL_DOCS_CHECKSUMS}"
    "${docs_tgz_sha256}  ${docs_tgz_name}\n"
    "${docs_zip_sha256}  ${docs_zip_name}\n")

message(STATUS "Documentation archive: ${SGL_DOCS_TGZ}")
message(STATUS "Documentation archive: ${SGL_DOCS_ZIP}")
message(STATUS "SHA-256 checksums: ${SGL_DOCS_CHECKSUMS}")
