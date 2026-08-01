<!--
SPDX-License-Identifier: MIT

Copyright (c) 2025 Dylan Hong

This file is released under the MIT License.
For conditions of distribution and use, see the LICENSE file.
-->

Convert Benchmark: YUV444-PLANAR Source
=======================================

[Convert index](../README.md) | [Benchmark index](../../README.md) | [Main README](../../../README.md#operation-benchmarks)

Every supported RGB and YUV destination is measured from the
`yuv444-planar` source format.

RGB Destinations
----------------
![yuv444-planar to RGB formats median latency](rgb.svg)

YUV Destinations
----------------
![yuv444-planar to YUV formats median latency](yuv.svg)

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
| 1144 | yuv444-planar-to-rgba | generic | 1 | 1280x720 | 9,444 | 9,533 | 9,307 | 10,131 | 97,585,768 |
| 1145 | yuv444-planar-to-rgba | generic | 4 | 1280x720 | 3,094 | 3,102 | 2,899 | 3,329 | 297,866,839 |
| 1146 | yuv444-planar-to-rgba | simd | 1 | 1280x720 | 794 | 818 | 779 | 911 | 1,160,705,289 |
| 1147 | yuv444-planar-to-rgba | simd | 4 | 1280x720 | 395 | 397 | 327 | 466 | 2,333,164,556 |
| 1148 | yuv444-planar-to-argb | generic | 1 | 1280x720 | 8,951 | 9,319 | 8,856 | 10,543 | 102,960,563 |
| 1149 | yuv444-planar-to-argb | generic | 4 | 1280x720 | 3,931 | 4,376 | 2,695 | 9,877 | 234,444,161 |
| 1150 | yuv444-planar-to-argb | simd | 1 | 1280x720 | 928 | 926 | 913 | 952 | 993,103,448 |
| 1151 | yuv444-planar-to-argb | simd | 4 | 1280x720 | 299 | 311 | 279 | 385 | 3,082,274,247 |
| 1152 | yuv444-planar-to-bgra | generic | 1 | 1280x720 | 10,736 | 10,740 | 10,661 | 10,897 | 85,842,026 |
| 1153 | yuv444-planar-to-bgra | generic | 4 | 1280x720 | 2,951 | 3,110 | 2,757 | 4,242 | 312,300,914 |
| 1154 | yuv444-planar-to-bgra | simd | 1 | 1280x720 | 789 | 797 | 778 | 848 | 1,168,060,836 |
| 1155 | yuv444-planar-to-bgra | simd | 4 | 1280x720 | 403 | 420 | 313 | 644 | 2,286,848,635 |
| 1156 | yuv444-planar-to-abgr | generic | 1 | 1280x720 | 10,629 | 10,217 | 9,192 | 11,021 | 86,706,181 |
| 1157 | yuv444-planar-to-abgr | generic | 4 | 1280x720 | 2,961 | 2,971 | 2,817 | 3,143 | 311,246,200 |
| 1158 | yuv444-planar-to-abgr | simd | 1 | 1280x720 | 784 | 801 | 778 | 879 | 1,175,510,204 |
| 1159 | yuv444-planar-to-abgr | simd | 4 | 1280x720 | 450 | 444 | 342 | 514 | 2,048,000,000 |
| 1160 | yuv444-planar-to-rgb | generic | 1 | 1280x720 | 9,205 | 9,183 | 9,050 | 9,342 | 100,119,500 |
| 1161 | yuv444-planar-to-rgb | generic | 4 | 1280x720 | 2,933 | 3,010 | 2,789 | 3,856 | 314,217,524 |
| 1162 | yuv444-planar-to-rgb | simd | 1 | 1280x720 | 788 | 790 | 778 | 818 | 1,169,543,147 |
| 1163 | yuv444-planar-to-rgb | simd | 4 | 1280x720 | 236 | 277 | 232 | 400 | 3,905,084,745 |
| 1164 | yuv444-planar-to-bgr | generic | 1 | 1280x720 | 9,322 | 9,324 | 9,031 | 9,810 | 98,862,904 |
| 1165 | yuv444-planar-to-bgr | generic | 4 | 1280x720 | 2,930 | 2,911 | 2,774 | 3,000 | 314,539,249 |
| 1166 | yuv444-planar-to-bgr | simd | 1 | 1280x720 | 802 | 826 | 778 | 942 | 1,149,127,182 |
| 1167 | yuv444-planar-to-bgr | simd | 4 | 1280x720 | 382 | 384 | 353 | 440 | 2,412,565,445 |
| 1168 | yuv444-planar-to-rgba-planar | generic | 1 | 1280x720 | 8,936 | 8,965 | 8,808 | 9,320 | 103,133,393 |
| 1169 | yuv444-planar-to-rgba-planar | generic | 4 | 1280x720 | 2,927 | 3,033 | 2,821 | 3,861 | 314,861,633 |
| 1170 | yuv444-planar-to-rgba-planar | simd | 1 | 1280x720 | 728 | 747 | 716 | 852 | 1,265,934,065 |
| 1171 | yuv444-planar-to-rgba-planar | simd | 4 | 1280x720 | 352 | 366 | 230 | 492 | 2,618,181,818 |
| 1172 | yuv444-planar-to-argb-planar | generic | 1 | 1280x720 | 8,902 | 8,924 | 8,797 | 9,300 | 103,527,297 |
| 1173 | yuv444-planar-to-argb-planar | generic | 4 | 1280x720 | 2,861 | 3,003 | 2,783 | 3,906 | 322,125,131 |
| 1174 | yuv444-planar-to-argb-planar | simd | 1 | 1280x720 | 778 | 794 | 747 | 930 | 1,184,575,835 |
| 1175 | yuv444-planar-to-argb-planar | simd | 4 | 1280x720 | 390 | 401 | 361 | 502 | 2,363,076,923 |
| 1176 | yuv444-planar-to-bgra-planar | generic | 1 | 1280x720 | 8,896 | 8,964 | 8,812 | 9,269 | 103,597,122 |
| 1177 | yuv444-planar-to-bgra-planar | generic | 4 | 1280x720 | 2,824 | 2,822 | 2,754 | 2,895 | 326,345,609 |
| 1178 | yuv444-planar-to-bgra-planar | simd | 1 | 1280x720 | 758 | 764 | 747 | 810 | 1,215,831,134 |
| 1179 | yuv444-planar-to-bgra-planar | simd | 4 | 1280x720 | 371 | 383 | 349 | 426 | 2,484,097,035 |
| 1180 | yuv444-planar-to-abgr-planar | generic | 1 | 1280x720 | 8,911 | 8,919 | 8,769 | 9,131 | 103,422,735 |
| 1181 | yuv444-planar-to-abgr-planar | generic | 4 | 1280x720 | 2,819 | 2,842 | 2,728 | 3,012 | 326,924,441 |
| 1182 | yuv444-planar-to-abgr-planar | simd | 1 | 1280x720 | 757 | 764 | 747 | 809 | 1,217,437,252 |
| 1183 | yuv444-planar-to-abgr-planar | simd | 4 | 1280x720 | 385 | 419 | 354 | 715 | 2,393,766,233 |
| 1184 | yuv444-planar-to-rgb-planar | generic | 1 | 1280x720 | 8,914 | 8,960 | 8,780 | 9,575 | 103,387,929 |
| 1185 | yuv444-planar-to-rgb-planar | generic | 4 | 1280x720 | 2,852 | 2,837 | 2,722 | 3,020 | 323,141,654 |
| 1186 | yuv444-planar-to-rgb-planar | simd | 1 | 1280x720 | 767 | 790 | 747 | 873 | 1,201,564,537 |
| 1187 | yuv444-planar-to-rgb-planar | simd | 4 | 1280x720 | 378 | 373 | 302 | 443 | 2,438,095,238 |
| 1188 | yuv444-planar-to-bgr-planar | generic | 1 | 1280x720 | 8,975 | 8,984 | 8,805 | 9,175 | 102,685,236 |
| 1189 | yuv444-planar-to-bgr-planar | generic | 4 | 1280x720 | 2,856 | 2,860 | 2,801 | 2,958 | 322,689,075 |
| 1190 | yuv444-planar-to-bgr-planar | simd | 1 | 1280x720 | 726 | 751 | 716 | 867 | 1,269,421,487 |
| 1191 | yuv444-planar-to-bgr-planar | simd | 4 | 1280x720 | 350 | 373 | 327 | 451 | 2,633,142,857 |
| 1192 | yuv444-planar-to-yuv444-packed | generic | 1 | 1280x720 | 7,987 | 7,976 | 7,797 | 8,201 | 115,387,504 |
| 1193 | yuv444-planar-to-yuv444-packed | generic | 4 | 1280x720 | 2,569 | 2,600 | 2,482 | 2,933 | 358,738,808 |
| 1194 | yuv444-planar-to-yuv444-packed | simd | 1 | 1280x720 | 187 | 193 | 187 | 231 | 4,928,342,245 |
| 1195 | yuv444-planar-to-yuv444-packed | simd | 4 | 1280x720 | 84 | 94 | 59 | 152 | 10,971,428,571 |
| 1196 | yuv444-planar-to-yuv444-planar | generic | 1 | 1280x720 | 56 | 57 | 55 | 68 | 16,457,142,857 |
| 1197 | yuv444-planar-to-yuv444-planar | generic | 4 | 1280x720 | 55 | 60 | 54 | 95 | 16,756,363,636 |
| 1198 | yuv444-planar-to-yuv444-planar | simd | 1 | 1280x720 | 54 | 54 | 54 | 55 | 17,066,666,666 |
| 1199 | yuv444-planar-to-yuv444-planar | simd | 4 | 1280x720 | 61 | 65 | 54 | 112 | 15,108,196,721 |
| 1200 | yuv444-planar-to-yuyv | generic | 1 | 1280x720 | 6,224 | 6,252 | 6,179 | 6,459 | 148,071,979 |
| 1201 | yuv444-planar-to-yuyv | generic | 4 | 1280x720 | 2,037 | 2,032 | 1,920 | 2,144 | 452,430,044 |
| 1202 | yuv444-planar-to-yuyv | simd | 1 | 1280x720 | 333 | 346 | 332 | 406 | 2,767,567,567 |
| 1203 | yuv444-planar-to-yuyv | simd | 4 | 1280x720 | 200 | 181 | 112 | 220 | 4,608,000,000 |
| 1204 | yuv444-planar-to-yvyu | generic | 1 | 1280x720 | 6,223 | 6,320 | 6,032 | 7,095 | 148,095,773 |
| 1205 | yuv444-planar-to-yvyu | generic | 4 | 1280x720 | 2,072 | 2,065 | 1,928 | 2,169 | 444,787,644 |
| 1206 | yuv444-planar-to-yvyu | simd | 1 | 1280x720 | 357 | 372 | 347 | 472 | 2,581,512,605 |
| 1207 | yuv444-planar-to-yvyu | simd | 4 | 1280x720 | 244 | 232 | 119 | 322 | 3,777,049,180 |
| 1208 | yuv444-planar-to-uyvy | generic | 1 | 1280x720 | 6,357 | 6,381 | 6,271 | 6,639 | 144,974,044 |
| 1209 | yuv444-planar-to-uyvy | generic | 4 | 1280x720 | 2,065 | 2,060 | 1,941 | 2,176 | 446,295,399 |
| 1210 | yuv444-planar-to-uyvy | simd | 1 | 1280x720 | 391 | 409 | 332 | 570 | 2,357,033,248 |
| 1211 | yuv444-planar-to-uyvy | simd | 4 | 1280x720 | 169 | 164 | 102 | 204 | 5,453,254,437 |
| 1212 | yuv444-planar-to-vyuy | generic | 1 | 1280x720 | 6,873 | 6,879 | 6,645 | 7,181 | 134,089,917 |
| 1213 | yuv444-planar-to-vyuy | generic | 4 | 1280x720 | 2,248 | 2,264 | 2,152 | 2,406 | 409,964,412 |
| 1214 | yuv444-planar-to-vyuy | simd | 1 | 1280x720 | 350 | 360 | 346 | 407 | 2,633,142,857 |
| 1215 | yuv444-planar-to-vyuy | simd | 4 | 1280x720 | 197 | 201 | 129 | 322 | 4,678,172,588 |
| 1216 | yuv444-planar-to-yuv422-planar | generic | 1 | 1280x720 | 5,640 | 5,775 | 5,538 | 6,506 | 163,404,255 |
| 1217 | yuv444-planar-to-yuv422-planar | generic | 4 | 1280x720 | 1,875 | 1,991 | 1,750 | 2,782 | 491,520,000 |
| 1218 | yuv444-planar-to-yuv422-planar | simd | 1 | 1280x720 | 492 | 482 | 408 | 542 | 1,873,170,731 |
| 1219 | yuv444-planar-to-yuv422-planar | simd | 4 | 1280x720 | 136 | 142 | 124 | 217 | 6,776,470,588 |
| 1220 | yuv444-planar-to-yuv420-planar | generic | 1 | 1280x720 | 4,377 | 4,383 | 4,148 | 4,653 | 210,555,174 |
| 1221 | yuv444-planar-to-yuv420-planar | generic | 4 | 1280x720 | 1,424 | 1,541 | 1,315 | 2,095 | 647,191,011 |
| 1222 | yuv444-planar-to-yuv420-planar | simd | 1 | 1280x720 | 298 | 306 | 298 | 355 | 3,092,617,449 |
| 1223 | yuv444-planar-to-yuv420-planar | simd | 4 | 1280x720 | 94 | 96 | 93 | 116 | 9,804,255,319 |
| 1224 | yuv444-planar-to-nv12 | generic | 1 | 1280x720 | 4,451 | 4,533 | 4,133 | 5,020 | 207,054,594 |
| 1225 | yuv444-planar-to-nv12 | generic | 4 | 1280x720 | 1,448 | 1,578 | 1,319 | 2,233 | 636,464,088 |
| 1226 | yuv444-planar-to-nv12 | simd | 1 | 1280x720 | 252 | 255 | 251 | 281 | 3,657,142,857 |
| 1227 | yuv444-planar-to-nv12 | simd | 4 | 1280x720 | 80 | 84 | 79 | 102 | 11,520,000,000 |
| 1228 | yuv444-planar-to-nv21 | generic | 1 | 1280x720 | 4,210 | 4,285 | 4,143 | 4,975 | 218,907,363 |
| 1229 | yuv444-planar-to-nv21 | generic | 4 | 1280x720 | 1,382 | 1,485 | 1,345 | 2,219 | 666,859,623 |
| 1230 | yuv444-planar-to-nv21 | simd | 1 | 1280x720 | 251 | 261 | 251 | 322 | 3,671,713,147 |
| 1231 | yuv444-planar-to-nv21 | simd | 4 | 1280x720 | 90 | 106 | 79 | 196 | 10,240,000,000 |

Results depend on the input, CPU frequency policy, scheduler load, compiler,
and build options. Refresh them on the target machine before comparison.
