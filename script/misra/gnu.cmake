include(${CMAKE_CURRENT_LIST_DIR}/common.cmake)

set(MISRA_GCC
    -Wzero-as-null-pointer-constant # Warn if integer 0 used as null pointer
    -Wlogical-op                    # Warn on suspicious logical operator usage
    -Wduplicated-cond               # Warn on duplicated conditions in if/else
    -Wduplicated-branches           # Warn if branches have identical code
    -Wformat-overflow               # Warn on possible buffer overflow in printf-like
    -Wformat-truncation             # Warn on possible truncation in snprintf-like
    -Wclass-memaccess               # Warn on invalid use of memcpy on class members
    -Winit-self                     # Warn if variable is initialized with itself
    -Wself-assign                   # Warn if variable assigned to itself
    -Wself-move                     # Warn if variable moved onto itself
)

set(MISRA_C_WARN_LIST "${MISRA_COMMON} ${MISRA_COMMON_C} ${MISRA_GCC}")
set(MISRA_CXX_WARN_LIST "${MISRA_COMMON} ${MISRA_COMMON_CXX}")
list(JOIN MISRA_C_WARN_LIST " " MISRA_C_WARN)
list(JOIN MISRA_CXX_WARN_LIST " " MISRA_CXX_WARN)
