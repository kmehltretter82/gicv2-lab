"""Audit preserved H8 captures and create explicitly synthetic negative controls.

Run from the repository root after downloading both complete archives.
"""
from collections import Counter
import argparse
import json
from pathlib import Path
import shutil
import sys
import tarfile
import tempfile

ROOT = Path.cwd()
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("--out", required=True, type=Path)
parser.add_argument("--qemu-only", action="store_true")
args = parser.parse_args()
OUT = args.out.resolve()
sys.path.insert(0, str(ROOT / "tools"))
import h5_runner as h5
import h6_gate as evidence
import h7_runner as h7

contracts = {
    "split-eoi": (7, 26, "GuestActive_after_eoir", 0x400, 0),
    "priority": (8, 29, "GuestIAR_masked", 1023, 42),
    "redelivery": (9, 35, "GuestPending_after_first_dir", 0x400, 0),
}
summary = {"workloads": {}, "all_raw_traces_renormalized": True,
           "all_frozen_artifacts_verified": True,
           "all_prerequisites_verified": None if args.qemu_only else True}
with tempfile.TemporaryDirectory(prefix="gicv2-h8-audit-") as temporary:
    extracted = Path(temporary)
    archives = ["h8-qemu-campaign.tar.xz"]
    if not args.qemu_only:
        archives.append("h8-kvm-evidence.tar.xz")
    for name in archives:
        with tarfile.open(OUT / name) as packed:
            packed.extractall(extracted, filter="data")
    for workload, (markers, fields, key, original, forbidden) in contracts.items():
        baseline = extracted / "h8" / workload
        bundle = baseline / "bundle"
        identity = h7.verify_bundle(bundle)
        scenario = h5.load_scenario(bundle / "scenario.json")
        reference = h5.load_trace(baseline / "qemu/run-001/normalized.json")
        result = {**identity, "markers_per_capture": markers, "fields_per_capture": fields,
                  "runs": [], "raw_trace_hash_counts": {}}
        backends = [("qemu", baseline / "qemu")]
        if not args.qemu_only:
            backends.append(("kvm", extracted / "h8-kvm" / workload))
        for backend, parent in backends:
            directories = sorted(parent.glob("run-*"))
            assert [p.name for p in directories] == [f"run-{i:03d}" for i in range(1, 21)]
            hashes = Counter()
            for directory in directories:
                run = h5.load_json(directory / "run.json")
                assert run["status"] == "pass"
                assert run["image"]["sha256"] == identity["image_sha256"]
                assert run["scenario_sha256"] == identity["scenario_sha256"]
                raw = directory / "serial.log"
                digest = h5.sha256_file(raw)
                assert run["serial"]["sha256"] == digest
                hashes[digest] += 1
                trace = h5.load_trace(directory / "normalized.json")
                assert trace["raw_trace"]["sha256"] == digest
                regenerated = h5.normalize_trace(scenario, raw, run)
                assert trace["records"] == regenerated["records"]
                assert Counter(row["kind"] for row in trace["records"]) == {"marker": markers, "field": fields}
                comparison = h5.compare_traces(scenario, reference, trace)
                assert comparison["matches"]
                if backend == "kvm":
                    assert run["backend"] == "kvm"
                    assert h7.verify_bundle(directory / "bundle") == identity
                    assert not list((directory / "bundle").rglob("*.pyc"))
                    assert (directory / "stderr.log").read_bytes() == b""
                    assert run["worker_exit_status"] == -15
                    assert h5.load_json(directory / "comparison-qemu.json") == comparison
                    prerequisite = h5.load_json(directory / "unit-tests/unit-tests.json")
                    assert prerequisite["host"] == run["host"]
                    assert prerequisite["schema"] == h7.UNIT_SCHEMA
                    assert prerequisite["status"] == "pass" and prerequisite["accel"] == "kvm"
                    assert prerequisite["test"] == h7.TEST_NAME
                    for name, expected in prerequisite["artifacts"].items():
                        evidence.pinned_file(directory / "unit-tests", name, expected)
                    assert h7.unit_result_passed((directory / "unit-tests/unit-tests.log").read_text())
                    result["host"] = run["host"]
                result["runs"].append({"backend": backend, "run": directory.name,
                                       "serial_sha256": digest, "serial_bytes": raw.stat().st_size,
                                       "matches_qemu": True})
            result["raw_trace_hash_counts"][backend] = dict(hashes)
        loose = OUT / workload
        loose.mkdir()
        for backend, parent in backends:
            directory = parent / "run-001"
            shutil.copy2(directory / "serial.log", loose / (backend + "-run-001.log"))
        source = loose / ("qemu-run-001.log" if args.qemu_only else "kvm-run-001.log")
        raw = source.read_bytes()
        old = f"{key}=0x{original:016x}".encode()
        new = f"{key}=0x{forbidden:016x}".encode()
        assert raw.count(old) == 1
        negative = loose / "negative-controls"
        negative.mkdir()
        altered = negative / "synthetic-value.log"
        altered.write_bytes(raw.replace(old, new))
        altered_trace = h5.import_trace(scenario, "synthetic-negative-control", bundle / "guest.elf",
                                        altered, negative / "value", "local mutation; not hardware")
        comparison = h5.compare_traces(scenario, reference, altered_trace)
        assert not comparison["matches"]
        assert comparison["differences"][0]["kind"] == "one-sided-forbidden"
        h5.write_json(negative / "value/comparison.json", comparison)
        shared = h5.compare_traces(scenario, altered_trace, altered_trace)
        assert not shared["matches"]
        assert shared["differences"][0]["kind"] == "forbidden-outcome"
        h5.write_json(negative / "value/shared-forbidden-comparison.json", shared)
        rejection = {}
        final_marker = scenario["success_marker"].encode()
        for label, data in (("truncated", raw[:raw.index(final_marker)]),
                            ("marker-order", raw.replace(scenario["semantic_markers"][1].encode(),
                                                         scenario["semantic_markers"][2].encode(), 1))):
            path = negative / ("synthetic-" + label + ".log")
            path.write_bytes(data)
            try:
                h5.import_trace(scenario, "synthetic-negative-control", bundle / "guest.elf",
                                path, negative / label, "local mutation; not hardware")
            except h5.H5Error as error:
                rejection[label] = str(error)
            else:
                raise AssertionError("invalid synthetic trace was accepted: " + label)
        h5.write_json(negative / "rejections.json", rejection)
        result.update({"qemu_captures": 20, "kvm_captures": 0 if args.qemu_only else 20,
                       "value_and_shared_forbidden_rejected": True,
                       "truncation_and_marker_order_rejected": True})
        summary["workloads"][workload] = result
h5.write_json(OUT / "validation.json", summary)
print(json.dumps({name: {key: value for key, value in result.items() if key != "runs"}
                  for name, result in summary["workloads"].items()}, indent=2))
