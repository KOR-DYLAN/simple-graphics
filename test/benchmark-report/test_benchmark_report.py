#!/usr/bin/env python3
#
# SPDX-License-Identifier: MIT
#
# Copyright (c) 2025 Dylan Hong
#
# This file is released under the MIT License.
# For conditions of distribution and use, see the LICENSE file.

"""Regression tests for the common benchmark report generator."""

from __future__ import annotations

import argparse
import csv
import json
from pathlib import Path
import subprocess
import sys
import tempfile
from typing import cast
import unittest
from unittest import mock


ROOT = Path(__file__).resolve().parents[2]
REPORT_SCRIPT = ROOT / "tools" / "benchmark-report.py"
sys.path.insert(0, str(ROOT / "tools"))

import benchmark_environment


def parse_arguments() -> argparse.Namespace:
    parser = argparse.ArgumentParser()
    parser.add_argument("--gnuplot", required=True)
    return parser.parse_args()


class BenchmarkEnvironmentTest(unittest.TestCase):
    def test_windows_cim_json_is_parsed(self) -> None:
        detected = cast(
            benchmark_environment.WindowsCpuDetails,
            benchmark_environment._parse_windows_cpu(
                '{"name":"Test CPU","physical_cores":12,"logical_cpus":16}'
            ),
        )

        self.assertIsNotNone(detected)
        self.assertEqual("Test CPU", detected.name)
        self.assertEqual(12, detected.physical_cores)
        self.assertEqual(16, detected.logical_cpus)

    def test_wsl_uses_windows_host_cpu_and_runtime_cpu_count(self) -> None:
        windows_cpu = benchmark_environment.WindowsCpuDetails(
            name="Snapdragon(R) X 12-core X1E80100 @ 3.40 GHz",
            physical_cores=12,
            logical_cpus=12,
        )

        with (
            mock.patch.object(
                benchmark_environment, "is_wsl_runtime", return_value=True
            ),
            mock.patch.object(
                benchmark_environment,
                "_find_windows_powershell",
                return_value="powershell.exe",
            ),
            mock.patch.object(
                benchmark_environment,
                "_query_windows_cpu",
                return_value=windows_cpu,
            ),
            mock.patch.object(
                benchmark_environment.os, "cpu_count", return_value=8
            ),
        ):
            detected = benchmark_environment.detect_cpu()

        self.assertEqual(windows_cpu.name, detected.name)
        self.assertEqual(12, detected.host_physical_cores)
        self.assertEqual(12, detected.host_logical_cpus)
        self.assertEqual(8, detected.runtime_logical_cpus)
        self.assertTrue(detected.is_wsl)

    def test_wsl_falls_back_to_linux_cpu_when_cim_is_unavailable(self) -> None:
        with (
            mock.patch.object(
                benchmark_environment, "is_wsl_runtime", return_value=True
            ),
            mock.patch.object(
                benchmark_environment,
                "_find_windows_powershell",
                return_value=None,
            ),
            mock.patch.object(
                benchmark_environment,
                "_detect_linux_cpu",
                return_value="ARM implementer 0x51, part 0x001",
            ),
            mock.patch.object(
                benchmark_environment.os, "cpu_count", return_value=8
            ),
        ):
            detected = benchmark_environment.detect_cpu()

        self.assertEqual("ARM implementer 0x51, part 0x001", detected.name)
        self.assertIsNone(detected.host_physical_cores)
        self.assertIsNone(detected.host_logical_cpus)
        self.assertEqual(8, detected.runtime_logical_cpus)
        self.assertTrue(detected.is_wsl)


class BenchmarkReportTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.gnuplot = parse_arguments().gnuplot

    def write_metadata(self, directory: Path) -> Path:
        path = directory / "build-metadata.txt"
        path.write_text(
            "toolchain=llvm\n"
            "compiler_id=Clang\n"
            "compiler_version=18.1.3\n"
            "compiler_path=/usr/bin/clang\n"
            "build_type=Release\n"
            "effective_c_flags=-O2 -DNDEBUG\n"
            "compiler_warnings=ON (-Wall, -Wextra)\n"
            "auto_vectorization=OFF (-fno-vectorize)\n"
            "simd=ON (NEON)\n"
            "threading=ON (pthread)\n"
            "sanitizers=OFF\n"
            "stack_protector=OFF\n"
            "benchmark_compare=OFF\n"
            "target=Linux/aarch64\n"
            "execution=native\n",
            encoding="utf-8",
        )
        return path

    def write_compile_commands(self, directory: Path) -> Path:
        path = directory / "compile_commands.json"
        path.write_text(
            json.dumps([
                {
                    "file": "/src/library/sgl-core/example.c",
                    "command": (
                        "/usr/bin/clang -I/src/include -O2 -DNDEBUG "
                        "-std=gnu99 -Wall -Wextra -fno-vectorize "
                        "-o example.o -c /src/library/sgl-core/example.c"
                    ),
                }
            ]),
            encoding="utf-8",
        )
        return path

    def run_report(
        self,
        benchmark: str,
        csv_path: Path,
        output_dir: Path,
        metadata: Path,
        compile_commands: Path,
        extra_arguments: tuple[str, ...] = (),
    ) -> subprocess.CompletedProcess[str]:
        command = [
            sys.executable,
            "-B",
            str(REPORT_SCRIPT),
            "--benchmark",
            benchmark,
            "--csv",
            str(csv_path),
            "--output-dir",
            str(output_dir),
            "--metadata",
            str(metadata),
            "--compile-commands",
            str(compile_commands),
            "--source-root",
            str(ROOT),
            "--input",
            str(ROOT / "resource" / "sample.png"),
            "--warmup",
            "3",
            "--repeat",
            "10",
            "--gnuplot",
            self.gnuplot,
        ]
        command.extend(extra_arguments)
        return subprocess.run(
            command,
            check=False,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
        )

    def test_operation_report_has_gnuplot_chart_and_markdown_tables(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            directory = Path(temporary)
            csv_path = directory / "crop.csv"
            csv_path.write_text(
                "case,operation,backend,threads,width,height,pixels,"
                "avg_us,min_us,max_us,median_us,pixels_per_second\n"
                "0,center-half,generic,1,320,240,76800,120,110,140,118,650847457\n"
                "1,center-half,simd,1,320,240,76800,70,65,80,68,1129411764\n",
                encoding="utf-8",
            )
            metadata = self.write_metadata(directory)
            compile_commands = self.write_compile_commands(directory)
            output_dir = directory / "report"

            completed = self.run_report(
                "crop", csv_path, output_dir, metadata, compile_commands
            )

            self.assertEqual(0, completed.returncode, completed.stderr)
            report = (output_dir / "README.md").read_text(encoding="utf-8")
            chart = (output_dir / "latency.svg").read_text(encoding="utf-8")
            self.assertIn("Test Environment", report)
            self.assertRegex(report, r"\| (?:WSL logical|Logical) CPUs \|")
            self.assertIn("Build Configuration", report)
            self.assertIn("Auto-vectorization", report)
            self.assertIn("Median (us)", report)
            self.assertIn("center-half", report)
            self.assertIn("-fno-vectorize", report)
            self.assertIn("Produced by GNUPLOT", chart)
            self.assertIn('fill="#ffffff"', chart)
            self.assertIn('font-size="16.00"', chart)
            self.assertIn('font-size="24.00"', chart)

    def test_rotate_report_requires_ne10_bilinear_rows(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            directory = Path(temporary)
            csv_path = directory / "rotate.csv"
            csv_path.write_text(
                "case,operation,backend,threads,width,height,pixels,"
                "avg_us,min_us,max_us,median_us,pixels_per_second\n"
                "0,rotate-030,generic,1,2203,1896,4176888,5000,4900,"
                "5100,4950,843815757\n"
                "1,rotate-030,simd,1,2203,1896,4176888,4800,4700,"
                "4900,4750,879344842\n"
                "2,rotate-030,cairo-nearest,1,2203,1896,4176888,"
                "4600,4500,4700,4550,918000000\n"
                "3,rotate-030,ne10-c-bilinear,1,2203,1896,4176888,"
                "17000,16500,17500,16800,248624285\n",
                encoding="utf-8",
            )
            metadata = self.write_metadata(directory)
            compile_commands = self.write_compile_commands(directory)
            output_dir = directory / "rotate-report"

            completed = self.run_report(
                "rotate",
                csv_path,
                output_dir,
                metadata,
                compile_commands,
                ("--require-simd", "--require-cairo", "--require-ne10"),
            )

            self.assertEqual(0, completed.returncode, completed.stderr)
            report = (output_dir / "README.md").read_text(encoding="utf-8")
            self.assertIn("ne10-c-bilinear", report)
            self.assertIn("cairo-nearest", report)
            self.assertIn("upstream AArch64 has no rotate NEON", report)

    def test_convert_report_generates_complete_fourcc_matrix(self) -> None:
        rgb_formats = (
            "rgba", "argb", "bgra", "abgr", "rgb", "bgr",
            "rgba-planar", "argb-planar", "bgra-planar",
            "abgr-planar", "rgb-planar", "bgr-planar",
        )
        yuv_formats = (
            "yuv444-packed", "yuv444-planar", "yuyv", "yvyu",
            "uyvy", "vyuy", "yuv422-planar", "yuv420-planar",
            "nv12", "nv21",
        )
        formats = rgb_formats + yuv_formats
        with tempfile.TemporaryDirectory() as temporary:
            directory = Path(temporary)
            csv_path = directory / "convert.csv"
            with csv_path.open("w", newline="", encoding="utf-8") as stream:
                writer = csv.writer(stream)
                writer.writerow([
                    "case", "operation", "backend", "threads", "width",
                    "height", "pixels", "avg_us", "min_us", "max_us",
                    "median_us", "pixels_per_second",
                ])
                case_index = 0
                for source in formats:
                    for destination in formats:
                        writer.writerow([
                            case_index,
                            f"{source}-to-{destination}",
                            "generic",
                            1,
                            1280,
                            720,
                            921600,
                            110 + case_index,
                            100 + case_index,
                            120 + case_index,
                            108 + case_index,
                            1000000,
                        ])
                        case_index += 1
                for format_name in ("rgba", "argb", "bgra", "abgr"):
                    writer.writerow([
                        case_index,
                        f"{format_name}-to-{format_name}",
                        "cairo-copy",
                        1,
                        1280,
                        720,
                        921600,
                        150,
                        140,
                        170,
                        145,
                        6355862068,
                    ])
                    case_index += 1
            metadata = self.write_metadata(directory)
            compile_commands = self.write_compile_commands(directory)
            output_dir = directory / "convert-report"

            completed = self.run_report(
                "convert",
                csv_path,
                output_dir,
                metadata,
                compile_commands,
                ("--require-cairo",),
            )

            self.assertEqual(0, completed.returncode, completed.stderr)
            index = (output_dir / "README.md").read_text(encoding="utf-8")
            self.assertIn("484 FourCC", index)
            self.assertIn("Cairo 1.18.4", index)
            self.assertIn("No arbitrary FourCC/color-convert API", index)
            self.assertIn("rgba/README.md", index)
            self.assertIn("yuv444-planar/README.md", index)
            self.assertIn("Input and Raw Outputs", index)
            self.assertIn("build/<toolchain>/convert-output/", index)
            self.assertIn("outside the measured conversion interval", index)
            for source in formats:
                source_dir = output_dir / source
                report = (source_dir / "README.md").read_text(
                    encoding="utf-8"
                )
                self.assertIn(f"{source}-to-rgba", report)
                self.assertIn(f"{source}-to-nv21", report)
                for chart_name in ("rgb.svg", "yuv.svg"):
                    chart = (source_dir / chart_name).read_text(
                        encoding="utf-8"
                    )
                    self.assertIn("Produced by GNUPLOT", chart)

    def test_resize_report_generates_every_channel_chart_and_table(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            directory = Path(temporary)
            csv_path = directory / "resize.csv"
            with csv_path.open("w", newline="", encoding="utf-8") as stream:
                writer = csv.writer(stream)
                writer.writerow([
                    "case",
                    "channels",
                    "method",
                    "backend",
                    "threads",
                    "width",
                    "height",
                    "avg_us",
                    "min_us",
                    "max_us",
                    "median_us",
                ])
                case_index = 0
                for channel in range(1, 5):
                    for method in ("nearest", "bilinear", "bicubic"):
                        for threads in (1, 8):
                            writer.writerow([
                                case_index,
                                channel,
                                method,
                                "generic",
                                threads,
                                640,
                                480,
                                100 + case_index,
                                90 + case_index,
                                120 + case_index,
                                98 + case_index,
                            ])
                            case_index += 1
            metadata = self.write_metadata(directory)
            compile_commands = self.write_compile_commands(directory)
            output_dir = directory / "resize-report"

            completed = self.run_report(
                "resize", csv_path, output_dir, metadata, compile_commands
            )

            self.assertEqual(0, completed.returncode, completed.stderr)
            index = (output_dir / "README.md").read_text(encoding="utf-8")
            self.assertIn("build/<toolchain>/resize-output/", index)
            self.assertIn("Input and Measurement Matrix", index)
            self.assertIn("generic-lut", index)
            self.assertIn("Comparison Backends", index)
            for channel in range(1, 5):
                channel_dir = output_dir / f"{channel}ch"
                report = (channel_dir / "README.md").read_text(
                    encoding="utf-8"
                )
                self.assertIn("Results", report)
                self.assertIn("bicubic", report)
                for chart_name in (
                    "summary.svg",
                    "nearest.svg",
                    "bilinear.svg",
                    "bicubic.svg",
                ):
                    chart = (channel_dir / chart_name).read_text(
                        encoding="utf-8"
                    )
                    self.assertIn("Produced by GNUPLOT", chart)


if __name__ == "__main__":
    unittest.main(argv=[sys.argv[0]])
