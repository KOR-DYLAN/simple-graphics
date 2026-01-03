set(SGL_CFG_HAS_THREAD FALSE)
set(SGL_CFG_HAS_PTHREAD FALSE)
set(SGL_CFG_HAS_WINTHREAD FALSE)

# --------------------------------
# 1. thread detection
# --------------------------------
if (CMAKE_SYSTEM_NAME STREQUAL "Linux")
    message(STATUS "pthread supported: YES")
    set(SGL_CFG_HAS_PTHREAD TRUE)
    set(SGL_CFG_HAS_THREAD TRUE)
elseif (CMAKE_SYSTEM_NAME STREQUAL "Windows")
    message(STATUS "Win32 threads supported: YES")
    set(SGL_CFG_HAS_WINTHREAD TRUE)
    set(SGL_CFG_HAS_THREAD TRUE)
else()
    message(STATUS "Thead is not deteced...")
endif()
