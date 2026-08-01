#!/usr/bin/env python3
#
# SPDX-License-Identifier: MIT
#
# Copyright (c) 2025 Dylan Hong
#
# This file is released under the MIT License.
# For conditions of distribution and use, see the LICENSE file.

"""Detect the host and runtime environment used by benchmark reports."""

from __future__ import annotations

from dataclasses import dataclass
import json
import os
from pathlib import Path
import platform
import shutil
import subprocess


NOT_REPORTED = "not reported"
POWERSHELL_TIMEOUT_SECONDS = 10
WINDOWS_POWERSHELL_CANDIDATES = (
    Path("/mnt/c/Windows/System32/WindowsPowerShell/v1.0/powershell.exe"),
    Path("/mnt/c/Program Files/PowerShell/7/pwsh.exe"),
)


@dataclass(frozen=True)
class CpuDetails:
    """CPU identity together with host and runtime-visible topology."""

    name: str
    runtime_logical_cpus: int | None
    host_physical_cores: int | None = None
    host_logical_cpus: int | None = None
    is_wsl: bool = False


@dataclass(frozen=True)
class WindowsCpuDetails:
    """CPU data returned by the Windows Win32_Processor CIM provider."""

    name: str
    physical_cores: int | None
    logical_cpus: int | None


def detect_os() -> str:
    """Return the distribution description when os-release is available."""

    description = platform.platform()
    os_release = Path("/etc/os-release")

    if os_release.exists():
        values: dict[str, str] = {}
        for line in os_release.read_text(encoding="utf-8").splitlines():
            if "=" in line:
                key, value = line.split("=", 1)
                values[key] = value.strip().strip('"')
        description = values.get("PRETTY_NAME", description)

    return description


def is_wsl_runtime() -> bool:
    """Return True when the current Linux process runs under WSL."""

    release = platform.release().lower()
    return (
        "microsoft" in release
        or "WSL_DISTRO_NAME" in os.environ
        or "WSL_INTEROP" in os.environ
    )


def _positive_integer(value: object) -> int | None:
    result: int | None = None

    try:
        converted = int(str(value))
        if converted > 0:
            result = converted
    except (TypeError, ValueError):
        result = None

    return result


def _parse_windows_cpu(output: str) -> WindowsCpuDetails | None:
    result: WindowsCpuDetails | None = None

    try:
        values = json.loads(output)
    except (json.JSONDecodeError, TypeError):
        values = None

    if isinstance(values, dict):
        name = str(values.get("name", "")).strip()
        if name:
            result = WindowsCpuDetails(
                name=name,
                physical_cores=_positive_integer(values.get("physical_cores")),
                logical_cpus=_positive_integer(values.get("logical_cpus")),
            )

    return result


def _find_windows_powershell() -> str | None:
    executable: str | None = None

    for name in ("powershell.exe", "pwsh.exe"):
        found = shutil.which(name)
        if found:
            executable = found
            break

    if executable is None:
        for candidate in WINDOWS_POWERSHELL_CANDIDATES:
            if candidate.is_file():
                executable = str(candidate)
                break

    return executable


def _query_windows_cpu(executable: str) -> WindowsCpuDetails | None:
    script = (
        "$cpus = @(Get-CimInstance -ClassName Win32_Processor); "
        "$names = @($cpus | ForEach-Object { $_.Name.Trim() }); "
        "$physical = ($cpus | Measure-Object -Property NumberOfCores -Sum).Sum; "
        "$logical = ($cpus | Measure-Object "
        "-Property NumberOfLogicalProcessors -Sum).Sum; "
        "[ordered]@{name=($names -join '; '); "
        "physical_cores=$physical; logical_cpus=$logical} "
        "| ConvertTo-Json -Compress"
    )
    result: WindowsCpuDetails | None = None

    try:
        completed = subprocess.run(
            [executable, "-NoProfile", "-NonInteractive", "-Command", script],
            check=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.DEVNULL,
            text=True,
            timeout=POWERSHELL_TIMEOUT_SECONDS,
        )
        result = _parse_windows_cpu(completed.stdout.strip())
    except (OSError, subprocess.CalledProcessError, subprocess.TimeoutExpired):
        result = None

    return result


def _detect_linux_cpu() -> str:
    result = platform.processor().strip()
    cpuinfo = Path("/proc/cpuinfo")

    if result.lower() in ("aarch64", "arm64", "x86_64", "amd64"):
        result = ""

    if cpuinfo.exists():
        first_cpu: dict[str, str] = {}
        for line in cpuinfo.read_text(encoding="utf-8").splitlines():
            if not line:
                break
            if ":" in line:
                key, value = line.split(":", 1)
                first_cpu[key.strip()] = value.strip()
        for name in ("model name", "Hardware", "Model"):
            value = first_cpu.get(name, "")
            if value and value != "-":
                result = value
                break
        if not result and first_cpu.get("CPU implementer"):
            result = "ARM implementer {}, part {}".format(
                first_cpu["CPU implementer"],
                first_cpu.get("CPU part", "unknown"),
            )

    return result or NOT_REPORTED


def detect_cpu() -> CpuDetails:
    """Prefer the Windows host CPU identity when running under WSL."""

    wsl = is_wsl_runtime()
    windows_cpu: WindowsCpuDetails | None = None

    if wsl:
        executable = _find_windows_powershell()
        if executable is not None:
            windows_cpu = _query_windows_cpu(executable)

    if windows_cpu is not None:
        result = CpuDetails(
            name=windows_cpu.name,
            runtime_logical_cpus=os.cpu_count(),
            host_physical_cores=windows_cpu.physical_cores,
            host_logical_cpus=windows_cpu.logical_cpus,
            is_wsl=True,
        )
    else:
        result = CpuDetails(
            name=_detect_linux_cpu(),
            runtime_logical_cpus=os.cpu_count(),
            is_wsl=wsl,
        )

    return result


def detect_memory() -> str:
    """Return the memory visible to the benchmark process."""

    result = NOT_REPORTED
    meminfo = Path("/proc/meminfo")

    if meminfo.exists():
        for line in meminfo.read_text(encoding="utf-8").splitlines():
            if line.startswith("MemTotal:"):
                kib = int(line.split()[1])
                result = f"{kib / (1024.0 * 1024.0):.2f} GiB"
                break

    return result
