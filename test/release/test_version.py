#!/usr/bin/env python3
#
# SPDX-License-Identifier: MIT
#
# Copyright (c) 2025 Dylan Hong
#
# This file is released under the MIT License.
# For conditions of distribution and use, see the LICENSE file.

"""Regression tests for release version and tag validation."""

from __future__ import annotations

import sys
import tempfile
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT))

from script.release.version import VersionError, read_version, validate_tag


class ReleaseVersionTest(unittest.TestCase):
    def write_version(self, contents: str) -> Path:
        temporary_directory = tempfile.TemporaryDirectory()
        self.addCleanup(temporary_directory.cleanup)
        version_file = Path(temporary_directory.name) / "SGL_VERSION"
        version_file.write_text(contents, encoding="ascii")
        return version_file

    def test_reads_release_version(self) -> None:
        version_file = self.write_version(
            "VERSION_MAJOR = 1\n"
            "VERSION_MINOR = 2\n"
            "PATCHLEVEL = 3\n"
            "VERSION_TWEAK = 4\n"
            "EXTRAVERSION =\n"
        )

        self.assertEqual(read_version(version_file), "1.2.3.4")

    def test_preserves_extra_version(self) -> None:
        version_file = self.write_version(
            "VERSION_MAJOR = 1\n"
            "VERSION_MINOR = 2\n"
            "PATCHLEVEL = 3\n"
            "VERSION_TWEAK = 4\n"
            "EXTRAVERSION = rc1\n"
        )

        self.assertEqual(read_version(version_file), "1.2.3.4.rc1")

    def test_rejects_missing_component(self) -> None:
        version_file = self.write_version(
            "VERSION_MAJOR = 1\n"
            "VERSION_MINOR = 2\n"
            "PATCHLEVEL = 3\n"
            "EXTRAVERSION =\n"
        )

        with self.assertRaisesRegex(VersionError, "VERSION_TWEAK"):
            read_version(version_file)

    def test_rejects_non_numeric_component(self) -> None:
        version_file = self.write_version(
            "VERSION_MAJOR = one\n"
            "VERSION_MINOR = 2\n"
            "PATCHLEVEL = 3\n"
            "VERSION_TWEAK = 4\n"
            "EXTRAVERSION =\n"
        )

        with self.assertRaisesRegex(VersionError, "VERSION_MAJOR must be numeric"):
            read_version(version_file)

    def test_accepts_matching_release_tag(self) -> None:
        validate_tag("1.2.3.4", "v1.2.3.4")

    def test_rejects_mismatched_release_tag(self) -> None:
        with self.assertRaisesRegex(VersionError, "expected 'v1.2.3.4'"):
            validate_tag("1.2.3.4", "v1.2.3")


if __name__ == "__main__":
    unittest.main()
