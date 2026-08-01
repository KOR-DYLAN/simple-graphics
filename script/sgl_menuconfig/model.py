# SPDX-License-Identifier: MIT

"""Shared data models for the SGL configuration tools."""

from __future__ import annotations

from dataclasses import dataclass
from pathlib import Path


@dataclass(frozen=True)
class OptionSpec:
    name: str
    label: str
    category: str
    kind: str
    default: str
    description: str
    choices: tuple[str, ...] = ()
    minimum: int = 0
    depends_on: str = ""
    cmake_name: str | None = None
    selects: tuple[str, ...] = ()
    assignments: tuple[tuple[str, str], ...] = ()


@dataclass(frozen=True)
class KconfigModel:
    title: str
    specs: tuple[OptionSpec, ...]


@dataclass(frozen=True)
class BuildContext:
    root: Path
    config_path: Path
    kconfig_path: Path
    model: KconfigModel
    config: dict[str, str]
    values: dict[str, str]
    build: Path
    profile_build: Path
    build_root: Path
    cmake: str
    ctest: str
    cpack: str
    nproc: int
    verbose: bool


class KconfigError(ValueError):
    """Raised when a Kconfig definition or configuration is invalid."""


@dataclass(frozen=True)
class TestSpec:
    name: str
    description: str
    default_args: str

    @property
    def config_key(self) -> str:
        return "RUN_ARGS_" + self.name.upper()


@dataclass(frozen=True)
class DialogLayout:
    top: int
    left: int
    height: int
    width: int

    @property
    def bottom(self) -> int:
        return self.top + self.height - 1

    @property
    def right(self) -> int:
        return self.left + self.width - 1
