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

- CPPCHECK_MAX_CTU_DEPTH
  Sets the maximum Cppcheck cross-translation-unit analysis depth used by the
  standalone report script. Defaults to 4.

  Example:
    make cppcheck-report CPPCHECK_MAX_CTU_DEPTH=6

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

- cppcheck-report
  Runs the standalone Cppcheck report script with all available Cppcheck
  checker classes enabled, exhaustive checking, inconclusive findings,
  library checks, CTU analysis and the optional MISRA C:2012 addon. The report
  is saved under `report/<date>-<commit>/` and includes XML, text, active
  checker, MISRA addon summary, summary and metadata files. Cppcheck's native
  `--checkers-report` output lists built-in checker state, so it may still say
  `Misra is not enabled` even when MISRA addon findings are present in the XML
  and text reports. When `cppcheck-htmlreport` is installed, an HTML report is
  also generated at `report/<date>-<commit>/html/index.html`.

  Example:
    make cppcheck-report

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

Resize Benchmark
----------------
The `resize` test application writes a benchmark CSV at:

  benchmark/resize-benchmark.csv

Resized PNG outputs for visual debugging are written separately at:

  build/output/

Open `tools/resize-benchmark-viewer.html` in a browser to inspect the CSV as
latency and thread-scaling charts. The viewer can also export a representative
Markdown snapshot for README or release notes, and download the current chart
view, one chart per interpolation method, or the external backend comparison
chart as SVG. The external SVG export uses the selected size filter; with all
sizes selected it includes every available 4-channel external comparison size
instead of only the largest output.

By default the benchmark only builds SGL rows. Configure with
`-DWITH_BENCHMARK_COMPARE=ON` when optional comparison backends are needed:

| Backend | Methods | Source |
| --- | --- | --- |
| Cairo + pixman | nearest, bilinear | fixed test-only tarballs, built with Meson/Ninja |
| NE10 | bilinear | fixed test-only tarball, built with CMake |

The benchmark records the source channel count in the CSV. When the input path
ends in `sample.png`, the test runner uses prebuilt sibling inputs named
`sample-1ch.png`, `sample-2ch.png`, `sample-3ch.png`, and `sample-4ch.png` if
all four exist; otherwise it runs the single PNG passed on the command line.
The comparison rows use the same output sizes, repeat count, and CSV format as
the SGL rows. SGL `generic` and `simd` rows measure the convenience path that
builds a temporary lookup table per resize call. SGL `generic-lut` and
`simd-lut` rows build the lookup table once per benchmark case and reuse it in
the timed loop, which represents repeated resizing with fixed geometry.
External backends are recorded only as 1-thread baseline rows because they do
not consume the SGL threadpool. Comparison dependencies are downloaded into
`downloads/` and are built only when `WITH_BENCHMARK_COMPARE` is enabled. Cairo
and NE10 rows are raw backend timing checks and should be treated as
experimental until pixel-accuracy validation is added. Cairo and NE10 are
recorded only for 4 channel rows; NE10 provides a bilinear RGBA resize API, so
it is recorded only for bilinear rows.

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
