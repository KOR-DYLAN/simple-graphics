#!/usr/bin/env python3
#
# SPDX-License-Identifier: MIT
#
# Copyright (c) 2025 Dylan Hong
#
# This file is released under the MIT License.
# For conditions of distribution and use, see the LICENSE file.
#

"""Generate interpolation-specific resize benchmark SVG snapshots."""

import argparse
import csv
from pathlib import Path
from xml.sax.saxutils import escape


BACKENDS = [
    ("generic", 1, "generic 1t", "#38bdf8"),
    ("simd", 1, "simd 1t", "#fb7185"),
    ("simd-lut", 1, "simd-lut 1t", "#a78bfa"),
    ("simd-lut", 8, "simd-lut 8t", "#4ade80"),
]
CHART_BACKENDS = [
    ("generic", 1, "generic 1t", "#38bdf8"),
    ("simd", 1, "simd 1t", "#fb7185"),
    ("simd-lut", 1, "simd-lut 1t", "#a78bfa"),
    ("simd-lut", 8, "simd-lut 8t", "#4ade80"),
    ("cairo", 1, "cairo 1t", "#f97316"),
    ("ne10", 1, "ne10 1t", "#60a5fa"),
]
EXTERNAL_BACKENDS = [
    ("generic", 1, "generic 1t"),
    ("simd", 1, "simd 1t"),
    ("simd-lut", 8, "simd-lut 8t"),
    ("cairo", 1, "cairo 1t"),
    ("ne10", 1, "ne10 1t"),
]
EXTERNAL_BACKEND_NAMES = {"cairo", "ne10"}
EXTERNAL_METHODS = ["nearest", "bilinear"]
METHODS = ["nearest", "bilinear", "bicubic"]
SOURCE_PIXELS = 1920 * 1080


def parse_args():
    parser = argparse.ArgumentParser(
        description="Generate interpolation-specific resize benchmark SVG snapshots."
    )
    parser.add_argument(
        "--csv",
        default="benchmark/resize-benchmark.csv",
        help="benchmark CSV path",
    )
    parser.add_argument(
        "--output-dir",
        default="benchmark",
        help="directory where SVG files are written",
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
            row["size"] = f"{row['width']}x{row['height']}"
            rows.append(row)

    return rows


def size_pixels(size):
    width, height = size.split("x", 1)
    return int(width) * int(height)


def direction_label(size):
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


def find_row(rows, method, backend, threads, size):
    found = None

    for row in rows:
        if (
            (row["channels"] == 4)
            and (row["method"] == method)
            and (row["backend"] == backend)
            and (row["threads"] == threads)
            and (row["size"] == size)
        ):
            found = row
            break

    return found


def has_external_rows(rows, method=None):
    found = False

    for row in rows:
        if (
            (row["channels"] == 4)
            and (row["backend"] in EXTERNAL_BACKEND_NAMES)
            and ((method is None) or (row["method"] == method))
        ):
            found = True
            break

    return found


def external_sizes(rows, method=None):
    sizes = {
        row["size"]
        for row in rows
        if (
            (row["channels"] == 4)
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
            (row["channels"] == 4)
            and (row["backend"] in EXTERNAL_BACKEND_NAMES)
            and ((method is None) or (row["method"] == method))
        )
    }

    return [item for item in EXTERNAL_METHODS if item in methods]


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
        lines.extend(
            [
                svg_rect("header", 48, top + 82, 1184, 36, rx="6"),
                svg_text("head", 70, top + 106, "case"),
                svg_text("head", 340, top + 106, "generic 1t", text_anchor="end"),
                svg_text("head", 500, top + 106, "simd 1t", text_anchor="end"),
                svg_text("head", 660, top + 106, "simd-lut 8t", text_anchor="end"),
                svg_text("head", 810, top + 106, "cairo 1t", text_anchor="end"),
                svg_text("head", 960, top + 106, "ne10 1t", text_anchor="end"),
                svg_text("head", 1200, top + 106, "best external / simd-lut 8t", text_anchor="end"),
            ]
        )

        for size in sizes:
            for item_method in methods:
                if find_row(rows, item_method, "simd-lut", 8, size) is not None:
                    l8 = find_row(rows, item_method, "simd-lut", 8, size)
                    external = [
                        find_row(rows, item_method, backend, threads, size)
                        for backend, threads in (("cairo", 1), ("ne10", 1))
                    ]
                    external = [row for row in external if row is not None]

                    lines.append(f'  <line class="line" x1="48" y1="{y - 20}" x2="1232" y2="{y - 20}"/>')
                    lines.append(svg_text("cell", 70, y, f"{item_method} {direction_label(size)}"))

                    for x, backend, threads, _label in (
                        (340, "generic", 1, "generic 1t"),
                        (500, "simd", 1, "simd 1t"),
                        (660, "simd-lut", 8, "simd-lut 8t"),
                        (810, "cairo", 1, "cairo 1t"),
                        (960, "ne10", 1, "ne10 1t"),
                    ):
                        row = find_row(rows, item_method, backend, threads, size)
                        value = ms(row["avg_us"]) if row is not None else "n/a"
                        value_class = "cell" if row is not None else "muted"
                        lines.append(svg_text(value_class, x, y, value, text_anchor="end"))

                    if len(external) > 0:
                        best_external = min(external, key=lambda row: row["avg_us"])
                        ratio = relative_label(best_external["avg_us"], l8["avg_us"])
                    else:
                        ratio = "n/a"

                    lines.append(svg_text("speed", 1200, y, ratio, text_anchor="end"))
                    y += 34


def method_sizes(rows, method):
    sizes = {
        row["size"]
        for row in rows
        if (row["channels"] == 4) and (row["method"] == method)
    }

    return sorted(sizes, key=size_pixels)


def append_table(lines, rows, method, sizes):
    y = 242

    lines.extend(
        [
            svg_rect("panel", 28, 132, 1224, 240, rx="8"),
            svg_text("text", 58, 172, f"{method} path comparison", font_size="22", font_weight="800"),
            svg_text(
                "muted",
                58,
                194,
                "4-channel rows; external columns are filled when Cairo/NE10 compare rows exist.",
                font_size="13",
            ),
            svg_rect("header", 48, 216, 1184, 38, rx="6"),
            svg_text("head", 70, 240, "direction"),
            svg_text("head", 270, 240, "generic 1t", text_anchor="end"),
            svg_text("head", 395, 240, "simd 1t", text_anchor="end"),
            svg_text("head", 540, 240, "simd-lut 1t", text_anchor="end"),
            svg_text("head", 685, 240, "simd-lut 8t", text_anchor="end"),
            svg_text("head", 825, 240, "generic/lut8", text_anchor="end"),
            svg_text("head", 955, 240, "lut 1t/8t", text_anchor="end"),
            svg_text("head", 1065, 240, "cairo 1t", text_anchor="end"),
            svg_text("head", 1200, 240, "ne10 1t", text_anchor="end"),
        ]
    )

    for size in sizes:
        values = [
            find_row(rows, method, backend, threads, size) for backend, threads, _label, _color in BACKENDS
        ]
        generic = values[0]
        lut8 = values[3]
        cairo = find_row(rows, method, "cairo", 1, size)
        ne10 = find_row(rows, method, "ne10", 1, size)

        y += 48
        lines.append(f'  <line class="line" x1="48" y1="{y - 28}" x2="1232" y2="{y - 28}"/>')
        lines.append(svg_text("cell", 70, y, direction_label(size)))
        lines.append(svg_text("cell", 270, y, ms(generic["avg_us"]), text_anchor="end"))
        lines.append(svg_text("cell", 395, y, ms(values[1]["avg_us"]), text_anchor="end"))
        lines.append(svg_text("cell", 540, y, ms(values[2]["avg_us"]), text_anchor="end"))
        lines.append(svg_text("cell", 685, y, ms(lut8["avg_us"]), text_anchor="end"))
        lines.append(
            svg_text(
                "speed",
                825,
                y,
                ratio_label(generic["avg_us"], lut8["avg_us"]),
                text_anchor="end",
            )
        )
        lines.append(
            svg_text(
                "speed",
                955,
                y,
                ratio_label(values[2]["avg_us"], lut8["avg_us"]),
                text_anchor="end",
            )
        )
        lines.append(
            svg_text(
                "cell" if cairo is not None else "muted",
                1065,
                y,
                ms(cairo["avg_us"]) if cairo is not None else "n/a",
                text_anchor="end",
            )
        )
        lines.append(
            svg_text(
                "cell" if ne10 is not None else "muted",
                1200,
                y,
                ms(ne10["avg_us"]) if ne10 is not None else "n/a",
                text_anchor="end",
            )
        )


def append_direction_latency_chart(lines, rows, method, size, top):
    items = []
    y = top + 76
    max_value = 1

    for backend, threads, label, color in CHART_BACKENDS:
        row = find_row(rows, method, backend, threads, size)
        if row is not None:
            items.append((label, row["avg_us"], color))
            max_value = max(max_value, row["avg_us"])

    lines.extend(
        [
            svg_rect("panel", 28, top, 1224, 310, rx="8"),
            svg_text("text", 58, top + 40, f"{method} {direction_label(size)} latency comparison", font_size="22", font_weight="800"),
            svg_text(
                "muted",
                58,
                top + 62,
                "SGL paths and available Cairo/NE10 rows; bars are normalized to the slowest displayed row.",
                font_size="13",
            ),
        ]
    )

    if len(items) == 0:
        lines.append(svg_text("muted", 78, top + 126, "No rows are available for this chart.", font_size="15", font_weight="700"))
    else:
        if (method == "bicubic") and not has_external_rows(rows, method):
            lines.append(svg_text("muted", 842, top + 62, "Cairo/NE10 bicubic rows are unavailable.", font_size="13"))

    for label, value_us, color in items:
        width = max(4, round((value_us / max_value) * 760))
        y += 30
        lines.append(svg_text("small", 78, y + 13, label))
        lines.append(f'  <rect x="360" y="{y}" width="{width}" height="16" rx="3" fill="{color}"/>')
        lines.append(svg_text("cell", 360 + width + 14, y + 13, ms(value_us)))


def build_svg(rows, method, csv_path):
    sizes = method_sizes(rows, method)
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
        f'<svg xmlns="http://www.w3.org/2000/svg" width="1280" height="1120" viewBox="0 0 1280 1120" role="img" aria-labelledby="title desc">',
        f'  <title id="title">SGL resize benchmark {escape(method)} comparison</title>',
        f'  <desc id="desc">{escape(method)} resize benchmark snapshot measured from {escape(str(csv_path))}. The chart compares 4-channel downscale and upscale rows.</desc>',
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
        '  <rect class="bg" x="0" y="0" width="1280" height="1120"/>',
        "",
        svg_text("text", 36, 52, f"SGL Resize Benchmark: {method}", font_size="30", font_weight="800"),
        svg_text("muted", 36, 80, f"Average latency from {csv_path}, Release build, resource/sample.png", font_size="14"),
        svg_text("muted", 36, 102, "4-channel SGL rows; 640x480 downscale and 2560x1440 upscale are shown together.", font_size="13"),
    ]

    append_table(lines, rows, method, sizes)
    append_direction_latency_chart(lines, rows, method, sizes[0], 410)
    append_direction_latency_chart(lines, rows, method, sizes[1], 750)
    lines.extend(
        [
            svg_text("small", 36, 1098, "Note: benchmark results are workload and environment dependent; rerun on the target machine before publishing final claims."),
            "</svg>",
        ]
    )

    return "\n".join(lines) + "\n"


def build_summary_svg(rows, csv_path):
    sizes = method_sizes(rows, METHODS[0])
    cases = [(size, method) for size in sizes for method in METHODS]
    max_value = max(
        find_row(rows, method, "simd-lut", 8, size)["avg_us"]
        for size, method in cases
    )
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
        '<svg xmlns="http://www.w3.org/2000/svg" width="1280" height="1340" viewBox="0 0 1280 1340" role="img" aria-labelledby="title desc">',
        '  <title id="title">SGL resize benchmark direction comparison</title>',
        f'  <desc id="desc">Resize benchmark snapshot measured from {escape(str(csv_path))}. The chart compares 4-channel downscale and upscale rows across nearest, bilinear, and bicubic interpolation.</desc>',
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
        '  <rect class="bg" x="0" y="0" width="1280" height="1340"/>',
        "",
        svg_text("text", 36, 52, "SGL Resize Benchmark Snapshot", font_size="30", font_weight="800"),
        svg_text("muted", 36, 80, f"Average latency from {csv_path}, Release build, resource/sample.png", font_size="14"),
        svg_text("muted", 36, 102, "4-channel SGL rows; 640x480 downscale and 2560x1440 upscale are shown together.", font_size="13"),
        svg_rect("panel", 28, 132, 1224, 430, rx="8"),
        svg_text("text", 58, 172, "SGL path comparison by resize direction", font_size="22", font_weight="800"),
        svg_text("muted", 58, 194, "The ratio compares generic 1-thread latency against SIMD LUT with 8 threads.", font_size="13"),
        svg_rect("header", 48, 216, 1184, 38, rx="6"),
        svg_text("head", 70, 240, "direction"),
        svg_text("head", 240, 240, "method"),
        svg_text("head", 430, 240, "generic 1t", text_anchor="end"),
        svg_text("head", 580, 240, "simd 1t", text_anchor="end"),
        svg_text("head", 760, 240, "simd-lut 1t", text_anchor="end"),
        svg_text("head", 930, 240, "simd-lut 8t", text_anchor="end"),
        svg_text("head", 1200, 240, "generic / simd-lut 8t", text_anchor="end"),
    ]
    y = 244

    for size, method in cases:
        values = [
            find_row(rows, method, backend, threads, size) for backend, threads, _label, _color in BACKENDS
        ]
        generic = values[0]
        lut8 = values[3]

        y += 48
        lines.append(f'  <line class="line" x1="48" y1="{y - 28}" x2="1232" y2="{y - 28}"/>')
        lines.append(svg_text("cell", 70, y, direction_label(size)))
        lines.append(svg_text("cell", 240, y, method))
        lines.append(svg_text("cell", 430, y, ms(generic["avg_us"]), text_anchor="end"))
        lines.append(svg_text("cell", 580, y, ms(values[1]["avg_us"]), text_anchor="end"))
        lines.append(svg_text("cell", 760, y, ms(values[2]["avg_us"]), text_anchor="end"))
        lines.append(svg_text("cell", 930, y, ms(lut8["avg_us"]), text_anchor="end"))
        lines.append(
            svg_text(
                "speed",
                1200,
                y,
                ratio_label(generic["avg_us"], lut8["avg_us"]),
                text_anchor="end",
            )
        )

    lines.extend(
        [
            svg_rect("panel", 28, 600, 1224, 360, rx="8"),
            svg_text("text", 58, 640, "Threaded SIMD LUT latency by interpolation", font_size="22", font_weight="800"),
            svg_text("muted", 58, 662, "Bars show SIMD LUT with 8 threads; each bar is normalized to the slowest displayed case.", font_size="13"),
            svg_rect("down", 58, 684, 14, 10, rx="2"),
            svg_text("small", 80, 694, "downscale 640x480"),
            svg_rect("up", 218, 684, 14, 10, rx="2"),
            svg_text("small", 240, 694, "upscale 2560x1440"),
        ]
    )
    y = 724

    for size, method in cases:
        value_us = find_row(rows, method, "simd-lut", 8, size)["avg_us"]
        width = max(4, round((value_us / max_value) * 820))
        css_class = "down" if size_pixels(size) < SOURCE_PIXELS else "up"

        lines.append(svg_text("small", 78, y + 13, f"{method} {direction_label(size)}"))
        lines.append(svg_rect(css_class, 310, y, width, 16, rx="3"))
        lines.append(svg_text("cell", 310 + width + 14, y + 13, ms(value_us)))
        y += 38

    append_external_comparison(lines, rows, 990, 270)
    lines.extend(
        [
            svg_text("small", 36, 1298, "Note: benchmark results are workload and environment dependent; rerun on the target machine before publishing final claims."),
            svg_text("small", 36, 1320, "Use the interpolation-specific SVGs for a less crowded view of nearest, bilinear, and bicubic rows."),
            "</svg>",
        ]
    )

    return "\n".join(lines) + "\n"


def main():
    args = parse_args()
    csv_path = Path(args.csv)
    output_dir = Path(args.output_dir)
    rows = read_rows(csv_path)

    output_dir.mkdir(parents=True, exist_ok=True)
    summary_path = output_dir / "resize-benchmark-summary.svg"
    summary_path.write_text(build_summary_svg(rows, csv_path), encoding="utf-8")
    print(summary_path)

    for method in METHODS:
        output_path = output_dir / f"resize-benchmark-{method}.svg"
        output_path.write_text(build_svg(rows, method, csv_path), encoding="utf-8")
        print(output_path)


if __name__ == "__main__":
    main()
