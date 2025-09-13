set(MISRA_COMMON
    -Wall                           # Enable common warnings
    -Wextra                         # Enable extra warnings
    -Wpedantic                      # Enforce strict ISO C/C++ compliance
    -Wshadow                        # Warn if a variable shadows another
    -Wconversion                    # Warn on implicit type conversions
    -Wsign-conversion               # Warn on implicit signed/unsigned conversions
    -Wfloat-equal                   # Warn on floating-point equality comparisons
    -Wdouble-promotion              # Warn when float is promoted to double
    -Wcast-qual                     # Warn if const/volatile qualifiers are discarded
    -Wcast-align                    # Warn on potential misaligned pointer casts
    -Wpointer-arith                 # Warn on pointer arithmetic on void* or functions
    -Wundef                         # Warn if undefined macros are used in #if
    -Wunreachable-code              # Warn about unreachable code
    -Wswitch-enum                   # Warn if a switch does not handle all enum values
    -Wimplicit-fallthrough          # Warn when case falls through without annotation
    -Wvla                           # Warn when using variable-length arrays
    -Wmissing-prototypes            # Warn if function defined without prototype
    -Wformat=2                      # Stronger format string checks
    -Wformat-security               # Warn on unsafe format functions
    -Wnull-dereference              # Warn on potential null pointer dereference
    -Wmissing-include-dirs          # Warn if included directory does not exist
    -Wredundant-decls               # Warn on repeated declarations
)

set(MISRA_COMMON_C
    -Wold-style-definition          # Warn on K&R-style function definitions
    -Wstrict-prototypes             # Require parameter types in prototypes
    -Wnested-externs                # Warn if extern appears inside functions
    -Wdeclaration-after-statement   # Warn if code before all declarations
    -Wmissing-declarations          # Warn if global function has no prior declaration
    -Wno-unknown-pragmas            # Suppress warnings for unknown pragmas
)

set(MISRA_COMMON_CXX
    -Wold-style-cast                # Warn on C-style casts
    -Woverloaded-virtual            # Warn if overloaded virtual hides base version
    -Wnon-virtual-dtor              # Warn if class with virtual funcs has no virtual dtor
    -Wnoexcept                      # Warn if noexcept specification is not correct
    -Wuseless-cast                  # Warn if cast has no effect
    -Wctor-dtor-privacy             # Warn if constructor/dtor declared private
    -Wsign-promo                    # Warn on integer promotions in C++
    -Wstrict-null-sentinel          # Warn on non-null as end of vararg function
)