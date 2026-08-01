#!/usr/bin/env python3
#
# SPDX-License-Identifier: MIT
#
# Copyright (c) 2025 Dylan Hong
#
# This file is released under the MIT License.
# For conditions of distribution and use, see the LICENSE file.

"""Generate SGL benchmark Markdown tables and gnuplot SVG charts."""

from __future__ import annotations

import argparse
import csv
from datetime import datetime, timezone
import json
from pathlib import Path
import platform
import shlex
import shutil
import subprocess
import tempfile

from benchmark_environment import detect_cpu, detect_memory, detect_os


BENCHMARK_TITLES = {
    "crop": "Crop Benchmark",
    "convert": "Convert Benchmark",
    "rotate": "Rotate Benchmark",
    "resize": "Resize Benchmark",
}
BENCHMARK_DESCRIPTIONS = {
    "crop": (
        "Measures full-frame, corner, centered, wide, and tall crop regions "
        "across the available backends and worker counts. Cairo is compared "
        "with byte-exact ARGB32 SOURCE copies; NE10 has no crop API."
    ),
    "convert": (
        "Measures every source and destination combination across the "
        "supported packed, planar, RGB, and YUV FourCC formats. Cairo is "
        "limited to byte-exact 32-bit packed identity copies because it has "
        "no arbitrary FourCC conversion API; NE10 has no color-convert API."
    ),
    "rotate": (
        "Measures representative 30-, 45-, 60-, 90-, 120-, 180-, 240-, "
        "270-, and 360-degree rotations with direct coordinate calculation "
        "and caller-owned LUT reuse. SGL rows use nearest-neighbor sampling. "
        "Cairo is reported as single-thread ARGB32 nearest-neighbor. NE10 "
        "v1.2.1 is reported as single-thread RGBA bilinear C because upstream "
        "AArch64 has no rotate NEON implementation."
    ),
}
METHODS = ("nearest", "bilinear", "bicubic")
CHANNELS = (1, 2, 3, 4)
CONVERT_RGB_FORMATS = (
    "rgba",
    "argb",
    "bgra",
    "abgr",
    "rgb",
    "bgr",
    "rgba-planar",
    "argb-planar",
    "bgra-planar",
    "abgr-planar",
    "rgb-planar",
    "bgr-planar",
)
CONVERT_YUV_FORMATS = (
    "yuv444-packed",
    "yuv444-planar",
    "yuyv",
    "yvyu",
    "uyvy",
    "vyuy",
    "yuv422-planar",
    "yuv420-planar",
    "nv12",
    "nv21",
)
CONVERT_FORMATS = CONVERT_RGB_FORMATS + CONVERT_YUV_FORMATS
BACKEND_ORDER = {
    "generic": 0,
    "generic-lut": 1,
    "simd": 2,
    "simd-lut": 3,
    "cairo": 4,
    "cairo-copy": 5,
    "cairo-nearest": 6,
    "ne10-c": 7,
    "ne10-neon": 8,
    "ne10": 8,
    "ne10-c-bilinear": 9,
}
COLORS = (
    "#2563eb",
    "#0f766e",
    "#dc2626",
    "#7c3aed",
    "#ea580c",
    "#0891b2",
    "#65a30d",
    "#be185d",
    "#475569",
    "#a16207",
)
LICENSE_HEADER = """<!--
SPDX-License-Identifier: MIT

Copyright (c) 2025 Dylan Hong

This file is released under the MIT License.
For conditions of distribution and use, see the LICENSE file.
-->
"""


def parse_arguments() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Generate benchmark Markdown and gnuplot SVG reports."
    )
    parser.add_argument(
        "--benchmark",
        choices=tuple(BENCHMARK_TITLES),
        required=True,
    )
    parser.add_argument("--csv", required=True, help="benchmark CSV path")
    parser.add_argument(
        "--output-dir", required=True, help="report output directory"
    )
    parser.add_argument("--metadata", required=True)
    parser.add_argument("--compile-commands", default="")
    parser.add_argument("--source-root", default=".")
    parser.add_argument("--input", default="")
    parser.add_argument("--warmup", type=int, required=True)
    parser.add_argument("--repeat", type=int, required=True)
    parser.add_argument("--gnuplot", default="gnuplot")
    parser.add_argument("--validate-only", action="store_true")
    parser.add_argument("--require-simd", action="store_true")
    parser.add_argument("--require-cairo", action="store_true")
    parser.add_argument("--require-ne10", action="store_true")
    return parser.parse_args()


def read_rows(csv_path: Path, benchmark: str) -> list[dict[str, object]]:
    rows: list[dict[str, object]] = []
    required = {
        "case",
        "backend",
        "threads",
        "width",
        "height",
        "avg_us",
        "min_us",
        "max_us",
        "median_us",
    }
    required.add("method" if benchmark == "resize" else "operation")

    with csv_path.open(newline="", encoding="utf-8") as stream:
        reader = csv.DictReader(stream)
        fields = set(reader.fieldnames or ())
        missing = sorted(required - fields)
        if missing:
            raise ValueError(
                "CSV columns are missing: " + ", ".join(missing)
            )

        for raw_row in reader:
            row: dict[str, object] = dict(raw_row)
            for name in (
                "case",
                "threads",
                "width",
                "height",
                "avg_us",
                "min_us",
                "max_us",
                "median_us",
            ):
                row[name] = int(str(raw_row[name]))
            row["channels"] = int(str(raw_row.get("channels", "0") or "0"))
            row["pixels"] = int(
                str(
                    raw_row.get("pixels", "")
                    or (int(row["width"]) * int(row["height"]))
                )
            )
            median_us = int(row["median_us"])
            default_rate = 0
            if median_us > 0:
                default_rate = (int(row["pixels"]) * 1_000_000) // median_us
            row["pixels_per_second"] = int(
                str(raw_row.get("pixels_per_second", "") or default_rate)
            )
            rows.append(row)

    if not rows:
        raise ValueError(f"benchmark CSV contains no rows: {csv_path}")
    return rows


def read_metadata(path: Path) -> dict[str, str]:
    metadata: dict[str, str] = {}

    with path.open(encoding="utf-8") as stream:
        for source_line in stream:
            line = source_line.rstrip("\n")
            if (not line) or line.startswith("#") or ("=" not in line):
                continue
            name, value = line.split("=", 1)
            metadata[name.strip()] = value.strip()

    return metadata


def validate_resize_rows(
    rows: list[dict[str, object]],
    require_simd: bool,
    require_cairo: bool,
    require_ne10: bool,
    ) -> None:
    missing: list[str] = []
    required_sgl = [("generic", 1), ("generic", 8)]
    if require_simd:
        required_sgl.extend((("simd", 1), ("simd", 8)))

    for channel in CHANNELS:
        for method in METHODS:
            sizes = sorted({
                (int(row["width"]), int(row["height"]))
                for row in rows
                if (
                    int(row["channels"]) == channel
                    and str(row["method"]) == method
                    and str(row["backend"]) == "generic"
                    and int(row["threads"]) == 1
                )
            })
            if not sizes:
                missing.append(f"{channel}ch {method} generic 1t")
            for width, height in sizes:
                for backend, threads in required_sgl:
                    if find_resize_row(
                        rows,
                        channel,
                        method,
                        width,
                        height,
                        backend,
                        threads,
                    ) is None:
                        missing.append(
                            f"{channel}ch {method} {width}x{height} "
                            f"{backend} {threads}t"
                        )

    four_channel_sizes = sorted({
        (str(row["method"]), int(row["width"]), int(row["height"]))
        for row in rows
        if int(row["channels"]) == 4
        and str(row["backend"]) == "generic"
        and int(row["threads"]) == 1
    })
    if require_cairo:
        for method, width, height in four_channel_sizes:
            if method in ("nearest", "bilinear") and find_resize_row(
                rows, 4, method, width, height, "cairo", 1
            ) is None:
                missing.append(f"4ch {method} {width}x{height} cairo 1t")
    if require_ne10:
        for method, width, height in four_channel_sizes:
            if method == "bilinear" and (
                find_resize_row(
                    rows, 4, method, width, height, "ne10-c", 1
                ) is None
                or find_resize_row(
                    rows, 4, method, width, height, "ne10-neon", 1
                ) is None
            ):
                missing.append(
                    f"4ch {method} {width}x{height} ne10 C/NEON 1t"
                )

    if missing:
        raise ValueError(
            "required resize benchmark rows are missing: " + ", ".join(missing)
        )


def validate_operation_rows(
    rows: list[dict[str, object]],
    benchmark: str,
    require_simd: bool,
    require_cairo: bool = False,
    require_ne10: bool = False,
) -> None:
    backends = {str(row["backend"]) for row in rows}
    missing: list[str] = []
    cairo_backend = {
        "crop": "cairo",
        "convert": "cairo-copy",
        "rotate": "cairo-nearest",
    }.get(benchmark, "cairo")

    if "generic" not in backends:
        missing.append("generic backend")
    if require_simd and "simd" not in backends:
        missing.append("SIMD backend")
    if require_cairo and cairo_backend not in backends:
        missing.append(f"{cairo_backend} backend")
    if require_ne10 and "ne10-c-bilinear" not in backends:
        missing.append("NE10 C bilinear backend")
    reference_operations = {
        str(row["operation"])
        for row in rows
        if str(row["backend"]) == "generic" and int(row["threads"]) == 1
    }
    if require_cairo and benchmark in ("crop", "rotate"):
        cairo_operations = {
            str(row["operation"])
            for row in rows
            if str(row["backend"]) == cairo_backend
            and int(row["threads"]) == 1
        }
        for operation in sorted(reference_operations - cairo_operations):
            missing.append(f"{operation} {cairo_backend} 1t")
    if require_ne10 and benchmark == "rotate":
        ne10_operations = {
            str(row["operation"])
            for row in rows
            if str(row["backend"]) == "ne10-c-bilinear"
            and int(row["threads"]) == 1
        }
        for operation in sorted(reference_operations - ne10_operations):
            missing.append(f"{operation} NE10 C bilinear 1t")
    if missing:
        raise ValueError(
            "required operation benchmark rows are missing: "
            + ", ".join(missing)
        )


def validate_convert_rows(rows: list[dict[str, object]]) -> None:
    expected_operations = {
        f"{source}-to-{destination}"
        for source in CONVERT_FORMATS
        for destination in CONVERT_FORMATS
    }
    configurations = {
        (str(row["backend"]), int(row["threads"]))
        for row in rows
        if str(row["backend"]) in ("generic", "simd")
    }
    available = {
        (
            str(row["operation"]),
            str(row["backend"]),
            int(row["threads"]),
        )
        for row in rows
    }
    missing = sorted(
        (operation, backend, threads)
        for operation in expected_operations
        for backend, threads in configurations
        if (operation, backend, threads) not in available
    )
    unexpected = sorted(
        {
            str(row["operation"])
            for row in rows
            if str(row["operation"]) not in expected_operations
        }
    )
    expected_cairo = {
        f"{format_name}-to-{format_name}"
        for format_name in ("rgba", "argb", "bgra", "abgr")
    }
    cairo_operations = {
        str(row["operation"])
        for row in rows
        if str(row["backend"]) == "cairo-copy"
        and int(row["threads"]) == 1
    }
    invalid_cairo = sorted(cairo_operations - expected_cairo)
    missing_cairo = sorted(expected_cairo - cairo_operations)
    if not configurations:
        raise ValueError("convert benchmark has no execution configurations")
    if missing:
        preview = ", ".join(
            f"{operation} {backend} {threads}t"
            for operation, backend, threads in missing[:8]
        )
        raise ValueError(
            f"convert matrix is missing {len(missing)} cases: {preview}"
        )
    if unexpected:
        raise ValueError(
            "convert matrix has unknown operations: "
            + ", ".join(unexpected[:8])
        )
    if cairo_operations and missing_cairo:
        raise ValueError(
            "Cairo packed-copy subset is missing: "
            + ", ".join(missing_cairo)
        )
    if invalid_cairo:
        raise ValueError(
            "Cairo packed-copy subset has unsupported operations: "
            + ", ".join(invalid_cairo[:8])
        )


def find_resize_row(
    rows: list[dict[str, object]],
    channel: int,
    method: str,
    width: int,
    height: int,
    backend: str,
    threads: int,
) -> dict[str, object] | None:
    aliases = {backend}
    if backend == "ne10-neon":
        aliases.add("ne10")

    return next(
        (
            row
            for row in rows
            if int(row["channels"]) == channel
            and str(row["method"]) == method
            and int(row["width"]) == width
            and int(row["height"]) == height
            and str(row["backend"]) in aliases
            and int(row["threads"]) == threads
        ),
        None,
    )


def command_output(command: list[str], default: str) -> str:
    try:
        completed = subprocess.run(
            command,
            check=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.DEVNULL,
            text=True,
        )
        result = completed.stdout.strip()
    except (OSError, subprocess.CalledProcessError):
        result = default
    return result or default


def relative_display(path_text: str, source_root: Path) -> str:
    if not path_text:
        return "not specified"
    path = Path(path_text)
    try:
        return str(path.resolve().relative_to(source_root.resolve()))
    except ValueError:
        return str(path)


def extract_compile_flags(path: Path) -> str:
    if (not path.exists()) or (not path.is_file()):
        return ""

    entries = json.loads(path.read_text(encoding="utf-8"))
    selected: list[str] = []
    for entry in entries:
        source = str(entry.get("file", ""))
        if "/library/sgl-core/" in source.replace("\\", "/"):
            if "arguments" in entry:
                selected = [str(item) for item in entry["arguments"]]
            else:
                selected = shlex.split(str(entry.get("command", "")))
            break

    flags: list[str] = []
    skip_next = False
    accepted = (
        "-O",
        "-g",
        "-std=",
        "-W",
        "-f",
        "-m",
        "--target=",
        "--gcc-toolchain=",
    )
    for index, token in enumerate(selected):
        if index == 0:
            continue
        if skip_next:
            skip_next = False
            continue
        if token in ("-I", "-isystem", "-o", "-c"):
            skip_next = True
            continue
        if token == "-DNDEBUG" or token.startswith(accepted):
            flags.append(token)
    return " ".join(dict.fromkeys(flags))


def markdown_escape(value: object) -> str:
    return str(value).replace("|", "\\|").replace("\n", " ")


def markdown_table(headers: list[str], body: list[list[object]]) -> str:
    lines = [
        "| " + " | ".join(headers) + " |",
        "| " + " | ".join("---" for _item in headers) + " |",
    ]
    for row in body:
        lines.append(
            "| " + " | ".join(markdown_escape(item) for item in row) + " |"
        )
    return "\n".join(lines)


def external_coverage_table(benchmark: str) -> str:
    coverage = {
        "crop": [
            [
                "Cairo 1.18.4",
                "ARGB32 SOURCE copy",
                "All crop regions, 1 thread",
                "Byte-exact",
            ],
            ["NE10 v1.2.1", "Not available", "No crop API", "N/A"],
        ],
        "convert": [
            [
                "Cairo 1.18.4",
                "ARGB32 SOURCE copy",
                "RGBA/ARGB/BGRA/ABGR identity, 1 thread",
                "Byte-exact",
            ],
            [
                "NE10 v1.2.1",
                "Not available",
                "No arbitrary FourCC/color-convert API",
                "N/A",
            ],
        ],
        "rotate": [
            [
                "Cairo 1.18.4",
                "ARGB32 nearest-neighbor",
                "All angles, 1 thread, SGL output canvas",
                "Same sampling class",
            ],
            [
                "NE10 v1.2.1",
                "RGBA bilinear C",
                "All angles, 1 thread",
                "Different interpolation and canvas rounding",
            ],
        ],
    }
    rows = coverage.get(benchmark, [])
    return markdown_table(
        ["Library", "Measured path", "Coverage", "Result relation"],
        rows,
    )


def environment_tables(
    metadata: dict[str, str],
    args: argparse.Namespace,
    source_root: Path,
) -> tuple[str, str]:
    gnuplot_version = command_output(
        [args.gnuplot, "--version"], "not available"
    )
    revision = command_output(
        ["git", "-C", str(source_root), "rev-parse", "--short", "HEAD"],
        "not available",
    )
    dirty = command_output(
        [
            "git",
            "-C",
            str(source_root),
            "status",
            "--porcelain",
            "--untracked-files=no",
        ],
        "",
    )
    if dirty and revision != "not available":
        revision += " (dirty)"
    flags = extract_compile_flags(Path(args.compile_commands))
    if not flags:
        flags = metadata.get("effective_c_flags", "compiler default")
    runner = metadata.get("execution", "native")
    kernel = f"{platform.system()} {platform.release()}"
    if "microsoft" in platform.release().lower():
        kernel += " (WSL2)"

    cpu = detect_cpu()
    cpu_rows: list[list[object]] = [["CPU", cpu.name]]
    if cpu.host_physical_cores is not None:
        cpu_rows.append(["Host physical cores", cpu.host_physical_cores])
    if cpu.host_logical_cpus is not None:
        cpu_rows.append(["Host logical CPUs", cpu.host_logical_cpus])
    logical_cpu_label = "WSL logical CPUs" if cpu.is_wsl else "Logical CPUs"
    cpu_rows.append([
        logical_cpu_label,
        cpu.runtime_logical_cpus or "not reported",
    ])

    environment = markdown_table(
        ["Environment", "Value"],
        [
            ["Report generated", datetime.now(timezone.utc).isoformat(timespec="seconds")],
            ["OS", detect_os()],
            ["Kernel", kernel],
            ["Runtime architecture", platform.machine() or "not reported"],
            ["Target", metadata.get("target", "not reported")],
            *cpu_rows,
            ["Visible memory", detect_memory()],
            ["Execution", runner],
            ["Input", relative_display(args.input, source_root)],
            ["Source revision", revision],
            ["Chart generator", gnuplot_version],
        ],
    )
    build = markdown_table(
        ["Build option", "Value"],
        [
            ["Toolchain", metadata.get("toolchain", "not reported")],
            [
                "Compiler",
                "{} {}".format(
                    metadata.get("compiler_id", "not reported"),
                    metadata.get("compiler_version", ""),
                ).strip(),
            ],
            ["Compiler path", metadata.get("compiler_path", "not reported")],
            ["Build type", metadata.get("build_type", "not reported")],
            ["C flags", f"`{flags}`"],
            ["Compiler warnings", metadata.get("compiler_warnings", "not reported")],
            ["Auto-vectorization", metadata.get("auto_vectorization", "not reported")],
            ["SIMD", metadata.get("simd", "not reported")],
            ["Threading", metadata.get("threading", "not reported")],
            ["Sanitizers", metadata.get("sanitizers", "not reported")],
            ["Stack protector", metadata.get("stack_protector", "not reported")],
            ["External comparison", metadata.get("benchmark_compare", "not reported")],
            ["Warm-up calls", args.warmup],
            ["Measured calls", args.repeat],
        ],
    )
    return environment, build


def gnuplot_escape(value: str) -> str:
    return value.replace("\\", "\\\\").replace('"', '\\"')


def write_chart(
    output_path: Path,
    title: str,
    categories: list[str],
    series: list[tuple[str, list[int | None]]],
    gnuplot: str,
) -> None:
    if not categories or not series:
        raise ValueError(f"chart has no data: {title}")

    output_path.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix="sgl-gnuplot-") as directory:
        temporary = Path(directory)
        data_path = temporary / "chart.tsv"
        script_path = temporary / "chart.gnuplot"
        with data_path.open("w", newline="", encoding="utf-8") as stream:
            writer = csv.writer(stream, delimiter="\t")
            writer.writerow(["case", *(name for name, _values in series)])
            for index, category in enumerate(categories):
                writer.writerow([
                    category,
                    *(
                        "NaN" if values[index] is None else values[index]
                        for _name, values in series
                    ),
                ])

        lines = [
            "set terminal svg size 1280,720 dynamic enhanced "
            "font 'sans,16' background rgb '#ffffff'",
            f'set output "{gnuplot_escape(str(output_path.resolve()))}"',
            f'set title "{gnuplot_escape(title)}" font ",24" '
            "textcolor rgb '#0f172a'",
            "set datafile separator '\\t'",
            "set ylabel 'Median latency (us, logarithmic scale)' "
            "textcolor rgb '#0f172a'",
            "set logscale y 10",
            "set yrange [1:*]",
            "set format y '%.0f'",
            "set grid ytics lc rgb '#cbd5e1'",
            "set border lc rgb '#64748b'",
            "set style data histograms",
            "set style histogram clustered gap 1",
            "set style fill solid 0.86 border rgb '#ffffff'",
            "set boxwidth 0.88",
            "set key outside top center horizontal maxrows 2 "
            "textcolor rgb '#0f172a'",
            "set tics textcolor rgb '#1e293b'",
        ]
        if len(categories) > 3:
            lines.append("set xtics rotate by -18")
        for index, color in enumerate(COLORS, start=1):
            lines.append(f"set style line {index} lc rgb '{color}'")
        lines.append(
            'plot for [column=2:{}] "{}" using column:xticlabels(1) '
            "title columnheader ls (column - 1)".format(
                len(series) + 1,
                gnuplot_escape(str(data_path.resolve())),
            )
        )
        script_path.write_text("\n".join(lines) + "\n", encoding="utf-8")
        subprocess.run([gnuplot, str(script_path)], check=True)

    if (not output_path.exists()) or output_path.stat().st_size == 0:
        raise RuntimeError(f"gnuplot did not produce {output_path}")
    svg = output_path.read_text(encoding="utf-8")
    normalized_svg = "\n".join(
        line.rstrip() for line in svg.splitlines()
    ).rstrip() + "\n"
    output_path.write_text(normalized_svg, encoding="utf-8")


def backend_label(backend: str, threads: int) -> str:
    display = {
        "simd": "SIMD",
        "simd-lut": "SIMD LUT",
        "generic-lut": "generic LUT",
        "ne10-c": "NE10 C",
        "ne10-neon": "NE10 NEON",
        "ne10": "NE10 NEON",
        "ne10-c-bilinear": "NE10 C bilinear",
        "cairo": "Cairo",
        "cairo-copy": "Cairo copy",
        "cairo-nearest": "Cairo nearest",
    }.get(backend, backend)
    return f"{display} {threads}t"


def operation_chart_data(
    rows: list[dict[str, object]],
) -> tuple[list[str], list[tuple[str, list[int | None]]]]:
    categories = list(dict.fromkeys(str(row["operation"]) for row in rows))
    keys = sorted(
        {
            (str(row["backend"]), int(row["threads"]))
            for row in rows
        },
        key=lambda item: (item[1], BACKEND_ORDER.get(item[0], 99)),
    )
    series: list[tuple[str, list[int | None]]] = []
    for backend, threads in keys:
        values: list[int | None] = []
        for operation in categories:
            found = next(
                (
                    row
                    for row in rows
                    if str(row["operation"]) == operation
                    and str(row["backend"]) == backend
                    and int(row["threads"]) == threads
                ),
                None,
            )
            values.append(None if found is None else int(found["median_us"]))
        series.append((backend_label(backend, threads), values))
    return categories, series


def operation_result_table(rows: list[dict[str, object]]) -> str:
    result_rows = []
    for row in sorted(rows, key=lambda item: int(item["case"])):
        result_rows.append([
            row["case"],
            row["operation"],
            row["backend"],
            row["threads"],
            f"{row['width']}x{row['height']}",
            f"{int(row['median_us']):,}",
            f"{int(row['avg_us']):,}",
            f"{int(row['min_us']):,}",
            f"{int(row['max_us']):,}",
            f"{int(row['pixels_per_second']):,}",
        ])
    return markdown_table(
        [
            "Case",
            "Operation",
            "Backend",
            "Threads",
            "Size",
            "Median (us)",
            "Average (us)",
            "Min (us)",
            "Max (us)",
            "Pixels/s",
        ],
        result_rows,
    )


def operation_markdown(
    benchmark: str,
    rows: list[dict[str, object]],
    environment: str,
    build: str,
) -> str:
    table = operation_result_table(rows)
    coverage = external_coverage_table(benchmark)
    return f"""{LICENSE_HEADER}
{BENCHMARK_TITLES[benchmark]}
{'=' * len(BENCHMARK_TITLES[benchmark])}

[Benchmark index](../README.md) | [Main README](../../README.md#operation-benchmarks)

{BENCHMARK_DESCRIPTIONS[benchmark]} The chart and table are generated from
[`benchmark.csv`](benchmark.csv) after the benchmark test completes.

Median Latency
--------------
![{BENCHMARK_TITLES[benchmark]} median latency](latency.svg)

External Backend Coverage
-------------------------
{coverage}

Cairo surface, context, and pattern setup is outside the timed region; the
timed operation includes `cairo_paint()` and destination surface flush.

Test Environment
----------------
{environment}

Build Configuration
-------------------
{build}

Results
-------
{table}

Results depend on the input, CPU frequency policy, scheduler load, compiler,
and build options. Refresh them on the target machine before comparison.
"""


def convert_source_rows(
    rows: list[dict[str, object]], source: str
) -> list[dict[str, object]]:
    prefix = f"{source}-to-"
    return [
        row for row in rows if str(row["operation"]).startswith(prefix)
    ]


def convert_chart_data(
    rows: list[dict[str, object]],
    source: str,
    destinations: tuple[str, ...],
) -> tuple[list[str], list[tuple[str, list[int | None]]]]:
    source_rows = convert_source_rows(rows, source)
    keys = sorted(
        {
            (str(row["backend"]), int(row["threads"]))
            for row in source_rows
        },
        key=lambda item: (item[1], BACKEND_ORDER.get(item[0], 99)),
    )
    series: list[tuple[str, list[int | None]]] = []
    for backend, threads in keys:
        values: list[int | None] = []
        for destination in destinations:
            operation = f"{source}-to-{destination}"
            found = next(
                (
                    row
                    for row in source_rows
                    if str(row["operation"]) == operation
                    and str(row["backend"]) == backend
                    and int(row["threads"]) == threads
                ),
                None,
            )
            values.append(None if found is None else int(found["median_us"]))
        if any(value is not None for value in values):
            series.append((backend_label(backend, threads), values))
    return list(destinations), series


def convert_source_markdown(
    source: str,
    rows: list[dict[str, object]],
    environment: str,
    build: str,
) -> str:
    title = f"Convert Benchmark: {source.upper()} Source"
    table = operation_result_table(rows)
    coverage = external_coverage_table("convert")
    return f"""{LICENSE_HEADER}
{title}
{'=' * len(title)}

[Convert index](../README.md) | [Benchmark index](../../README.md) | [Main README](../../../README.md#operation-benchmarks)

Every supported RGB and YUV destination is measured from the
`{source}` source format.

RGB Destinations
----------------
![{source} to RGB formats median latency](rgb.svg)

YUV Destinations
----------------
![{source} to YUV formats median latency](yuv.svg)

External Backend Coverage
-------------------------
{coverage}

Test Environment
----------------
{environment}

Build Configuration
-------------------
{build}

Results
-------
{table}

Results depend on the input, CPU frequency policy, scheduler load, compiler,
and build options. Refresh them on the target machine before comparison.
"""


def convert_index_markdown(
    rows: list[dict[str, object]],
    environment: str,
    build: str,
) -> str:
    configurations = sorted(
        {
            (str(row["backend"]), int(row["threads"])) for row in rows
        },
        key=lambda item: (item[1], BACKEND_ORDER.get(item[0], 99)),
    )
    index_rows = [
        [
            source.upper(),
            f"[{source} results]({source}/README.md)",
            f"[{len(CONVERT_RGB_FORMATS)} RGB]({source}/rgb.svg)",
            f"[{len(CONVERT_YUV_FORMATS)} YUV]({source}/yuv.svg)",
        ]
        for source in CONVERT_FORMATS
    ]
    index_table = markdown_table(
        ["Source FourCC", "Result table", "RGB chart", "YUV chart"],
        index_rows,
    )
    configuration_text = ", ".join(
        backend_label(backend, threads)
        for backend, threads in configurations
    )
    pair_count = len(CONVERT_FORMATS) * len(CONVERT_FORMATS)
    sgl_case_count = sum(
        1 for row in rows if str(row["backend"]) in ("generic", "simd")
    )
    external_case_count = len(rows) - sgl_case_count
    coverage = external_coverage_table("convert")
    return f"""{LICENSE_HEADER}
Convert Benchmark
=================

[Benchmark index](../README.md) | [Main README](../../README.md#operation-benchmarks)

{BENCHMARK_DESCRIPTIONS['convert']} The SGL matrix contains {pair_count} FourCC
directions and {sgl_case_count:,} measured cases. External libraries add
{external_case_count:,} capability-matched cases. Available configurations:
{configuration_text}.

Representative Overview
-----------------------
### RGBA source
![RGBA to RGB formats median latency](rgba/rgb.svg)
![RGBA to YUV formats median latency](rgba/yuv.svg)

### YUV444 planar source
![YUV444 planar to RGB formats median latency](yuv444-planar/rgb.svg)
![YUV444 planar to YUV formats median latency](yuv444-planar/yuv.svg)

Input and Raw Outputs
---------------------
The benchmark scales the complete `resource/sample.png` input to 1280x720,
then derives all 12 RGB and 10 YUV source FourCC images before measuring every
valid conversion direction. `SGL_TEST_CONVERT_MATRIX_WIDTH` and
`SGL_TEST_CONVERT_MATRIX_HEIGHT` configure this matrix resolution.

Every backend and worker-count result is saved under
`build/<toolchain>/convert-output/`, including byte-identical results. File
output time is outside the measured conversion interval. Names encode the case
index, conversion family, source and destination, worker count, and resolution:

    0000_convert_generic_rgb2rgb_rgba-to-rgba_1thread_1280x720.raw

Source Matrix
-------------
{index_table}

External Backend Coverage
-------------------------
{coverage}

Cairo surface and context setup is outside the timed region; the timed
operation includes `cairo_paint()` and destination surface flush.

Test Environment
----------------
{environment}

Build Configuration
-------------------
{build}

The complete CSV is stored in [`benchmark.csv`](benchmark.csv). Results depend
on the input, CPU frequency policy, scheduler load, compiler, and build
options. Refresh them on the target machine before comparison.
"""


def resize_series_keys(
    rows: list[dict[str, object]], channel: int
) -> list[tuple[str, int]]:
    available = {
        (str(row["backend"]), int(row["threads"]))
        for row in rows
        if int(row["channels"]) == channel
    }
    maximum_threads = max(
        (
            threads
            for backend, threads in available
            if backend in ("generic-lut", "simd-lut")
        ),
        default=1,
    )
    candidates = [
        ("generic", 1),
        ("generic-lut", maximum_threads),
        ("simd", 1),
        ("simd-lut", maximum_threads),
        ("cairo", 1),
        ("ne10-c", 1),
        ("ne10-neon", 1),
        ("ne10", 1),
    ]
    return [item for item in candidates if item in available]


def resize_chart_data(
    rows: list[dict[str, object]],
    channel: int,
    method_filter: str | None,
) -> tuple[list[str], list[tuple[str, list[int | None]]]]:
    sizes = sorted({
        (int(row["width"]), int(row["height"]))
        for row in rows
        if int(row["channels"]) == channel
        and (method_filter is None or str(row["method"]) == method_filter)
    }, key=lambda item: item[0] * item[1])
    methods = METHODS if method_filter is None else (method_filter,)
    cases = [(method, width, height) for width, height in sizes for method in methods]
    categories = [
        f"{method} {width}x{height}" if method_filter is None else f"{width}x{height}"
        for method, width, height in cases
    ]
    series: list[tuple[str, list[int | None]]] = []
    for backend, threads in resize_series_keys(rows, channel):
        values: list[int | None] = []
        for method, width, height in cases:
            found = find_resize_row(
                rows, channel, method, width, height, backend, threads
            )
            values.append(None if found is None else int(found["median_us"]))
        if any(value is not None for value in values):
            series.append((backend_label(backend, threads), values))
    return categories, series


def resize_result_table(rows: list[dict[str, object]], channel: int) -> str:
    result_rows: list[list[object]] = []
    selected = sorted(
        (row for row in rows if int(row["channels"]) == channel),
        key=lambda item: int(item["case"]),
    )
    for row in selected:
        result_rows.append([
            row["case"],
            row["method"],
            row["backend"],
            row["threads"],
            f"{row['width']}x{row['height']}",
            f"{int(row['median_us']):,}",
            f"{int(row['avg_us']):,}",
            f"{int(row['min_us']):,}",
            f"{int(row['max_us']):,}",
        ])
    return markdown_table(
        [
            "Case",
            "Method",
            "Backend",
            "Threads",
            "Size",
            "Median (us)",
            "Average (us)",
            "Min (us)",
            "Max (us)",
        ],
        result_rows,
    )


def resize_channel_markdown(
    channel: int,
    environment: str,
    build: str,
    table: str,
) -> str:
    label = "Channel" if channel == 1 else "Channels"
    navigation = []
    for item in CHANNELS:
        item_label = "channel" if item == 1 else "channels"
        if item == channel:
            navigation.append(f"**{item} {item_label}**")
        else:
            navigation.append(f"[{item} {item_label}](../{item}ch/README.md)")
    return f"""{LICENSE_HEADER}
SGL Resize Benchmark: {channel} {label}
{'=' * (22 + len(str(channel)) + len(label))}

[Resize index](../README.md) | [Benchmark index](../../README.md) | {' | '.join(navigation)}

The charts and Markdown result table are generated from
[`../benchmark.csv`](../benchmark.csv) after the resize benchmark completes.

Overview
--------
![SGL {channel}-channel resize benchmark](summary.svg)

Nearest
-------
![SGL {channel}-channel nearest resize benchmark](nearest.svg)

Bilinear
--------
![SGL {channel}-channel bilinear resize benchmark](bilinear.svg)

Bicubic
-------
![SGL {channel}-channel bicubic resize benchmark](bicubic.svg)

Test Environment
----------------
{environment}

Build Configuration
-------------------
{build}

Results
-------
{table}

Results depend on the input, CPU frequency policy, scheduler load, compiler,
and build options. Refresh them on the target machine before comparison.
"""


def resize_index_markdown(environment: str, build: str) -> str:
    return f"""{LICENSE_HEADER}
Resize Benchmark Reports
========================

[Main README](../../README.md#resize-benchmark) | [Benchmark index](../README.md)

All charts are generated by gnuplot and all result tables are generated as
Markdown from [`benchmark.csv`](benchmark.csv).

| Input channels | Report | External baselines |
| ---: | --- | --- |
| 1 | [1-channel report](1ch/README.md) | SGL paths only |
| 2 | [2-channel report](2ch/README.md) | SGL paths only |
| 3 | [3-channel report](3ch/README.md) | SGL paths only |
| 4 | [4-channel report](4ch/README.md) | Cairo and NE10 where available |

Outputs
-------
| Output | Path |
| --- | --- |
| Build-local benchmark CSV | `build/<toolchain>/benchmark/resize-benchmark.csv` |
| Build-local Markdown and charts | `build/<toolchain>/benchmark/resize/` |
| Versioned benchmark CSV | `benchmark/resize/benchmark.csv` |
| Resized PNG debug outputs | `build/<toolchain>/resize-output/` |

Input and Measurement Matrix
----------------------------
The default `resource/sample.png` input is 1920x1080. When sibling
`sample-1ch.png` through `sample-4ch.png` files are present, the runner uses
all four channel variants; otherwise it measures only the supplied PNG. The
default matrix includes a 640x480 downscale and a 2560x1440 upscale.

The `generic` and `simd` rows include temporary lookup-table construction in
each resize call. The `generic-lut` and `simd-lut` rows build the table once per
case and reuse it throughout the timed loop. Reports use median latency by
default to reduce scheduler outlier bias.

Comparison Backends
-------------------
| Backend | Methods | Coverage |
| --- | --- | --- |
| Cairo + pixman | nearest, bilinear | 4-channel, single-thread baseline |
| NE10 | bilinear | 4-channel `ne10-c` and `ne10-neon` baselines |

Cairo and NE10 do not consume the SGL threadpool, so their rows use one worker.
NE10 runtime dispatch must select NEON, and its C and NEON outputs are compared
byte-for-byte outside the timed loop. Cairo requires Meson and Ninja; NE10
makes the complete comparison benchmark an AArch64-only target.

Interactive CSV Viewer
----------------------
Open `../../tools/resize-benchmark-viewer.html` for optional CSV filtering and
chart export. Versioned report charts are generated by gnuplot; the browser
viewer is an inspection tool and is not part of report generation.

Test Environment
----------------
{environment}

Build Configuration
-------------------
{build}
"""


def generate_operation_report(
    args: argparse.Namespace,
    rows: list[dict[str, object]],
    output_dir: Path,
    environment: str,
    build: str,
) -> None:
    categories, series = operation_chart_data(rows)
    write_chart(
        output_dir / "latency.svg",
        f"SGL {BENCHMARK_TITLES[args.benchmark]} Median Latency",
        categories,
        series,
        args.gnuplot,
    )
    report = operation_markdown(
        args.benchmark, rows, environment, build
    )
    (output_dir / "README.md").write_text(report, encoding="utf-8")


def generate_convert_report(
    args: argparse.Namespace,
    rows: list[dict[str, object]],
    output_dir: Path,
    environment: str,
    build: str,
) -> None:
    stale_chart = output_dir / "latency.svg"
    if stale_chart.exists():
        stale_chart.unlink()
    for source in CONVERT_FORMATS:
        source_dir = output_dir / source
        source_rows = convert_source_rows(rows, source)
        source_dir.mkdir(parents=True, exist_ok=True)
        for family, destinations in (
            ("rgb", CONVERT_RGB_FORMATS),
            ("yuv", CONVERT_YUV_FORMATS),
        ):
            categories, series = convert_chart_data(
                rows, source, destinations
            )
            write_chart(
                source_dir / f"{family}.svg",
                f"SGL Convert: {source.upper()} to {family.upper()}",
                categories,
                series,
                args.gnuplot,
            )
        report = convert_source_markdown(
            source, source_rows, environment, build
        )
        (source_dir / "README.md").write_text(
            report, encoding="utf-8"
        )
    index = convert_index_markdown(rows, environment, build)
    (output_dir / "README.md").write_text(index, encoding="utf-8")


def generate_resize_report(
    args: argparse.Namespace,
    rows: list[dict[str, object]],
    output_dir: Path,
    environment: str,
    build: str,
) -> None:
    (output_dir / "README.md").write_text(
        resize_index_markdown(environment, build), encoding="utf-8"
    )
    for channel in CHANNELS:
        channel_dir = output_dir / f"{channel}ch"
        channel_dir.mkdir(parents=True, exist_ok=True)
        channel_label = "Channel" if channel == 1 else "Channels"
        categories, series = resize_chart_data(rows, channel, None)
        write_chart(
            channel_dir / "summary.svg",
            f"SGL Resize Median Latency: {channel} {channel_label}",
            categories,
            series,
            args.gnuplot,
        )
        for method in METHODS:
            categories, series = resize_chart_data(rows, channel, method)
            write_chart(
                channel_dir / f"{method}.svg",
                f"SGL {method.title()} Resize: {channel} {channel_label}",
                categories,
                series,
                args.gnuplot,
            )
        (channel_dir / "README.md").write_text(
            resize_channel_markdown(
                channel,
                environment,
                build,
                resize_result_table(rows, channel),
            ),
            encoding="utf-8",
        )


def main() -> int:
    args = parse_arguments()
    csv_path = Path(args.csv)
    output_dir = Path(args.output_dir)
    source_root = Path(args.source_root)

    try:
        rows = read_rows(csv_path, args.benchmark)
        if args.benchmark == "resize":
            validate_resize_rows(
                rows,
                args.require_simd,
                args.require_cairo,
                args.require_ne10,
            )
        else:
            validate_operation_rows(
                rows,
                args.benchmark,
                args.require_simd,
                args.require_cairo,
                args.require_ne10,
            )
            if args.benchmark == "convert":
                validate_convert_rows(rows)
        if args.validate_only:
            print(f"validated {args.benchmark} benchmark: {csv_path}")
            return 0
        metadata = read_metadata(Path(args.metadata))
        environment, build = environment_tables(metadata, args, source_root)
        output_dir.mkdir(parents=True, exist_ok=True)
        report_csv = output_dir / "benchmark.csv"
        if csv_path.resolve() != report_csv.resolve():
            shutil.copy2(csv_path, report_csv)
        if args.benchmark == "resize":
            generate_resize_report(
                args, rows, output_dir, environment, build
            )
        elif args.benchmark == "convert":
            generate_convert_report(
                args, rows, output_dir, environment, build
            )
        else:
            generate_operation_report(
                args, rows, output_dir, environment, build
            )
    except (OSError, ValueError, RuntimeError, subprocess.CalledProcessError) as error:
        raise SystemExit(f"error: {error}") from error

    print(f"benchmark report: {output_dir.resolve()}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
