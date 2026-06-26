set(SGL_CFG_HAS_THREAD FALSE)
set(SGL_CFG_HAS_PTHREAD FALSE)
set(SGL_CFG_HAS_WINTHREAD FALSE)

if(NOT WITH_THREAD)
    message(STATUS "Threading is force disabled")
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    # Linux builds use pthread through CMake's Threads package.  The actual
    # imported target is linked by library/sgl-core/CMakeLists.txt.
    message(STATUS "pthread supported: YES")
    set(SGL_CFG_HAS_PTHREAD TRUE)
    set(SGL_CFG_HAS_THREAD TRUE)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Windows")
    message(STATUS "Win32 threads supported: YES")
    set(SGL_CFG_HAS_WINTHREAD TRUE)
    set(SGL_CFG_HAS_THREAD TRUE)
else()
    message(STATUS "Threading is not detected for ${CMAKE_SYSTEM_NAME}")
endif()
