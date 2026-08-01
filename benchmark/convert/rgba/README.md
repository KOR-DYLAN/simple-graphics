<!--
SPDX-License-Identifier: MIT

Copyright (c) 2025 Dylan Hong

This file is released under the MIT License.
For conditions of distribution and use, see the LICENSE file.
-->

Convert Benchmark: RGBA Source
==============================

[Convert index](../README.md) | [Benchmark index](../../README.md) | [Main README](../../../README.md#operation-benchmarks)

Every supported RGB and YUV destination is measured from the
`rgba` source format.

RGB Destinations
----------------
![rgba to RGB formats median latency](rgb.svg)

YUV Destinations
----------------
![rgba to YUV formats median latency](yuv.svg)

External Backend Coverage
-------------------------
| Library | Measured path | Coverage | Result relation |
| --- | --- | --- | --- |
| Cairo 1.18.4 | ARGB32 SOURCE copy | RGBA/ARGB/BGRA/ABGR identity, 1 thread | Byte-exact |
| NE10 v1.2.1 | Not available | No arbitrary FourCC/color-convert API | N/A |

Test Environment
----------------
| Environment | Value |
| --- | --- |
| Report generated | 2026-08-01T04:11:47+00:00 |
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
| 0 | rgba-to-rgba | generic | 1 | 1280x720 | 82 | 82 | 78 | 86 | 11,239,024,390 |
| 1 | rgba-to-rgba | generic | 4 | 1280x720 | 80 | 87 | 77 | 141 | 11,520,000,000 |
| 2 | rgba-to-rgba | simd | 1 | 1280x720 | 76 | 76 | 71 | 79 | 12,126,315,789 |
| 3 | rgba-to-rgba | simd | 4 | 1280x720 | 73 | 73 | 70 | 83 | 12,624,657,534 |
| 4 | rgba-to-argb | generic | 1 | 1280x720 | 3,941 | 3,965 | 3,903 | 4,100 | 233,849,276 |
| 5 | rgba-to-argb | generic | 4 | 1280x720 | 1,345 | 1,343 | 1,253 | 1,445 | 685,204,460 |
| 6 | rgba-to-argb | simd | 1 | 1280x720 | 350 | 357 | 344 | 419 | 2,633,142,857 |
| 7 | rgba-to-argb | simd | 4 | 1280x720 | 193 | 201 | 173 | 248 | 4,775,129,533 |
| 8 | rgba-to-bgra | generic | 1 | 1280x720 | 4,662 | 4,434 | 3,946 | 4,811 | 197,683,397 |
| 9 | rgba-to-bgra | generic | 4 | 1280x720 | 1,393 | 1,392 | 1,261 | 1,480 | 661,593,682 |
| 10 | rgba-to-bgra | simd | 1 | 1280x720 | 344 | 354 | 344 | 374 | 2,679,069,767 |
| 11 | rgba-to-bgra | simd | 4 | 1280x720 | 190 | 202 | 130 | 368 | 4,850,526,315 |
| 12 | rgba-to-abgr | generic | 1 | 1280x720 | 3,958 | 3,969 | 3,837 | 4,206 | 232,844,871 |
| 13 | rgba-to-abgr | generic | 4 | 1280x720 | 1,300 | 1,314 | 1,261 | 1,384 | 708,923,076 |
| 14 | rgba-to-abgr | simd | 1 | 1280x720 | 344 | 352 | 343 | 380 | 2,679,069,767 |
| 15 | rgba-to-abgr | simd | 4 | 1280x720 | 200 | 207 | 165 | 262 | 4,608,000,000 |
| 16 | rgba-to-rgb | generic | 1 | 1280x720 | 3,936 | 3,950 | 3,857 | 4,072 | 234,146,341 |
| 17 | rgba-to-rgb | generic | 4 | 1280x720 | 1,343 | 1,391 | 1,240 | 1,774 | 686,224,869 |
| 18 | rgba-to-rgb | simd | 1 | 1280x720 | 375 | 392 | 374 | 438 | 2,457,600,000 |
| 19 | rgba-to-rgb | simd | 4 | 1280x720 | 208 | 214 | 150 | 300 | 4,430,769,230 |
| 20 | rgba-to-bgr | generic | 1 | 1280x720 | 3,827 | 3,846 | 3,757 | 3,974 | 240,815,259 |
| 21 | rgba-to-bgr | generic | 4 | 1280x720 | 1,271 | 1,298 | 1,250 | 1,503 | 725,098,347 |
| 22 | rgba-to-bgr | simd | 1 | 1280x720 | 374 | 393 | 374 | 479 | 2,464,171,122 |
| 23 | rgba-to-bgr | simd | 4 | 1280x720 | 238 | 233 | 167 | 291 | 3,872,268,907 |
| 24 | rgba-to-rgba-planar | generic | 1 | 1280x720 | 3,452 | 3,509 | 3,423 | 3,725 | 266,975,666 |
| 25 | rgba-to-rgba-planar | generic | 4 | 1280x720 | 1,173 | 1,185 | 1,133 | 1,288 | 785,677,749 |
| 26 | rgba-to-rgba-planar | simd | 1 | 1280x720 | 312 | 318 | 311 | 342 | 2,953,846,153 |
| 27 | rgba-to-rgba-planar | simd | 4 | 1280x720 | 195 | 202 | 160 | 293 | 4,726,153,846 |
| 28 | rgba-to-argb-planar | generic | 1 | 1280x720 | 4,046 | 4,142 | 3,821 | 4,644 | 227,780,523 |
| 29 | rgba-to-argb-planar | generic | 4 | 1280x720 | 1,320 | 1,326 | 1,236 | 1,426 | 698,181,818 |
| 30 | rgba-to-argb-planar | simd | 1 | 1280x720 | 312 | 323 | 311 | 393 | 2,953,846,153 |
| 31 | rgba-to-argb-planar | simd | 4 | 1280x720 | 165 | 172 | 107 | 280 | 5,585,454,545 |
| 32 | rgba-to-bgra-planar | generic | 1 | 1280x720 | 4,550 | 4,558 | 4,530 | 4,631 | 202,549,450 |
| 33 | rgba-to-bgra-planar | generic | 4 | 1280x720 | 1,389 | 1,419 | 1,284 | 1,688 | 663,498,920 |
| 34 | rgba-to-bgra-planar | simd | 1 | 1280x720 | 372 | 381 | 312 | 471 | 2,477,419,354 |
| 35 | rgba-to-bgra-planar | simd | 4 | 1280x720 | 172 | 159 | 99 | 219 | 5,358,139,534 |
| 36 | rgba-to-abgr-planar | generic | 1 | 1280x720 | 3,869 | 3,977 | 3,800 | 4,475 | 238,201,085 |
| 37 | rgba-to-abgr-planar | generic | 4 | 1280x720 | 1,322 | 1,331 | 1,255 | 1,435 | 697,125,567 |
| 38 | rgba-to-abgr-planar | simd | 1 | 1280x720 | 312 | 317 | 311 | 339 | 2,953,846,153 |
| 39 | rgba-to-abgr-planar | simd | 4 | 1280x720 | 177 | 171 | 96 | 246 | 5,206,779,661 |
| 40 | rgba-to-rgb-planar | generic | 1 | 1280x720 | 3,587 | 3,600 | 3,545 | 3,675 | 256,927,794 |
| 41 | rgba-to-rgb-planar | generic | 4 | 1280x720 | 1,232 | 1,234 | 1,208 | 1,270 | 748,051,948 |
| 42 | rgba-to-rgb-planar | simd | 1 | 1280x720 | 333 | 334 | 311 | 370 | 2,767,567,567 |
| 43 | rgba-to-rgb-planar | simd | 4 | 1280x720 | 193 | 201 | 152 | 309 | 4,775,129,533 |
| 44 | rgba-to-bgr-planar | generic | 1 | 1280x720 | 3,716 | 3,695 | 3,343 | 4,150 | 248,008,611 |
| 45 | rgba-to-bgr-planar | generic | 4 | 1280x720 | 1,150 | 1,154 | 1,114 | 1,209 | 801,391,304 |
| 46 | rgba-to-bgr-planar | simd | 1 | 1280x720 | 360 | 352 | 312 | 408 | 2,560,000,000 |
| 47 | rgba-to-bgr-planar | simd | 4 | 1280x720 | 159 | 163 | 108 | 223 | 5,796,226,415 |
| 48 | rgba-to-yuv444-packed | generic | 1 | 1280x720 | 14,332 | 14,388 | 14,188 | 14,859 | 64,303,656 |
| 49 | rgba-to-yuv444-packed | generic | 4 | 1280x720 | 4,403 | 4,452 | 4,345 | 4,835 | 209,311,832 |
| 50 | rgba-to-yuv444-packed | simd | 1 | 1280x720 | 1,045 | 1,060 | 1,027 | 1,148 | 881,913,875 |
| 51 | rgba-to-yuv444-packed | simd | 4 | 1280x720 | 498 | 505 | 424 | 601 | 1,850,602,409 |
| 52 | rgba-to-yuv444-planar | generic | 1 | 1280x720 | 14,788 | 14,972 | 14,176 | 16,244 | 62,320,800 |
| 53 | rgba-to-yuv444-planar | generic | 4 | 1280x720 | 4,491 | 4,481 | 4,318 | 4,676 | 205,210,420 |
| 54 | rgba-to-yuv444-planar | simd | 1 | 1280x720 | 1,024 | 1,035 | 995 | 1,151 | 900,000,000 |
| 55 | rgba-to-yuv444-planar | simd | 4 | 1280x720 | 498 | 516 | 432 | 717 | 1,850,602,409 |
| 56 | rgba-to-yuyv | generic | 1 | 1280x720 | 12,533 | 12,633 | 12,309 | 13,457 | 73,533,870 |
| 57 | rgba-to-yuyv | generic | 4 | 1280x720 | 3,837 | 3,856 | 3,740 | 4,079 | 240,187,646 |
| 58 | rgba-to-yuyv | simd | 1 | 1280x720 | 1,084 | 1,094 | 1,062 | 1,197 | 850,184,501 |
| 59 | rgba-to-yuyv | simd | 4 | 1280x720 | 477 | 487 | 444 | 565 | 1,932,075,471 |
| 60 | rgba-to-yvyu | generic | 1 | 1280x720 | 12,420 | 12,512 | 12,317 | 13,038 | 74,202,898 |
| 61 | rgba-to-yvyu | generic | 4 | 1280x720 | 3,923 | 4,005 | 3,699 | 4,589 | 234,922,253 |
| 62 | rgba-to-yvyu | simd | 1 | 1280x720 | 1,111 | 1,145 | 1,092 | 1,277 | 829,522,952 |
| 63 | rgba-to-yvyu | simd | 4 | 1280x720 | 527 | 520 | 421 | 616 | 1,748,766,603 |
| 64 | rgba-to-uyvy | generic | 1 | 1280x720 | 12,770 | 12,756 | 12,550 | 12,999 | 72,169,146 |
| 65 | rgba-to-uyvy | generic | 4 | 1280x720 | 4,004 | 4,032 | 3,790 | 4,503 | 230,169,830 |
| 66 | rgba-to-uyvy | simd | 1 | 1280x720 | 1,081 | 1,092 | 1,062 | 1,155 | 852,543,940 |
| 67 | rgba-to-uyvy | simd | 4 | 1280x720 | 532 | 545 | 470 | 676 | 1,732,330,827 |
| 68 | rgba-to-vyuy | generic | 1 | 1280x720 | 13,074 | 13,136 | 12,953 | 13,502 | 70,491,050 |
| 69 | rgba-to-vyuy | generic | 4 | 1280x720 | 4,086 | 4,094 | 4,013 | 4,194 | 225,550,660 |
| 70 | rgba-to-vyuy | simd | 1 | 1280x720 | 1,107 | 1,123 | 1,092 | 1,261 | 832,520,325 |
| 71 | rgba-to-vyuy | simd | 4 | 1280x720 | 495 | 511 | 486 | 588 | 1,861,818,181 |
| 72 | rgba-to-yuv422-planar | generic | 1 | 1280x720 | 11,980 | 11,991 | 11,823 | 12,354 | 76,928,213 |
| 73 | rgba-to-yuv422-planar | generic | 4 | 1280x720 | 3,773 | 3,783 | 3,714 | 3,901 | 244,261,860 |
| 74 | rgba-to-yuv422-planar | simd | 1 | 1280x720 | 1,110 | 1,117 | 1,093 | 1,156 | 830,270,270 |
| 75 | rgba-to-yuv422-planar | simd | 4 | 1280x720 | 496 | 504 | 449 | 580 | 1,858,064,516 |
| 76 | rgba-to-yuv420-planar | generic | 1 | 1280x720 | 10,543 | 10,565 | 10,426 | 10,832 | 87,413,449 |
| 77 | rgba-to-yuv420-planar | generic | 4 | 1280x720 | 3,360 | 3,384 | 3,207 | 3,641 | 274,285,714 |
| 78 | rgba-to-yuv420-planar | simd | 1 | 1280x720 | 1,045 | 1,076 | 992 | 1,263 | 881,913,875 |
| 79 | rgba-to-yuv420-planar | simd | 4 | 1280x720 | 450 | 456 | 392 | 522 | 2,048,000,000 |
| 80 | rgba-to-nv12 | generic | 1 | 1280x720 | 10,541 | 10,540 | 10,425 | 10,649 | 87,430,035 |
| 81 | rgba-to-nv12 | generic | 4 | 1280x720 | 3,349 | 3,340 | 3,241 | 3,486 | 275,186,622 |
| 82 | rgba-to-nv12 | simd | 1 | 1280x720 | 1,021 | 1,056 | 966 | 1,208 | 902,644,466 |
| 83 | rgba-to-nv12 | simd | 4 | 1280x720 | 425 | 410 | 358 | 456 | 2,168,470,588 |
| 84 | rgba-to-nv21 | generic | 1 | 1280x720 | 10,592 | 10,605 | 10,509 | 10,753 | 87,009,063 |
| 85 | rgba-to-nv21 | generic | 4 | 1280x720 | 3,324 | 3,350 | 3,281 | 3,473 | 277,256,317 |
| 86 | rgba-to-nv21 | simd | 1 | 1280x720 | 994 | 1,034 | 982 | 1,352 | 927,162,977 |
| 87 | rgba-to-nv21 | simd | 4 | 1280x720 | 445 | 457 | 430 | 517 | 2,071,011,235 |
| 1936 | rgba-to-rgba | cairo-copy | 1 | 1280x720 | 138 | 140 | 125 | 168 | 6,678,260,869 |

Results depend on the input, CPU frequency policy, scheduler load, compiler,
and build options. Refresh them on the target machine before comparison.
