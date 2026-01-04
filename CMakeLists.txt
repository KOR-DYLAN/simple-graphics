# minimum cmake version
cmake_minimum_required(VERSION 3.16)

# project name
project(simple-graphics-library)

include(script/config/sgl-config.cmake)

# executable
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG            ${CMAKE_BINARY_DIR}/Debug/bin)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE          ${CMAKE_BINARY_DIR}/Release/bin)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO   ${CMAKE_BINARY_DIR}/RelWithDebInfo/bin)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL       ${CMAKE_BINARY_DIR}/MinSizeRel/bin)

# shared library
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_DEBUG            ${CMAKE_BINARY_DIR}/Debug/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_RELEASE          ${CMAKE_BINARY_DIR}/Release/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_RELWITHDEBINFO   ${CMAKE_BINARY_DIR}/RelWithDebInfo/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_MINSIZEREL       ${CMAKE_BINARY_DIR}/MinSizeRel/lib)

# static library
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_DEBUG            ${CMAKE_BINARY_DIR}/Debug/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_RELEASE          ${CMAKE_BINARY_DIR}/Release/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_RELWITHDEBINFO   ${CMAKE_BINARY_DIR}/RelWithDbgInfo/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_MINSIZEREL       ${CMAKE_BINARY_DIR}/MinSizeRel/lib)

# Common Include Dir
include_directories(${CMAKE_SOURCE_DIR}/include)

# Add Subdirectory
add_subdirectory(library)
add_subdirectory(test)

#============================================================================
# CPack Target
#============================================================================
set(CPACK_GENERATOR
    ZIP
    TGZ
)
set(CPACK_SOURCE_GENERATOR
    ZIP
    TGZ
)
set(CPACK_SOURCE_IGNORE_FILES .git/ _CPack_Packages/ package .vscode 3rd-party .gitmodules .gitignore prebuild "${CMAKE_BINARY_DIR}/")

set(CPACK_PACKAGE_NAME "sgl")
set(CPACK_PACKAGE_VERSION ${SGL_FULL_VERSION})
set(CPACK_PACKAGE_DIRECTORY "${CMAKE_SOURCE_DIR}/package")

if("${PROJECT_BINARY_DIR}" STREQUAL "${PROJECT_SOURCE_DIR}")
    message(WARNING "Building to source folder is not recommended. Cpack will be unable to generate source package.")
endif()

include(CPack)
