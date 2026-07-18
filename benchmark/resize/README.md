<!--
SPDX-License-Identifier: MIT

Copyright (c) 2025 Dylan Hong

This file is released under the MIT License.
For conditions of distribution and use, see the LICENSE file.
-->

Resize Benchmark Reports
========================

[Main README](../../README.md#resize-benchmark) | [Benchmark index](../README.md)

| Input channels | Report | External baselines |
| ---: | --- | --- |
| 1 | [1-channel report](1ch/README.md) | SGL paths only |
| 2 | [2-channel report](2ch/README.md) | SGL paths only |
| 3 | [3-channel report](3ch/README.md) | SGL paths only |
| 4 | [4-channel representative report](4ch/README.md) | Cairo and NE10 where supported |

The reports are generated from `benchmark.csv` by
`tools/resize-benchmark-summary-svg.py`. Run `make benchmark-update` to rerun
the benchmark, validate required rows, and refresh every channel report.
