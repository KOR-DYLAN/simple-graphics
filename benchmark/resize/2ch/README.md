<!--
SPDX-License-Identifier: MIT

Copyright (c) 2025 Dylan Hong

This file is released under the MIT License.
For conditions of distribution and use, see the LICENSE file.
-->

SGL Resize Benchmark: 2 Channels
================================

[Benchmark index](../README.md) | [Main README](../../../README.md#resize-benchmark) | [1 channel](../1ch/README.md) | **2 channels** | [3 channels](../3ch/README.md) | [4 channels](../4ch/README.md)

This report uses `resource/sample-2ch.png`. Cairo and NE10 are omitted because the external comparison paths in this benchmark require 4-channel input.

Overview
--------
![SGL 2-channel resize benchmark summary](summary.svg)

Nearest
-------
![SGL 2-channel nearest resize benchmark](nearest.svg)

Bilinear
--------
![SGL 2-channel bilinear resize benchmark](bilinear.svg)

Bicubic
-------
![SGL 2-channel bicubic resize benchmark](bicubic.svg)
