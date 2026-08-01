#!/usr/bin/env python3
#
# SPDX-License-Identifier: MIT
#
# Copyright (c) 2025 Dylan Hong
#
# This file is released under the MIT License.
# For conditions of distribution and use, see the LICENSE file.

"""Regression tests for the Makefile curses-menu configuration backend."""

from __future__ import annotations

import importlib.util
from pathlib import Path
import re
import sys
import tempfile
import unittest
from unittest import mock


ROOT = Path(__file__).resolve().parents[2]
MODULE_PATH = ROOT / "script" / "menuconfig.py"
sys.dont_write_bytecode = True
MODULE_SPEC = importlib.util.spec_from_file_location(
    "sgl_menuconfig_entrypoint", MODULE_PATH
)
if MODULE_SPEC is None or MODULE_SPEC.loader is None:
    raise RuntimeError("cannot load menuconfig module")
MENUCONFIG = importlib.util.module_from_spec(MODULE_SPEC)
sys.modules[MODULE_SPEC.name] = MENUCONFIG
MODULE_SPEC.loader.exec_module(MENUCONFIG)


class MenuConfigTest(unittest.TestCase):
    def test_menuconfig_entrypoint_uses_split_modules(self) -> None:
        package = ROOT / "script" / "sgl_menuconfig"
        expected = {
            "__init__.py",
            "build.py",
            "cli.py",
            "commands.py",
            "kconfig.py",
            "model.py",
            "ui.py",
        }
        entrypoint_lines = MODULE_PATH.read_text(
            encoding="utf-8"
        ).splitlines()

        self.assertEqual(expected, {
            path.name for path in package.glob("*.py")
        })
        self.assertLessEqual(len(entrypoint_lines), 40)

    def test_menu_is_the_build_configuration_command(self) -> None:
        arguments = MENUCONFIG.parse_arguments(["menu"])

        self.assertEqual("menu", arguments.command)
        with mock.patch(
            "sgl_menuconfig.cli.menu_command", return_value=0
        ) as command:
            result = MENUCONFIG.main(["menu", "--root", str(ROOT)])
        self.assertEqual(0, result)
        command.assert_called_once_with(ROOT / ".config", ROOT / "Kconfig")

        with mock.patch("sys.stderr"):
            with self.assertRaises(SystemExit):
                MENUCONFIG.parse_arguments(["option"])

    def test_kconfig_uses_kernel_indentation_style(self) -> None:
        paths = (ROOT / "Kconfig",) + tuple(
            sorted((ROOT / "kconfig").glob("Kconfig.*"))
        )

        for path in paths:
            lines = path.read_text(encoding="utf-8").splitlines()
            for line in lines:
                if line[:1].isspace():
                    self.assertTrue(
                        line.startswith("\t"),
                        "{} indentation must start with a tab".format(path),
                    )

    def test_kconfig_source_is_relative_and_recursive(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            fragments = root / "fragments"
            fragments.mkdir()
            (root / "Kconfig").write_text(
                'mainmenu "Source test"\n'
                'source "fragments/Kconfig.first"\n',
                encoding="utf-8",
            )
            (fragments / "Kconfig.first").write_text(
                'source "Kconfig.second"\n',
                encoding="utf-8",
            )
            (fragments / "Kconfig.second").write_text(
                'config ENABLED\n'
                '\tbool "Enabled"\n'
                '\tdefault y\n',
                encoding="utf-8",
            )

            model = MENUCONFIG.load_kconfig(root / "Kconfig")

        self.assertEqual("Source test", model.title)
        self.assertEqual(("ENABLED",), tuple(
            spec.name for spec in model.specs
        ))

    def test_recursive_kconfig_source_is_rejected(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / "Kconfig").write_text(
                'source "Kconfig.loop"\n',
                encoding="utf-8",
            )
            (root / "Kconfig.loop").write_text(
                'source "Kconfig"\n',
                encoding="utf-8",
            )

            with self.assertRaisesRegex(
                MENUCONFIG.KconfigError,
                "recursive Kconfig source",
            ):
                MENUCONFIG.load_kconfig(root / "Kconfig")

    def test_makefile_only_defines_bootstrap_variables(self) -> None:
        contents = (ROOT / "Makefile").read_text(encoding="utf-8")
        assignments = re.findall(
            r"(?m)^([A-Za-z_][A-Za-z0-9_]*)\s*[:+?]?=",
            contents,
        )
        project_assignments = [
            name for name in assignments if name != "phony"
        ]

        self.assertEqual(["PYTHON", "TOPDIR"], project_assignments)
        self.assertIn("PYTHON ?= python3", contents)
        self.assertIn("TOPDIR :=", contents)
        self.assertIn('$(PYTHON) "$(TOPDIR)/script/menuconfig.py"', contents)
        self.assertIn(".PHONY: $(phony)", contents)

    def test_project_kconfig_defines_options_and_cmake_mapping(self) -> None:
        model = MENUCONFIG.load_kconfig(ROOT / "Kconfig")
        specs = {spec.name: spec for spec in model.specs}

        self.assertEqual("SGL Build Configuration", model.title)
        self.assertEqual(33, len(model.specs))
        self.assertEqual(
            ("llvm", "gnu", "aarch64-none-linux-llvm",
             "aarch64-none-linux-gnu"),
            specs["TOOLCHAIN"].choices,
        )
        self.assertEqual(
            "CMAKE_INSTALL_PREFIX",
            specs["INSTALL_PREFIX"].cmake_name,
        )
        self.assertEqual(
            "ON",
            specs["WITH_AUTO_VECTORIZATION"].default,
        )
        self.assertEqual(
            "WITH_AUTO_VECTORIZATION",
            specs["WITH_AUTO_VECTORIZATION"].cmake_name,
        )
        self.assertEqual(
            "WITH_CPPCHECK",
            specs["WITH_CPPCHECK_MISRA"].depends_on,
        )
        self.assertEqual(
            "1280",
            specs["SGL_TEST_CONVERT_MATRIX_WIDTH"].default,
        )
        self.assertEqual(
            "SGL_TEST_CONVERT_MATRIX_WIDTH",
            specs["SGL_TEST_CONVERT_MATRIX_WIDTH"].cmake_name,
        )
        self.assertEqual(
            "720",
            specs["SGL_TEST_CONVERT_MATRIX_HEIGHT"].default,
        )
        self.assertEqual(
            "SGL_TEST_CONVERT_MATRIX_HEIGHT",
            specs["SGL_TEST_CONVERT_MATRIX_HEIGHT"].cmake_name,
        )

    def test_dependencies_and_runtime_preset_are_resolved(self) -> None:
        specs = MENUCONFIG.build_option_specs()
        values = MENUCONFIG.load_option_values(
            {
                "WITH_CPPCHECK": "OFF",
                "WITH_CPPCHECK_MISRA": "ON",
                "WITH_RUNTIME_TEST": "ON",
            },
            specs,
        )

        self.assertEqual("OFF", values["WITH_CPPCHECK_MISRA"])
        self.assertEqual("Debug", values["BUILD_TYPE"])
        self.assertEqual("ON", values["WITH_TEST_APP"])
        self.assertEqual("ON", values["WITH_SANITIZER"])
        self.assertEqual("ON", values["WITH_STACK_PROTECTOR"])
        self.assertEqual("all", values["STACK_PROTECTOR_MODE"])
        self.assertEqual("1", values["SGL_TEST_CROP_REPEAT_COUNT"])
        self.assertEqual("0", values["SGL_TEST_CROP_WARMUP_COUNT"])
        self.assertEqual("1", values["SGL_TEST_CONVERT_REPEAT_COUNT"])
        self.assertEqual("0", values["SGL_TEST_CONVERT_WARMUP_COUNT"])
        self.assertEqual("1", values["SGL_TEST_ROTATE_REPEAT_COUNT"])
        self.assertEqual("0", values["SGL_TEST_ROTATE_WARMUP_COUNT"])
        self.assertEqual("1", values["SGL_TEST_RESIZE_REPEAT_COUNT"])
        self.assertEqual("0", values["SGL_TEST_RESIZE_WARMUP_COUNT"])

    def test_invalid_kconfig_dependency_is_rejected(self) -> None:
        source = """
mainmenu "Invalid"
config ENABLED
    bool "Enabled"
    default y
    depends on UNKNOWN
"""

        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "Kconfig"
            path.write_text(source, encoding="utf-8")
            with self.assertRaises(MENUCONFIG.KconfigError):
                MENUCONFIG.load_kconfig(path)

    def test_sync_adds_defaults_and_removes_obsolete_symbols(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / ".config"
            MENUCONFIG.save_config(
                path,
                {"TARGET": "rotate", "OBSOLETE_OPTION": "ON"},
            )
            result = MENUCONFIG.sync_command(path, ROOT / "Kconfig")
            loaded = MENUCONFIG.load_config(path)

        self.assertEqual(0, result)
        self.assertEqual("rotate", loaded["TARGET"])
        self.assertEqual("llvm", loaded["TOOLCHAIN"])
        self.assertNotIn("OBSOLETE_OPTION", loaded)

    def test_build_context_derives_runtime_paths_from_environment(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            base = Path(directory)
            environment = {
                "BUILD_ROOT": str(base / "output"),
                "TOOLCHAIN": "gnu",
                "WITH_RUNTIME_TEST": "ON",
                "SANITIZERS": "thread",
            }
            with mock.patch.dict("os.environ", environment, clear=False):
                context = MENUCONFIG.load_build_context(
                    ROOT, base / ".config", ROOT / "Kconfig"
                )

        self.assertEqual("Debug", context.values["BUILD_TYPE"])
        self.assertEqual("ON", context.values["WITH_SANITIZER"])
        self.assertEqual(
            base / "output" / "runtime-gnu-thread",
            context.build,
        )

    def test_clangd_database_tracks_active_build(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            llvm_build = root / "build" / "llvm"
            gnu_build = root / "build" / "gnu"
            llvm_build.mkdir(parents=True)
            gnu_build.mkdir(parents=True)
            (llvm_build / "compile_commands.json").write_text(
                "llvm\n", encoding="utf-8"
            )
            (gnu_build / "compile_commands.json").write_text(
                "gnu\n", encoding="utf-8"
            )

            MENUCONFIG.update_clangd_compile_database(root, llvm_build)
            database = root / "build" / "compile_commands.json"
            self.assertEqual("llvm\n", database.read_text(encoding="utf-8"))

            MENUCONFIG.update_clangd_compile_database(root, gnu_build)
            self.assertEqual("gnu\n", database.read_text(encoding="utf-8"))

    def test_clangd_database_requires_cmake_output(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)

            with self.assertRaisesRegex(
                MENUCONFIG.KconfigError,
                "CMake did not generate compilation database",
            ):
                MENUCONFIG.update_clangd_compile_database(
                    root, root / "build" / "llvm"
                )

    def test_dialog_layout_requires_usable_terminal_size(self) -> None:
        class Window:
            def __init__(self, height: int, width: int) -> None:
                self.height = height
                self.width = width

            def getmaxyx(self) -> tuple[int, int]:
                return self.height, self.width

        layout = MENUCONFIG.make_dialog_layout(Window(24, 80), 28)

        self.assertIsNotNone(layout)
        self.assertEqual(19, layout.height)
        self.assertEqual(72, layout.width)
        self.assertIsNone(
            MENUCONFIG.make_dialog_layout(Window(19, 80), 28)
        )
        self.assertIsNone(
            MENUCONFIG.make_dialog_layout(Window(24, 71), 28)
        )

    def test_config_round_trip_escapes_make_characters(self) -> None:
        values = {
            "TOOLCHAIN": "llvm",
            "ARGS": "$HOME/image #1.png",
            "TARGET": "sample",
        }

        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / ".config"
            MENUCONFIG.save_config(path, values)
            loaded = MENUCONFIG.load_config(path)
            contents = path.read_text(encoding="utf-8")

        self.assertEqual(values, loaded)
        self.assertIn("$$HOME/image \\#1.png", contents)

    def test_option_normalization_and_merge(self) -> None:
        specs = MENUCONFIG.build_option_specs()
        source = {
            "TARGET": "rotate",
            "WITH_SIMD": "false",
            "NPROC": "0",
        }
        values = MENUCONFIG.load_option_values(source, specs)

        self.assertEqual("OFF", values["WITH_SIMD"])
        self.assertNotEqual("0", values["NPROC"])

        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / ".config"
            MENUCONFIG.save_option_values(path, source, values)
            loaded = MENUCONFIG.load_config(path)

        self.assertEqual("rotate", loaded["TARGET"])
        self.assertEqual("OFF", loaded["WITH_SIMD"])

    def test_run_arguments_are_saved_per_target(self) -> None:
        root = Path("/tmp/sgl-menu-test")
        tests = MENUCONFIG.build_test_specs(root, with_thread=False)
        config = {
            "TARGET": "convert",
            "ARGS": "custom-input.png",
        }
        selected, arguments = MENUCONFIG.load_run_values(config, tests)

        self.assertEqual("convert", tests[selected].name)
        self.assertEqual("custom-input.png", arguments["convert"])
        resize_index = next(
            index for index, test in enumerate(tests)
            if test.name == "resize"
        )
        arguments["resize"] = "input.png output"

        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / ".config"
            MENUCONFIG.save_run_values(
                path, config, tests, resize_index, arguments
            )
            loaded = MENUCONFIG.load_config(path)

        self.assertEqual("resize", loaded["TARGET"])
        self.assertEqual("input.png output", loaded["ARGS"])
        self.assertEqual("custom-input.png", loaded["RUN_ARGS_CONVERT"])
        self.assertNotIn("RUN_ARGS_THREADPOOL", loaded)

    def test_default_arguments_match_test_programs(self) -> None:
        root = Path("/workspace/simple-graphics")
        tests = {
            test.name: test
            for test in MENUCONFIG.build_test_specs(root)
        }

        self.assertEqual(
            str(root / "resource" / "sample.png"),
            tests["convert"].default_args,
        )
        self.assertEqual(
            str(root / "resource" / "sample.png"),
            tests["crop"].default_args,
        )
        self.assertEqual(
            str(root / "resource" / "sample.png"),
            tests["rotate"].default_args,
        )
        self.assertIn(str(root / "build" / "llvm" / "resize-output"),
                      tests["resize"].default_args)
        self.assertIn("threadpool", tests)

    def test_legacy_resize_output_is_migrated_to_active_build(self) -> None:
        root = Path("/workspace/simple-graphics")
        build = root / "build" / "gnu"
        tests = MENUCONFIG.build_test_specs(root, build=build)
        legacy = "{} {}".format(
            root / "resource" / "sample.png",
            root / "build" / "output",
        )
        config = {
            "TARGET": "resize",
            "ARGS": legacy,
            "RUN_ARGS_RESIZE": legacy,
        }

        MENUCONFIG.migrate_legacy_resize_arguments(config, tests, root)

        expected = "{} {}".format(
            root / "resource" / "sample.png",
            build / "resize-output",
        )
        self.assertEqual(expected, config["ARGS"])
        self.assertEqual(expected, config["RUN_ARGS_RESIZE"])


if __name__ == "__main__":
    unittest.main()
