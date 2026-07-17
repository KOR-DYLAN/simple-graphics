<!--
SPDX-License-Identifier: MIT

Copyright (c) 2025 Dylan Hong

This file is released under the MIT License.
For conditions of distribution and use, see the LICENSE file.
-->

Project Build and Run Guide
===========================

Overview
--------
Simple Graphics Library (SGL) is a small C graphics-processing library focused
on explicit memory management, portable scalar image operations, and optional
ARM NEON acceleration. The repository also contains test applications for
regression checks, PNG-based resize benchmarking, and AArch64 QEMU execution.

The default Make target configures and builds the project. `make run` builds
first, then runs the selected test application. Native host builds and AArch64
cross builds are both supported through CMake toolchain files.

Project Status
--------------
Implemented:

| Area | Current support |
| --- | --- |
| Memory pool | Caller-owned process-wide pool with `sgl_malloc`, `sgl_calloc`, and `sgl_free`. |
| Memory operations | `sgl_memcpy` and `sgl_memset`, with NEON memory routines when available. |
| Resize | Nearest, bilinear, and bicubic resize for 1, 2, 3, and 4 byte-per-pixel inputs. |
| Resize acceleration | Generic scalar path plus ARM NEON SIMD paths when `WITH_SIMD=ON` and NEON is detected. |
| Resize LUT reuse | Optional prebuilt lookup tables for repeated resize operations with fixed geometry. |
| Threading | Optional pthread-backed threadpool on Linux, plus dummy backend when threading is disabled. |
| Queue | Fixed-capacity queue used by tests and threaded execution paths. |
| Test apps | `resize`, `memory`, `queue`, and `sample` applications. |
| Test image I/O | PNG load/save helpers built from test-only zlib-ng/libpng dependencies. |
| Cross-run support | AArch64 Linux toolchains with QEMU runner and detected sysroot. |
| Packaging | Install/export rules, pkg-config metadata, CMake package config, CPack archives. |

Known limitations:

| Area | Current limitation |
| --- | --- |
| Color conversion, crop, rotate | Source files exist, but public API coverage is not exposed in `sgl-core.h` yet. |
| SIMD coverage | NEON paths exist for memory and resize. x86 SIMD flags are detected, but no x86 resize backend is implemented. |
| Thread backend | pthread is supported on Linux. Windows thread detection exists, but the current library implementation is not wired as a Win32 backend. |
| External benchmark backends | Cairo and NE10 rows are timing comparisons only; pixel-accuracy validation is not implemented yet. |
| QEMU execution | QEMU support is intended for AArch64 Linux user-mode binaries, not full-system emulation. |

Planned or likely next support:

| Area | Direction |
| --- | --- |
| Public API expansion | Expose and document color conversion, crop, and rotate operations. |
| Validation | Add pixel-accuracy checks for resize and optional external benchmark backends. |
| SIMD | Expand optimized implementations beyond the current ARM NEON coverage. |
| Platform support | Improve non-Linux threading/runtime coverage after the core APIs stabilize. |
| Benchmark tooling | Keep CSV/SVG reporting useful for comparing compiler flags, SIMD, thread counts, and external backends. |

Quick Start
-----------
Use these commands after installing the packages for your platform.

| Task | Command |
| --- | --- |
| Configure and build | `make build` |
| Run the default resize benchmark | `make run` |
| Run with explicit input/output paths | `make run INPUT=resource/sample.png OUTPUT=build/output` |
| Run a short regression test | `make run TARGET=memory ARGS=` |
| Run the resize app through AArch64 QEMU | `make run TOOLCHAIN=aarch64-none-linux-llvm` |
| Run a short AArch64 QEMU smoke test | `make run TOOLCHAIN=aarch64-none-linux-llvm TARGET=memory ARGS=` |
| Disable cppcheck while iterating locally | `make run WITH_CPPCHECK=OFF` |

Default behavior:

| Default | Value | Meaning |
| --- | --- | --- |
| `TOOLCHAIN` | `llvm` | Build and run native host binaries with Clang/LLVM. |
| `BUILD_TYPE` | `Release` | Configure CMake for optimized release builds. |
| `TARGET` | `resize` | Run the resize benchmark application. |
| `INPUT` | `resource/sample.png` | Input image passed to `resize`. |
| `OUTPUT` | `build/output` | Directory where resized PNG outputs are written. |
| `ARGS` | `$(INPUT) $(OUTPUT)` | Runtime arguments passed to the selected target. |
| `NPROC` | `nproc` output | Number of parallel build jobs. |
| `BUILD` | `build/$(TOOLCHAIN)` | Toolchain-specific CMake build tree. |
| `INSTALL_PREFIX` | `$(BUILD)/install` | Default CMake install prefix when none is provided. |

AArch64 toolchains, such as `aarch64-none-linux-llvm`, run target binaries
through QEMU when QEMU is available.

Directory Layout
----------------
| Path | Purpose |
| --- | --- |
| `Makefile` | Main entry point for configure, build, run, install, and package targets. |
| `build/<TOOLCHAIN>/` | Default CMake build tree for each toolchain. |
| `build/output/` | Default resize output directory shared by run commands. |
| `resource/` | Input resources such as sample PNG images. |
| `script/` | Toolchain, package, QEMU, SIMD, thread, and analysis configuration. |
| `test/` | Sample, benchmark, regression, and shared test utility targets. |
| `library/` | Core SGL library target and implementation modules. |

Prerequisites
-------------
Required for normal test builds:

| Requirement | Notes |
| --- | --- |
| CMake and Make | CMake generates the build tree; Make drives common workflows. |
| C/C++ compiler | LLVM/Clang or GNU toolchain. |
| Git and network access | Used during configure when resolving test dependency release tags. |
| `gawk` | Required while building the test libpng dependency. |
| `cppcheck` | Required only when `WITH_CPPCHECK=ON`. |
| `qemu-aarch64-static` or `qemu-aarch64` | Required only for AArch64 cross-run execution. |
| `nproc` | Used by the default Makefile flow to choose parallel jobs. |

Platform Package Setup
----------------------
Install the host-side packages for your platform:

| Platform | Command |
| --- | --- |
| Ubuntu / WSL | `sudo apt update`<br>`sudo apt install -y build-essential cmake make git gawk clang llvm cppcheck qemu-user-static` |
| Fedora | `sudo dnf install -y gcc gcc-c++ cmake make git gawk clang llvm cppcheck qemu-user-static` |
| Arch Linux | `sudo pacman -S --needed base-devel cmake git gawk clang llvm cppcheck qemu-user-static` |
| macOS / Homebrew | `brew install cmake make git gawk llvm cppcheck qemu coreutils` |

On macOS, use a native toolchain first and pass GNU `nproc` explicitly:

  make TOOLCHAIN=llvm NPROC=$(gnproc)

Native GCC package names:

| Platform | Package |
| --- | --- |
| Ubuntu / WSL | `sudo apt install -y gcc g++` |
| Fedora | `sudo dnf install -y gcc gcc-c++` |
| Arch Linux | `sudo pacman -S --needed gcc` |
| macOS | Use `TOOLCHAIN=llvm` unless a separate GCC setup is needed. |

AArch64 cross builds in this repository use the
`aarch64-none-linux-gnu-` tool prefix. Install the Arm GNU Toolchain for
`aarch64-none-linux-gnu`, then add its `bin` directory to `PATH` so commands
such as `aarch64-none-linux-gnu-gcc` are available:

  export PATH=/opt/arm-gnu-toolchain-<version>-x86_64-aarch64-none-linux-gnu/bin:$PATH

Check the required tools:

  aarch64-none-linux-gnu-gcc -print-sysroot
  qemu-aarch64-static --version

If your platform installs QEMU without the `-static` suffix, check
`qemu-aarch64 --version` instead. CMake looks for `qemu-aarch64-static` first
and falls back to `qemu-aarch64`.

For `TOOLCHAIN=aarch64-none-linux-llvm`, both Clang/LLVM and the Arm GNU
Toolchain are needed. Clang emits AArch64 code, while the GNU toolchain
supplies the sysroot, linker runtime pieces, and target C library. For
`TOOLCHAIN=aarch64-none-linux-gnu`, the same Arm GNU Toolchain is used directly
as the compiler.

Configuration Variables
-----------------------
The following variables can be overridden from the command line:

Most CMake feature options are defined in `option.cmake`. `config.mk` forwards
the supported Make variables to CMake, so they can be written as
`make OPTION=value`. The same options can also be passed directly to CMake as
`-DOPTION=value`.

| Variable | Default | Purpose | Example |
| --- | --- | --- | --- |
| `TOOLCHAIN` | `llvm` | Selects `script/toolchain/<TOOLCHAIN>.cmake`. Supported values: `llvm`, `gnu`, `aarch64-none-linux-llvm`, `aarch64-none-linux-gnu`. | `make TOOLCHAIN=gnu` |
| `BUILD_TYPE` | `Release` | CMake build type: `Debug`, `Release`, `RelWithDebInfo`, or `MinSizeRel`. | `make BUILD_TYPE=Debug` |
| `NPROC` | `nproc` output | Parallel build job count. | `make NPROC=4` |
| `V` | `0` | Set non-zero for verbose build output. | `make V=1` |
| `BUILD` | `build/$(TOOLCHAIN)` | CMake build directory. Separate defaults prevent native and cross caches from mixing. | `make BUILD=build/debug-llvm` |
| `INSTALL_PREFIX` | `$(BUILD)/install` | Forwarded to `CMAKE_INSTALL_PREFIX`. | `make INSTALL_PREFIX=/opt/sgl install` |
| `INPUT` | `resource/sample.png` | Input image path for the default `resize` run. | `make run INPUT=resource/sample-4ch.png` |
| `OUTPUT` | `build/output` | Output directory for resized PNG files. | `make run OUTPUT=/tmp/sgl-output` |
| `ARGS` | `$(INPUT) $(OUTPUT)` | Full argument string passed to the selected test application. | `make run TARGET=memory ARGS=` |

Feature and analysis options:

| Option | Default | Purpose | Example |
| --- | --- | --- | --- |
| `WITH_COMPILER_WARNINGS` | `ON` | Enables common `-Wall` and `-Wextra` warnings for C/C++. | `make WITH_COMPILER_WARNINGS=OFF` |
| `WITH_CPPCHECK` | `ON` | Enables cppcheck analysis for SGL library sources. | `make WITH_CPPCHECK=OFF` |
| `WITH_CPPCHECK_MISRA` | `ON` | Enables the cppcheck MISRA C:2012 addon when cppcheck is enabled. | `make WITH_CPPCHECK_MISRA=OFF` |
| `WITH_CPPCHECK_WARNINGS_AS_ERRORS` | `ON` | Makes cppcheck findings fail the build. | `make WITH_CPPCHECK_WARNINGS_AS_ERRORS=ON` |
| `CPPCHECK_MISRA_RULE_TEXTS` | empty | Optional licensed MISRA rule-headlines file. | `make WITH_CPPCHECK_MISRA=ON CPPCHECK_MISRA_RULE_TEXTS=/path/to/misra.txt` |
| `CPPCHECK_MAX_CTU_DEPTH` | `4` | CTU depth used by the standalone `cppcheck-report` target. | `make cppcheck-report CPPCHECK_MAX_CTU_DEPTH=6` |
| `WITH_TEST_APP` | `ON` | Builds applications under `test/`. | `make WITH_TEST_APP=OFF` |
| `WITH_BENCHMARK_COMPARE` | `OFF` | Builds optional resize comparison backends. May require Meson and Ninja. | `make WITH_BENCHMARK_COMPARE=ON` |
| `WITH_SIMD` | `ON` | Enables architecture-specific SIMD detection and sources. | `make WITH_SIMD=OFF` |
| `WITH_THREAD` | `ON` | Enables platform thread support and the real threadpool backend. | `make WITH_THREAD=OFF` |

Build Targets
-------------
| Target | Purpose | Command |
| --- | --- | --- |
| `all` | Default target. Same as `build`. | `make` |
| `config` | Configure the project using CMake. | `make config` |
| `build` | Build the configured project. | `make build` |
| `install` | Install headers, library, and package metadata. | `make install` |
| `package` | Build binary CPack archives. | `make package` |
| `package-source` | Build source CPack archives. | `make package-source` |
| `clean` | Clean generated build outputs while keeping CMake cache. | `make clean` |
| `distclean` | Remove the entire build directory. | `make distclean` |
| `run` | Build and run the selected test application. | `make run` |
| `list-tests` | Print runnable test applications and their arguments. | `make list-tests` |
| `cppcheck-report` | Generate the standalone cppcheck report under `report/<date>-<commit>/`. | `make cppcheck-report` |

`cppcheck-report` runs the standalone report script with exhaustive checking,
inconclusive findings, library checks, CTU analysis, and the optional MISRA
C:2012 addon. When `cppcheck-htmlreport` is installed, an HTML report is also
generated under the same report directory.

Running the Application
-----------------------
`make run` builds the project if needed, then launches `TARGET` with `ARGS`.

Execution behavior:
- Native toolchains (`llvm`, `gnu`) run the binary directly from the build tree.
- AArch64 cross toolchains (`aarch64-none-linux-llvm`,
  `aarch64-none-linux-gnu`) run the binary through `qemu-aarch64-static`.
- The default `BUILD` path includes the selected `TOOLCHAIN`, so native and
  cross CMake caches do not overwrite each other.
- The cross toolchain writes the detected sysroot to `<BUILD>/sysroot.txt`.
  The generated `<BUILD>/run-test.sh` uses the same sysroot with QEMU:

  qemu-aarch64-static -L <sysroot> <BUILD>/<BUILD_TYPE>/bin/<target> <args>

The default execution command is equivalent to:

  <BUILD>/<BUILD_TYPE>/bin/resize resource/sample.png build/output

Common run commands:

| Task | Command |
| --- | --- |
| AArch64 QEMU run | `make run TOOLCHAIN=aarch64-none-linux-llvm` |
| Short AArch64 QEMU smoke test | `make run TOOLCHAIN=aarch64-none-linux-llvm TARGET=memory ARGS=` |
| Native run | `make run TOOLCHAIN=llvm` |
| List test applications | `make list-tests` |
| Inspect CTest commands | `ctest --test-dir build/<TOOLCHAIN> -N -V` |
| Run all AArch64 CTest entries | `cmake --build build/aarch64-none-linux-llvm --target run-qemu` |

Resize Benchmark
----------------
The `resize` test application produces:

| Output | Path |
| --- | --- |
| Benchmark CSV | `benchmark/resize-benchmark.csv` |
| Resized PNG debug outputs | `build/output/` |

Open `tools/resize-benchmark-viewer.html` in a browser to inspect the CSV as
latency and thread-scaling charts. The viewer can export:

- a representative Markdown snapshot for README or release notes
- the current chart view
- one SVG chart per interpolation method
- an external-backend comparison chart as SVG

By default the benchmark only builds SGL rows. Configure with
`-DWITH_BENCHMARK_COMPARE=ON` when optional comparison backends are needed:

| Backend | Methods | Source |
| --- | --- | --- |
| Cairo + pixman | nearest, bilinear | fixed test-only tarballs, built with Meson/Ninja |
| NE10 | bilinear | fixed test-only tarball, built with CMake |

Benchmark behavior:

- The CSV records the source channel count.
- If the input path ends in `sample.png`, the runner uses sibling inputs named
  `sample-1ch.png`, `sample-2ch.png`, `sample-3ch.png`, and `sample-4ch.png`
  when all four exist.
- Otherwise, the runner uses only the PNG passed on the command line.
- SGL `generic` and `simd` rows measure the convenience path that builds a
  temporary lookup table per resize call.
- SGL `generic-lut` and `simd-lut` rows build the lookup table once per case and
  reuse it in the timed loop.
- External backends are recorded only as 1-thread baseline rows because they do
  not consume the SGL threadpool.
- Cairo and NE10 rows are raw timing checks and should be treated as
  experimental until pixel-accuracy validation is added.
- Cairo and NE10 rows are recorded only for 4-channel inputs. NE10 is recorded
  only for bilinear rows because it provides a bilinear RGBA resize API.

The default sample image is 1920x1080. The benchmark matrix keeps one
downscale output, 640x480, and one upscale output, 2560x1440, to cover both
resize directions without making the default run too large.

The following SVGs are sample runs from `resource/sample.png`. They highlight
4-channel SGL path comparisons across both default resize directions, including
SIMD single-thread rows, prebuilt-LUT rows, and the no-external-LUT convenience
path. The overview keeps all interpolation methods together, while the
method-specific SVGs keep downscale and upscale comparisons easier to scan.
Treat them as benchmark snapshots, not universal claims; CPU model, clock
policy, compiler, build flags, and input image all affect the result.

![SGL resize benchmark summary](benchmark/resize-benchmark-summary.svg)

![SGL nearest resize benchmark](benchmark/resize-benchmark-nearest.svg)

![SGL bilinear resize benchmark](benchmark/resize-benchmark-bilinear.svg)

![SGL bicubic resize benchmark](benchmark/resize-benchmark-bicubic.svg)

Refresh the interpolation-specific SVG snapshots after rerunning the benchmark:

  python3 tools/resize-benchmark-summary-svg.py

The generated SVGs include Cairo/NE10 external comparison rows when the CSV
contains them; otherwise they show that the current CSV has no external backend
rows.

| Environment | Value |
| --- | --- |
| Host CPU | Snapdragon(R) X 12-core X1E80100 |
| Host base clock | 3.42 GHz |
| WSL visible CPU | aarch64, Qualcomm vendor, 8 logical CPUs visible to Linux |
| WSL CPU model string | not reported by Linux inside WSL2 |
| OS | Windows on WSL2, Linux 6.18.33.1-microsoft-standard-WSL2 aarch64 |
| Compiler | GCC 13.3.0, Ubuntu 13.3.0-6ubuntu2~24.04.1 |
| Build | Release |
| Input | resource/sample.png |

SGL 4-channel path timing, using average latency:

| Scenario | SGL generic 1t | SGL simd 1t | SGL simd-lut 1t | SGL simd-lut 8t | Generic / simd-lut 8t |
| --- | ---: | ---: | ---: | ---: | ---: |
| nearest 640x480 | 0.304 ms | 0.378 ms | 0.305 ms | 0.619 ms | 0.49x |
| bilinear 640x480 | 0.701 ms | 0.581 ms | 0.621 ms | 0.316 ms | 2.22x |
| bicubic 640x480 | 8.197 ms | 4.605 ms | 4.562 ms | 1.365 ms | 6.01x |
| nearest 2560x1440 | 3.231 ms | 1.177 ms | 1.213 ms | 0.598 ms | 5.40x |
| bilinear 2560x1440 | 3.943 ms | 3.180 ms | 3.732 ms | 1.400 ms | 2.82x |
| bicubic 2560x1440 | 91.126 ms | 44.975 ms | 42.906 ms | 7.337 ms | 12.42x |

A ratio below 1x means the threaded SIMD LUT path is slower than the generic
1-thread path for that small case.

Cairo/NE10 external comparison, using average latency:

| Scenario | SGL generic 1t | SGL simd 1t | SGL simd-lut 8t | Cairo 1t | NE10 1t | Best external / simd-lut 8t |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| nearest 640x480 | 0.304 ms | 0.378 ms | 0.619 ms | 0.890 ms | n/a | 1.44x slower |
| nearest 2560x1440 | 3.231 ms | 1.177 ms | 0.598 ms | 1.770 ms | n/a | 2.96x slower |
| bilinear 640x480 | 0.701 ms | 0.581 ms | 0.316 ms | 0.364 ms | 0.759 ms | 1.15x slower |
| bilinear 2560x1440 | 3.943 ms | 3.180 ms | 1.400 ms | 3.667 ms | 4.683 ms | 2.62x slower |

Representative optimized SGL thread scaling, using average latency:

| Scenario | simd-lut 1t | simd-lut 8t | Speedup |
| --- | ---: | ---: | ---: |
| nearest 640x480 | 0.305 ms | 0.619 ms | 0.49x |
| bilinear 640x480 | 0.621 ms | 0.316 ms | 1.97x |
| bicubic 640x480 | 4.562 ms | 1.365 ms | 3.34x |
| nearest 2560x1440 | 1.213 ms | 0.598 ms | 2.03x |
| bilinear 2560x1440 | 3.732 ms | 1.400 ms | 2.67x |
| bicubic 2560x1440 | 42.906 ms | 7.337 ms | 5.85x |

Notes
-----
- Toolchain files live under `script/toolchain/<TOOLCHAIN>.cmake`.
- QEMU is required only when running non-host AArch64 binaries.
- The default resize input is `resource/sample.png`.
- The default resize output directory is `build/output`.

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
