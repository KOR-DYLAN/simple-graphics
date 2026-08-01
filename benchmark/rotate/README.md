<!--
SPDX-License-Identifier: MIT

Copyright (c) 2025 Dylan Hong

This file is released under the MIT License.
For conditions of distribution and use, see the LICENSE file.
-->

Rotate Benchmark
================

[Benchmark index](../README.md) | [Main README](../../README.md#operation-benchmarks)

Measures representative 30-, 45-, 60-, 90-, 120-, 180-, 240-, 270-, and 360-degree rotations with direct coordinate calculation and caller-owned LUT reuse. SGL rows use nearest-neighbor sampling. Cairo is reported as single-thread ARGB32 nearest-neighbor. NE10 v1.2.1 is reported as single-thread RGBA bilinear C because upstream AArch64 has no rotate NEON implementation. The chart and table are generated from
[`benchmark.csv`](benchmark.csv) after the benchmark test completes.

Median Latency
--------------
![Rotate Benchmark median latency](latency.svg)

External Backend Coverage
-------------------------
| Library | Measured path | Coverage | Result relation |
| --- | --- | --- | --- |
| Cairo 1.18.4 | ARGB32 nearest-neighbor | All angles, 1 thread, SGL output canvas | Same sampling class |
| NE10 v1.2.1 | RGBA bilinear C | All angles, 1 thread | Different interpolation and canvas rounding |

Cairo surface, context, and pattern setup is outside the timed region; the
timed operation includes `cairo_paint()` and destination surface flush.

Test Environment
----------------
| Environment | Value |
| --- | --- |
| Report generated | 2026-08-01T04:10:19+00:00 |
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
| 0 | rotate-030 | generic | 1 | 2203x1896 | 4,665 | 4,768 | 4,525 | 5,451 | 895,367,202 |
| 1 | rotate-030 | generic-lut | 1 | 2203x1896 | 3,283 | 3,395 | 3,181 | 4,449 | 1,272,277,794 |
| 2 | rotate-030 | simd | 1 | 2203x1896 | 4,678 | 4,720 | 4,515 | 5,151 | 892,879,008 |
| 3 | rotate-030 | simd-lut | 1 | 2203x1896 | 1,816 | 1,813 | 1,517 | 2,189 | 2,300,048,458 |
| 4 | rotate-030 | generic | 4 | 2203x1896 | 1,572 | 1,585 | 1,481 | 1,686 | 2,657,053,435 |
| 5 | rotate-030 | generic-lut | 4 | 2203x1896 | 1,381 | 1,426 | 1,239 | 1,786 | 3,024,538,740 |
| 6 | rotate-030 | simd | 4 | 2203x1896 | 1,584 | 1,581 | 1,495 | 1,649 | 2,636,924,242 |
| 7 | rotate-030 | simd-lut | 4 | 2203x1896 | 996 | 1,030 | 930 | 1,382 | 4,193,662,650 |
| 8 | rotate-045 | generic | 1 | 2122x2122 | 5,113 | 5,146 | 4,955 | 5,452 | 880,673,577 |
| 9 | rotate-045 | generic-lut | 1 | 2122x2122 | 4,150 | 4,155 | 3,682 | 4,879 | 1,085,032,289 |
| 10 | rotate-045 | simd | 1 | 2122x2122 | 5,020 | 5,055 | 4,938 | 5,351 | 896,988,844 |
| 11 | rotate-045 | simd-lut | 1 | 2122x2122 | 1,712 | 1,878 | 1,586 | 2,567 | 2,630,189,252 |
| 12 | rotate-045 | generic | 4 | 2122x2122 | 1,658 | 1,672 | 1,541 | 1,854 | 2,715,852,834 |
| 13 | rotate-045 | generic-lut | 4 | 2122x2122 | 1,391 | 1,476 | 1,260 | 1,898 | 3,237,156,002 |
| 14 | rotate-045 | simd | 4 | 2122x2122 | 1,561 | 1,610 | 1,532 | 1,838 | 2,884,614,990 |
| 15 | rotate-045 | simd-lut | 4 | 2122x2122 | 1,047 | 1,161 | 908 | 2,175 | 4,300,748,806 |
| 16 | rotate-060 | generic | 1 | 1896x2203 | 4,924 | 4,882 | 4,697 | 5,112 | 848,271,324 |
| 17 | rotate-060 | generic-lut | 1 | 1896x2203 | 3,593 | 3,737 | 3,460 | 4,651 | 1,162,507,097 |
| 18 | rotate-060 | simd | 1 | 1896x2203 | 4,849 | 4,999 | 4,719 | 5,824 | 861,391,627 |
| 19 | rotate-060 | simd-lut | 1 | 1896x2203 | 1,823 | 1,857 | 1,651 | 2,437 | 2,291,216,675 |
| 20 | rotate-060 | generic | 4 | 1896x2203 | 1,631 | 1,650 | 1,478 | 1,849 | 2,560,936,848 |
| 21 | rotate-060 | generic-lut | 4 | 1896x2203 | 1,520 | 1,575 | 1,243 | 2,485 | 2,747,952,631 |
| 22 | rotate-060 | simd | 4 | 1896x2203 | 1,788 | 1,859 | 1,589 | 2,239 | 2,336,067,114 |
| 23 | rotate-060 | simd-lut | 4 | 1896x2203 | 1,049 | 1,107 | 906 | 1,530 | 3,981,780,743 |
| 24 | rotate-090 | generic | 1 | 1080x1920 | 3,405 | 3,429 | 3,278 | 3,705 | 608,986,784 |
| 25 | rotate-090 | generic-lut | 1 | 1080x1920 | 2,345 | 2,392 | 2,090 | 3,103 | 884,264,392 |
| 26 | rotate-090 | simd | 1 | 1080x1920 | 778 | 855 | 729 | 1,305 | 2,665,295,629 |
| 27 | rotate-090 | simd-lut | 1 | 1080x1920 | 776 | 849 | 712 | 1,412 | 2,672,164,948 |
| 28 | rotate-090 | generic | 4 | 1080x1920 | 1,152 | 1,186 | 1,076 | 1,465 | 1,800,000,000 |
| 29 | rotate-090 | generic-lut | 4 | 1080x1920 | 840 | 879 | 754 | 1,040 | 2,468,571,428 |
| 30 | rotate-090 | simd | 4 | 1080x1920 | 376 | 401 | 346 | 527 | 5,514,893,617 |
| 31 | rotate-090 | simd-lut | 4 | 1080x1920 | 361 | 378 | 308 | 471 | 5,744,044,321 |
| 32 | rotate-120 | generic | 1 | 1896x2203 | 4,806 | 4,835 | 4,673 | 5,144 | 869,098,626 |
| 33 | rotate-120 | generic-lut | 1 | 1896x2203 | 3,640 | 3,676 | 3,447 | 4,114 | 1,147,496,703 |
| 34 | rotate-120 | simd | 1 | 1896x2203 | 4,757 | 4,766 | 4,651 | 4,951 | 878,050,872 |
| 35 | rotate-120 | simd-lut | 1 | 1896x2203 | 1,762 | 1,831 | 1,653 | 2,452 | 2,370,538,024 |
| 36 | rotate-120 | generic | 4 | 1896x2203 | 1,679 | 1,690 | 1,548 | 1,935 | 2,487,723,645 |
| 37 | rotate-120 | generic-lut | 4 | 1896x2203 | 1,453 | 1,500 | 1,356 | 1,705 | 2,874,664,831 |
| 38 | rotate-120 | simd | 4 | 1896x2203 | 1,559 | 1,641 | 1,458 | 1,892 | 2,679,209,749 |
| 39 | rotate-120 | simd-lut | 4 | 1896x2203 | 1,046 | 1,051 | 859 | 1,373 | 3,993,200,764 |
| 40 | rotate-180 | generic | 1 | 1920x1080 | 3,346 | 3,349 | 3,323 | 3,407 | 619,725,044 |
| 41 | rotate-180 | generic-lut | 1 | 1920x1080 | 1,355 | 1,396 | 1,332 | 1,778 | 1,530,332,103 |
| 42 | rotate-180 | simd | 1 | 1920x1080 | 387 | 452 | 289 | 781 | 5,358,139,534 |
| 43 | rotate-180 | simd-lut | 1 | 1920x1080 | 285 | 329 | 269 | 510 | 7,275,789,473 |
| 44 | rotate-180 | generic | 4 | 1920x1080 | 1,174 | 1,212 | 905 | 1,555 | 1,766,269,165 |
| 45 | rotate-180 | generic-lut | 4 | 1920x1080 | 551 | 623 | 483 | 1,301 | 3,763,339,382 |
| 46 | rotate-180 | simd | 4 | 1920x1080 | 291 | 301 | 277 | 416 | 7,125,773,195 |
| 47 | rotate-180 | simd-lut | 4 | 1920x1080 | 288 | 298 | 264 | 381 | 7,200,000,000 |
| 48 | rotate-240 | generic | 1 | 1896x2203 | 4,914 | 4,934 | 4,793 | 5,179 | 849,997,557 |
| 49 | rotate-240 | generic-lut | 1 | 1896x2203 | 3,648 | 3,707 | 3,538 | 4,198 | 1,144,980,263 |
| 50 | rotate-240 | simd | 1 | 1896x2203 | 4,808 | 5,013 | 4,690 | 5,716 | 868,737,104 |
| 51 | rotate-240 | simd-lut | 1 | 1896x2203 | 1,920 | 2,060 | 1,879 | 2,603 | 2,175,462,500 |
| 52 | rotate-240 | generic | 4 | 1896x2203 | 1,653 | 1,666 | 1,505 | 1,976 | 2,526,852,994 |
| 53 | rotate-240 | generic-lut | 4 | 1896x2203 | 1,371 | 1,401 | 1,309 | 1,583 | 3,046,599,562 |
| 54 | rotate-240 | simd | 4 | 1896x2203 | 1,576 | 1,607 | 1,486 | 1,764 | 2,650,309,644 |
| 55 | rotate-240 | simd-lut | 4 | 1896x2203 | 994 | 985 | 770 | 1,119 | 4,202,100,603 |
| 56 | rotate-270 | generic | 1 | 1080x1920 | 3,392 | 3,461 | 3,296 | 3,917 | 611,320,754 |
| 57 | rotate-270 | generic-lut | 1 | 1080x1920 | 2,082 | 2,117 | 2,050 | 2,342 | 995,965,417 |
| 58 | rotate-270 | simd | 1 | 1080x1920 | 681 | 799 | 646 | 1,207 | 3,044,933,920 |
| 59 | rotate-270 | simd-lut | 1 | 1080x1920 | 900 | 897 | 720 | 1,126 | 2,304,000,000 |
| 60 | rotate-270 | generic | 4 | 1080x1920 | 1,264 | 1,323 | 1,175 | 1,667 | 1,640,506,329 |
| 61 | rotate-270 | generic-lut | 4 | 1080x1920 | 891 | 899 | 805 | 1,075 | 2,327,272,727 |
| 62 | rotate-270 | simd | 4 | 1080x1920 | 518 | 585 | 392 | 1,229 | 4,003,088,803 |
| 63 | rotate-270 | simd-lut | 4 | 1080x1920 | 603 | 611 | 413 | 842 | 3,438,805,970 |
| 64 | rotate-360 | generic | 1 | 1920x1080 | 2,850 | 2,914 | 2,823 | 3,466 | 727,578,947 |
| 65 | rotate-360 | generic-lut | 1 | 1920x1080 | 1,164 | 1,219 | 1,134 | 1,459 | 1,781,443,298 |
| 66 | rotate-360 | simd | 1 | 1920x1080 | 370 | 369 | 265 | 511 | 5,604,324,324 |
| 67 | rotate-360 | simd-lut | 1 | 1920x1080 | 317 | 317 | 267 | 366 | 6,541,324,921 |
| 68 | rotate-360 | generic | 4 | 1920x1080 | 996 | 1,098 | 930 | 1,693 | 2,081,927,710 |
| 69 | rotate-360 | generic-lut | 4 | 1920x1080 | 532 | 534 | 485 | 622 | 3,897,744,360 |
| 70 | rotate-360 | simd | 4 | 1920x1080 | 285 | 298 | 266 | 352 | 7,275,789,473 |
| 71 | rotate-360 | simd-lut | 4 | 1920x1080 | 272 | 283 | 238 | 350 | 7,623,529,411 |
| 72 | rotate-030 | cairo-nearest | 1 | 2203x1896 | 4,523 | 4,515 | 4,028 | 5,388 | 923,477,338 |
| 73 | rotate-045 | cairo-nearest | 1 | 2122x2122 | 4,294 | 4,296 | 3,999 | 4,651 | 1,048,645,551 |
| 74 | rotate-060 | cairo-nearest | 1 | 1896x2203 | 3,869 | 3,969 | 3,643 | 4,640 | 1,079,578,185 |
| 75 | rotate-090 | cairo-nearest | 1 | 1080x1920 | 1,304 | 1,320 | 1,150 | 1,554 | 1,590,184,049 |
| 76 | rotate-120 | cairo-nearest | 1 | 1896x2203 | 3,886 | 3,902 | 3,699 | 4,132 | 1,074,855,378 |
| 77 | rotate-180 | cairo-nearest | 1 | 1920x1080 | 455 | 461 | 432 | 506 | 4,557,362,637 |
| 78 | rotate-240 | cairo-nearest | 1 | 1896x2203 | 3,768 | 3,778 | 3,677 | 3,909 | 1,108,515,923 |
| 79 | rotate-270 | cairo-nearest | 1 | 1080x1920 | 1,282 | 1,325 | 1,016 | 1,705 | 1,617,472,698 |
| 80 | rotate-360 | cairo-nearest | 1 | 1920x1080 | 463 | 460 | 438 | 478 | 4,478,617,710 |
| 81 | rotate-030 | ne10-c-bilinear | 1 | 2203x1896 | 16,486 | 16,542 | 16,257 | 16,938 | 253,359,699 |
| 82 | rotate-045 | ne10-c-bilinear | 1 | 2122x2122 | 17,712 | 17,815 | 16,812 | 18,985 | 254,227,868 |
| 83 | rotate-060 | ne10-c-bilinear | 1 | 1896x2203 | 16,736 | 16,741 | 16,333 | 17,055 | 249,575,047 |
| 84 | rotate-090 | ne10-c-bilinear | 1 | 1081x1921 | 15,462 | 15,523 | 15,346 | 15,829 | 134,303,518 |
| 85 | rotate-120 | ne10-c-bilinear | 1 | 1896x2203 | 16,736 | 16,801 | 16,199 | 17,822 | 249,575,047 |
| 86 | rotate-180 | ne10-c-bilinear | 1 | 1921x1081 | 15,080 | 15,353 | 14,550 | 17,063 | 137,705,636 |
| 87 | rotate-240 | ne10-c-bilinear | 1 | 1896x2203 | 17,131 | 17,020 | 16,610 | 17,356 | 243,820,442 |
| 88 | rotate-270 | ne10-c-bilinear | 1 | 1081x1921 | 16,080 | 16,195 | 15,692 | 16,900 | 129,141,853 |
| 89 | rotate-360 | ne10-c-bilinear | 1 | 1921x1081 | 14,468 | 14,518 | 14,317 | 14,803 | 143,530,619 |

Results depend on the input, CPU frequency policy, scheduler load, compiler,
and build options. Refresh them on the target machine before comparison.
