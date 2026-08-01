# SPDX-License-Identifier: MIT

"""Kconfig parsing, value resolution, and .config persistence."""

from __future__ import annotations

import os
from dataclasses import dataclass
from pathlib import Path
import re
import shlex
import tempfile
from typing import Mapping

from .model import KconfigError, KconfigModel, OptionSpec, TestSpec


CONFIG_ASSIGNMENT = re.compile(
    r"^([A-Za-z_][A-Za-z0-9_]*)\s*(?::=|=)\s?(.*)$"
)
DEPENDENCY_TOKEN = re.compile(
    r"\s*(&&|\|\||!|\(|\)|[A-Za-z_][A-Za-z0-9_]*)"
)
DEFAULT_KCONFIG = Path(__file__).resolve().parents[2] / "Kconfig"


@dataclass
class _OptionBuilder:
    name: str
    category: str
    label: str = ""
    kind: str = ""
    default: str = ""
    description: str = ""
    choices: tuple[str, ...] = ()
    minimum: int = 0
    depends_on: str = ""
    cmake_name: str | None = None
    selects: tuple[str, ...] = ()
    assignments: tuple[tuple[str, str], ...] = ()


@dataclass(frozen=True)
class _KconfigLine:
    path: Path
    number: int
    text: str


def _parse_words(text: str, path: Path, line_number: int) -> list[str]:
    try:
        result = shlex.split(text, comments=True, posix=True)
    except ValueError as error:
        raise KconfigError(
            "{}:{}: {}".format(path, line_number, error)
        ) from error

    return result


def _expand_default(value: str) -> str:
    result = value

    if value == "${CPU_COUNT}":
        result = str(os.cpu_count() or 1)

    return result


def _load_kconfig_lines(
    path: Path,
    include_stack: tuple[Path, ...] = (),
) -> list[_KconfigLine]:
    resolved_path = path.resolve()

    if resolved_path in include_stack:
        chain = include_stack + (resolved_path,)
        raise KconfigError(
            "recursive Kconfig source: {}".format(
                " -> ".join(str(item) for item in chain)
            )
        )
    if not resolved_path.is_file():
        raise KconfigError(
            "{}: Kconfig file not found".format(resolved_path)
        )

    result: list[_KconfigLine] = []
    stack = include_stack + (resolved_path,)
    source_lines = resolved_path.read_text(
        encoding="utf-8"
    ).splitlines()

    for line_number, raw_line in enumerate(source_lines, start=1):
        stripped = raw_line.strip()
        words: list[str] = []
        if (
            raw_line == raw_line.lstrip()
            and stripped
            and not stripped.startswith("#")
        ):
            words = _parse_words(stripped, resolved_path, line_number)

        if (
            raw_line == raw_line.lstrip()
            and words
            and words[0] == "source"
        ):
            if len(words) != 2:
                raise KconfigError(
                    "{}:{}: source requires one path".format(
                        resolved_path, line_number
                    )
                )
            source_path = Path(os.path.expandvars(words[1]))
            if not source_path.is_absolute():
                source_path = resolved_path.parent / source_path
            result.extend(_load_kconfig_lines(source_path, stack))
        else:
            result.append(
                _KconfigLine(resolved_path, line_number, raw_line)
            )

    return result


def _finish_option(
    builder: _OptionBuilder | None,
    path: Path,
    line_number: int,
) -> OptionSpec | None:
    result = None

    if builder is not None:
        kind_map = {
            "bool": "boolean",
            "enum": "choice",
            "int": "integer",
            "string": "text",
        }
        if builder.kind not in kind_map:
            raise KconfigError(
                "{}:{}: {} has no supported type".format(
                    path, line_number, builder.name
                )
            )
        if not builder.label:
            raise KconfigError(
                "{}:{}: {} has no prompt".format(
                    path, line_number, builder.name
                )
            )

        default = _expand_default(builder.default)
        kind = kind_map[builder.kind]
        if kind == "boolean":
            if default.lower() in ("y", "yes", "true", "on", "1"):
                default = "ON"
            elif default.lower() in ("n", "no", "false", "off", "0"):
                default = "OFF"
            else:
                raise KconfigError(
                    "{}:{}: invalid boolean default for {}".format(
                        path, line_number, builder.name
                    )
                )
        elif kind == "choice":
            if not builder.choices or default not in builder.choices:
                raise KconfigError(
                    "{}:{}: invalid enum values for {}".format(
                        path, line_number, builder.name
                    )
                )
        elif kind == "integer":
            try:
                if int(default, 10) < builder.minimum:
                    raise ValueError
            except ValueError as error:
                raise KconfigError(
                    "{}:{}: invalid integer default for {}".format(
                        path, line_number, builder.name
                    )
                ) from error

        result = OptionSpec(
            builder.name,
            builder.label,
            builder.category,
            kind,
            default,
            builder.description or builder.label,
            builder.choices,
            builder.minimum,
            builder.depends_on,
            builder.cmake_name,
            builder.selects,
            builder.assignments,
        )

    return result


def _validate_kconfig(model: KconfigModel, path: Path) -> None:
    names = {spec.name for spec in model.specs}
    by_name = {spec.name: spec for spec in model.specs}
    defaults = {spec.name: spec.default for spec in model.specs}

    if len(names) != len(model.specs):
        raise KconfigError("{}: duplicate config symbol".format(path))

    for spec in model.specs:
        references = re.findall(
            r"[A-Za-z_][A-Za-z0-9_]*", spec.depends_on
        )
        for reference in references:
            if reference not in names and reference not in ("y", "n"):
                raise KconfigError(
                    "{}: {} depends on unknown symbol {}".format(
                        path, spec.name, reference
                    )
                )
        try:
            dependency_is_met(spec, defaults)
        except KconfigError as error:
            raise KconfigError(
                "{}: invalid dependency for {}: {}".format(
                    path, spec.name, spec.depends_on
                )
            ) from error
        if (spec.selects or spec.assignments) and spec.kind != "boolean":
            raise KconfigError(
                "{}: {} relations require a bool source".format(
                    path, spec.name
                )
            )
        for target in spec.selects:
            if target not in names:
                raise KconfigError(
                    "{}: {} selects unknown symbol {}".format(
                        path, spec.name, target
                    )
                )
            if by_name[target].kind != "boolean":
                raise KconfigError(
                    "{}: {} selects non-bool symbol {}".format(
                        path, spec.name, target
                    )
                )
        for target, _ in spec.assignments:
            if target not in names:
                raise KconfigError(
                    "{}: {} sets unknown symbol {}".format(
                        path, spec.name, target
                    )
                )
        for target, value in spec.assignments:
            target_spec = by_name[target]
            normalized = normalize_option_value(target_spec, value)
            if normalized != value:
                raise KconfigError(
                    "{}: {} sets invalid value for {}".format(
                        path, spec.name, target
                    )
                )


def load_kconfig(path: Path) -> KconfigModel:
    root_path = path
    lines = _load_kconfig_lines(path)
    title = "SGL Build Configuration"
    categories: list[str] = []
    specs: list[OptionSpec] = []
    builder: _OptionBuilder | None = None
    index = 0

    while index < len(lines):
        source_line = lines[index]
        raw_line = source_line.text
        path = source_line.path
        stripped = raw_line.strip()
        line_number = source_line.number
        index += 1

        if not stripped or stripped.startswith("#"):
            continue

        words = _parse_words(stripped, path, line_number)
        directive = words[0] if words else ""

        if directive in ("mainmenu", "menu", "endmenu", "config"):
            spec = _finish_option(builder, path, line_number)
            if spec is not None:
                specs.append(spec)
            builder = None

        if directive == "mainmenu":
            if len(words) != 2:
                raise KconfigError(
                    "{}:{}: mainmenu requires one title".format(
                        path, line_number
                    )
                )
            title = words[1]
        elif directive == "menu":
            if len(words) != 2:
                raise KconfigError(
                    "{}:{}: menu requires one title".format(
                        path, line_number
                    )
                )
            categories.append(words[1])
        elif directive == "endmenu":
            if not categories:
                raise KconfigError(
                    "{}:{}: unmatched endmenu".format(path, line_number)
                )
            categories.pop()
        elif directive == "config":
            if len(words) != 2:
                raise KconfigError(
                    "{}:{}: config requires one symbol".format(
                        path, line_number
                    )
                )
            category = categories[-1] if categories else "General"
            builder = _OptionBuilder(words[1], category)
        elif directive == "help":
            if builder is None:
                raise KconfigError(
                    "{}:{}: help outside config".format(path, line_number)
                )
            help_indent = len(raw_line) - len(raw_line.lstrip())
            help_lines: list[str] = []
            while index < len(lines):
                help_line = lines[index].text
                help_text = help_line.strip()
                help_line_indent = len(help_line) - len(help_line.lstrip())
                if help_text and help_line_indent <= help_indent:
                    break
                if help_text:
                    help_lines.append(help_text)
                index += 1
            builder.description = " ".join(help_lines)
        elif builder is None:
            raise KconfigError(
                "{}:{}: unsupported directive {}".format(
                    path, line_number, directive
                )
            )
        elif directive in ("bool", "enum", "int", "string"):
            if len(words) != 2:
                raise KconfigError(
                    "{}:{}: {} requires one prompt".format(
                        path, line_number, directive
                    )
                )
            builder.kind = directive
            builder.label = words[1]
        elif directive == "default":
            if len(words) != 2:
                raise KconfigError(
                    "{}:{}: default requires one value".format(
                        path, line_number
                    )
                )
            builder.default = words[1]
        elif directive == "values":
            if len(words) < 2:
                raise KconfigError(
                    "{}:{}: values requires at least one value".format(
                        path, line_number
                    )
                )
            builder.choices = tuple(words[1:])
        elif directive == "minimum":
            if len(words) != 2:
                raise KconfigError(
                    "{}:{}: minimum requires one integer".format(
                        path, line_number
                    )
                )
            try:
                builder.minimum = int(words[1], 10)
            except ValueError as error:
                raise KconfigError(
                    "{}:{}: invalid minimum".format(path, line_number)
                ) from error
        elif directive == "depends":
            if len(words) < 3 or words[1] != "on":
                raise KconfigError(
                    "{}:{}: expected 'depends on EXPR'".format(
                        path, line_number
                    )
                )
            builder.depends_on = " ".join(words[2:])
        elif directive == "cmake":
            if len(words) > 2:
                raise KconfigError(
                    "{}:{}: cmake accepts at most one name".format(
                        path, line_number
                    )
                )
            builder.cmake_name = (
                words[1] if len(words) == 2 else builder.name
            )
        elif directive == "select":
            if len(words) != 2:
                raise KconfigError(
                    "{}:{}: select requires one symbol".format(
                        path, line_number
                    )
                )
            builder.selects += (words[1],)
        elif directive == "set":
            if len(words) != 3:
                raise KconfigError(
                    "{}:{}: set requires a symbol and value".format(
                        path, line_number
                    )
                )
            builder.assignments += ((words[1], words[2]),)
        else:
            raise KconfigError(
                "{}:{}: unsupported property {}".format(
                    path, line_number, directive
                )
            )

    spec = _finish_option(builder, path, len(lines) + 1)
    if spec is not None:
        specs.append(spec)
    if categories:
        raise KconfigError("{}: unclosed menu".format(path))

    model = KconfigModel(title, tuple(specs))
    _validate_kconfig(model, root_path)
    return model


def build_option_specs(
    kconfig_path: Path | None = None,
) -> tuple[OptionSpec, ...]:
    path = kconfig_path if kconfig_path is not None else DEFAULT_KCONFIG
    return load_kconfig(path).specs


def build_test_specs(
    root: Path,
    with_thread: bool = True,
    build: Path | None = None,
) -> tuple[TestSpec, ...]:
    sample = str(root / "resource" / "sample.png")
    active_build = build if build is not None else root / "build" / "llvm"
    output = str(active_build / "resize-output")
    tests = [
        TestSpec("memory", "Memory allocator regression and stress test", ""),
        TestSpec("queue", "Queue regression test", ""),
        TestSpec("crop", "Crop benchmark and regression test", sample),
        TestSpec("convert", "Pixel-format conversion and raw output test", sample),
        TestSpec("rotate", "Rotate benchmark and regression test", sample),
        TestSpec("resize", "Resize benchmark and regression test",
                 sample + " " + output),
        TestSpec("sample", "PNG load/save sample application", sample),
    ]
    if with_thread:
        tests.append(TestSpec("threadpool", "Threadpool regression test", ""))

    return tuple(tests)


def migrate_legacy_resize_arguments(
    config: dict[str, str],
    tests: tuple[TestSpec, ...],
    root: Path,
) -> None:
    resize = next((test for test in tests if test.name == "resize"), None)
    legacy = "{} {}".format(
        root / "resource" / "sample.png",
        root / "build" / "output",
    )

    if resize is not None:
        if config.get(resize.config_key) == legacy:
            config[resize.config_key] = resize.default_args
        if config.get("TARGET") == "resize" and config.get("ARGS") == legacy:
            config["ARGS"] = resize.default_args


def decode_make_value(value: str) -> str:
    result = value.replace(r"\#", "#")
    result = result.replace("$$", "$")
    return result


def encode_make_value(value: str) -> str:
    if "\n" in value or "\r" in value:
        raise ValueError("configuration values cannot contain newlines")

    result = value.replace("$", "$$")
    result = result.replace("#", r"\#")
    return result


def load_config(path: Path) -> dict[str, str]:
    values: dict[str, str] = {}

    if path.is_file():
        for line in path.read_text(encoding="utf-8").splitlines():
            match = CONFIG_ASSIGNMENT.match(line)
            if match is not None:
                values[match.group(1)] = decode_make_value(match.group(2))

    return values


def config_key_order(
    specs: tuple[OptionSpec, ...] | None = None,
) -> tuple[str, ...]:
    active_specs = specs if specs is not None else build_option_specs()
    option_keys = tuple(spec.name for spec in active_specs)
    run_keys = tuple(
        "RUN_ARGS_" + name.upper()
        for name in (
            "memory", "queue", "crop", "convert", "rotate", "resize",
            "sample", "threadpool",
        )
    )
    return option_keys + ("TARGET", "ARGS") + run_keys


def filter_known_config(
    values: Mapping[str, str],
    specs: tuple[OptionSpec, ...],
) -> dict[str, str]:
    known = set(config_key_order(specs))
    return {
        key: value for key, value in values.items()
        if key in known
    }


def save_config(
    path: Path,
    values: Mapping[str, str],
    specs: tuple[OptionSpec, ...] | None = None,
) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    ordered = []
    emitted = set()

    for key in config_key_order(specs):
        if key in values:
            ordered.append((key, values[key]))
            emitted.add(key)
    for key in sorted(values):
        if key not in emitted:
            ordered.append((key, values[key]))

    descriptor, temporary_name = tempfile.mkstemp(
        prefix="." + path.name + ".", dir=str(path.parent), text=True
    )
    try:
        with os.fdopen(descriptor, "w", encoding="utf-8") as stream:
            stream.write("# Generated from Kconfig; do not edit manually.\n")
            stream.write("# Use `make menu` to change build settings.\n")
            for key, value in ordered:
                stream.write(
                    "{} := {}\n".format(key, encode_make_value(str(value)))
                )
        os.chmod(temporary_name, 0o644)
        os.replace(temporary_name, path)
    except BaseException:
        try:
            os.unlink(temporary_name)
        except FileNotFoundError:
            pass
        raise


def normalize_boolean(value: str, default: str = "OFF") -> str:
    enabled = {"1", "ON", "TRUE", "YES", "on", "true", "yes"}
    disabled = {"0", "OFF", "FALSE", "NO", "off", "false", "no"}
    result = default

    if value in enabled:
        result = "ON"
    elif value in disabled:
        result = "OFF"

    return result


def normalize_option_value(spec: OptionSpec, value: str) -> str:
    result = value

    if spec.kind == "boolean":
        result = normalize_boolean(value, spec.default)
    elif spec.kind == "choice":
        if value not in spec.choices:
            result = spec.default
    elif spec.kind == "integer":
        try:
            number = int(value, 10)
            if number < spec.minimum:
                result = spec.default
            else:
                result = str(number)
        except ValueError:
            result = spec.default

    return result


def _tokenize_dependency(expression: str) -> tuple[str, ...]:
    tokens: list[str] = []
    position = 0

    while position < len(expression):
        match = DEPENDENCY_TOKEN.match(expression, position)
        if match is None:
            raise KconfigError(
                "invalid dependency expression: {}".format(expression)
            )
        tokens.append(match.group(1))
        position = match.end()

    return tuple(tokens)


class _DependencyParser:
    def __init__(
        self,
        tokens: tuple[str, ...],
        values: Mapping[str, str],
    ) -> None:
        self.tokens = tokens
        self.values = values
        self.position = 0

    def current(self) -> str | None:
        result = None

        if self.position < len(self.tokens):
            result = self.tokens[self.position]

        return result

    def consume(self, expected: str | None = None) -> str:
        token = self.current()

        if token is None or (expected is not None and token != expected):
            raise KconfigError("invalid dependency expression")
        self.position += 1
        return token

    def parse_primary(self) -> bool:
        token = self.consume()
        result = False

        if token == "(":
            result = self.parse_or()
            self.consume(")")
        elif token == "y":
            result = True
        elif token != "n":
            result = normalize_boolean(
                self.values.get(token, "OFF"), "OFF"
            ) == "ON"

        return result

    def parse_unary(self) -> bool:
        result = False

        if self.current() == "!":
            self.consume("!")
            result = not self.parse_unary()
        else:
            result = self.parse_primary()

        return result

    def parse_and(self) -> bool:
        result = self.parse_unary()

        while self.current() == "&&":
            self.consume("&&")
            right = self.parse_unary()
            result = result and right

        return result

    def parse_or(self) -> bool:
        result = self.parse_and()

        while self.current() == "||":
            self.consume("||")
            right = self.parse_and()
            result = result or right

        return result

    def parse(self) -> bool:
        result = self.parse_or()

        if self.current() is not None:
            raise KconfigError("invalid dependency expression")

        return result


def dependency_is_met(
    spec: OptionSpec,
    values: Mapping[str, str],
) -> bool:
    result = True

    if spec.depends_on:
        tokens = _tokenize_dependency(spec.depends_on)
        result = _DependencyParser(tokens, values).parse()

    return result


def resolve_option_values(
    specs: tuple[OptionSpec, ...],
    values: Mapping[str, str],
) -> dict[str, str]:
    result = {
        spec.name: normalize_option_value(
            spec, values.get(spec.name, spec.default)
        )
        for spec in specs
    }
    by_name = {spec.name: spec for spec in specs}

    for _ in range(len(specs) + 1):
        previous = dict(result)

        for spec in specs:
            if (
                spec.kind == "boolean"
                and result[spec.name] == "ON"
                and dependency_is_met(spec, result)
            ):
                for target in spec.selects:
                    target_spec = by_name[target]
                    result[target] = normalize_option_value(
                        target_spec, "ON"
                    )
                for target, value in spec.assignments:
                    target_spec = by_name[target]
                    result[target] = normalize_option_value(
                        target_spec, value
                    )

        for spec in specs:
            if (
                spec.kind == "boolean"
                and not dependency_is_met(spec, result)
            ):
                result[spec.name] = "OFF"

        if result == previous:
            break

    return result


def load_option_values(
    config: Mapping[str, str],
    specs: tuple[OptionSpec, ...],
) -> dict[str, str]:
    return resolve_option_values(specs, config)


def build_cmake_arguments(
    specs: tuple[OptionSpec, ...],
    values: Mapping[str, str],
) -> tuple[str, ...]:
    resolved = resolve_option_values(specs, values)
    result = []

    for spec in specs:
        value = resolved[spec.name]
        if spec.cmake_name is not None:
            result.append("-D{}={}".format(spec.cmake_name, value))

    return tuple(result)


def save_option_values(
    path: Path,
    current_config: Mapping[str, str],
    option_values: Mapping[str, str],
    specs: tuple[OptionSpec, ...] | None = None,
) -> None:
    active_specs = specs if specs is not None else build_option_specs()
    merged = filter_known_config(current_config, active_specs)
    merged.update(option_values)
    save_config(path, merged, active_specs)


def synchronize_config(config_path: Path, kconfig_path: Path) -> None:
    model = load_kconfig(kconfig_path)
    config = load_config(config_path)
    values = load_option_values(config, model.specs)
    merged = filter_known_config(config, model.specs)
    merged.update(values)
    save_config(config_path, merged, model.specs)


def load_run_values(
    config: Mapping[str, str],
    tests: tuple[TestSpec, ...],
) -> tuple[int, dict[str, str]]:
    names = tuple(test.name for test in tests)
    selected_name = config.get("TARGET", "resize")
    selected = names.index(selected_name) if selected_name in names else 0
    arguments = {
        test.name: config.get(test.config_key, test.default_args)
        for test in tests
    }
    if "ARGS" in config and config.get("TARGET") in arguments:
        arguments[str(config["TARGET"])] = config["ARGS"]

    return selected, arguments


def save_run_values(
    path: Path,
    current_config: Mapping[str, str],
    tests: tuple[TestSpec, ...],
    selected: int,
    arguments: Mapping[str, str],
    specs: tuple[OptionSpec, ...] | None = None,
) -> None:
    merged = dict(current_config)
    selected_test = tests[selected]

    for test in tests:
        merged[test.config_key] = arguments[test.name]
    merged["TARGET"] = selected_test.name
    merged["ARGS"] = arguments[selected_test.name]
    save_config(path, merged, specs)
