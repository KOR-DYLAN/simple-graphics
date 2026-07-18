<!--
SPDX-License-Identifier: MIT

Copyright (c) 2025 Dylan Hong

This file is released under the MIT License.
For conditions of distribution and use, see the LICENSE file.
-->

SGL Resize Benchmark: 4 Channels
================================

[Benchmark index](../README.md) | [Main README](../../../README.md#resize-benchmark) | [1 channel](../1ch/README.md) | [2 channels](../2ch/README.md) | [3 channels](../3ch/README.md) | **4 channels**

This is the representative benchmark displayed in the main README. It uses `resource/sample-4ch.png` and includes Cairo and NE10 rows where those external backends support the interpolation method.

Overview
--------
![SGL 4-channel resize benchmark summary](summary.svg)

Nearest
-------
![SGL 4-channel nearest resize benchmark](nearest.svg)

Bilinear
--------
![SGL 4-channel bilinear resize benchmark](bilinear.svg)

Bicubic
-------
![SGL 4-channel bicubic resize benchmark](bicubic.svg)
