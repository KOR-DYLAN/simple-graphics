<!--
SPDX-License-Identifier: MIT

Copyright (c) 2025 Dylan Hong

This file is released under the MIT License.
For conditions of distribution and use, see the LICENSE file.
-->

SGL Benchmarks
==============

| Benchmark | Markdown table and gnuplot charts | Update command |
| --- | --- | --- |
| Convert | [Conversion report](convert/README.md) | `make benchmark-update` |
| Crop | [Crop report](crop/README.md) | `make benchmark-update` |
| Rotate | [Rotation report](rotate/README.md) | `make benchmark-update` |
| Resize | [Channel and interpolation reports](resize/README.md) | `make benchmark-update` |

Measurement and Reports
-----------------------
Each application runs its functional regressions before measuring the case
matrix. By default, every case performs three untimed warm-up calls followed by
ten measured calls. Reports use median latency for charts and retain average,
minimum, maximum, dimensions, backend, worker count, and integer pixels per
second in the generated Markdown tables and CSV data.

Available SGL generic and SIMD paths are measured with one worker and a
four-worker threadpool. Application reports document any LUT reuse, format,
sampling, or external-backend differences that affect comparison. Use
`make menu` to change the warm-up and repeat counts.

CTest writes build-local reports under
`build/<toolchain>/benchmark/<application>/`. Each report records the compiler,
effective C flags, build type, automatic-vectorization/SIMD/thread settings,
OS, kernel, CPU, visible memory, target architecture, execution runner, source
revision, and gnuplot version. Under WSL, Windows `Win32_Processor` supplies the
host CPU name and core counts while the WSL-visible logical CPU count is shown
separately. `/proc/cpuinfo` is used when Windows interop is unavailable.

Updating Snapshots
------------------
Run either command to execute every benchmark and refresh the versioned CSV,
Markdown, and gnuplot SVG snapshots:

    make benchmark-update

    cmake --build build/llvm --target benchmark-update

The report generator rejects incomplete matrices when the selected build
requires SIMD, Cairo, or NE10 rows. `benchmark-svg` remains a Makefile
compatibility alias for `benchmark-update`.

Generated benchmark values are workload and environment dependent. Refresh
the snapshots on the target machine before using them for performance claims.
