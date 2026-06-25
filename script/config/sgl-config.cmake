option(WITH_COMPILER_WARNINGS "Enable common compiler warnings" ON)
option(WITH_CPPCHECK          "Enable cppcheck static analysis" ON)
option(WITH_CPPCHECK_MISRA    "Enable cppcheck MISRA C:2012 addon" ON)
option(WITH_CPPCHECK_WARNINGS_AS_ERRORS
       "Treat cppcheck findings as build errors" ON)
option(WITH_TEST_APP          "Enable Test Application Feature" ON)
option(WITH_SIMD              "Enable SIMD Feature"             ON)
option(WITH_THREAD            "Enable Thread Feature"           ON)

set(CPPCHECK_MISRA_RULE_TEXTS "" CACHE FILEPATH
    "Optional path to the licensed MISRA C rule headlines file")

include(${CMAKE_CURRENT_LIST_DIR}/compiler-warnings.cmake)
include(${CMAKE_CURRENT_LIST_DIR}/cppcheck.cmake)
include(${CMAKE_CURRENT_LIST_DIR}/load-version.cmake)
include(${CMAKE_CURRENT_LIST_DIR}/arch/detect-arch.cmake)
include(${CMAKE_CURRENT_LIST_DIR}/simd/detect-simd.cmake)
include(${CMAKE_CURRENT_LIST_DIR}/thread/detect-thread.cmake)
include(${CMAKE_CURRENT_LIST_DIR}/detect-install-dirs.cmake)

function(sgl_print_configuration_summary)
    if(WITH_COMPILER_WARNINGS AND
       CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
        set(COMPILER_WARNINGS_STATUS
            "ON (-Wall, -Wextra)")
    elseif(WITH_COMPILER_WARNINGS)
        set(COMPILER_WARNINGS_STATUS
            "OFF (unsupported compiler: ${CMAKE_C_COMPILER_ID})")
    else()
        set(COMPILER_WARNINGS_STATUS "OFF")
    endif()

    if(WITH_CPPCHECK)
        set(CPPCHECK_STATUS "ON (${CPPCHECK_EXECUTABLE})")
    else()
        set(CPPCHECK_STATUS "OFF")
    endif()

    if(WITH_CPPCHECK AND WITH_CPPCHECK_MISRA)
        set(CPPCHECK_MISRA_STATUS "ON")
        if(CPPCHECK_MISRA_RULE_TEXTS)
            string(APPEND CPPCHECK_MISRA_STATUS
                " (rule texts: ${CPPCHECK_MISRA_RULE_TEXTS})")
        else()
            string(APPEND CPPCHECK_MISRA_STATUS
                " (without licensed rule texts)")
        endif()
    elseif(WITH_CPPCHECK_MISRA)
        set(CPPCHECK_MISRA_STATUS
            "OFF (cppcheck is disabled)")
    else()
        set(CPPCHECK_MISRA_STATUS "OFF")
    endif()

    if(WITH_CPPCHECK AND WITH_CPPCHECK_WARNINGS_AS_ERRORS)
        set(CPPCHECK_ERRORS_STATUS "ON")
    elseif(WITH_CPPCHECK_WARNINGS_AS_ERRORS)
        set(CPPCHECK_ERRORS_STATUS
            "OFF (cppcheck is disabled)")
    else()
        set(CPPCHECK_ERRORS_STATUS "OFF")
    endif()

    if(WITH_TEST_APP)
        set(TEST_APP_STATUS "ON")
    else()
        set(TEST_APP_STATUS "OFF")
    endif()

    if(SGL_CFG_HAS_SIMD)
        set(SIMD_STATUS "ON")
        if(SGL_CFG_HAS_NEON)
            string(APPEND SIMD_STATUS " (NEON)")
        elseif(SGL_CFG_HAS_AVX2)
            string(APPEND SIMD_STATUS " (AVX2)")
        elseif(SGL_CFG_HAS_SSE42)
            string(APPEND SIMD_STATUS " (SSE4.2)")
        endif()
    elseif(WITH_SIMD)
        set(SIMD_STATUS "OFF (requested, but not detected)")
    else()
        set(SIMD_STATUS "OFF")
    endif()

    if(SGL_CFG_HAS_THREAD)
        if(SGL_CFG_HAS_PTHREAD)
            set(THREAD_STATUS "ON (pthread)")
        elseif(SGL_CFG_HAS_WINTHREAD)
            set(THREAD_STATUS "ON (Win32)")
        else()
            set(THREAD_STATUS "ON")
        endif()
    elseif(WITH_THREAD)
        set(THREAD_STATUS "OFF (requested, but not detected)")
    else()
        set(THREAD_STATUS "OFF")
    endif()

    message(STATUS "")
    message(STATUS "simple-graphics configuration summary")
    message(STATUS "  Compiler warnings ............ ${COMPILER_WARNINGS_STATUS}")
    message(STATUS "  cppcheck ..................... ${CPPCHECK_STATUS}")
    message(STATUS "  cppcheck MISRA C:2012 ........ ${CPPCHECK_MISRA_STATUS}")
    message(STATUS "  cppcheck findings as errors .. ${CPPCHECK_ERRORS_STATUS}")
    message(STATUS "  Test applications ............ ${TEST_APP_STATUS}")
    message(STATUS "  SIMD ......................... ${SIMD_STATUS}")
    message(STATUS "  Threading .................... ${THREAD_STATUS}")
    message(STATUS "")
endfunction()

sgl_print_configuration_summary()

# The user is allowed (but discouraged) to set absolute CMAKE_INSTALL_*DIR paths.
# If they do, we copy these non-relocatable paths into the pkg-config file.
if(IS_ABSOLUTE "${CMAKE_INSTALL_INCLUDEDIR}")
    set(PC_INC_INSTALL_DIR "${CMAKE_INSTALL_INCLUDEDIR}")
else()
    set(PC_INC_INSTALL_DIR "\${prefix}/${CMAKE_INSTALL_INCLUDEDIR}")
endif()

if(IS_ABSOLUTE "${CMAKE_INSTALL_BINDIR}")
    set(PC_BIN_INSTALL_DIR "${CMAKE_INSTALL_BINDIR}")
else()
    set(PC_BIN_INSTALL_DIR "\${exec_prefix}/${CMAKE_INSTALL_BINDIR}")
endif()

if(IS_ABSOLUTE "${CMAKE_INSTALL_LIBDIR}")
    set(PC_LIB_INSTALL_DIR "${CMAKE_INSTALL_LIBDIR}")
else()
    set(PC_LIB_INSTALL_DIR "\${exec_prefix}/${CMAKE_INSTALL_LIBDIR}")
endif()

set(SGL_CORE_EXPORT_NAME sgl-core)
set(SGL_CORE_PC sgl-core.pc)
set(SGL_CORE_PACKAGE_CONFIGNAME sgl-core-package)
configure_file(
    ${CMAKE_CURRENT_LIST_DIR}/${SGL_CORE_PC}.cmakein
    ${CMAKE_BINARY_DIR}/${SGL_CORE_PC} @ONLY
)

configure_file(
    ${CMAKE_CURRENT_LIST_DIR}/sgl-config.h.in
    ${CMAKE_BINARY_DIR}/sgl-config.h
)

include_directories(${CMAKE_BINARY_DIR})
