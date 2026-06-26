file(READ "${CMAKE_SOURCE_DIR}/SGL_VERSION" _version_file)

function(sgl_read_version_component OUTPUT_VAR PATTERN)
    # SGL_VERSION follows the simple KEY = VALUE format used by the original
    # project.  Keep parsing local to one helper so adding a new component later
    # does not duplicate regex boilerplate.
    string(REGEX MATCH "${PATTERN}" _ "${_version_file}")
    set(${OUTPUT_VAR} "${CMAKE_MATCH_1}" PARENT_SCOPE)
endfunction()

sgl_read_version_component(SGL_VERSION_MAJOR
    "VERSION_MAJOR[ \t]*=[ \t]*([0-9]+)")
sgl_read_version_component(SGL_VERSION_MINOR
    "VERSION_MINOR[ \t]*=[ \t]*([0-9]+)")
sgl_read_version_component(SGL_VERSION_PATCH
    "PATCHLEVEL[ \t]*=[ \t]*([0-9]+)")
sgl_read_version_component(SGL_VERSION_TWEAK
    "VERSION_TWEAK[ \t]*=[ \t]*([0-9]+)")
sgl_read_version_component(SGL_VERSION_EXTRA
    "EXTRAVERSION[ \t]*=[ \t]*([^\n]*)")

set(SGL_FULL_VERSION ${SGL_VERSION_MAJOR}.${SGL_VERSION_MINOR}.${SGL_VERSION_PATCH}.${SGL_VERSION_TWEAK}.${SGL_VERSION_EXTRA})
string(REGEX REPLACE "\\.$" "" SGL_FULL_VERSION "${SGL_FULL_VERSION}")
string(STRIP "${SGL_FULL_VERSION}" SGL_FULL_VERSION)
