# SPDX-License-Identifier: MIT

"""Command-line dispatch for the SGL configuration tools."""

from __future__ import annotations

import argparse
import curses
import os
from pathlib import Path
import sys

from .build import execute_target_command
from .commands import (
    cmake_args_command,
    menu_command,
    run_command,
    show_command,
    symbols_command,
    sync_command,
)
from .model import KconfigError


def parse_arguments(argv: list[str]) -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="SGL curses build-option and test-run menus"
    )
    parser.add_argument(
        "command", choices=(
            "menu", "run", "show", "sync", "cmake-args",
            "symbols", "execute",
        ),
        help="menu or non-interactive inspection command",
    )
    parser.add_argument(
        "--root", default=".", help="project root directory"
    )
    parser.add_argument(
        "--config", default=None, help="configuration file path"
    )
    parser.add_argument(
        "--kconfig", default=None, help="Kconfig definition path"
    )
    parser.add_argument(
        "--environment", action="store_true",
        help="override config symbols from the process environment",
    )
    parser.add_argument(
        "--execute", action="store_true",
        help="build and execute the test selected by the run menu",
    )
    parser.add_argument(
        "--target", default=None, help="build operation for execute"
    )
    return parser.parse_args(argv)


def main(argv: list[str] | None = None) -> int:
    arguments = parse_arguments(argv if argv is not None else sys.argv[1:])
    root = Path(arguments.root).resolve()
    config_argument = (
        arguments.config
        if arguments.config is not None
        else os.environ.get("CONFIG_FILE")
    )
    config_path = (
        Path(config_argument).resolve()
        if config_argument is not None
        else root / ".config"
    )
    kconfig_argument = (
        arguments.kconfig
        if arguments.kconfig is not None
        else os.environ.get("KCONFIG")
    )
    kconfig_path = (
        Path(kconfig_argument).resolve()
        if kconfig_argument is not None
        else root / "Kconfig"
    )
    result = 1

    try:
        if arguments.command == "menu":
            result = menu_command(config_path, kconfig_path)
        elif arguments.command == "run":
            result = run_command(
                root, config_path, kconfig_path, arguments.execute
            )
        elif arguments.command == "show":
            result = show_command(config_path, kconfig_path)
        elif arguments.command == "sync":
            result = sync_command(config_path, kconfig_path)
        elif arguments.command == "cmake-args":
            result = cmake_args_command(
                config_path, kconfig_path, arguments.environment
            )
        elif arguments.command == "symbols":
            result = symbols_command(kconfig_path)
        else:
            result = execute_target_command(
                root, config_path, kconfig_path, arguments.target
            )
    except (curses.error, KconfigError, OSError, RuntimeError) as error:
        print("menuconfig: {}".format(error), file=sys.stderr)
        result = 2
    except KeyboardInterrupt:
        result = 130

    return result

