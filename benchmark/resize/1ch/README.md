<!--
SPDX-License-Identifier: MIT

Copyright (c) 2025 Dylan Hong

This file is released under the MIT License.
For conditions of distribution and use, see the LICENSE file.
-->

SGL Resize Benchmark: 1 Channel
================================

[Benchmark index](../README.md) | [Main README](../../../README.md#resize-benchmark) | **1 channel** | [2 channels](../2ch/README.md) | [3 channels](../3ch/README.md) | [4 channels](../4ch/README.md)

This report uses `resource/sample-1ch.png`. Cairo and NE10 are omitted because the external comparison paths in this benchmark require 4-channel input.

Overview
--------
![SGL 1-channel resize benchmark summary](summary.svg)

Nearest
-------
![SGL 1-channel nearest resize benchmark](nearest.svg)

Bilinear
--------
![SGL 1-channel bilinear resize benchmark](bilinear.svg)

Bicubic
-------
![SGL 1-channel bicubic resize benchmark](bicubic.svg)
