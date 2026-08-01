<!--
SPDX-License-Identifier: MIT

Copyright (c) 2025 Dylan Hong

This file is released under the MIT License.
For conditions of distribution and use, see the LICENSE file.
-->

SGL Documentation
=================

[Main README](../README.md)

Environment Setup
-----------------
- [Python package-manager and pyenv setup](python-setup.md)
- [GitHub Pages and Release publishing](github-publishing.md)

User Manual and API Reference
-----------------------------
- Run `make docs` to generate the Doxygen manual at
  `build/<toolchain>/docs/html/index.html`.
- Run `make package-docs` to generate versioned `.tar.gz` and `.zip` release
  archives plus SHA-256 checksums under `package/`.
- When the manual has been generated before `make install` or CPack packaging,
  it is installed under `share/doc/sgl/html`.
- [Doxygen manual sources](api/mainpage.dox)
- [Getting started](api/getting-started.dox)
- [Image operations](api/image-operations.dox)
- [Pixel formats and buffer sizes](api/pixel-formats.dox)
- [Memory, queue, and threading](api/runtime-services.dox)

Core Internals
--------------
- [Memory pool allocator](animations/core/memory-allocator.html)
- [Circular queue](animations/core/queue.html)
- [Thread pool](animations/core/threadpool.html)

Profiling
---------
- [Linux LTTng and Trace Compass profiling](profiling-lttng.md)

Resize Algorithms
-----------------
- [Nearest-neighbor resize](animations/resize/nearest-neighbor.html)
- [Bilinear resize](animations/resize/bilinear.html)
- [Bicubic resize](animations/resize/bicubic.html)
