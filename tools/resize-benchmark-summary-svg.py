#!/usr/bin/env python3
#
# SPDX-License-Identifier: MIT
#
# Copyright (c) 2025 Dylan Hong
#
# This file is released under the MIT License.
# For conditions of distribution and use, see the LICENSE file.
#

"""Generate representative and channel-specific resize benchmark SVGs."""

import argparse
import csv
from pathlib import Path
from xml.sax.saxutils import escape


SUMMARY_BACKENDS = [
    ("generic", 1, "generic 1t", "#38bdf8"),
    ("simd", 1, "simd 1t", "#fb7185"),
    ("simd-lut", 1, "simd-lut 1t", "#a78bfa"),
    ("simd-lut", 8, "simd-lut 8t", "#4ade80"),
]
TABLE_BACKENDS = [
    ("generic", 1, "generic 1t"),
    ("generic", 8, "generic 8t"),
    ("generic-lut", 1, "generic-lut 1t"),
    ("generic-lut", 8, "generic-lut 8t"),
    ("simd", 1, "simd 1t"),
    ("simd", 8, "simd 8t"),
    ("simd-lut", 1, "simd-lut 1t"),
    ("simd-lut", 8, "simd-lut 8t"),
]
OPTIONAL_TABLE_BACKENDS = [
    ("cairo", 1, "cairo 1t"),
    ("ne10-c", 1, "ne10 C 1t"),
    ("ne10-neon", 1, "ne10 NEON 1t"),
]
CHART_BACKENDS = [
    ("generic-lut", 1, "generic-lut 1t", "#38bdf8"),
    ("generic-lut", 8, "generic-lut 8t", "#2563eb"),
    ("simd-lut", 1, "simd-lut 1t", "#fb7185"),
    ("simd-lut", 8, "simd-lut 8t", "#4ade80"),
]
OPTIONAL_CHART_BACKENDS = [
    ("cairo", 1, "cairo 1t", "#f97316"),
    ("ne10-neon", 1, "ne10 NEON 1t", "#a78bfa"),
]
EXTERNAL_BACKEND_NAMES = {"cairo", "ne10", "ne10-c", "ne10-neon"}
EXTERNAL_METHODS = ["nearest", "bilinear"]
REQUIRED_CAIRO_ROWS = [
    ("cairo", "nearest", "cairo"),
    ("cairo", "bilinear", "cairo"),
]
REQUIRED_NE10_ROWS = [
    ("ne10-c", "bilinear", "ne10 C"),
    ("ne10-neon", "bilinear", "ne10 NEON"),
]
METHODS = ["nearest", "bilinear", "bicubic"]
REQUIRED_CHANNELS = [1, 2, 3, 4]
REPRESENTATIVE_CHANNEL = 4
SOURCE_PIXELS = 1920 * 1080
MIN_SVG_WIDTH = 1280
PAGE_MARGIN = 28
CONTENT_LEFT = 48
CONTENT_RIGHT_PADDING = 20
TABLE_CELL_PADDING = 16
TABLE_ROW_HEIGHT = 52
CHART_ROW_HEIGHT = 34


def parse_args():
    parser = argparse.ArgumentParser(
        description="Generate representative and channel-specific resize benchmark SVGs."
    )
    parser.add_argument(
        "--csv",
        default="benchmark/resize/benchmark.csv",
        help="benchmark CSV path",
    )
    parser.add_argument(
        "--output-dir",
        default="benchmark/resize",
        help="directory where SVG files are written",
    )
    parser.add_argument(
        "--validate-only",
        action="store_true",
        help="validate required benchmark rows without writing SVG files",
    )
    parser.add_argument(
        "--require-external",
        action="store_true",
        help="also require Cairo and NE10 external comparison rows",
    )
    parser.add_argument(
        "--require-cairo",
        action="store_true",
        help="also require Cairo external comparison rows",
    )
    parser.add_argument(
        "--require-ne10",
        action="store_true",
        help="also require NE10 external comparison rows",
    )
    parser.add_argument(
        "--require-simd",
        action="store_true",
        help="also require SIMD resize backend rows",
    )
    return parser.parse_args()


def read_rows(csv_path):
    rows = []

    with Path(csv_path).open(newline="", encoding="utf-8") as stream:
        for row in csv.DictReader(stream):
            row["channels"] = int(row.get("channels", "4") or "4")
            row["threads"] = int(row["threads"])
            row["width"] = int(row["width"])
            row["height"] = int(row["height"])
            row["avg_us"] = int(row["avg_us"])
            row["median_us"] = int(row.get("median_us") or row["avg_us"])
            row["size"] = f"{row['width']}x{row['height']}"
            rows.append(row)

    return rows


def size_pixels(size):
    width, height = size.split("x", 1)
    return int(width) * int(height)


def resize_case_label(size):
    pixels = size_pixels(size)
    label = "same-size"

    if pixels < SOURCE_PIXELS:
        label = "downscale"
    elif pixels > SOURCE_PIXELS:
        label = "upscale"

    return f"{label} {size}"


def ms(value_us):
    return f"{value_us / 1000.0:.3f} ms"


def ratio_label(numerator_us, denominator_us):
    label = "n/a"

    if denominator_us > 0:
        label = f"{numerator_us / denominator_us:.2f}x"

    return label


def relative_label(value_us, baseline_us):
    label = "n/a"

    if (value_us > 0) and (baseline_us > 0):
        if value_us >= baseline_us:
            label = f"{value_us / baseline_us:.2f}x slower"
        else:
            label = f"{baseline_us / value_us:.2f}x faster"

    return label


def svg_text(css_class, x, y, text, **attrs):
    attr_text = "".join(f' {key.replace("_", "-")}="{value}"' for key, value in attrs.items())
    return f'  <text class="{css_class}" x="{x}" y="{y}"{attr_text}>{escape(text)}</text>'


def svg_rect(css_class, x, y, width, height, **attrs):
    attr_text = "".join(f' {key.replace("_", "-")}="{value}"' for key, value in attrs.items())
    return f'  <rect class="{css_class}" x="{x}" y="{y}" width="{width}" height="{height}"{attr_text}/>'


def find_row(
    rows,
    method,
    backend,
    threads,
    size,
    channel=REPRESENTATIVE_CHANNEL,
):
    found = None
    backend_names = {backend}

    if backend == "ne10-neon":
        backend_names.add("ne10")

    for row in rows:
        if (
            (row["channels"] == channel)
            and (row["method"] == method)
            and (row["backend"] in backend_names)
            and (row["threads"] == threads)
            and (row["size"] == size)
        ):
            found = row
            break

    return found


def validate_sgl_rows(rows, require_simd):
    missing = []
    required_backends = [
        ("generic", 1, "generic 1t"),
        ("generic", 8, "generic 8t"),
        ("generic-lut", 1, "generic-lut 1t"),
        ("generic-lut", 8, "generic-lut 8t"),
    ]

    if require_simd:
        required_backends.extend(
            [
                ("simd", 1, "simd 1t"),
                ("simd", 8, "simd 8t"),
                ("simd-lut", 1, "simd-lut 1t"),
                ("simd-lut", 8, "simd-lut 8t"),
            ]
        )

    for method in METHODS:
        sizes = {
            row["size"]
            for row in rows
            if (
                (row["channels"] == REPRESENTATIVE_CHANNEL)
                and (row["method"] == method)
                and (row["backend"] == "generic")
                and (row["threads"] == 1)
            )
        }

        if len(sizes) == 0:
            missing.append(f"4ch generic 1t {method} baseline")
            continue

        for channel in REQUIRED_CHANNELS:
            for size in sorted(sizes, key=size_pixels):
                for backend, threads, label in required_backends:
                    if find_row(
                        rows,
                        method,
                        backend,
                        threads,
                        size,
                        channel,
                    ) is None:
                        missing.append(
                            f"{channel}ch {label} {method} {size}"
                        )

    if len(missing) > 0:
        raise ValueError(
            "required channel benchmark rows are missing: "
            + ", ".join(missing)
        )


def validate_external_rows(rows, required_rows, description):
    missing = []

    for backend, method, label in required_rows:
        sizes = {
            row["size"]
            for row in rows
            if (
                (row["channels"] == REPRESENTATIVE_CHANNEL)
                and (row["method"] == method)
                and (row["backend"] == "generic")
                and (row["threads"] == 1)
            )
        }

        if len(sizes) == 0:
            missing.append(f"generic 1t {method} baseline")
        else:
            for size in sorted(sizes, key=size_pixels):
                if find_row(rows, method, backend, 1, size) is None:
                    missing.append(f"{label} 1t {method} {size}")

    if len(missing) > 0:
        raise ValueError(
            f"required {description} benchmark rows are missing: "
            + ", ".join(missing)
        )


def external_sizes(rows, method=None):
    sizes = {
        row["size"]
        for row in rows
        if (
            (row["channels"] == REPRESENTATIVE_CHANNEL)
            and (row["backend"] in EXTERNAL_BACKEND_NAMES)
            and ((method is None) or (row["method"] == method))
        )
    }

    return sorted(sizes, key=size_pixels)


def external_methods(rows, method=None):
    methods = {
        row["method"]
        for row in rows
        if (
            (row["channels"] == REPRESENTATIVE_CHANNEL)
            and (row["backend"] in EXTERNAL_BACKEND_NAMES)
            and ((method is None) or (row["method"] == method))
        )
    }

    return [item for item in EXTERNAL_METHODS if item in methods]


def comparison_baseline(rows, method, size, channel=REPRESENTATIVE_CHANNEL):
    for backend, threads, label in (
        ("simd-lut", 8, "simd-lut 8t"),
        ("generic-lut", 8, "generic-lut 8t"),
    ):
        row = find_row(rows, method, backend, threads, size, channel)
        if row is not None:
            return row, label

    return None, "baseline"


def append_external_comparison(lines, rows, top, height, method=None):
    title = "Cairo / NE10 external comparison"
    methods = external_methods(rows, method)
    sizes = external_sizes(rows, method)

    if method is not None:
        title = f"{method} Cairo / NE10 comparison"

    lines.extend(
        [
            svg_rect("panel", 28, top, 1224, height, rx="8"),
            svg_text("text", 58, top + 40, title, font_size="22", font_weight="800"),
            svg_text(
                "muted",
                58,
                top + 62,
                "4-channel rows only; Cairo provides nearest/bilinear and NE10 provides bilinear RGBA.",
                font_size="13",
            ),
        ]
    )

    if (len(methods) == 0) or (len(sizes) == 0):
        if method == "bicubic":
            message = "Cairo/NE10 do not provide bicubic rows in this benchmark."
        else:
            message = "No Cairo/NE10 rows in this CSV; rerun with -DWITH_BENCHMARK_COMPARE=ON to fill this section."

        lines.append(svg_text("muted", 78, top + 112, message, font_size="15", font_weight="700"))
    else:
        y = top + 146
        baseline_label = "simd-lut 8t"
        for size in sizes:
            for item_method in methods:
                _baseline, candidate_label = comparison_baseline(rows, item_method, size)
                if _baseline is not None:
                    baseline_label = candidate_label
                    break
            else:
                continue
            break

        lines.extend(
            [
                svg_rect("header", 48, top + 82, 1184, 36, rx="6"),
                svg_text("head", 70, top + 106, "case"),
                svg_text("head", 340, top + 106, "generic 1t", text_anchor="end"),
                svg_text("head", 500, top + 106, "simd 1t", text_anchor="end"),
                svg_text("head", 660, top + 106, "simd-lut 8t", text_anchor="end"),
                svg_text("head", 810, top + 106, "cairo 1t", text_anchor="end"),
                svg_text("head", 960, top + 106, "ne10 NEON 1t", text_anchor="end"),
                svg_text("head", 1200, top + 106, f"best external / {baseline_label}", text_anchor="end"),
            ]
        )

        for size in sizes:
            for item_method in methods:
                baseline, _baseline_label = comparison_baseline(rows, item_method, size)
                if baseline is not None:
                    external = [
                        find_row(rows, item_method, backend, threads, size)
                        for backend, threads in (
                            ("cairo", 1),
                            ("ne10-c", 1),
                            ("ne10-neon", 1),
                        )
                    ]
                    external = [row for row in external if row is not None]

                    lines.append(f'  <line class="line" x1="48" y1="{y - 20}" x2="1232" y2="{y - 20}"/>')
                    lines.append(svg_text("cell", 70, y, f"{item_method} {resize_case_label(size)}"))

                    for x, backend, threads, _label in (
                        (340, "generic", 1, "generic 1t"),
                        (500, "simd", 1, "simd 1t"),
                        (660, "simd-lut", 8, "simd-lut 8t"),
                        (810, "cairo", 1, "cairo 1t"),
                        (960, "ne10-neon", 1, "ne10 NEON 1t"),
                    ):
                        row = find_row(rows, item_method, backend, threads, size)
                        value = ms(row["median_us"]) if row is not None else "n/a"
                        value_class = "cell" if row is not None else "muted"
                        lines.append(svg_text(value_class, x, y, value, text_anchor="end"))

                    if len(external) > 0:
                        best_external = min(external, key=lambda row: row["median_us"])
                        ratio = relative_label(best_external["median_us"], baseline["median_us"])
                    else:
                        ratio = "n/a"

                    lines.append(svg_text("speed", 1200, y, ratio, text_anchor="end"))
                    y += 34


def method_sizes(rows, method, channel=REPRESENTATIVE_CHANNEL):
    sizes = {
        row["size"]
        for row in rows
        if (row["channels"] == channel) and (row["method"] == method)
    }

    return sorted(sizes, key=size_pixels)


def backend_is_available(rows, method, backend, threads, sizes, channel):
    return any(
        find_row(rows, method, backend, threads, size, channel) is not None
        for size in sizes
    )


def method_table_backends(rows, method, sizes, channel):
    backends = list(TABLE_BACKENDS)

    for backend in OPTIONAL_TABLE_BACKENDS:
        name, threads, _label = backend
        if backend_is_available(
            rows, method, name, threads, sizes, channel
        ):
            backends.append(backend)

    return backends


def estimated_column_width(label, values, minimum):
    longest = max([label, *values], key=len)
    return max(minimum, (len(longest) * 8) + (TABLE_CELL_PADDING * 2))


def method_table_layout(rows, method, sizes, channel):
    backends = method_table_backends(rows, method, sizes, channel)
    widths = [
        estimated_column_width(
            "resize case",
            [resize_case_label(size) for size in sizes],
            200,
        )
    ]

    for backend, threads, label in backends:
        values = []
        for size in sizes:
            row = find_row(
                rows, method, backend, threads, size, channel
            )
            values.append(ms(row["median_us"]) if row is not None else "n/a")
        widths.append(estimated_column_width(label, values, 136))

    required_width = sum(widths) + CONTENT_LEFT + CONTENT_RIGHT_PADDING + PAGE_MARGIN
    svg_width = max(MIN_SVG_WIDTH, required_width)
    available_width = svg_width - CONTENT_LEFT - CONTENT_RIGHT_PADDING - PAGE_MARGIN
    extra_width = available_width - sum(widths)

    if extra_width > 0:
        per_column = extra_width // len(widths)
        remainder = extra_width % len(widths)
        widths = [
            width + per_column + (1 if index < remainder else 0)
            for index, width in enumerate(widths)
        ]

    return backends, widths, svg_width


def method_table_height(size_count):
    return 132 + (size_count * TABLE_ROW_HEIGHT)


def append_table(
    lines,
    rows,
    method,
    sizes,
    top,
    svg_width,
    backends,
    widths,
    channel,
):
    panel_height = method_table_height(len(sizes))
    panel_width = svg_width - (PAGE_MARGIN * 2)
    header_top = top + 84
    header_width = svg_width - CONTENT_LEFT - CONTENT_RIGHT_PADDING - PAGE_MARGIN
    column_starts = []
    current_x = CONTENT_LEFT

    for width in widths:
        column_starts.append(current_x)
        current_x += width

    lines.extend(
        [
            svg_rect("panel", PAGE_MARGIN, top, panel_width, panel_height, rx="8"),
            svg_text("text", 58, top + 40, f"{method} path comparison", font_size="22", font_weight="800"),
            svg_text(
                "muted",
                58,
                top + 62,
                f"{channel}-channel median latency; external columns appear only when measured for this method.",
                font_size="13",
            ),
            svg_rect("header", CONTENT_LEFT, header_top, header_width, 38, rx="6"),
            svg_text("head", column_starts[0] + TABLE_CELL_PADDING, header_top + 24, "resize case"),
        ]
    )

    for index, (_backend, _threads, label) in enumerate(backends, start=1):
        lines.append(
            svg_text(
                "head",
                column_starts[index] + widths[index] - TABLE_CELL_PADDING,
                header_top + 24,
                label,
                text_anchor="end",
            )
        )

    row_y = header_top + 72
    line_right = svg_width - PAGE_MARGIN - CONTENT_RIGHT_PADDING
    for size in sizes:
        lines.append(
            f'  <line class="line" x1="{CONTENT_LEFT}" y1="{row_y - 26}" '
            f'x2="{line_right}" y2="{row_y - 26}"/>'
        )
        lines.append(
            svg_text(
                "cell",
                column_starts[0] + TABLE_CELL_PADDING,
                row_y,
                resize_case_label(size),
            )
        )

        for index, (backend, threads, _label) in enumerate(backends, start=1):
            row = find_row(
                rows, method, backend, threads, size, channel
            )
            value = ms(row["median_us"]) if row is not None else "n/a"
            css_class = "cell" if row is not None else "muted"
            lines.append(
                svg_text(
                    css_class,
                    column_starts[index] + widths[index] - TABLE_CELL_PADDING,
                    row_y,
                    value,
                    text_anchor="end",
                )
            )

        row_y += TABLE_ROW_HEIGHT


def chart_items(rows, method, size, channel):
    items = []

    for backend, threads, label, color in [*CHART_BACKENDS, *OPTIONAL_CHART_BACKENDS]:
        row = find_row(
            rows, method, backend, threads, size, channel
        )
        if row is not None:
            items.append((label, row["median_us"], color))

    return items


def chart_panel_height(item_count):
    return 190 if item_count == 0 else 106 + (item_count * CHART_ROW_HEIGHT)


def append_resize_case_latency_chart(
    lines, method, size, top, svg_width, items, channel
):
    panel_height = chart_panel_height(len(items))
    panel_width = svg_width - (PAGE_MARGIN * 2)
    panel_right = svg_width - PAGE_MARGIN
    max_value = max((value for _label, value, _color in items), default=1)
    label_width = max(
        220,
        max((len(label) for label, _value, _color in items), default=0) * 8 + 44,
    )
    value_width = max(
        112,
        max((len(ms(value)) for _label, value, _color in items), default=0) * 8 + 28,
    )
    bar_x = CONTENT_LEFT + label_width
    bar_max_width = max(240, panel_right - 30 - value_width - bar_x)

    lines.extend(
        [
            svg_rect("panel", PAGE_MARGIN, top, panel_width, panel_height, rx="8"),
            svg_text(
                "text",
                58,
                top + 40,
                f"{method} {resize_case_label(size)} latency comparison",
                font_size="22",
                font_weight="800",
            ),
            svg_text(
                "muted",
                58,
                top + 62,
                f"{channel}-channel prebuilt-LUT SGL paths and available external baselines; bars are normalized to the slowest displayed row.",
                font_size="13",
            ),
        ]
    )

    if len(items) == 0:
        lines.append(
            svg_text(
                "muted",
                78,
                top + 126,
                "No rows are available for this chart.",
                font_size="15",
                font_weight="700",
            )
        )

    row_top = top + 94
    for label, value_us, color in items:
        width = max(4, round((value_us / max_value) * bar_max_width))
        lines.append(svg_text("small", 78, row_top + 14, label))
        lines.append(
            f'  <rect x="{bar_x}" y="{row_top}" width="{width}" '
            f'height="18" rx="3" fill="{color}"/>'
        )
        lines.append(svg_text("cell", bar_x + width + 14, row_top + 14, ms(value_us)))
        row_top += CHART_ROW_HEIGHT


def build_svg(
    rows,
    method,
    csv_path,
    channel=REPRESENTATIVE_CHANNEL,
):
    sizes = method_sizes(rows, method, channel)
    backends, widths, svg_width = method_table_layout(
        rows, method, sizes, channel
    )
    table_top = 132
    table_height = method_table_height(len(sizes))
    next_top = table_top + table_height + 38
    chart_layouts = []

    for size in sizes:
        items = chart_items(rows, method, size, channel)
        height = chart_panel_height(len(items))
        chart_layouts.append((size, next_top, items))
        next_top += height + 38

    svg_height = next_top + 12
    lines = [
        "<!--",
        "SPDX-License-Identifier: MIT",
        "",
        "Copyright (c) 2025 Dylan Hong",
        "",
        "This file is released under the MIT License.",
        "For conditions of distribution and use, see the LICENSE file.",
        "-->",
        "",
        f'<svg xmlns="http://www.w3.org/2000/svg" width="{svg_width}" height="{svg_height}" viewBox="0 0 {svg_width} {svg_height}" role="img" aria-labelledby="title desc">',
        f'  <title id="title">SGL {channel}-channel resize benchmark {escape(method)} comparison</title>',
        f'  <desc id="desc">{escape(method)} resize benchmark snapshot measured from {escape(str(csv_path))}. The chart compares available {channel}-channel resize cases.</desc>',
        "  <defs>",
        "    <style>",
        "      .bg { fill: #101418; }",
        "      .panel { fill: #171d23; stroke: #34424a; stroke-width: 1.5; }",
        "      .header { fill: #1f262e; }",
        "      .line { stroke: #34424a; stroke-width: 1; }",
        "      .text { fill: #eef4f3; font-family: Inter, \"Noto Sans KR\", system-ui, sans-serif; }",
        "      .muted { fill: #9ba9a7; font-family: Inter, \"Noto Sans KR\", system-ui, sans-serif; }",
        "      .small { fill: #9ba9a7; font-family: Inter, \"Noto Sans KR\", system-ui, sans-serif; font-size: 12px; }",
        "      .cell { fill: #eef4f3; font-family: Inter, \"Noto Sans KR\", system-ui, sans-serif; font-size: 14px; font-weight: 650; }",
        "      .head { fill: #c6d4d1; font-family: Inter, \"Noto Sans KR\", system-ui, sans-serif; font-size: 12px; font-weight: 800; }",
        "      .speed { fill: #facc15; font-family: Inter, \"Noto Sans KR\", system-ui, sans-serif; font-size: 14px; font-weight: 800; }",
        "    </style>",
        "  </defs>",
        "",
        f'  <rect class="bg" x="0" y="0" width="{svg_width}" height="{svg_height}"/>',
        "",
        svg_text("text", 36, 52, f"SGL Resize Benchmark: {method}", font_size="30", font_weight="800"),
        svg_text("muted", 36, 80, f"Median latency from {csv_path}, Release build, resource/sample.png", font_size="14"),
        svg_text("muted", 36, 102, f"{channel}-channel median latency; table columns and chart rows adapt to available benchmark entries.", font_size="13"),
    ]

    append_table(
        lines,
        rows,
        method,
        sizes,
        table_top,
        svg_width,
        backends,
        widths,
        channel,
    )
    for size, top, items in chart_layouts:
        append_resize_case_latency_chart(
            lines, method, size, top, svg_width, items, channel
        )

    lines.extend(
        [
            svg_text(
                "small",
                36,
                svg_height - 22,
                "Note: benchmark results are workload and environment dependent; rerun on the target machine before publishing final claims.",
            ),
            "</svg>",
        ]
    )

    return "\n".join(lines) + "\n"


def build_summary_svg(
    rows,
    csv_path,
    channel=REPRESENTATIVE_CHANNEL,
):
    sizes = method_sizes(rows, METHODS[0], channel)
    cases = [(size, method) for size in sizes for method in METHODS]
    summary_target = ("simd-lut", 8, "simd-lut 8t")
    if any(
        find_row(rows, method, summary_target[0], summary_target[1], size, channel)
        is None
        for size, method in cases
    ):
        summary_target = ("generic-lut", 8, "generic-lut 8t")

    max_value = max(
        find_row(
            rows, method, summary_target[0], summary_target[1], size, channel
        )["median_us"]
        for size, method in cases
    )
    svg_height = 1340 if channel == REPRESENTATIVE_CHANNEL else 1030
    lines = [
        "<!--",
        "SPDX-License-Identifier: MIT",
        "",
        "Copyright (c) 2025 Dylan Hong",
        "",
        "This file is released under the MIT License.",
        "For conditions of distribution and use, see the LICENSE file.",
        "-->",
        "",
        f'<svg xmlns="http://www.w3.org/2000/svg" width="1280" height="{svg_height}" viewBox="0 0 1280 {svg_height}" role="img" aria-labelledby="title desc">',
        f'  <title id="title">SGL {channel}-channel resize benchmark overview</title>',
        f'  <desc id="desc">Resize benchmark snapshot measured from {escape(str(csv_path))}. The chart compares {channel}-channel downscale and upscale rows across nearest, bilinear, and bicubic interpolation.</desc>',
        "  <defs>",
        "    <style>",
        "      .bg { fill: #101418; }",
        "      .panel { fill: #171d23; stroke: #34424a; stroke-width: 1.5; }",
        "      .header { fill: #1f262e; }",
        "      .line { stroke: #34424a; stroke-width: 1; }",
        "      .text { fill: #eef4f3; font-family: Inter, \"Noto Sans KR\", system-ui, sans-serif; }",
        "      .muted { fill: #9ba9a7; font-family: Inter, \"Noto Sans KR\", system-ui, sans-serif; }",
        "      .small { fill: #9ba9a7; font-family: Inter, \"Noto Sans KR\", system-ui, sans-serif; font-size: 12px; }",
        "      .cell { fill: #eef4f3; font-family: Inter, \"Noto Sans KR\", system-ui, sans-serif; font-size: 14px; font-weight: 650; }",
        "      .head { fill: #c6d4d1; font-family: Inter, \"Noto Sans KR\", system-ui, sans-serif; font-size: 12px; font-weight: 800; }",
        "      .speed { fill: #facc15; font-family: Inter, \"Noto Sans KR\", system-ui, sans-serif; font-size: 14px; font-weight: 800; }",
        "      .down { fill: #38bdf8; }",
        "      .up { fill: #fb7185; }",
        "    </style>",
        "  </defs>",
        "",
        f'  <rect class="bg" x="0" y="0" width="1280" height="{svg_height}"/>',
        "",
        svg_text("text", 36, 52, "SGL Resize Benchmark Snapshot", font_size="30", font_weight="800"),
        svg_text("muted", 36, 80, f"Median latency from {csv_path}, Release build, resource/sample.png", font_size="14"),
        svg_text("muted", 36, 102, f"{channel}-channel SGL rows; 640x480 downscale and 2560x1440 upscale are shown together.", font_size="13"),
        svg_rect("panel", 28, 132, 1224, 430, rx="8"),
        svg_text("text", 58, 172, "SGL path comparison by resize case", font_size="22", font_weight="800"),
        svg_text("muted", 58, 194, f"The ratio compares generic 1-thread latency against {summary_target[2]}.", font_size="13"),
        svg_rect("header", 48, 216, 1184, 38, rx="6"),
        svg_text("head", 70, 240, "resize case"),
        svg_text("head", 240, 240, "method"),
        svg_text("head", 430, 240, "generic 1t", text_anchor="end"),
        svg_text("head", 580, 240, "simd 1t", text_anchor="end"),
        svg_text("head", 760, 240, "simd-lut 1t", text_anchor="end"),
        svg_text("head", 930, 240, summary_target[2], text_anchor="end"),
        svg_text("head", 1200, 240, f"generic / {summary_target[2]}", text_anchor="end"),
    ]
    y = 244

    for size, method in cases:
        values = [
            find_row(rows, method, backend, threads, size, channel)
            for backend, threads, _label, _color in SUMMARY_BACKENDS
        ]
        generic = values[0]
        target = find_row(
            rows, method, summary_target[0], summary_target[1], size, channel
        )

        y += 48
        lines.append(f'  <line class="line" x1="48" y1="{y - 28}" x2="1232" y2="{y - 28}"/>')
        lines.append(svg_text("cell", 70, y, resize_case_label(size)))
        lines.append(svg_text("cell", 240, y, method))
        lines.append(svg_text("cell", 430, y, ms(generic["median_us"]), text_anchor="end"))
        for x, row in ((580, values[1]), (760, values[2]), (930, target)):
            css_class = "cell" if row is not None else "muted"
            value = ms(row["median_us"]) if row is not None else "n/a"
            lines.append(svg_text(css_class, x, y, value, text_anchor="end"))
        lines.append(
            svg_text(
                "speed",
                1200,
                y,
                ratio_label(generic["median_us"], target["median_us"]),
                text_anchor="end",
            )
        )

    lines.extend(
        [
            svg_rect("panel", 28, 600, 1224, 360, rx="8"),
            svg_text("text", 58, 640, f"Threaded {summary_target[2]} latency by interpolation", font_size="22", font_weight="800"),
            svg_text("muted", 58, 662, f"Bars show {summary_target[2]}; each bar is normalized to the slowest displayed case.", font_size="13"),
            svg_rect("down", 58, 684, 14, 10, rx="2"),
            svg_text("small", 80, 694, "downscale 640x480"),
            svg_rect("up", 218, 684, 14, 10, rx="2"),
            svg_text("small", 240, 694, "upscale 2560x1440"),
        ]
    )
    y = 724

    for size, method in cases:
        value_us = find_row(
            rows, method, summary_target[0], summary_target[1], size, channel
        )["median_us"]
        width = max(4, round((value_us / max_value) * 820))
        css_class = "down" if size_pixels(size) < SOURCE_PIXELS else "up"

        lines.append(svg_text("small", 78, y + 13, f"{method} {resize_case_label(size)}"))
        lines.append(svg_rect(css_class, 310, y, width, 16, rx="3"))
        lines.append(svg_text("cell", 310 + width + 14, y + 13, ms(value_us)))
        y += 38

    if channel == REPRESENTATIVE_CHANNEL:
        append_external_comparison(lines, rows, 990, 270)
        footer_y = 1298
    else:
        footer_y = 988

    lines.extend(
        [
            svg_text("small", 36, footer_y, "Note: benchmark results are workload and environment dependent; rerun on the target machine before publishing final claims."),
            svg_text("small", 36, footer_y + 22, "Use the interpolation-specific SVGs for a less crowded view of nearest, bilinear, and bicubic rows."),
            "</svg>",
        ]
    )

    return "\n".join(lines) + "\n"


def build_channel_markdown(channel):
    navigation = []
    title_label = f"{channel} Channel" if channel == 1 else f"{channel} Channels"

    for item in REQUIRED_CHANNELS:
        label = f"{item} channel" if item == 1 else f"{item} channels"
        if item == channel:
            navigation.append(f"**{label}**")
        else:
            navigation.append(f"[{label}](../{item}ch/README.md)")

    if channel == REPRESENTATIVE_CHANNEL:
        description = (
            "This is the representative benchmark displayed in the main README. "
            "It uses `resource/sample-4ch.png` and includes Cairo and NE10 rows "
            "where those external backends support the interpolation method."
        )
    else:
        description = (
            f"This report uses `resource/sample-{channel}ch.png`. "
            "Cairo and NE10 are omitted because the external comparison paths "
            "in this benchmark require 4-channel input."
        )

    return f"""<!--
SPDX-License-Identifier: MIT

Copyright (c) 2025 Dylan Hong

This file is released under the MIT License.
For conditions of distribution and use, see the LICENSE file.
-->

SGL Resize Benchmark: {title_label}
================================

[Benchmark index](../README.md) | [Main README](../../../README.md#resize-benchmark) | {" | ".join(navigation)}

{description}

Overview
--------
![SGL {channel}-channel resize benchmark summary](summary.svg)

Nearest
-------
![SGL {channel}-channel nearest resize benchmark](nearest.svg)

Bilinear
--------
![SGL {channel}-channel bilinear resize benchmark](bilinear.svg)

Bicubic
-------
![SGL {channel}-channel bicubic resize benchmark](bicubic.svg)
"""


def main():
    args = parse_args()
    csv_path = Path(args.csv)
    output_dir = Path(args.output_dir)
    rows = read_rows(csv_path)
    require_cairo = args.require_cairo or args.require_external
    require_ne10 = args.require_ne10 or args.require_external

    try:
        validate_sgl_rows(rows, args.require_simd)
        if require_cairo:
            validate_external_rows(rows, REQUIRED_CAIRO_ROWS, "Cairo")
        if require_ne10:
            validate_external_rows(rows, REQUIRED_NE10_ROWS, "NE10")
    except ValueError as error:
        raise SystemExit(f"error: {error}") from error

    if args.validate_only:
        message = f"validated required generic 1/2/3/4-channel SGL rows: {csv_path}"
        if args.require_simd:
            message = f"validated required generic and SIMD SGL rows: {csv_path}"
        if require_cairo and require_ne10:
            message = (
                "validated required 1/2/3/4-channel SGL and "
                f"Cairo/NE10 rows: {csv_path}"
            )
        elif require_cairo:
            message = (
                "validated required 1/2/3/4-channel SGL and "
                f"Cairo rows: {csv_path}"
            )
        elif require_ne10:
            message = (
                "validated required 1/2/3/4-channel SGL and "
                f"NE10 rows: {csv_path}"
            )
        print(message)
        return

    output_dir.mkdir(parents=True, exist_ok=True)
    for channel in REQUIRED_CHANNELS:
        channel_dir = output_dir / f"{channel}ch"
        channel_dir.mkdir(parents=True, exist_ok=True)

        report_path = channel_dir / "README.md"
        report_path.write_text(
            build_channel_markdown(channel),
            encoding="utf-8",
        )
        print(report_path)

        summary_path = channel_dir / "summary.svg"
        summary_path.write_text(
            build_summary_svg(rows, csv_path, channel),
            encoding="utf-8",
        )
        print(summary_path)

        for method in METHODS:
            output_path = channel_dir / f"{method}.svg"
            output_path.write_text(
                build_svg(rows, method, csv_path, channel),
                encoding="utf-8",
            )
            print(output_path)


if __name__ == "__main__":
    main()
