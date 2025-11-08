include(${CMAKE_CURRENT_LIST_DIR}/common.cmake)

set(MISRA_GCC
    -Wlogical-op                    # Warn on suspicious logical operator usage
    -Wduplicated-cond               # Warn on duplicated conditions in if/else
    -Wduplicated-branches           # Warn if branches have identical code
    -Wformat-overflow               # Warn on possible buffer overflow in printf-like
    -Wformat-truncation             # Warn on possible truncation in snprintf-like
    -Winit-self                     # Warn if variable is initialized with itself
)

set(MISRA_C_WARN_LIST "${MISRA_COMMON} ${MISRA_COMMON_C} ${MISRA_GCC}")
set(MISRA_CXX_WARN_LIST "${MISRA_COMMON} ${MISRA_COMMON_CXX}")
list(JOIN MISRA_C_WARN_LIST " " MISRA_C_WARN)
list(JOIN MISRA_CXX_WARN_LIST " " MISRA_CXX_WARN)
