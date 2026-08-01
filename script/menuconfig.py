#!/usr/bin/env python3
#
# SPDX-License-Identifier: MIT
#
# Copyright (c) 2025 Dylan Hong
#
# This file is released under the MIT License.
# For conditions of distribution and use, see the LICENSE file.

"""Compatibility entry point for the modular SGL menuconfig package."""

from __future__ import annotations

from pathlib import Path
import sys


SCRIPT_DIR = Path(__file__).resolve().parent
if str(SCRIPT_DIR) not in sys.path:
    sys.path.insert(0, str(SCRIPT_DIR))

from sgl_menuconfig import *  # noqa: E402,F403
from sgl_menuconfig import __all__  # noqa: E402,F401


if __name__ == "__main__":
    sys.exit(main())
