Project Build and Run Guide
===========================

Overview
--------
This project uses CMake and Make to build and run a target application.
The default target name is `resize`, and the project supports native
execution as well as cross-compilation for AArch64 using different toolchains.

Directory Layout
----------------
- TOPDIR      : Root directory of the project (where the Makefile is located)
- WORKSPACE   : Same as TOPDIR
- BUILD       : Build output directory (default: <TOPDIR>/build)
- resource/   : Contains input resources (e.g., sample images)
- script/     : Toolchain configuration files

Prerequisites
-------------
- CMake
- Make
- A C/C++ compiler (LLVM or GNU toolchain)
- cppcheck (optional; enables build-time static analysis)
- qemu-aarch64 (required only for non-host execution)
- Linux environment (nproc is used to determine parallel jobs)

Configuration Variables
-----------------------
The following variables can be overridden from the command line:

- TOOLCHAIN
  Selects the toolchain to use.
  Possible values:
    - llvm (default)
    - gnu
    - aarch64-none-linux-llvm
    - aarch64-none-linux-gnu

  Example:
    make TOOLCHAIN=gnu

- BUILD_TYPE
  Specifies the CMake build type.
  Possible values:
    - Debug
    - Release (default)
    - RelWithDebInfo
    - MinSizeRel

  Example:
    make BUILD_TYPE=Debug

- NPROC
  Number of parallel build jobs.
  Default: number of available CPU cores.

- V
  Verbose build output.
  Set to non-zero to enable verbose mode.

  Example:
    make V=1

- WITH_CPPCHECK
  Enables cppcheck analysis for the SGL library sources. Test applications are
  excluded. Defaults to ON, and configuration fails when cppcheck is
  unavailable. Set it to OFF to build without static analysis.

  Example:
    make WITH_CPPCHECK=OFF

- WITH_CPPCHECK_MISRA
  Enables the cppcheck MISRA C:2012 addon for C sources. Defaults to ON and
  requires `WITH_CPPCHECK=ON`. The open-source addon provides partial MISRA
  coverage.

  Example:
    make WITH_CPPCHECK_MISRA=OFF

- CPPCHECK_MISRA_RULE_TEXTS
  Optional path to a licensed MISRA C rule-headlines file. When omitted,
  diagnostics contain rule identifiers without the proprietary rule text.

  Example:
    make WITH_CPPCHECK_MISRA=ON \
      CPPCHECK_MISRA_RULE_TEXTS=/path/to/misra-rule-headlines.txt

- WITH_CPPCHECK_WARNINGS_AS_ERRORS
  Makes cppcheck findings, including enabled MISRA findings, fail the build by
  passing `--error-exitcode=1`. Defaults to OFF.

  Example:
    make WITH_CPPCHECK_WARNINGS_AS_ERRORS=ON

- WITH_COMPILER_WARNINGS
  Enables the commonly used `-Wall` and `-Wextra` compiler warnings for C and
  C++. Defaults to ON.

  Example:
    make WITH_COMPILER_WARNINGS=OFF

Build Targets
-------------
- all
  Default target. Same as `build`.

- config
  Configure the project using CMake.

  Example:
    make config

- build
  Build the project using CMake.

  Example:
    make build

- clean
  Clean build artifacts.

  Example:
    make clean

- distclean
  Remove the entire build directory.

  Example:
    make distclean

Running the Application
-----------------------
- run
  Builds the project (if needed) and runs the target binary.

  Example:
    make run

Execution Behavior:
- If HOSTENV is TRUE (default for llvm and gnu toolchains),
  the binary is executed directly on the host system.
- Otherwise, the binary is executed using qemu-aarch64 with the
  sysroot specified in build/sysroot.txt.

The default execution command is equivalent to:

  <BUILD>/<BUILD_TYPE>/bin/resize resource/sample.png

Notes
-----
- Toolchain files are expected in:
    script/toolchain/<TOOLCHAIN>.cmake
- qemu-aarch64 is only required for non-host (cross) execution.
- The default input argument is `resource/sample.png`.

Memory Pool
-----------
SGL uses `sgl_malloc`, `sgl_calloc`, and `sgl_free` internally. Their calling
conventions match `malloc`, `calloc`, and `free`, but they never use the C
runtime heap. A caller-owned pool must be registered before using an SGL
operation that allocates memory:

```c
static unsigned char sgl_pool[1024 * 1024];

if (sgl_memory_pool_initialize(sgl_pool, sizeof(sgl_pool)) != SGL_SUCCESS) {
    /* handle initialization failure */
}

/* Use SGL normally. */

if (sgl_memory_pool_deinitialize() != SGL_SUCCESS) {
    /* Pool allocations are still alive. */
}
```

The allocator supports variable-size blocks, splits and coalesces free blocks,
and serializes allocation/free operations when thread support is enabled.
Initialize and deinitialize the pool outside concurrent SGL activity.
