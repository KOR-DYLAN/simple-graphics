# Cross-compile for Linux/aarch64 with the GNU cross compiler suite.
#
# TRIPLE intentionally keeps the trailing dash because every GNU tool is named
# by prefixing it to the generic command name, for example:
# aarch64-none-linux-gnu-gcc, aarch64-none-linux-gnu-ar, and so on.
set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR aarch64)

set(TRIPLE "aarch64-none-linux-gnu-")

include(${CMAKE_CURRENT_LIST_DIR}/gnu.cmake)
