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
set(SGL_TEST_PIXMAN_TAG
    "pixman-0.46.4"
    CACHE STRING "pixman release tag used for resize benchmark comparison")
set(SGL_TEST_CAIRO_TAG
    "1.18.4"
    CACHE STRING "cairo release tag used for resize benchmark comparison")
set(SGL_TEST_NE10_TAG
    "v1.2.1"
    CACHE STRING "NE10 release tag used for resize benchmark comparison")

function(sgl_resolve_latest_release_tag
         OUTPUT_VAR REPOSITORY_URL TAG_GLOB TAG_PATTERN)
    # Test dependencies intentionally follow the latest stable upstream release
    # by default.  Callers can still pin a specific version by setting
    # SGL_TEST_ZLIB_TAG or SGL_TEST_LIBPNG_TAG to an explicit tag value.
    execute_process(
        COMMAND git ls-remote --tags --refs ${REPOSITORY_URL}
                refs/tags/${TAG_GLOB}
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
            string(REGEX MATCH "[0-9].*" SGL_TAG_VERSION "${SGL_TAG}")
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
        "v*"
        "^v[0-9]+(\\.[0-9]+)*$")
endif()

if(SGL_TEST_LIBPNG_TAG STREQUAL "latest")
    sgl_resolve_latest_release_tag(SGL_TEST_LIBPNG_RESOLVED_TAG
        "https://github.com/pnggroup/libpng.git"
        "v*"
        "^v[0-9]+\\.[0-9]+\\.[0-9]+$")
endif()

message(STATUS
    "test dependency zlib tag: ${SGL_TEST_ZLIB_RESOLVED_TAG}")
message(STATUS
    "test dependency libpng tag: ${SGL_TEST_LIBPNG_RESOLVED_TAG}")

if(WITH_BENCHMARK_COMPARE)
    message(STATUS
        "test dependency pixman tag: ${SGL_TEST_PIXMAN_TAG}")
    message(STATUS
        "test dependency cairo tag: ${SGL_TEST_CAIRO_TAG}")
    message(STATUS
        "test dependency NE10 tag: ${SGL_TEST_NE10_TAG}")
endif()

set(SGL_TEST_ZLIB_URL
    "https://github.com/madler/zlib/archive/refs/tags/${SGL_TEST_ZLIB_RESOLVED_TAG}.tar.gz"
    CACHE STRING "zlib source tarball URL used for test dependencies")
set(SGL_TEST_LIBPNG_URL
    "https://github.com/pnggroup/libpng/archive/refs/tags/${SGL_TEST_LIBPNG_RESOLVED_TAG}.tar.gz"
    CACHE STRING "libpng source tarball URL used for test dependencies")
set(SGL_TEST_PIXMAN_URL
    "https://gitlab.freedesktop.org/pixman/pixman/-/archive/${SGL_TEST_PIXMAN_TAG}/pixman-${SGL_TEST_PIXMAN_TAG}.tar.gz"
    CACHE STRING "pixman source tarball URL used for benchmark comparison")
set(SGL_TEST_CAIRO_URL
    "https://gitlab.freedesktop.org/cairo/cairo/-/archive/${SGL_TEST_CAIRO_TAG}/cairo-${SGL_TEST_CAIRO_TAG}.tar.gz"
    CACHE STRING "cairo source tarball URL used for benchmark comparison")
set(SGL_TEST_NE10_URL
    "https://github.com/projectNe10/Ne10/archive/refs/tags/${SGL_TEST_NE10_TAG}.tar.gz"
    CACHE STRING "NE10 source tarball URL used for benchmark comparison")

set(SGL_TEST_PIXMAN_ARCHIVE_NAME
    "${SGL_TEST_PIXMAN_TAG}.tar.gz")
set(SGL_TEST_PIXMAN_SOURCE_DIRECTORY
    "pixman-${SGL_TEST_PIXMAN_TAG}")
set(SGL_TEST_CAIRO_ARCHIVE_NAME
    "cairo-${SGL_TEST_CAIRO_TAG}.tar.gz")
set(SGL_TEST_NE10_ARCHIVE_NAME
    "NE10-${SGL_TEST_NE10_TAG}.tar.gz")
set(SGL_TEST_CAIRO_PIXMAN_WRAP_SCRIPT
    "${CMAKE_BINARY_DIR}/test-deps/write-cairo-pixman-wrap.cmake")

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
    "${SGL_TEST_DEPS_INSTALL_DIR}/include/cairo"
    "${SGL_TEST_DEPS_INSTALL_DIR}/include/libpng16"
    "${SGL_TEST_DEPS_INSTALL_DIR}/include/ne10"
    "${SGL_TEST_DEPS_INSTALL_DIR}/include/pixman-1"
    "${SGL_TEST_DEPS_INSTALL_DIR}/lib")

set(SGL_TEST_ZLIB_LIBRARY
    "${SGL_TEST_DEPS_INSTALL_DIR}/lib/${CMAKE_STATIC_LIBRARY_PREFIX}z${CMAKE_STATIC_LIBRARY_SUFFIX}")
set(SGL_TEST_LIBPNG_LIBRARY
    "${SGL_TEST_DEPS_INSTALL_DIR}/lib/${CMAKE_STATIC_LIBRARY_PREFIX}png16${CMAKE_STATIC_LIBRARY_SUFFIX}")
set(SGL_TEST_PIXMAN_LIBRARY
    "${SGL_TEST_DEPS_INSTALL_DIR}/lib/${CMAKE_STATIC_LIBRARY_PREFIX}pixman-1${CMAKE_STATIC_LIBRARY_SUFFIX}")
set(SGL_TEST_CAIRO_LIBRARY
    "${SGL_TEST_DEPS_INSTALL_DIR}/lib/${CMAKE_STATIC_LIBRARY_PREFIX}cairo${CMAKE_STATIC_LIBRARY_SUFFIX}")
set(SGL_TEST_NE10_LIBRARY
    "${SGL_TEST_DEPS_INSTALL_DIR}/lib/${CMAKE_STATIC_LIBRARY_PREFIX}NE10${CMAKE_STATIC_LIBRARY_SUFFIX}")

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

set(SGL_TEST_OPTIONAL_DEPENDENCIES)
set(SGL_TEST_HAS_CAIRO_DEPENDENCY FALSE)
set(SGL_TEST_HAS_NE10_DEPENDENCY FALSE)

if(WITH_BENCHMARK_COMPARE)
    find_program(SGL_TEST_MESON_EXECUTABLE meson)
    find_program(SGL_TEST_NINJA_EXECUTABLE ninja)

    ExternalProject_Add(sgl-test-ne10
        URL ${SGL_TEST_NE10_URL}
        DOWNLOAD_DIR "${SGL_TEST_DEPS_DOWNLOAD_DIR}"
        DOWNLOAD_NAME "${SGL_TEST_NE10_ARCHIVE_NAME}"
        PREFIX "${CMAKE_BINARY_DIR}/test-deps/ne10"
        BINARY_DIR "${CMAKE_BINARY_DIR}/test-deps/ne10/src/sgl-test-ne10-build"
        CMAKE_ARGS
            ${SGL_TEST_DEPS_CMAKE_ARGS}
            -DGNULINUX_PLATFORM=ON
            -DNE10_LINUX_TARGET_ARCH=aarch64
            -DNE10_BUILD_EXAMPLES=OFF
            -DNE10_BUILD_UNIT_TEST=OFF
            -DNE10_BUILD_SHARED=OFF
            -DNE10_BUILD_STATIC=ON
            -DNE10_ENABLE_DSP=OFF
            -DNE10_ENABLE_IMGPROC=ON
        INSTALL_COMMAND
            ${CMAKE_COMMAND} -E copy_directory
                <SOURCE_DIR>/inc
                ${SGL_TEST_DEPS_INSTALL_DIR}/include/ne10
            COMMAND
            ${CMAKE_COMMAND} -E copy
                <BINARY_DIR>/modules/${CMAKE_STATIC_LIBRARY_PREFIX}NE10${CMAKE_STATIC_LIBRARY_SUFFIX}
                ${SGL_TEST_NE10_LIBRARY}
    )
    list(APPEND SGL_TEST_OPTIONAL_DEPENDENCIES sgl-test-ne10)
    set(SGL_TEST_HAS_NE10_DEPENDENCY TRUE)

    if(SGL_TEST_MESON_EXECUTABLE AND SGL_TEST_NINJA_EXECUTABLE)
        file(WRITE "${SGL_TEST_CAIRO_PIXMAN_WRAP_SCRIPT}"
"file(SHA256
    \"${SGL_TEST_DEPS_DOWNLOAD_DIR}/${SGL_TEST_PIXMAN_ARCHIVE_NAME}\"
    SGL_TEST_PIXMAN_SHA256)

file(WRITE
\"\${SGL_TEST_CAIRO_SOURCE_DIR}/subprojects/pixman.wrap\"
\"[wrap-file]
directory = ${SGL_TEST_PIXMAN_SOURCE_DIRECTORY}
source_url = file://${SGL_TEST_DEPS_DOWNLOAD_DIR}/${SGL_TEST_PIXMAN_ARCHIVE_NAME}
source_filename = ${SGL_TEST_PIXMAN_ARCHIVE_NAME}
source_hash = \${SGL_TEST_PIXMAN_SHA256}

[provide]
pixman-1 = idep_pixman
\")
")

        # Keep pixman as its own download target so the tarball lives in the
        # workspace-level download cache.  Cairo still builds pixman through its
        # Meson fallback path because that works even when pkg-config is not
        # installed in a minimal test environment.
        ExternalProject_Add(sgl-test-pixman
            URL ${SGL_TEST_PIXMAN_URL}
            DOWNLOAD_DIR "${SGL_TEST_DEPS_DOWNLOAD_DIR}"
            DOWNLOAD_NAME "${SGL_TEST_PIXMAN_ARCHIVE_NAME}"
            PREFIX "${CMAKE_BINARY_DIR}/test-deps/pixman"
            SOURCE_DIR "${CMAKE_BINARY_DIR}/test-deps/pixman/src/sgl-test-pixman"
            CONFIGURE_COMMAND
                ""
            BUILD_COMMAND
                ""
            INSTALL_COMMAND
                ""
        )

        ExternalProject_Add(sgl-test-cairo
            URL ${SGL_TEST_CAIRO_URL}
            DOWNLOAD_DIR "${SGL_TEST_DEPS_DOWNLOAD_DIR}"
            DOWNLOAD_NAME "${SGL_TEST_CAIRO_ARCHIVE_NAME}"
            PREFIX "${CMAKE_BINARY_DIR}/test-deps/cairo"
            SOURCE_DIR "${CMAKE_BINARY_DIR}/test-deps/cairo/src/sgl-test-cairo"
            BINARY_DIR "${CMAKE_BINARY_DIR}/test-deps/cairo/src/sgl-test-cairo-build"
            DEPENDS sgl-test-pixman
            PATCH_COMMAND
                ${CMAKE_COMMAND}
                    -DSGL_TEST_CAIRO_SOURCE_DIR=<SOURCE_DIR>
                    -P "${SGL_TEST_CAIRO_PIXMAN_WRAP_SCRIPT}"
            CONFIGURE_COMMAND
                ${CMAKE_COMMAND} -E env
                    PKG_CONFIG_PATH=${SGL_TEST_DEPS_INSTALL_DIR}/lib/pkgconfig
                    ${SGL_TEST_MESON_EXECUTABLE} setup
                        <BINARY_DIR>
                        <SOURCE_DIR>
                        --force-fallback-for=pixman-1
                        --prefix=${SGL_TEST_DEPS_INSTALL_DIR}
                        --libdir=lib
                        --buildtype=release
                        --default-library=static
                        -Dtests=disabled
                        -Dpng=disabled
                        -Dzlib=disabled
                        -Dglib=disabled
                        -Dspectre=disabled
                        -Dfreetype=disabled
                        -Dfontconfig=disabled
                        -Dxlib=disabled
                        -Dxcb=disabled
            BUILD_COMMAND
                ${SGL_TEST_MESON_EXECUTABLE} compile -C <BINARY_DIR>
            INSTALL_COMMAND
                ${SGL_TEST_MESON_EXECUTABLE} install -C <BINARY_DIR>
        )

        list(APPEND SGL_TEST_OPTIONAL_DEPENDENCIES
            sgl-test-pixman
            sgl-test-cairo)
        set(SGL_TEST_HAS_CAIRO_DEPENDENCY TRUE)
    else()
        message(STATUS
            "test dependency cairo disabled: meson and ninja are required")
    endif()
else()
    message(STATUS
        "resize benchmark comparison dependencies: disabled")
endif()

add_custom_target(sgl-test-dependencies
    DEPENDS
        sgl-test-zlib
        sgl-test-libpng
        ${SGL_TEST_OPTIONAL_DEPENDENCIES})

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

if(SGL_TEST_HAS_CAIRO_DEPENDENCY)
    add_library(SGLTest::Pixman STATIC IMPORTED GLOBAL)
    set_target_properties(SGLTest::Pixman PROPERTIES
        IMPORTED_LOCATION "${SGL_TEST_PIXMAN_LIBRARY}"
        INTERFACE_INCLUDE_DIRECTORIES
            "${SGL_TEST_DEPS_INSTALL_DIR}/include/pixman-1"
        INTERFACE_LINK_LIBRARIES "m")

    add_library(SGLTest::Cairo STATIC IMPORTED GLOBAL)
    set_target_properties(SGLTest::Cairo PROPERTIES
        IMPORTED_LOCATION "${SGL_TEST_CAIRO_LIBRARY}"
        INTERFACE_INCLUDE_DIRECTORIES
            "${SGL_TEST_DEPS_INSTALL_DIR}/include/cairo;${SGL_TEST_DEPS_INSTALL_DIR}/include/pixman-1"
        INTERFACE_LINK_LIBRARIES "SGLTest::Pixman;m")
endif()

if(SGL_TEST_HAS_NE10_DEPENDENCY)
    add_library(SGLTest::NE10 STATIC IMPORTED GLOBAL)
    set_target_properties(SGLTest::NE10 PROPERTIES
        IMPORTED_LOCATION "${SGL_TEST_NE10_LIBRARY}"
        INTERFACE_INCLUDE_DIRECTORIES
            "${SGL_TEST_DEPS_INSTALL_DIR}/include/ne10"
        INTERFACE_LINK_LIBRARIES "m;stdc++")
    add_dependencies(SGLTest::NE10 sgl-test-ne10)
endif()
