#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""Deterministic host-side trace capture, comparison, and reduction for H5.

The tool deliberately does not boot physical hardware.  ``capture-qemu`` is
the only execution backend in this first H5 slice.  ``import-trace`` accepts a
serial capture made by an explicitly approved future Pi 400 or KVM run, pins
the image and scenario hashes, and subjects it to the same normalizer.
"""

import argparse
import hashlib
import json
import re
import shutil
import subprocess
import sys
import time
from pathlib import Path


SCENARIO_SCHEMA = "gicv2-lab.scenario.v1"
TRACE_SCHEMA = "gicv2-lab.trace.v1"
RUN_SCHEMA = "gicv2-lab.run.v1"
COMPARISON_SCHEMA = "gicv2-lab.comparison.v1"
REPEAT_SCHEMA = "gicv2-lab.repeat.v1"
PREFIX_SCHEMA = "gicv2-lab.trace-prefix.v1"
REDUCTION_SCHEMA = "gicv2-lab.reduction.v1"
RUNNER_VERSION = "1"

KEY_VALUE_RE = re.compile(r"^([A-Za-z][A-Za-z0-9_]*)=(\S+)$")


class H5Error(RuntimeError):
    """A malformed scenario, trace, or requested operation."""


def sha256_file(path):
    digest = hashlib.sha256()
    with path.open("rb") as source:
        for block in iter(lambda: source.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def artifact_metadata(path):
    metadata = {"path": str(path.resolve())}
    if path.is_file():
        metadata["sha256"] = sha256_file(path)
    else:
        metadata["sha256"] = None
    return metadata


def load_json(path):
    try:
        with path.open("r", encoding="utf-8") as source:
            value = json.load(source)
    except (OSError, json.JSONDecodeError) as error:
        raise H5Error("cannot read JSON {}: {}".format(path, error)) from error
    if not isinstance(value, dict):
        raise H5Error("{} must contain a JSON object".format(path))
    return value


def write_json(path, value, replace=False):
    if path.exists() and not replace:
        raise H5Error("refusing to overwrite {}".format(path))
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w", encoding="utf-8") as destination:
        json.dump(value, destination, indent=2, sort_keys=True)
        destination.write("\n")


def require_string(value, location):
    if not isinstance(value, str) or not value:
        raise H5Error("{} must be a non-empty string".format(location))
    return value


def require_string_list(value, location):
    if not isinstance(value, list) or not value:
        raise H5Error("{} must be a non-empty list".format(location))
    result = []
    for index, item in enumerate(value):
        result.append(require_string(item, "{}[{}]".format(location, index)))
    if len(result) != len(set(result)):
        raise H5Error("{} must not contain duplicates".format(location))
    return result


def require_sha256(value, location):
    value = require_string(value, location)
    if not re.fullmatch(r"[0-9a-f]{64}", value):
        raise H5Error("{} must be a lowercase SHA-256 digest".format(location))
    return value


def canonical_value(value):
    """Canonicalize numeric UART values without preserving print width."""
    value = require_string(value, "numeric value")
    if re.fullmatch(r"0[xX][0-9a-fA-F]+", value):
        return "0x{:x}".format(int(value, 16))
    if re.fullmatch(r"[0-9]+", value):
        return "0x{:x}".format(int(value, 10))
    raise H5Error("semantic UART value is not an unsigned integer: {}".format(value))


def expand_field_group(fields, group_name):
    group = fields.get(group_name)
    if group is None:
        return []
    if not isinstance(group, dict):
        raise H5Error("fields.{} must be an object".format(group_name))
    prefixes = require_string_list(group.get("prefixes"),
                                   "fields.{}.prefixes".format(group_name))
    suffixes = require_string_list(group.get("suffixes"),
                                   "fields.{}.suffixes".format(group_name))
    return ["{}_{}".format(prefix, suffix)
            for suffix in suffixes for prefix in prefixes]


def load_scenario(path):
    document = load_json(path)
    if document.get("schema") != SCENARIO_SCHEMA:
        raise H5Error("{} is not a {} document".format(path, SCENARIO_SCHEMA))

    scenario_id = require_string(document.get("id"), "scenario id")
    success_marker = require_string(document.get("success_marker"),
                                    "success_marker")
    markers = require_string_list(document.get("semantic_markers"),
                                  "semantic_markers")
    if success_marker != markers[-1]:
        raise H5Error("success_marker must be the final semantic marker")

    failures = document.get("failure_substrings", [])
    if not isinstance(failures, list):
        raise H5Error("failure_substrings must be a list")
    failures = [require_string(item, "failure_substrings") for item in failures]

    qemu = document.get("qemu")
    if not isinstance(qemu, dict):
        raise H5Error("qemu must be an object")
    machine = require_string(qemu.get("machine"), "qemu.machine")
    cpu = require_string(qemu.get("cpu"), "qemu.cpu")

    fields = document.get("fields")
    if not isinstance(fields, dict):
        raise H5Error("fields must be an object")
    exact = require_string_list(fields.get("exact"), "fields.exact")
    expected_keys = exact[:]
    for group_name in ("initial", "snapshot", "context", "guest_state"):
        expected_keys.extend(expand_field_group(fields, group_name))
    if len(expected_keys) != len(set(expected_keys)):
        raise H5Error("expanded semantic field names must be unique")

    raw_rules = document.get("field_rules", [])
    if not isinstance(raw_rules, list):
        raise H5Error("field_rules must be a list")
    rules = {}
    for index, raw_rule in enumerate(raw_rules):
        if not isinstance(raw_rule, dict):
            raise H5Error("field_rules[{}] must be an object".format(index))
        key = require_string(raw_rule.get("key"),
                             "field_rules[{}].key".format(index))
        if key not in expected_keys:
            raise H5Error("field rule names non-semantic key {}".format(key))
        if key in rules:
            raise H5Error("duplicate field rule for {}".format(key))
        mode = require_string(raw_rule.get("mode"),
                              "field_rules[{}].mode".format(index))
        if mode == "equal":
            rules[key] = {"mode": "equal"}
        elif mode == "allowed":
            values = raw_rule.get("values")
            values = require_string_list(values,
                                         "field_rules[{}].values".format(index))
            rules[key] = {
                "mode": "allowed",
                "values": sorted({canonical_value(value) for value in values}),
            }
        else:
            raise H5Error("unsupported comparison mode {}".format(mode))

    return {
        "document": document,
        "id": scenario_id,
        "path": path.resolve(),
        "sha256": sha256_file(path),
        "machine": machine,
        "cpu": cpu,
        "success_marker": success_marker,
        "semantic_markers": markers,
        "failure_substrings": failures,
        "expected_keys": set(expected_keys),
        "rules": rules,
    }


def marker_record(sequence, marker):
    return {"seq": sequence, "kind": "marker", "name": marker}


def field_record(sequence, key, value):
    return {
        "seq": sequence,
        "kind": "field",
        "name": key,
        "value": value,
    }


def normalize_trace(scenario, raw_path, run):
    try:
        raw = raw_path.read_bytes()
    except OSError as error:
        raise H5Error("cannot read trace {}: {}".format(raw_path, error)) from error

    text = raw.decode("utf-8", errors="replace")
    markers = scenario["semantic_markers"]
    marker_index = 0
    seen_keys = set()
    records = []

    for line_number, raw_line in enumerate(text.splitlines(), 1):
        line = raw_line.strip()
        if not line:
            continue
        for substring in scenario["failure_substrings"]:
            if substring in line:
                raise H5Error("failure marker at {}:{}: {}".format(
                    raw_path, line_number, line))

        if marker_index < len(markers) and line == markers[marker_index]:
            records.append(marker_record(len(records), line))
            marker_index += 1
            continue
        if line in markers:
            expected = markers[marker_index] if marker_index < len(markers) \
                else "end of trace"
            raise H5Error("marker out of order at {}:{}: got {!r}, expected {!r}".format(
                raw_path, line_number, line, expected))

        match = KEY_VALUE_RE.fullmatch(line)
        if not match:
            continue
        key, raw_value = match.groups()
        if key not in scenario["expected_keys"]:
            continue
        if key in seen_keys:
            raise H5Error("duplicate semantic field at {}:{}: {}".format(
                raw_path, line_number, key))
        seen_keys.add(key)
        records.append(field_record(len(records), key, canonical_value(raw_value)))

    if marker_index != len(markers):
        raise H5Error("trace {} ended before semantic marker {!r}".format(
            raw_path, markers[marker_index]))
    missing_keys = sorted(scenario["expected_keys"] - seen_keys)
    if missing_keys:
        raise H5Error("trace {} is missing semantic fields: {}".format(
            raw_path, ", ".join(missing_keys)))

    return {
        "schema": TRACE_SCHEMA,
        "runner_version": RUNNER_VERSION,
        "scenario": {
            "id": scenario["id"],
            "sha256": scenario["sha256"],
        },
        "raw_trace": {
            "path": str(raw_path.resolve()),
            "sha256": hashlib.sha256(raw).hexdigest(),
        },
        "run": run,
        "records": records,
    }


def load_trace(path):
    trace = load_json(path)
    if trace.get("schema") != TRACE_SCHEMA:
        raise H5Error("{} is not a {} document".format(path, TRACE_SCHEMA))
    if not isinstance(trace.get("records"), list):
        raise H5Error("{} has no records list".format(path))
    for index, record in enumerate(trace["records"]):
        if not isinstance(record, dict) or record.get("seq") != index:
            raise H5Error("{} has an invalid record at index {}".format(path, index))
        kind = record.get("kind")
        if kind not in ("marker", "field"):
            raise H5Error("{} has an invalid record kind at index {}".format(
                path, index))
        require_string(record.get("name"), "trace record name")
        if kind == "field":
            canonical_value(record.get("value"))
    trace_identity(trace)
    return trace


def trace_identity(trace):
    scenario = trace.get("scenario")
    run = trace.get("run")
    if not isinstance(scenario, dict) or not isinstance(run, dict):
        raise H5Error("trace has incomplete scenario or run metadata")
    image = run.get("image")
    if not isinstance(image, dict):
        raise H5Error("trace has no image metadata")
    return {
        "scenario_id": require_string(scenario.get("id"), "trace scenario id"),
        "scenario_sha256": require_sha256(scenario.get("sha256"),
                                           "trace scenario SHA-256"),
        "image_sha256": require_sha256(image.get("sha256"),
                                         "trace image SHA-256"),
    }


def compare_records(scenario, left_records, right_records):
    differences = []
    common = min(len(left_records), len(right_records))
    for index in range(common):
        left = left_records[index]
        right = right_records[index]
        if left.get("kind") != right.get("kind") or \
                left.get("name") != right.get("name"):
            differences.append({
                "index": index,
                "kind": "record-order-mismatch",
                "left": left,
                "right": right,
            })
            break
        if left.get("kind") != "field":
            continue
        rule = scenario["rules"].get(left["name"], {"mode": "equal"})
        left_value = left.get("value")
        right_value = right.get("value")
        if rule["mode"] == "allowed":
            allowed = rule["values"]
            left_allowed = left_value in allowed
            right_allowed = right_value in allowed
            if not left_allowed or not right_allowed:
                kind = "one-sided-forbidden" if left_allowed != right_allowed \
                    else "forbidden-outcome"
                differences.append({
                    "index": index,
                    "kind": kind,
                    "key": left["name"],
                    "allowed": allowed,
                    "left": left,
                    "right": right,
                })
            continue
        if left_value != right_value:
            differences.append({
                "index": index,
                "kind": "field-value-mismatch",
                "key": left["name"],
                "left": left,
                "right": right,
            })

    if len(left_records) != len(right_records):
        differences.append({
            "index": common,
            "kind": "record-count-mismatch",
            "left": left_records[common] if common < len(left_records) else None,
            "right": right_records[common] if common < len(right_records) else None,
        })
    return differences


def compare_traces(scenario, left, right):
    left_identity = trace_identity(left)
    right_identity = trace_identity(right)
    provenance_differences = []
    for key in ("scenario_id", "scenario_sha256", "image_sha256"):
        if left_identity.get(key) != right_identity.get(key):
            provenance_differences.append({
                "kind": "provenance-mismatch",
                "key": key,
                "left": left_identity.get(key),
                "right": right_identity.get(key),
            })
    if left_identity["scenario_id"] != scenario["id"] or \
            left_identity["scenario_sha256"] != scenario["sha256"]:
        provenance_differences.append({
            "kind": "scenario-does-not-match-comparison-contract",
            "side": "left",
        })
    if right_identity["scenario_id"] != scenario["id"] or \
            right_identity["scenario_sha256"] != scenario["sha256"]:
        provenance_differences.append({
            "kind": "scenario-does-not-match-comparison-contract",
            "side": "right",
        })

    differences = provenance_differences
    if not differences:
        differences = compare_records(scenario, left["records"], right["records"])

    return {
        "schema": COMPARISON_SCHEMA,
        "runner_version": RUNNER_VERSION,
        "scenario": {
            "id": scenario["id"],
            "sha256": scenario["sha256"],
        },
        "left": {
            "backend": left["run"].get("backend"),
            "raw_trace_sha256": left["raw_trace"].get("sha256"),
            **left_identity,
        },
        "right": {
            "backend": right["run"].get("backend"),
            "raw_trace_sha256": right["raw_trace"].get("sha256"),
            **right_identity,
        },
        "matches": not differences,
        "differences": differences,
    }


def prepare_output_directory(path):
    if path.exists():
        if not path.is_dir():
            raise H5Error("output path is not a directory: {}".format(path))
        if any(path.iterdir()):
            raise H5Error("refusing to reuse non-empty output directory {}".format(path))
    else:
        path.mkdir(parents=True)


def qemu_version(path):
    try:
        completed = subprocess.run([str(path), "--version"], check=True,
                                   capture_output=True, text=True)
    except (OSError, subprocess.CalledProcessError) as error:
        raise H5Error("cannot query QEMU version {}: {}".format(path, error)) \
            from error
    return completed.stdout.strip()


def qemu_command(scenario, qemu, image, serial):
    return [
        str(qemu),
        "-machine", scenario["machine"],
        "-accel", "tcg",
        "-cpu", scenario["cpu"],
        "-kernel", str(image),
        "-display", "none",
        "-monitor", "none",
        "-serial", "file:{}".format(serial),
        "-no-reboot",
    ]


def capture_qemu(scenario, image, qemu, output, timeout, replay_of=None):
    image = image.resolve()
    qemu = qemu.resolve()
    output = output.resolve()
    if not image.is_file():
        raise H5Error("image does not exist: {}".format(image))
    if not qemu.is_file():
        raise H5Error("QEMU executable does not exist: {}".format(qemu))
    if timeout <= 0:
        raise H5Error("timeout must be positive")
    prepare_output_directory(output)
    serial = output / "serial.log"
    stderr = output / "stderr.log"
    command = qemu_command(scenario, qemu, image, serial)
    started = time.monotonic()
    process = None
    observed_pass = False

    with stderr.open("wb") as error_log:
        try:
            try:
                process = subprocess.Popen(command, stdin=subprocess.DEVNULL,
                                           stdout=subprocess.DEVNULL,
                                           stderr=error_log)
            except OSError as error:
                raise H5Error("cannot start QEMU {}: {}".format(qemu, error)) \
                    from error
            while time.monotonic() - started < timeout:
                if serial.exists():
                    text = serial.read_bytes().decode("utf-8", errors="replace")
                    if scenario["success_marker"] in text:
                        observed_pass = True
                        break
                if process.poll() is not None:
                    break
                time.sleep(0.05)
        finally:
            if process is not None and process.poll() is None:
                process.terminate()
                try:
                    process.wait(timeout=2)
                except subprocess.TimeoutExpired:
                    process.kill()
                    process.wait()

    run = {
        "schema": RUN_SCHEMA,
        "runner_version": RUNNER_VERSION,
        "backend": "qemu-tcg",
        "scenario_path": str(scenario["path"]),
        "scenario_sha256": scenario["sha256"],
        "image": {
            "path": str(image),
            "sha256": sha256_file(image),
        },
        "qemu": {
            "path": str(qemu),
            "sha256": sha256_file(qemu),
            "version": qemu_version(qemu),
        },
        "command": command,
        "serial": artifact_metadata(serial),
        "stderr": artifact_metadata(stderr),
        "timeout_seconds": timeout,
    }
    if replay_of is not None:
        run["replay_of"] = str(replay_of.resolve())
    if not observed_pass:
        run["status"] = "no-pass-marker"
        write_json(output / "run.json", run)
        raise H5Error("QEMU capture did not observe {!r}; raw output is in {}".format(
            scenario["success_marker"], output))
    run["status"] = "pass"
    try:
        normalized = normalize_trace(scenario, serial, run)
    except H5Error:
        run["status"] = "invalid-trace"
        write_json(output / "run.json", run)
        raise
    write_json(output / "run.json", run)
    write_json(output / "normalized.json", normalized)
    return normalized


def import_trace(scenario, backend, image, serial_source, output, backend_id):
    image = image.resolve()
    serial_source = serial_source.resolve()
    output = output.resolve()
    if not image.is_file():
        raise H5Error("image does not exist: {}".format(image))
    if not serial_source.is_file():
        raise H5Error("serial trace does not exist: {}".format(serial_source))
    backend = require_string(backend, "backend")
    backend_id = require_string(backend_id, "backend_id")
    prepare_output_directory(output)
    serial = output / "serial.log"
    shutil.copyfile(serial_source, serial)
    run = {
        "schema": RUN_SCHEMA,
        "runner_version": RUNNER_VERSION,
        "backend": backend,
        "backend_id": backend_id,
        "scenario_path": str(scenario["path"]),
        "scenario_sha256": scenario["sha256"],
        "image": {
            "path": str(image),
            "sha256": sha256_file(image),
        },
        "source_serial": {
            "path": str(serial_source),
            "sha256": sha256_file(serial_source),
        },
        "serial": artifact_metadata(serial),
    }
    run["status"] = "imported"
    try:
        normalized = normalize_trace(scenario, serial, run)
    except H5Error:
        run["status"] = "invalid-trace"
        write_json(output / "run.json", run)
        raise
    write_json(output / "run.json", run)
    write_json(output / "normalized.json", normalized)
    return normalized


def replay_qemu(run_path, qemu_override, output, timeout):
    run = load_json(run_path)
    if run.get("schema") != RUN_SCHEMA or run.get("backend") != "qemu-tcg":
        raise H5Error("{} is not a QEMU H5 run manifest".format(run_path))
    scenario_path = Path(require_string(run.get("scenario_path"),
                                        "run.scenario_path"))
    image = run.get("image")
    if not isinstance(image, dict):
        raise H5Error("run image metadata is missing")
    image_path = Path(require_string(image.get("path"), "run.image.path"))
    expected_image_sha = require_sha256(image.get("sha256"), "run.image.sha256")
    if not image_path.is_file():
        raise H5Error("replay image no longer exists: {}".format(image_path))
    if sha256_file(image_path) != expected_image_sha:
        raise H5Error("image no longer matches the replay manifest")
    if qemu_override is None:
        qemu = run.get("qemu")
        if not isinstance(qemu, dict):
            raise H5Error("run QEMU metadata is missing")
        qemu_path = Path(require_string(qemu.get("path"), "run.qemu.path"))
        expected_qemu_sha = require_sha256(qemu.get("sha256"),
                                           "run.qemu.sha256")
        if not qemu_path.is_file():
            raise H5Error("replay QEMU no longer exists: {}".format(qemu_path))
        if sha256_file(qemu_path) != expected_qemu_sha:
            raise H5Error("QEMU no longer matches the replay manifest")
    else:
        qemu_path = qemu_override
    scenario = load_scenario(scenario_path)
    expected_scenario_sha = require_string(run.get("scenario_sha256"),
                                           "run.scenario_sha256")
    if scenario["sha256"] != expected_scenario_sha:
        raise H5Error("scenario no longer matches the replay manifest")
    return capture_qemu(scenario, image_path, qemu_path, output, timeout,
                        replay_of=run_path)


def repeat_qemu(scenario, image, qemu, output, timeout, runs):
    if runs < 2:
        raise H5Error("repeat-qemu requires at least two runs")
    output = output.resolve()
    prepare_output_directory(output)
    baseline = None
    comparisons = []
    run_summaries = []
    for number in range(1, runs + 1):
        run_output = output / "run-{:03d}".format(number)
        normalized = capture_qemu(scenario, image, qemu, run_output, timeout)
        run_summaries.append({
            "run": number,
            "raw_trace_sha256": normalized["raw_trace"]["sha256"],
            "normalized_trace": str((run_output / "normalized.json").resolve()),
        })
        if baseline is None:
            baseline = normalized
            continue
        comparison = compare_traces(scenario, baseline, normalized)
        comparison["run"] = number
        comparison_path = output / "comparison-{:03d}.json".format(number)
        write_json(comparison_path, comparison)
        comparisons.append({
            "run": number,
            "matches": comparison["matches"],
            "path": str(comparison_path.resolve()),
        })
        if not comparison["matches"]:
            summary = {
                "schema": REPEAT_SCHEMA,
                "runner_version": RUNNER_VERSION,
                "backend": "qemu-tcg",
                "status": "semantic-mismatch",
                "runs_requested": runs,
                "runs_completed": number,
                "runs": run_summaries,
                "comparisons": comparisons,
            }
            write_json(output / "repeat.json", summary)
            raise H5Error("QEMU repeat run {} diverged from run 1; see {}".format(
                number, comparison_path))

    summary = {
        "schema": REPEAT_SCHEMA,
        "runner_version": RUNNER_VERSION,
        "backend": "qemu-tcg",
        "status": "all-semantic-traces-match",
        "runs_requested": runs,
        "runs_completed": runs,
        "runs": run_summaries,
        "comparisons": comparisons,
    }
    write_json(output / "repeat.json", summary)
    return summary


def reduce_trace_prefix(scenario, left, right, output):
    comparison = compare_traces(scenario, left, right)
    if comparison["matches"]:
        raise H5Error("cannot reduce matching traces")
    difference = comparison["differences"][0]
    if "index" not in difference:
        raise H5Error("cannot reduce a provenance mismatch")
    index = difference["index"]
    output = output.resolve()
    prepare_output_directory(output)
    left_prefix = dict(left)
    right_prefix = dict(right)
    left_prefix["schema"] = PREFIX_SCHEMA
    right_prefix["schema"] = PREFIX_SCHEMA
    left_prefix["records"] = left["records"][:index + 1]
    right_prefix["records"] = right["records"][:index + 1]
    write_json(output / "left-prefix.json", left_prefix)
    write_json(output / "right-prefix.json", right_prefix)
    reduction = {
        "schema": REDUCTION_SCHEMA,
        "runner_version": RUNNER_VERSION,
        "scenario": comparison["scenario"],
        "scope": "trace-prefix",
        "first_difference": difference,
        "left": {
            "original_records": len(left["records"]),
            "retained_records": len(left_prefix["records"]),
            "dropped_records": len(left["records"]) - len(left_prefix["records"]),
        },
        "right": {
            "original_records": len(right["records"]),
            "retained_records": len(right_prefix["records"]),
            "dropped_records": len(right["records"]) - len(right_prefix["records"]),
        },
        "limitation": (
            "This reduces the diagnostic trace to the shortest prefix containing "
            "the first semantic difference. It does not yet remove guest operations "
            "or claim that a shorter executable scenario reproduces the result."
        ),
    }
    write_json(output / "reduction.json", reduction)
    return reduction


def parse_args(argv):
    parser = argparse.ArgumentParser(description=__doc__)
    subparsers = parser.add_subparsers(dest="command", required=True)

    capture = subparsers.add_parser("capture-qemu",
                                    help="run the fixed scenario under QEMU TCG")
    capture.add_argument("--scenario", required=True, type=Path)
    capture.add_argument("--image", required=True, type=Path)
    capture.add_argument("--qemu", required=True, type=Path)
    capture.add_argument("--out", required=True, type=Path)
    capture.add_argument("--timeout", default=10.0, type=float)

    imported = subparsers.add_parser("import-trace",
                                     help="normalize a captured non-QEMU serial trace")
    imported.add_argument("--scenario", required=True, type=Path)
    imported.add_argument("--backend", required=True)
    imported.add_argument("--backend-id", default="unspecified")
    imported.add_argument("--image", required=True, type=Path)
    imported.add_argument("--serial", required=True, type=Path)
    imported.add_argument("--out", required=True, type=Path)

    replay = subparsers.add_parser("replay-qemu",
                                   help="replay a pinned successful QEMU run")
    replay.add_argument("--run", required=True, type=Path)
    replay.add_argument("--qemu", type=Path)
    replay.add_argument("--out", required=True, type=Path)
    replay.add_argument("--timeout", default=10.0, type=float)

    repeat = subparsers.add_parser("repeat-qemu",
                                   help="capture and compare fresh QEMU runs")
    repeat.add_argument("--scenario", required=True, type=Path)
    repeat.add_argument("--image", required=True, type=Path)
    repeat.add_argument("--qemu", required=True, type=Path)
    repeat.add_argument("--out", required=True, type=Path)
    repeat.add_argument("--runs", default=10, type=int)
    repeat.add_argument("--timeout", default=10.0, type=float)

    compare = subparsers.add_parser("compare",
                                    help="compare two normalized traces")
    compare.add_argument("--scenario", required=True, type=Path)
    compare.add_argument("--left", required=True, type=Path)
    compare.add_argument("--right", required=True, type=Path)
    compare.add_argument("--out", required=True, type=Path)

    reduce = subparsers.add_parser("reduce",
                                   help="retain the shortest divergent trace prefix")
    reduce.add_argument("--scenario", required=True, type=Path)
    reduce.add_argument("--left", required=True, type=Path)
    reduce.add_argument("--right", required=True, type=Path)
    reduce.add_argument("--out", required=True, type=Path)
    return parser.parse_args(argv)


def main(argv):
    args = parse_args(argv)
    try:
        if args.command == "capture-qemu":
            capture_qemu(load_scenario(args.scenario), args.image, args.qemu,
                         args.out, args.timeout)
            return 0
        if args.command == "import-trace":
            import_trace(load_scenario(args.scenario), args.backend, args.image,
                         args.serial, args.out, args.backend_id)
            return 0
        if args.command == "replay-qemu":
            replay_qemu(args.run, args.qemu, args.out, args.timeout)
            return 0
        if args.command == "repeat-qemu":
            repeat_qemu(load_scenario(args.scenario), args.image, args.qemu,
                        args.out, args.timeout, args.runs)
            return 0
        if args.command == "compare":
            scenario = load_scenario(args.scenario)
            comparison = compare_traces(scenario, load_trace(args.left),
                                        load_trace(args.right))
            write_json(args.out, comparison)
            return 0 if comparison["matches"] else 1
        if args.command == "reduce":
            reduce_trace_prefix(load_scenario(args.scenario),
                                load_trace(args.left), load_trace(args.right),
                                args.out)
            return 0
        raise H5Error("unknown command {}".format(args.command))
    except H5Error as error:
        print("gicv2-lab H5: {}".format(error), file=sys.stderr)
        return 2


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
