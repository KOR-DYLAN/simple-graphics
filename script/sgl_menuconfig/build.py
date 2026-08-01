# SPDX-License-Identifier: MIT

"""Build configuration and target execution."""

from __future__ import annotations

import os
from pathlib import Path
import shlex
import shutil
import subprocess
from typing import Mapping

from .kconfig import (
    build_cmake_arguments,
    build_test_specs,
    load_config,
    load_kconfig,
    load_option_values,
    migrate_legacy_resize_arguments,
    resolve_option_values,
    synchronize_config,
)
from .model import BuildContext, KconfigError, TestSpec


def _resolve_path(root: Path, value: str) -> Path:
    path = Path(value).expanduser()

    if not path.is_absolute():
        path = root / path

    return path.resolve()


def load_build_context(
    root: Path,
    config_path: Path,
    kconfig_path: Path,
) -> BuildContext:
    synchronize_config(config_path, kconfig_path)
    model = load_kconfig(kconfig_path)
    config = load_config(config_path)
    for spec in model.specs:
        if spec.name in os.environ:
            config[spec.name] = os.environ[spec.name]
    values = load_option_values(config, model.specs)

    build_root = _resolve_path(
        root, os.environ.get("BUILD_ROOT", str(root / "build"))
    )
    toolchain = values["TOOLCHAIN"]
    if "BUILD" in os.environ:
        build = _resolve_path(root, os.environ["BUILD"])
    elif values["WITH_RUNTIME_TEST"] == "ON":
        sanitizer = values["SANITIZERS"].replace(",", "-")
        build = build_root / (
            "runtime-{}-{}".format(toolchain, sanitizer)
        )
    else:
        build = build_root / toolchain

    profile_build = _resolve_path(
        root,
        os.environ.get(
            "PROFILE_BUILD",
            str(build_root / "profile-lttng" / toolchain),
        ),
    )
    return BuildContext(
        root,
        config_path,
        kconfig_path,
        model,
        config,
        values,
        build,
        profile_build,
        build_root,
        os.environ.get("CMAKE", "cmake"),
        os.environ.get("CTEST", "ctest"),
        os.environ.get("CPACK", "cpack"),
        int(values["NPROC"], 10),
        values["V"] != "0",
    )


def run_process(
    command: list[str],
    environment: Mapping[str, str] | None = None,
) -> int:
    print(shlex.join(command), flush=True)
    completed = subprocess.run(
        command,
        check=False,
        env=dict(environment) if environment is not None else None,
    )
    return completed.returncode


def split_environment_arguments(name: str, default: str = "") -> list[str]:
    try:
        result = shlex.split(os.environ.get(name, default))
    except ValueError as error:
        raise KconfigError(
            "invalid {}: {}".format(name, error)
        ) from error

    return result


def configure_project(
    context: BuildContext,
    build: Path,
    values: Mapping[str, str] | None = None,
) -> int:
    active_values = dict(context.values if values is None else values)
    if not active_values.get("INSTALL_PREFIX", ""):
        active_values["INSTALL_PREFIX"] = str(build / "install")

    toolchain_file = (
        context.root / "script" / "toolchain"
        / (active_values["TOOLCHAIN"] + ".cmake")
    )
    command = [
        context.cmake,
        "-DCMAKE_TOOLCHAIN_FILE=" + str(toolchain_file),
    ]
    command.extend(
        build_cmake_arguments(context.model.specs, active_values)
    )
    command.extend(split_environment_arguments("CMAKE_FLAGS"))
    command.extend((
        "-DCMAKE_BUILD_TYPE=" + active_values["BUILD_TYPE"],
        "-S" + str(context.root),
        "-B" + str(build),
    ))
    return run_process(command)


def build_project_target(
    context: BuildContext,
    build: Path,
    target: str | None = None,
) -> int:
    command = [context.cmake, "--build", str(build)]

    if target is not None:
        command.extend(("--target", target))
    command.extend(("-j", str(context.nproc)))
    if context.verbose:
        command.append("-v")

    return run_process(command)


def configure_and_build(
    context: BuildContext,
    target: str | None = None,
) -> int:
    result = configure_project(context, context.build)

    if result == 0:
        result = build_project_target(
            context, context.build, target
        )

    return result


def selected_test(
    context: BuildContext,
) -> tuple[TestSpec, str]:
    with_thread = context.values.get("WITH_THREAD", "OFF") == "ON"
    tests = build_test_specs(context.root, with_thread, context.build)
    migrate_legacy_resize_arguments(
        context.config, tests, context.root
    )
    by_name = {test.name: test for test in tests}
    target = os.environ.get(
        "TARGET", context.config.get("TARGET", "resize")
    )

    if target not in by_name:
        raise KconfigError("unknown or disabled test target: " + target)

    test = by_name[target]
    if "ARGS" in os.environ:
        arguments = os.environ["ARGS"]
    elif context.config.get("TARGET") == target and "ARGS" in context.config:
        arguments = context.config["ARGS"]
    else:
        arguments = test.default_args

    return test, arguments


def execute_run(
    context: BuildContext,
    test: TestSpec | None = None,
    arguments: str | None = None,
) -> int:
    if context.values.get("WITH_TEST_APP", "OFF") != "ON":
        raise KconfigError("test applications are disabled")

    if test is None:
        selected, selected_arguments = selected_test(context)
    else:
        selected = test
        selected_arguments = (
            arguments if arguments is not None else test.default_args
        )

    result = configure_project(context, context.build)
    if result == 0:
        result = build_project_target(
            context, context.build, selected.name
        )
    if result == 0:
        environment = dict(os.environ)
        environment["RUN_TARGET"] = selected.name
        environment["ARGS"] = selected_arguments
        result = run_process(
            [
                context.cmake, "--build", str(context.build),
                "--target", "run",
            ],
            environment,
        )

    return result


def execute_profile(context: BuildContext) -> int:
    values = dict(context.values)
    values.update({
        "WITH_LTTNG": "ON",
        "SGL_TEST_RESIZE_REPEAT_COUNT": os.environ.get(
            "PROFILE_REPEAT_COUNT", "1"
        ),
        "SGL_TEST_RESIZE_WARMUP_COUNT": os.environ.get(
            "PROFILE_WARMUP_COUNT", "0"
        ),
    })
    values = resolve_option_values(context.model.specs, values)
    result = configure_project(context, context.profile_build, values)

    if result == 0:
        environment = dict(os.environ)
        environment["SGL_LTTNG_KERNEL"] = os.environ.get(
            "PROFILE_KERNEL", "0"
        )
        result = run_process(
            [
                context.cmake, "--build", str(context.profile_build),
                "--target", "profile-lttng", "-j", str(context.nproc),
            ] + (["-v"] if context.verbose else []),
            environment,
        )

    return result


def execute_cppcheck_report(context: BuildContext) -> int:
    result = configure_project(context, context.build)

    if result == 0:
        environment = dict(os.environ)
        environment.update({
            "BUILD_DIR": str(context.build),
            "REPORT_ROOT": str(context.root / "report"),
            "WITH_CPPCHECK_MISRA": context.values[
                "WITH_CPPCHECK_MISRA"
            ],
            "CPPCHECK_MISRA_RULE_TEXTS": context.values[
                "CPPCHECK_MISRA_RULE_TEXTS"
            ],
            "CPPCHECK_MAX_CTU_DEPTH": context.values[
                "CPPCHECK_MAX_CTU_DEPTH"
            ],
            "CPPCHECK_JOBS": str(context.nproc),
        })
        result = run_process(
            [str(context.root / "script" / "cppcheck-report.sh")],
            environment,
        )

    return result


def execute_target_command(
    root: Path,
    config_path: Path,
    kconfig_path: Path,
    target: str | None,
) -> int:
    if target is None:
        raise KconfigError("execute requires a target")

    context = load_build_context(root, config_path, kconfig_path)
    result = 0

    if target == "config":
        result = configure_project(context, context.build)
    elif target == "build":
        result = configure_and_build(context)
    elif target == "test":
        result = configure_and_build(context)
        if result == 0:
            command = [
                context.ctest, "--test-dir", str(context.build),
            ]
            command.extend(split_environment_arguments(
                "CTEST_ARGS", "--output-on-failure"
            ))
            result = run_process(command)
    elif target == "install":
        result = configure_and_build(context)
        if result == 0:
            command = [
                context.cmake, "--install", str(context.build)
            ]
            if context.verbose:
                command.append("-v")
            result = run_process(command)
    elif target in ("package", "package-source"):
        result = configure_and_build(context)
        if result == 0:
            filename = (
                "CPackSourceConfig.cmake"
                if target == "package-source"
                else "CPackConfig.cmake"
            )
            command = [
                context.cpack, "--config", str(context.build / filename)
            ]
            if context.verbose:
                command.append("-v")
            result = run_process(command)
    elif target == "clean":
        result = build_project_target(context, context.build, "clean")
    elif target == "distclean":
        if context.build_root in (Path("/"), context.root):
            raise KconfigError("refusing unsafe build root")
        if context.build_root.exists():
            shutil.rmtree(context.build_root)
    elif target == "run-direct":
        result = execute_run(context)
    elif target in ("benchmark-update", "benchmark-svg"):
        result = configure_and_build(context, "benchmark-update")
    elif target == "profile-lttng":
        result = execute_profile(context)
    elif target == "list-tests":
        result = configure_and_build(context, "list-test-apps")
    elif target == "cppcheck-report":
        result = execute_cppcheck_report(context)
    else:
        raise KconfigError("unsupported Make target: " + target)

    return result
