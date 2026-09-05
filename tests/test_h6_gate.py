#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""Local evidence/driver tests. All SSH and serial commands are inert stubs."""

import argparse
import contextlib
import io
import json
import os
import shutil
import subprocess
import sys
import tarfile
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
import h5_runner as h5
import h6_gate as h6


TRACE = b"[unit] begin\nA=1\n[unit] PASS\n"


class H6GateTest(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.directory = Path(self.temporary.name)
        self.gate = self.directory / "gate"
        self.gate.mkdir()
        (self.gate / "image").mkdir()
        (self.gate / "image/gicv2-lab.elf").write_bytes(b"unit image")
        (self.gate / "image/gicv2-lab.bin").write_bytes(b"unit flat image")
        for name in ("tools/h6_gate.py", "tools/h5_runner.py", "scripts/h6-repeat.sh"):
            path = self.gate / "source" / name
            path.parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(ROOT / name, path)
        scenario = {"schema": h5.SCENARIO_SCHEMA, "id": "unit-v1",
                    "qemu": {"machine": "unit", "cpu": "unit"},
                    "success_marker": "[unit] PASS", "failure_substrings": [" FAIL:"],
                    "semantic_markers": ["[unit] begin", "[unit] PASS"],
                    "fields": {"exact": ["A"]}}
        h5.write_json(self.gate / "scenario.json", scenario)
        baseline = self.gate / "baseline-qemu"
        baseline.mkdir()
        (baseline / "serial.log").write_bytes(TRACE)
        run = {"schema": h5.RUN_SCHEMA, "status": "pass", "backend": "qemu-tcg",
               "image": h5.artifact_metadata(self.gate / "image/gicv2-lab.elf"),
               "serial": h5.artifact_metadata(baseline / "serial.log")}
        h5.write_json(baseline / "run.json", run)
        h5.write_json(baseline / "normalized.json", h5.normalize_trace(
            h5.load_scenario(self.gate / "scenario.json"), baseline / "serial.log", run))
        self.image_sha = run["image"]["sha256"]
        self.flat_sha = h5.sha256_file(self.gate / "image/gicv2-lab.bin")
        h5.write_json(self.gate / "gate.json", {
            "schema": h6.GATE_SCHEMA, "target_runs": 2,
            "image_sha256": self.image_sha, "flat_image_sha256": self.flat_sha,
            "artifacts": {str(p.relative_to(self.gate)): h5.sha256_file(p)
                          for p in h6.files_below(self.gate)}})
        self.stubdir = self.directory / "bin"
        self.stubdir.mkdir()
        for name in ("ssh", "stty", "lsof"):
            path = self.stubdir / name
            path.write_text('#!/bin/bash\nprintf "%s\\n" "$0 $*" >> "$CALL_LOG"\nexit 1\n')
            path.chmod(0o755)
        self.env = {**os.environ, "PYTHONDONTWRITEBYTECODE": "1",
                    "PATH": str(self.stubdir) + os.pathsep + os.environ["PATH"],
                    "PI_HOST": "unit@board.invalid", "PORT": "/dev/null",
                    "H6_LOCK_ROOT": str(self.directory / "locks"),
                    "CALL_LOG": str(self.directory / "calls.log"), "START_RUN": "1"}

    def start(self, number):
        h6.start(argparse.Namespace(gate=self.gate, run=number, runs=2,
                                   host=self.env["PI_HOST"], port=self.env["PORT"],
                                   remote_image="/boot/firmware/gicv2-lab-h6.bin",
                                   trace_timeout=120, linux_timeout=180))

    def finish(self, number, trace=TRACE, deployed=None):
        directory = self.gate / "runs/{:03d}".format(number)
        (directory / "serial.log").write_bytes(trace)
        (directory / "remote-image.sha256").write_text((deployed or self.flat_sha) + "  image.bin\n")
        (directory / "tryboot.txt").write_text("arm_64bit=1\nenable_uart=1\ndtoverlay=disable-bt\n"
                                               "init_uart_clock=3000000\nkernel=gicv2-lab-h6.bin\n")
        with contextlib.redirect_stdout(io.StringIO()):
            return h6.finish(argparse.Namespace(gate=self.gate, run=number,
                                                status="PASS", detail="unit fixture"))

    def driver(self, gate=None):
        return subprocess.run(["bash", str(ROOT / "scripts/h6-repeat.sh"), "2", str(gate or self.gate)],
                              env=self.env, capture_output=True, text=True, timeout=10)

    def test_resume_preserves_every_raw_trace_and_rejects_overwrite(self):
        self.start(1)
        self.assertEqual(self.finish(1), 0)
        with self.assertRaises(h5.H5Error):
            self.start(1)
        self.start(2)
        self.assertEqual(self.finish(2, TRACE.replace(b"A=1", b"A=0x0001")), 0)
        self.assertEqual(h6.verify(self.gate)["status"], "complete")
        self.assertEqual((self.gate / "runs/001/serial.log").read_bytes(), TRACE)
        self.assertNotEqual((self.gate / "runs/002/serial.log").read_bytes(), TRACE)

    def test_missing_or_changed_previous_raw_trace_prevents_resume(self):
        self.start(1)
        self.finish(1)
        (self.gate / "runs/001/serial.log").write_bytes(b"changed")
        with self.assertRaisesRegex(h5.H5Error, "changed frozen artifact"):
            self.start(2)

    def test_missing_records_and_fail_after_pass_are_rejected(self):
        for trace in (b"[unit] PASS\n", TRACE + b"[unit] FAIL: late error\n"):
            with self.subTest(trace=trace):
                self.start(1)
                self.assertEqual(self.finish(1, trace), 1)
                self.assertEqual(h6.verify(self.gate)["status"], "failed")
                with self.assertRaisesRegex(h5.H5Error, "cannot continue"):
                    self.start(2)
                shutil.rmtree(self.gate / "runs")

    def test_interrupted_attempt_cannot_be_skipped(self):
        self.start(1)
        self.assertEqual(h6.verify(self.gate)["status"], "incomplete")
        with self.assertRaisesRegex(h5.H5Error, "cannot continue"):
            self.start(2)

    def test_changed_deployed_image_is_not_accepted(self):
        self.start(1)
        self.assertEqual(self.finish(1, deployed="0" * 64), 1)
        self.assertEqual(h6.verify(self.gate)["status"], "failed")

    def test_boot_configuration_must_select_verified_image(self):
        directory = self.directory / "deployment"
        directory.mkdir()
        (directory / "remote-image.sha256").write_text(self.flat_sha + "  image.bin\n")
        (directory / "tryboot.txt").write_text("kernel=some-other-image.bin\n")
        with self.assertRaisesRegex(h5.H5Error, "does not select"):
            h6.validate_deployment(directory, self.flat_sha, "/boot/firmware/gicv2-lab-h6.bin")

    def test_changed_elf_is_rejected_before_any_external_command(self):
        (self.gate / "image/gicv2-lab.elf").write_bytes(b"another build")
        result = self.driver()
        self.assertNotEqual(result.returncode, 0, result.stdout)
        self.assertIn("changed frozen artifact", result.stderr)
        self.assertFalse(Path(self.env["CALL_LOG"]).exists())
        self.assertFalse((self.gate / ".gate.lock").exists())

    def test_resource_locks_cover_different_output_directories(self):
        other = self.directory / "other-gate"
        shutil.copytree(self.gate, other)
        lockroot = Path(self.env["H6_LOCK_ROOT"])
        lockroot.mkdir()
        for key in h6.resource_keys(self.env["PI_HOST"], self.env["PORT"]):
            with self.subTest(key=key):
                held = lockroot / key
                held.mkdir()
                result = self.driver(other)
                self.assertNotEqual(result.returncode, 0)
                self.assertIn("board or serial port is locked", result.stderr)
                self.assertTrue(held.exists())
                self.assertFalse(Path(self.env["CALL_LOG"]).exists())
                held.rmdir()

    def test_macos_serial_aliases_share_a_lock(self):
        self.assertEqual(h6.resource_keys("alice@BOARD", "/dev/tty.unit"),
                         h6.resource_keys("bob@board", "/dev/cu.unit"))

    def test_busy_serial_port_is_left_alone(self):
        (self.stubdir / "lsof").write_text('#!/bin/bash\necho 12345\nexit 0\n')
        result = self.driver()
        self.assertNotEqual(result.returncode, 0)
        self.assertFalse(Path(self.env["CALL_LOG"]).exists())
        saved = h5.load_json(self.gate / "runs/001/result.json")
        self.assertEqual(saved["status"], "FAIL")
        self.assertIn("serial port occupied", saved["detail"])

    def test_archive_is_self_contained_and_cannot_be_overwritten(self):
        self.start(1)
        self.finish(1)
        archive = self.directory / "bundle.tar.gz"
        args = argparse.Namespace(gate=self.gate, out=archive)
        with contextlib.redirect_stdout(io.StringIO()):
            h6.archive(args)
        with self.assertRaises(h5.H5Error):
            h6.archive(args)
        shutil.rmtree(self.gate)
        unpacked = self.directory / "unpacked"
        with tarfile.open(archive) as bundle:
            bundle.extractall(unpacked, filter="data")
        self.assertEqual(h6.verify(unpacked / "gate")["runs_passed"], 1)


if __name__ == "__main__":
    unittest.main()
