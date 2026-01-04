# read file: SGL_VERSION
file(READ "${CMAKE_SOURCE_DIR}/SGL_VERSION" _version_file)

# load each version number
string(REGEX MATCH "VERSION_MAJOR[ \t]*=[ \t]*([0-9]+)" _ ${_version_file})
set(SGL_VERSION_MAJOR "${CMAKE_MATCH_1}")

string(REGEX MATCH "VERSION_MINOR[ \t]*=[ \t]*([0-9]+)" _ ${_version_file})
set(SGL_VERSION_MINOR "${CMAKE_MATCH_1}")

string(REGEX MATCH "PATCHLEVEL[ \t]*=[ \t]*([0-9]+)" _ ${_version_file})
set(SGL_VERSION_PATCH "${CMAKE_MATCH_1}")

string(REGEX MATCH "VERSION_TWEAK[ \t]*=[ \t]*([0-9]+)" _ ${_version_file})
set(SGL_VERSION_TWEAK "${CMAKE_MATCH_1}")

string(REGEX MATCH "EXTRAVERSION[ \t]*=[ \t]*([^\n]*)" _ ${_version_file})
set(SGL_VERSION_EXTRA "${CMAKE_MATCH_1}")

# make full-version
set(SGL_FULL_VERSION ${SGL_VERSION_MAJOR}.${SGL_VERSION_MINOR}.${SGL_VERSION_PATCH}.${SGL_VERSION_TWEAK}.${SGL_VERSION_EXTRA})
string(REGEX REPLACE "\\.$" "" SGL_FULL_VERSION "${SGL_FULL_VERSION}")
string(STRIP "${SGL_FULL_VERSION}" SGL_FULL_VERSION)
