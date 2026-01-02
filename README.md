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

