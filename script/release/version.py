#!/usr/bin/env python3
#
# SPDX-License-Identifier: MIT
#
# Copyright (c) 2025 Dylan Hong
#
# This file is released under the MIT License.
# For conditions of distribution and use, see the LICENSE file.

"""Read SGL_VERSION and validate release tags."""

from __future__ import annotations

import argparse
import re
import sys
from pathlib import Path


VERSION_KEYS = (
    "VERSION_MAJOR",
    "VERSION_MINOR",
    "PATCHLEVEL",
    "VERSION_TWEAK",
    "EXTRAVERSION",
)
NUMERIC_KEYS = VERSION_KEYS[:-1]
ASSIGNMENT_PATTERN = re.compile(r"^([A-Z_]+)[ \t]*=[ \t]*(.*?)[ \t]*$")


class VersionError(ValueError):
    """Report malformed version metadata or a mismatched release tag."""


def read_version(version_file: Path) -> str:
    """Return the CMake-compatible version string from an SGL_VERSION file."""
    values: dict[str, str] = {}

    try:
        lines = version_file.read_text(encoding="ascii").splitlines()
    except OSError as error:
        raise VersionError(f"cannot read {version_file}: {error}") from error

    for line_number, line in enumerate(lines, start=1):
        stripped = line.strip()
        if not stripped or stripped.startswith("#"):
            continue

        match = ASSIGNMENT_PATTERN.fullmatch(line)
        if match is None:
            raise VersionError(
                f"{version_file}:{line_number}: invalid assignment"
            )

        key, value = match.groups()
        if key not in VERSION_KEYS:
            continue
        if key in values:
            raise VersionError(
                f"{version_file}:{line_number}: duplicate {key}"
            )
        values[key] = value

    missing = [key for key in VERSION_KEYS if key not in values]
    if missing:
        raise VersionError(
            f"{version_file}: missing {', '.join(missing)}"
        )

    for key in NUMERIC_KEYS:
        if not values[key].isdigit():
            raise VersionError(f"{version_file}: {key} must be numeric")

    components = [values[key] for key in VERSION_KEYS]
    return ".".join(components).rstrip(".")


def validate_tag(version: str, tag: str) -> None:
    """Require the release tag to be exactly v followed by the project version."""
    expected = f"v{version}"
    if tag != expected:
        raise VersionError(
            f"release tag {tag!r} does not match SGL_VERSION; expected {expected!r}"
        )


def parse_arguments(arguments: list[str]) -> argparse.Namespace:
    """Parse command-line arguments for release automation."""
    default_root = Path(__file__).resolve().parents[2]
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--root",
        type=Path,
        default=default_root,
        help="project root containing SGL_VERSION",
    )
    parser.add_argument(
        "--check-tag",
        metavar="TAG",
        help="require TAG to equal v<project-version>",
    )
    return parser.parse_args(arguments)


def main(arguments: list[str] | None = None) -> int:
    """Print the project version after optional release-tag validation."""
    options = parse_arguments(sys.argv[1:] if arguments is None else arguments)

    try:
        version = read_version(options.root / "SGL_VERSION")
        if options.check_tag is not None:
            validate_tag(version, options.check_tag)
    except VersionError as error:
        print(f"release version error: {error}", file=sys.stderr)
        return 1

    print(version)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
