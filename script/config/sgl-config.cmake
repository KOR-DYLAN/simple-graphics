include(${CMAKE_CURRENT_LIST_DIR}/simd/detect-simd.cmake)
include(${CMAKE_CURRENT_LIST_DIR}/thread/detect-thread.cmake)

configure_file(
    ${CMAKE_CURRENT_LIST_DIR}/sgl-config.h.in
    ${CMAKE_CURRENT_BINARY_DIR}/sgl-config.h
)

include_directories(${CMAKE_CURRENT_BINARY_DIR})
