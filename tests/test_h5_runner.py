#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later

import copy
import json
import sys
import tempfile
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
import h5_runner  # noqa: E402


SCENARIO = {
    "schema": "gicv2-lab.scenario.v1",
    "id": "unit-v1",
    "qemu": {
        "machine": "raspi400",
        "cpu": "cortex-a72,has_el3=off",
    },
    "success_marker": "[unit] PASS",
    "failure_substrings": [" FAIL:"],
    "semantic_markers": ["[unit] begin", "[unit] PASS"],
    "fields": {
        "exact": ["A", "B"],
    },
    "field_rules": [
        {
            "key": "B",
            "mode": "allowed",
            "values": ["1", "2"],
        }
    ],
}


class H5RunnerTest(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.directory = Path(self.temporary.name)
        self.scenario_path = self.directory / "scenario.json"
        self.scenario_path.write_text(json.dumps(SCENARIO), encoding="utf-8")
        self.scenario = h5_runner.load_scenario(self.scenario_path)

    def tearDown(self):
        self.temporary.cleanup()

    def trace(self, name, text, image_sha="a" * 64):
        raw_path = self.directory / (name + ".log")
        raw_path.write_text(text, encoding="utf-8")
        run = {
            "backend": name,
            "image": {"sha256": image_sha},
        }
        return h5_runner.normalize_trace(self.scenario, raw_path, run)

    def test_numeric_width_is_normalized_and_allowed_values_are_independent(self):
        left = self.trace("left", "[unit] begin\nA=0x0001\nB=1\n[unit] PASS\n")
        right = self.trace("right", "[unit] begin\nA=1\nB=0x2\n[unit] PASS\n")

        self.assertEqual(left["records"][1]["value"], "0x1")
        comparison = h5_runner.compare_traces(self.scenario, left, right)
        self.assertTrue(comparison["matches"])

    def test_one_sided_forbidden_result_has_a_precise_reduced_prefix(self):
        left = self.trace("left", "[unit] begin\nA=1\nB=1\n[unit] PASS\n")
        right = self.trace("right", "[unit] begin\nA=1\nB=3\n[unit] PASS\n")
        comparison = h5_runner.compare_traces(self.scenario, left, right)

        self.assertFalse(comparison["matches"])
        self.assertEqual(comparison["differences"][0]["kind"],
                         "one-sided-forbidden")
        output = self.directory / "reduced"
        reduction = h5_runner.reduce_trace_prefix(self.scenario, left, right,
                                                  output)
        self.assertEqual(reduction["first_difference"]["index"], 2)
        left_prefix = json.loads((output / "left-prefix.json").read_text(
            encoding="utf-8"))
        right_prefix = json.loads((output / "right-prefix.json").read_text(
            encoding="utf-8"))
        self.assertEqual(len(left_prefix["records"]), 3)
        self.assertEqual(len(right_prefix["records"]), 3)

    def test_exact_field_difference_is_reported(self):
        left = self.trace("left", "[unit] begin\nA=1\nB=1\n[unit] PASS\n")
        right = self.trace("right", "[unit] begin\nA=2\nB=1\n[unit] PASS\n")
        comparison = h5_runner.compare_traces(self.scenario, left, right)

        self.assertFalse(comparison["matches"])
        self.assertEqual(comparison["differences"][0]["kind"],
                         "field-value-mismatch")
        self.assertEqual(comparison["differences"][0]["key"], "A")

    def test_image_hash_is_a_hard_same_payload_gate(self):
        left = self.trace("left", "[unit] begin\nA=1\nB=1\n[unit] PASS\n")
        right = copy.deepcopy(left)
        right["run"]["backend"] = "right"
        right["run"]["image"]["sha256"] = "b" * 64
        comparison = h5_runner.compare_traces(self.scenario, left, right)

        self.assertFalse(comparison["matches"])
        self.assertEqual(comparison["differences"][0]["kind"],
                         "provenance-mismatch")
        self.assertEqual(comparison["differences"][0]["key"], "image_sha256")

    def test_qemu_command_explicitly_pins_tcg(self):
        command = h5_runner.qemu_command(self.scenario, Path("/qemu"),
                                         Path("/image"), Path("/serial"))
        self.assertIn("-accel", command)
        self.assertEqual(command[command.index("-accel") + 1], "tcg")

    def test_artifact_metadata_hashes_existing_file(self):
        artifact = self.directory / "artifact"
        artifact.write_bytes(b"H5\n")
        metadata = h5_runner.artifact_metadata(artifact)
        self.assertEqual(metadata["sha256"], h5_runner.sha256_file(artifact))
        self.assertTrue(metadata["path"].endswith("artifact"))

    def test_failure_and_marker_order_are_rejected(self):
        failed = self.directory / "failed.log"
        failed.write_text("[unit] begin\n[unit] FAIL: bad\nA=1\nB=1\n",
                          encoding="utf-8")
        with self.assertRaises(h5_runner.H5Error):
            h5_runner.normalize_trace(self.scenario, failed,
                                      {"backend": "unit", "image": {"sha256": "a"}})

        reordered = self.directory / "reordered.log"
        reordered.write_text("[unit] PASS\nA=1\nB=1\n[unit] begin\n",
                             encoding="utf-8")
        with self.assertRaises(h5_runner.H5Error):
            h5_runner.normalize_trace(self.scenario, reordered,
                                      {"backend": "unit", "image": {"sha256": "a"}})


if __name__ == "__main__":
    unittest.main()
