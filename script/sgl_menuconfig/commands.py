# SPDX-License-Identifier: MIT

"""Interactive and non-interactive menuconfig commands."""

from __future__ import annotations

import curses
import os
from pathlib import Path
import shlex

from .build import execute_run, load_build_context
from .kconfig import (
    build_cmake_arguments,
    build_test_specs,
    config_key_order,
    filter_known_config,
    load_config,
    load_kconfig,
    load_option_values,
    load_run_values,
    migrate_legacy_resize_arguments,
    save_option_values,
    save_run_values,
    synchronize_config,
)
from .ui import OptionMenu, RunMenu, run_curses_menu


def menu_command(config_path: Path, kconfig_path: Path) -> int:
    model = load_kconfig(kconfig_path)
    specs = model.specs
    config = load_config(config_path)
    values = load_option_values(config, specs)
    saved = run_curses_menu(
        lambda window: OptionMenu(
            window, specs, values, config_path, model.title
        ).run()
    )
    result = 0

    if saved:
        save_option_values(config_path, config, values, specs)
        print("Saved build options to {}".format(config_path))
    else:
        print("Build options unchanged")

    return result


def run_command(
    root: Path,
    config_path: Path,
    kconfig_path: Path,
    execute: bool,
) -> int:
    context = load_build_context(root, config_path, kconfig_path)
    model = context.model
    config = context.config
    option_values = context.values
    with_thread = option_values.get("WITH_THREAD", "OFF") == "ON"
    tests = build_test_specs(root, with_thread, context.build)
    migrate_legacy_resize_arguments(config, tests, root)
    selected, arguments = load_run_values(config, tests)
    menu: RunMenu | None = None

    def open_menu(window: curses.window) -> bool:
        nonlocal menu
        menu = RunMenu(
            window, tests, selected, arguments, config_path, root
        )
        return menu.run()

    accepted = run_curses_menu(open_menu)
    result = 0 if execute else 130

    if accepted and menu is not None:
        selected = menu.selected
        merged_config = filter_known_config(config, model.specs)
        merged_config.update(option_values)
        save_run_values(
            config_path, merged_config, tests, selected, arguments,
            model.specs,
        )
        test = tests[selected]
        print("Selected test: {}".format(test.name))
        print("Arguments: {}".format(arguments[test.name] or "<none>"))
        if execute:
            result = execute_run(
                context, test, arguments[test.name]
            )
        else:
            result = 0

    return result


def sync_command(config_path: Path, kconfig_path: Path) -> int:
    synchronize_config(config_path, kconfig_path)
    return 0


def cmake_args_command(
    config_path: Path,
    kconfig_path: Path,
    use_environment: bool,
) -> int:
    model = load_kconfig(kconfig_path)
    config = load_config(config_path)
    if use_environment:
        for spec in model.specs:
            if spec.name in os.environ:
                config[spec.name] = os.environ[spec.name]
    arguments = build_cmake_arguments(model.specs, config)
    print(" ".join(shlex.quote(argument) for argument in arguments))
    return 0
def symbols_command(kconfig_path: Path) -> int:
    model = load_kconfig(kconfig_path)
    print(" ".join(spec.name for spec in model.specs))
    return 0


def show_command(config_path: Path, kconfig_path: Path) -> int:
    model = load_kconfig(kconfig_path)
    config = load_config(config_path)

    for key in config_key_order(model.specs):
        if key in config:
            print("{}={}".format(key, config[key]))

    return 0
