# SPDX-License-Identifier: MIT
#
# Copyright (c) 2025 Dylan Hong
#
# This file is released under the MIT License.
# For conditions of distribution and use, see the LICENSE file.

if(NOT WITH_CPPCHECK)
    if(WITH_CPPCHECK_MISRA)
        message(WARNING
            "WITH_CPPCHECK_MISRA is ignored because WITH_CPPCHECK is disabled")
    endif()
    message(STATUS "cppcheck static analysis: disabled")
    return()
endif()

find_program(CPPCHECK_EXECUTABLE NAMES cppcheck)
if(NOT CPPCHECK_EXECUTABLE)
    message(FATAL_ERROR
        "WITH_CPPCHECK is enabled, but cppcheck was not found. "
        "Install cppcheck or configure with -DWITH_CPPCHECK=OFF.")
endif()

set(CPPCHECK_COMMON_OPTIONS
    --inline-suppr
    --quiet
    --suppress=missingIncludeSystem
    --suppressions-list=${CMAKE_CURRENT_LIST_DIR}/suppressions.txt
    --template=gcc
)

if(WITH_CPPCHECK_WARNINGS_AS_ERRORS)
    list(APPEND CPPCHECK_COMMON_OPTIONS --error-exitcode=1)
    message(STATUS "cppcheck findings as errors: enabled")
else()
    message(STATUS "cppcheck findings as errors: disabled")
endif()

set(CPPCHECK_C_ENABLED_CHECKS warning,performance,portability)
set(CPPCHECK_CXX_ENABLED_CHECKS warning,performance,portability)

if(WITH_CPPCHECK_MISRA)
    set(CPPCHECK_C_ENABLED_CHECKS warning,performance,portability,style)
    set(CPPCHECK_MISRA_ARGS_JSON "")
    if(CPPCHECK_MISRA_RULE_TEXTS)
        if(NOT EXISTS "${CPPCHECK_MISRA_RULE_TEXTS}")
            message(FATAL_ERROR
                "CPPCHECK_MISRA_RULE_TEXTS does not exist: "
                "${CPPCHECK_MISRA_RULE_TEXTS}")
        endif()

        file(TO_CMAKE_PATH "${CPPCHECK_MISRA_RULE_TEXTS}"
            CPPCHECK_MISRA_RULE_TEXTS_NORMALIZED)
        string(REPLACE "\"" "\\\""
            CPPCHECK_MISRA_RULE_TEXTS_ESCAPED
            "${CPPCHECK_MISRA_RULE_TEXTS_NORMALIZED}")
        set(CPPCHECK_MISRA_ARGS_JSON
            "\"--rule-texts=${CPPCHECK_MISRA_RULE_TEXTS_ESCAPED}\"")
    endif()

    configure_file(
        ${CMAKE_CURRENT_LIST_DIR}/cppcheck-misra.json.in
        ${CMAKE_BINARY_DIR}/cppcheck-misra.json
        @ONLY
    )

    set(CPPCHECK_MISRA_OPTION
        --addon=${CMAKE_BINARY_DIR}/cppcheck-misra.json)
    message(STATUS "cppcheck MISRA C:2012 addon: enabled")
else()
    message(STATUS "cppcheck MISRA C:2012 addon: disabled")
endif()

set(SGL_C_CPPCHECK
    ${CPPCHECK_EXECUTABLE}
    ${CPPCHECK_COMMON_OPTIONS}
    --enable=${CPPCHECK_C_ENABLED_CHECKS}
    ${CPPCHECK_MISRA_OPTION}
    --std=c99
)
set(SGL_CXX_CPPCHECK
    ${CPPCHECK_EXECUTABLE}
    ${CPPCHECK_COMMON_OPTIONS}
    --enable=${CPPCHECK_CXX_ENABLED_CHECKS}
    --std=c++11
)

message(STATUS "cppcheck static analysis: enabled (${CPPCHECK_EXECUTABLE})")
