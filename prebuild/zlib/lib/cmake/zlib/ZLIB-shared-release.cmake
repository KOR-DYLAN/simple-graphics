#----------------------------------------------------------------
# Generated CMake target import file for configuration "Release".
#----------------------------------------------------------------

# Commands may need to know the format version.
set(CMAKE_IMPORT_FILE_VERSION 1)

# Import target "ZLIB::ZLIB" for configuration "Release"
set_property(TARGET ZLIB::ZLIB APPEND PROPERTY IMPORTED_CONFIGURATIONS RELEASE)
set_target_properties(ZLIB::ZLIB PROPERTIES
  IMPORTED_LOCATION_RELEASE "${_IMPORT_PREFIX}/lib/libz.so.1.4.1.1"
  IMPORTED_SONAME_RELEASE "libz.so.1"
  )

list(APPEND _cmake_import_check_targets ZLIB::ZLIB )
list(APPEND _cmake_import_check_files_for_ZLIB::ZLIB "${_IMPORT_PREFIX}/lib/libz.so.1.4.1.1" )

# Commands beyond this point should not need to know the version.
set(CMAKE_IMPORT_FILE_VERSION)
