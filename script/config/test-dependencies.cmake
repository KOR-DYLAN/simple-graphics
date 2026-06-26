include(ExternalProject)

if(POLICY CMP0135)
    cmake_policy(SET CMP0135 NEW)
endif()

set(SGL_TEST_DEPS_INSTALL_DIR
    "${CMAKE_BINARY_DIR}/test-deps/install"
    CACHE PATH "Install prefix for test-only third-party dependencies")
set(SGL_TEST_DEPS_DOWNLOAD_DIR
    "${CMAKE_SOURCE_DIR}/downloads"
    CACHE PATH "Download cache for test-only third-party source tarballs")

set(SGL_TEST_ZLIB_TAG
    "latest"
    CACHE STRING "zlib release tag used for test dependencies")
set(SGL_TEST_LIBPNG_TAG
    "latest"
    CACHE STRING "libpng release tag used for test dependencies")

function(sgl_resolve_latest_release_tag OUTPUT_VAR REPOSITORY_URL TAG_PATTERN)
    # Test dependencies intentionally follow the latest stable upstream release
    # by default.  Callers can still pin a specific version by setting
    # SGL_TEST_ZLIB_TAG or SGL_TEST_LIBPNG_TAG to an explicit tag value.
    execute_process(
        COMMAND git ls-remote --tags --refs ${REPOSITORY_URL} refs/tags/v*
        RESULT_VARIABLE SGL_TAG_RESULT
        OUTPUT_VARIABLE SGL_TAG_OUTPUT
        ERROR_VARIABLE SGL_TAG_ERROR
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )

    if(NOT SGL_TAG_RESULT EQUAL 0)
        message(FATAL_ERROR
            "Failed to query release tags from ${REPOSITORY_URL}: "
            "${SGL_TAG_ERROR}")
    endif()

    set(SGL_LATEST_TAG "")
    set(SGL_LATEST_VERSION "0")
    string(REGEX MATCHALL "refs/tags/[^ \t\r\n]+" SGL_TAG_REFS
        "${SGL_TAG_OUTPUT}")

    foreach(SGL_TAG_REF IN LISTS SGL_TAG_REFS)
        string(REPLACE "refs/tags/" "" SGL_TAG "${SGL_TAG_REF}")
        if(SGL_TAG MATCHES "${TAG_PATTERN}")
            string(REGEX REPLACE "^v" "" SGL_TAG_VERSION "${SGL_TAG}")
            if(SGL_TAG_VERSION VERSION_GREATER SGL_LATEST_VERSION)
                set(SGL_LATEST_VERSION "${SGL_TAG_VERSION}")
                set(SGL_LATEST_TAG "${SGL_TAG}")
            endif()
        endif()
    endforeach()

    if(SGL_LATEST_TAG STREQUAL "")
        message(FATAL_ERROR
            "No stable release tag matched ${TAG_PATTERN} from "
            "${REPOSITORY_URL}")
    endif()

    set(${OUTPUT_VAR} "${SGL_LATEST_TAG}" PARENT_SCOPE)
endfunction()

set(SGL_TEST_ZLIB_RESOLVED_TAG "${SGL_TEST_ZLIB_TAG}")
set(SGL_TEST_LIBPNG_RESOLVED_TAG "${SGL_TEST_LIBPNG_TAG}")

if(SGL_TEST_ZLIB_TAG STREQUAL "latest")
    sgl_resolve_latest_release_tag(SGL_TEST_ZLIB_RESOLVED_TAG
        "https://github.com/madler/zlib.git"
        "^v[0-9]+(\\.[0-9]+)*$")
endif()

if(SGL_TEST_LIBPNG_TAG STREQUAL "latest")
    sgl_resolve_latest_release_tag(SGL_TEST_LIBPNG_RESOLVED_TAG
        "https://github.com/pnggroup/libpng.git"
        "^v[0-9]+\\.[0-9]+\\.[0-9]+$")
endif()

message(STATUS
    "test dependency zlib tag: ${SGL_TEST_ZLIB_RESOLVED_TAG}")
message(STATUS
    "test dependency libpng tag: ${SGL_TEST_LIBPNG_RESOLVED_TAG}")

set(SGL_TEST_ZLIB_URL
    "https://github.com/madler/zlib/archive/refs/tags/${SGL_TEST_ZLIB_RESOLVED_TAG}.tar.gz"
    CACHE STRING "zlib source tarball URL used for test dependencies")
set(SGL_TEST_LIBPNG_URL
    "https://github.com/pnggroup/libpng/archive/refs/tags/${SGL_TEST_LIBPNG_RESOLVED_TAG}.tar.gz"
    CACHE STRING "libpng source tarball URL used for test dependencies")

set(SGL_TEST_DEPS_CMAKE_ARGS
    -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
    -DCMAKE_INSTALL_PREFIX=${SGL_TEST_DEPS_INSTALL_DIR}
    -DCMAKE_POSITION_INDEPENDENT_CODE=ON
    -DBUILD_SHARED_LIBS=OFF
)

if(CMAKE_TOOLCHAIN_FILE)
    list(APPEND SGL_TEST_DEPS_CMAKE_ARGS
        -DCMAKE_TOOLCHAIN_FILE=${CMAKE_TOOLCHAIN_FILE})
endif()

file(MAKE_DIRECTORY
    "${SGL_TEST_DEPS_DOWNLOAD_DIR}"
    "${SGL_TEST_DEPS_INSTALL_DIR}/include"
    "${SGL_TEST_DEPS_INSTALL_DIR}/include/libpng16"
    "${SGL_TEST_DEPS_INSTALL_DIR}/lib")

set(SGL_TEST_ZLIB_LIBRARY
    "${SGL_TEST_DEPS_INSTALL_DIR}/lib/${CMAKE_STATIC_LIBRARY_PREFIX}z${CMAKE_STATIC_LIBRARY_SUFFIX}")
set(SGL_TEST_LIBPNG_LIBRARY
    "${SGL_TEST_DEPS_INSTALL_DIR}/lib/${CMAKE_STATIC_LIBRARY_PREFIX}png16${CMAKE_STATIC_LIBRARY_SUFFIX}")

ExternalProject_Add(sgl-test-zlib
    URL ${SGL_TEST_ZLIB_URL}
    DOWNLOAD_DIR "${SGL_TEST_DEPS_DOWNLOAD_DIR}"
    DOWNLOAD_NAME "zlib-${SGL_TEST_ZLIB_RESOLVED_TAG}.tar.gz"
    PREFIX "${CMAKE_BINARY_DIR}/test-deps/zlib"
    CMAKE_ARGS
        ${SGL_TEST_DEPS_CMAKE_ARGS}
        -DZLIB_BUILD_TESTING=OFF
        -DZLIB_BUILD_SHARED=OFF
)

ExternalProject_Add(sgl-test-libpng
    URL ${SGL_TEST_LIBPNG_URL}
    DOWNLOAD_DIR "${SGL_TEST_DEPS_DOWNLOAD_DIR}"
    DOWNLOAD_NAME "libpng-${SGL_TEST_LIBPNG_RESOLVED_TAG}.tar.gz"
    PREFIX "${CMAKE_BINARY_DIR}/test-deps/libpng"
    DEPENDS sgl-test-zlib
    CMAKE_ARGS
        ${SGL_TEST_DEPS_CMAKE_ARGS}
        -DPNG_SHARED=OFF
        -DPNG_STATIC=ON
        -DPNG_TESTS=OFF
        -DPNG_TOOLS=OFF
        -DZLIB_ROOT=${SGL_TEST_DEPS_INSTALL_DIR}
        -DZLIB_INCLUDE_DIR=${SGL_TEST_DEPS_INSTALL_DIR}/include
        -DZLIB_LIBRARY=${SGL_TEST_ZLIB_LIBRARY}
)

add_custom_target(sgl-test-dependencies
    DEPENDS sgl-test-zlib sgl-test-libpng)

add_library(SGLTest::ZLIB STATIC IMPORTED GLOBAL)
set_target_properties(SGLTest::ZLIB PROPERTIES
    IMPORTED_LOCATION "${SGL_TEST_ZLIB_LIBRARY}"
    INTERFACE_INCLUDE_DIRECTORIES "${SGL_TEST_DEPS_INSTALL_DIR}/include")

add_library(SGLTest::PNG STATIC IMPORTED GLOBAL)
set_target_properties(SGLTest::PNG PROPERTIES
    IMPORTED_LOCATION "${SGL_TEST_LIBPNG_LIBRARY}"
    INTERFACE_INCLUDE_DIRECTORIES
        "${SGL_TEST_DEPS_INSTALL_DIR}/include;${SGL_TEST_DEPS_INSTALL_DIR}/include/libpng16"
    INTERFACE_LINK_LIBRARIES "SGLTest::ZLIB;m")
