<!--
SPDX-License-Identifier: MIT

Copyright (c) 2025 Dylan Hong

This file is released under the MIT License.
For conditions of distribution and use, see the LICENSE file.
-->

Crop Benchmark
==============

[Benchmark index](../README.md) | [Main README](../../README.md#operation-benchmarks)

Measures full-frame, corner, centered, wide, and tall crop regions across the available backends and worker counts. Cairo is compared with byte-exact ARGB32 SOURCE copies; NE10 has no crop API. The chart and table are generated from
[`benchmark.csv`](benchmark.csv) after the benchmark test completes.

Median Latency
--------------
![Crop Benchmark median latency](latency.svg)

External Backend Coverage
-------------------------
| Library | Measured path | Coverage | Result relation |
| --- | --- | --- | --- |
| Cairo 1.18.4 | ARGB32 SOURCE copy | All crop regions, 1 thread | Byte-exact |
| NE10 v1.2.1 | Not available | No crop API | N/A |

Cairo surface, context, and pattern setup is outside the timed region; the
timed operation includes `cairo_paint()` and destination surface flush.

Test Environment
----------------
| Environment | Value |
| --- | --- |
| Report generated | 2026-08-01T04:09:46+00:00 |
| OS | Ubuntu 24.04.4 LTS |
| Kernel | Linux 6.18.33.2-microsoft-standard-WSL2 (WSL2) |
| Runtime architecture | aarch64 |
| Target | Linux/aarch64 |
| CPU | ARM implementer 0x51, part 0x001 |
| Logical CPUs | 8 |
| Visible memory | 3.81 GiB |
| Execution | native |
| Input | resource/sample.png |
| Source revision | b815597 (dirty) |
| Chart generator | gnuplot 6.0 patchlevel 0 |

Build Configuration
-------------------
| Build option | Value |
| --- | --- |
| Toolchain | llvm |
| Compiler | Clang 18.1.3 |
| Compiler path | /usr/bin/clang |
| Build type | Release |
| C flags | `-O2 -DNDEBUG -std=gnu99 -Wall -Wextra -fno-vectorize -fno-slp-vectorize -fno-builtin-memcpy -fno-builtin-memset` |
| Compiler warnings | ON (-Wall, -Wextra) |
| Auto-vectorization | OFF (-fno-vectorize, -fno-slp-vectorize) |
| SIMD | ON (NEON) |
| Threading | ON (pthread) |
| Sanitizers | OFF |
| Stack protector | OFF |
| External comparison | ON (Cairo/NE10 where supported) |
| Warm-up calls | 3 |
| Measured calls | 10 |

Results
-------
| Case | Operation | Backend | Threads | Size | Median (us) | Average (us) | Min (us) | Max (us) | Pixels/s |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 0 | full-frame | generic | 1 | 1920x1080 | 332 | 329 | 266 | 406 | 6,245,783,132 |
| 1 | full-frame | simd | 1 | 1920x1080 | 303 | 340 | 283 | 553 | 6,843,564,356 |
| 2 | full-frame | generic | 4 | 1920x1080 | 325 | 348 | 263 | 612 | 6,380,307,692 |
| 3 | full-frame | simd | 4 | 1920x1080 | 320 | 333 | 289 | 436 | 6,480,000,000 |
| 4 | top-left-quarter | generic | 1 | 960x540 | 71 | 74 | 65 | 93 | 7,301,408,450 |
| 5 | top-left-quarter | simd | 1 | 960x540 | 69 | 69 | 59 | 81 | 7,513,043,478 |
| 6 | top-left-quarter | generic | 4 | 960x540 | 130 | 120 | 40 | 231 | 3,987,692,307 |
| 7 | top-left-quarter | simd | 4 | 960x540 | 86 | 72 | 28 | 130 | 6,027,906,976 |
| 8 | top-right-quarter | generic | 1 | 960x540 | 77 | 78 | 67 | 93 | 6,732,467,532 |
| 9 | top-right-quarter | simd | 1 | 960x540 | 67 | 71 | 58 | 96 | 7,737,313,432 |
| 10 | top-right-quarter | generic | 4 | 960x540 | 35 | 48 | 26 | 90 | 14,811,428,571 |
| 11 | top-right-quarter | simd | 4 | 960x540 | 34 | 48 | 22 | 99 | 15,247,058,823 |
| 12 | center-half | generic | 1 | 960x540 | 72 | 76 | 68 | 89 | 7,200,000,000 |
| 13 | center-half | simd | 1 | 960x540 | 61 | 62 | 57 | 72 | 8,498,360,655 |
| 14 | center-half | generic | 4 | 960x540 | 33 | 51 | 20 | 100 | 15,709,090,909 |
| 15 | center-half | simd | 4 | 960x540 | 59 | 93 | 27 | 391 | 8,786,440,677 |
| 16 | bottom-left-quarter | generic | 1 | 960x540 | 70 | 72 | 64 | 88 | 7,405,714,285 |
| 17 | bottom-left-quarter | simd | 1 | 960x540 | 59 | 61 | 51 | 88 | 8,786,440,677 |
| 18 | bottom-left-quarter | generic | 4 | 960x540 | 112 | 137 | 39 | 310 | 4,628,571,428 |
| 19 | bottom-left-quarter | simd | 4 | 960x540 | 56 | 70 | 33 | 140 | 9,257,142,857 |
| 20 | bottom-right-quarter | generic | 1 | 960x540 | 68 | 68 | 54 | 83 | 7,623,529,411 |
| 21 | bottom-right-quarter | simd | 1 | 960x540 | 71 | 70 | 59 | 87 | 7,301,408,450 |
| 22 | bottom-right-quarter | generic | 4 | 960x540 | 91 | 91 | 30 | 171 | 5,696,703,296 |
| 23 | bottom-right-quarter | simd | 4 | 960x540 | 124 | 136 | 62 | 253 | 4,180,645,161 |
| 24 | center-wide | generic | 1 | 1440x270 | 42 | 47 | 36 | 83 | 9,257,142,857 |
| 25 | center-wide | simd | 1 | 1440x270 | 36 | 41 | 33 | 59 | 10,800,000,000 |
| 26 | center-wide | generic | 4 | 1440x270 | 121 | 97 | 18 | 157 | 3,213,223,140 |
| 27 | center-wide | simd | 4 | 1440x270 | 21 | 34 | 18 | 92 | 18,514,285,714 |
| 28 | center-tall | generic | 1 | 480x810 | 49 | 50 | 44 | 62 | 7,934,693,877 |
| 29 | center-tall | simd | 1 | 480x810 | 46 | 47 | 43 | 56 | 8,452,173,913 |
| 30 | center-tall | generic | 4 | 480x810 | 34 | 77 | 19 | 277 | 11,435,294,117 |
| 31 | center-tall | simd | 4 | 480x810 | 89 | 105 | 31 | 191 | 4,368,539,325 |
| 32 | full-frame | cairo | 1 | 1920x1080 | 483 | 483 | 463 | 502 | 4,293,167,701 |
| 33 | top-left-quarter | cairo | 1 | 960x540 | 109 | 133 | 88 | 264 | 4,755,963,302 |
| 34 | top-right-quarter | cairo | 1 | 960x540 | 78 | 80 | 75 | 97 | 6,646,153,846 |
| 35 | center-half | cairo | 1 | 960x540 | 72 | 72 | 67 | 80 | 7,200,000,000 |
| 36 | bottom-left-quarter | cairo | 1 | 960x540 | 77 | 78 | 72 | 90 | 6,732,467,532 |
| 37 | bottom-right-quarter | cairo | 1 | 960x540 | 70 | 73 | 66 | 95 | 7,405,714,285 |
| 38 | center-wide | cairo | 1 | 1440x270 | 44 | 45 | 43 | 54 | 8,836,363,636 |
| 39 | center-tall | cairo | 1 | 480x810 | 57 | 59 | 53 | 79 | 6,821,052,631 |

Results depend on the input, CPU frequency policy, scheduler load, compiler,
and build options. Refresh them on the target machine before comparison.
