include(${CMAKE_CURRENT_LIST_DIR}/common.cmake)

set(MISRA_LLVM
    -Wcovered-switch-default # Warn if switch default is unnecessary
    -Wreserved-id-macro      # Warn on use of reserved identifiers in macros
    -Wnewline-eof            # Warn if file does not end with newline
)

set(MISRA_C_WARN_LIST "${MISRA_COMMON} ${MISRA_COMMON_C} ${MISRA_LLVM}")
set(MISRA_CXX_WARN_LIST "${MISRA_COMMON} ${MISRA_COMMON_CXX}")
list(JOIN MISRA_C_WARN_LIST " " MISRA_C_WARN)
list(JOIN MISRA_CXX_WARN_LIST " " MISRA_CXX_WARN)
