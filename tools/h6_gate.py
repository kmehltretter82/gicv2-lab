#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""Prepare and audit H6 evidence locally; never connect to a board.

Preparation builds a source snapshot and captures a fresh QEMU baseline. The
separate, opt-in shell driver supplies hardware traces to start/finish.
"""

import argparse
import hashlib
import json
import os
import platform
import shutil
import subprocess
import sys
import tarfile
from datetime import datetime, timezone
from pathlib import Path

import h5_runner as h5


ROOT = Path(__file__).resolve().parents[1]
GATE_SCHEMA = "gicv2-lab.hardware-gate.v1"
SOURCE_DIRS = ("arch", "docs", "h7", "include", "scenarios", "scripts", "src",
               "tests", "tools")
SOURCE_FILES = ("AGENTS.md", "LICENSE", "Makefile", "README.md", "ROADMAP.md",
                "linker.ld", ".gitignore")


def require(condition, message):
    if not condition:
        raise h5.H5Error(message)


def utc_now():
    return datetime.now(timezone.utc).isoformat()


def output(command, cwd=None):
    return subprocess.check_output(command, cwd=cwd, text=True).strip()


def llvm_tools(directory):
    tools = {}
    for name in ("clang", "llvm-objcopy", "llvm-objdump", "ld.lld"):
        if name == "ld.lld":
            resolved = output([tools["clang"]["path"], "--print-prog-name=ld.lld"])
            resolved = shutil.which(resolved) or resolved
        else:
            resolved = str(directory / name) if directory else shutil.which(name)
        require(resolved is not None, "cannot find {}".format(name))
        # Preserve the invocation basename: ld.lld can be a symlink to the
        # multi-call lld executable, whose behaviour depends on argv[0].
        path = Path(resolved).absolute()
        tools[name] = {**h5.artifact_metadata(path), "path": str(path),
                       "version": output([str(path), "--version"])}
    return tools


def resource_keys(host, port):
    # tty.* and cu.* are two names for the same macOS serial device.
    serial = str(Path(port).resolve())
    for prefix in ("/dev/tty.", "/dev/cu."):
        if serial.startswith(prefix):
            serial = "macos-serial:" + serial[len(prefix):]
            break
    board = host.rsplit("@", 1)[-1].lower()
    return [kind + "-" + hashlib.sha256(value.encode()).hexdigest()
            for kind, value in (("board", board), ("serial", serial))]


def files_below(directory):
    return sorted(p for p in directory.rglob("*") if p.is_file())


def source_snapshot(destination):
    destination.mkdir()
    for name in SOURCE_FILES:
        shutil.copyfile(ROOT / name, destination / name)
    for name in SOURCE_DIRS:
        shutil.copytree(ROOT / name, destination / name,
                        ignore=shutil.ignore_patterns("__pycache__", "*.pyc"))


def prepare(args):
    require(args.runs > 0, "runs must be positive")
    gate = args.out.resolve()
    require(not gate.exists(), "refusing to reuse output directory {}".format(gate))
    scenario_path = args.scenario.resolve()
    h5.load_scenario(scenario_path)
    qemu = args.qemu.resolve()
    require(qemu.is_file(), "QEMU executable does not exist")
    toolchain = llvm_tools(args.llvm_bin)
    gate.mkdir(parents=True)
    source_snapshot(gate / "source")
    shutil.copyfile(scenario_path, gate / "scenario.json")
    (gate / "source.diff").write_bytes(subprocess.check_output(
        ["git", "diff", "--binary", "HEAD"], cwd=ROOT))
    command = ["make", "BUILD=" + str(gate / "image")]
    for variable, name in (("CLANG", "clang"), ("OBJCOPY", "llvm-objcopy"),
                           ("OBJDUMP", "llvm-objdump")):
        command.append(variable + "=" + toolchain[name]["path"])
    command.append("all")
    environment = {
        "created_utc": utc_now(), "source_commit": output(["git", "rev-parse", "HEAD"], ROOT),
        "source_status": output(["git", "status", "--porcelain=v1", "--untracked-files=all"], ROOT),
        "source_note": "source/ contains the working files, including uncommitted changes",
        "host": platform.platform(), "python": sys.version,
        "make_version": output(["make", "--version"]),
        "toolchain": toolchain, "build_command": command,
        "build_cwd": str(gate / "source"),
        "prepare_command": [sys.executable, str(Path(__file__).resolve()), *sys.argv[1:]],
    }
    h5.write_json(gate / "environment.json", environment)
    build_env = os.environ.copy()
    # Do not inherit jobserver flags, -e, or command overrides from an outer make.
    for name in ("MAKEFLAGS", "MFLAGS", "MAKEOVERRIDES"):
        build_env.pop(name, None)
    with (gate / "build.log").open("xb") as log:
        subprocess.run(command, cwd=gate / "source", env=build_env,
                       stdout=log, stderr=subprocess.STDOUT, check=True)
    scenario = h5.load_scenario(gate / "scenario.json")
    image = gate / "image/gicv2-lab.elf"
    baseline = h5.capture_qemu(scenario, image, qemu, gate / "baseline-qemu", args.timeout)
    require(h5.compare_traces(scenario, baseline, baseline)["matches"],
            "QEMU baseline contains a forbidden outcome")
    inventory = {str(p.relative_to(gate)): h5.sha256_file(p) for p in files_below(gate)}
    manifest = {"schema": GATE_SCHEMA, "created_utc": utc_now(),
                "target_runs": args.runs, "artifacts": inventory,
                "image_sha256": h5.sha256_file(image),
                "flat_image_sha256": h5.sha256_file(gate / "image/gicv2-lab.bin")}
    h5.write_json(gate / "gate.json", manifest)
    print(json.dumps(verify(gate), indent=2))


def pinned_file(gate, name, digest):
    path = gate / name
    require(path.resolve().is_relative_to(gate.resolve()), "artifact escapes gate directory")
    require(path.is_file() and h5.sha256_file(path) == digest,
            "missing or changed frozen artifact: {}".format(name))
    return path


def frozen_baseline(gate):
    manifest = h5.load_json(gate / "gate.json")
    require(manifest.get("schema") == GATE_SCHEMA, "not a prepared H6 gate")
    require(isinstance(manifest.get("target_runs"), int) and manifest["target_runs"] > 0,
            "invalid gate target")
    for name, digest in manifest["artifacts"].items():
        pinned_file(gate, name, digest)
    scenario = h5.load_scenario(gate / "scenario.json")
    run = h5.load_json(gate / "baseline-qemu/run.json")
    require(run.get("status") == "pass" and run.get("backend") == "qemu-tcg",
            "baseline is not a passing QEMU run")
    require(h5.sha256_file(gate / "image/gicv2-lab.elf") ==
            run["image"]["sha256"] == manifest["image_sha256"], "baseline ELF mismatch")
    require(h5.sha256_file(gate / "image/gicv2-lab.bin") == manifest["flat_image_sha256"],
            "flat image mismatch")
    raw = gate / "baseline-qemu/serial.log"
    require(h5.sha256_file(raw) == run["serial"]["sha256"], "baseline serial mismatch")
    baseline = h5.normalize_trace(scenario, raw, run)
    saved = h5.load_trace(gate / "baseline-qemu/normalized.json")
    require(baseline["records"] == saved["records"] and
            h5.compare_traces(scenario, baseline, saved)["matches"],
            "baseline normalization mismatch")
    return manifest, scenario, baseline


def verify(gate):
    manifest, scenario, baseline = frozen_baseline(gate)
    completed = 0
    status = "ready"
    attempts = sorted((gate / "runs").glob("*"))
    for number, directory in enumerate(attempts, 1):
        require(directory.is_dir() and directory.name == "{:03d}".format(number),
                "run directories must be a consecutive prefix starting at 001")
        require(status in ("ready", "passing"), "attempts exist after an incomplete or failed run")
        attempt = h5.load_json(directory / "attempt.json")
        require(attempt["run"] == number, "attempt number mismatch")
        if not (directory / "result.json").is_file():
            status = "incomplete"
            continue
        result = h5.load_json(directory / "result.json")
        for name, digest in result["artifacts"].items():
            pinned_file(directory, name, digest)
        if result["status"] != "PASS":
            status = "failed"
            continue
        validate_deployment(directory, manifest["flat_image_sha256"], attempt["remote_image"])
        raw = directory / "serial.log"
        run = h5.load_json(directory / "h5/run.json")
        require(run["image"]["sha256"] == manifest["image_sha256"], "run ELF mismatch")
        require(h5.sha256_file(raw) == run["serial"]["sha256"], "run serial mismatch")
        trace = h5.normalize_trace(scenario, raw, run)
        saved = h5.load_trace(directory / "h5/normalized.json")
        comparison = h5.compare_traces(scenario, baseline, trace)
        require(trace["records"] == saved["records"] and
                h5.compare_traces(scenario, trace, saved)["matches"], "run normalization mismatch")
        require(comparison == h5.load_json(directory / "comparison.json") and comparison["matches"],
                "run comparison mismatch")
        completed += 1
        status = "passing"
    require(len(attempts) <= manifest["target_runs"], "more attempts than the gate target")
    if completed == manifest["target_runs"]:
        status = "complete"
    return {"status": status, "runs_passed": completed, "runs_attempted": len(attempts),
            "target_runs": manifest["target_runs"], "next_run": len(attempts) + 1}


def start(args):
    state = verify(args.gate)
    require(state["status"] in ("ready", "passing"), "gate cannot continue: " + state["status"])
    require(args.run == state["next_run"], "resume must start at run {}".format(state["next_run"]))
    require(args.runs == state["target_runs"], "requested total differs from frozen gate target")
    manifest = h5.load_json(args.gate / "gate.json")
    for name in ("tools/h6_gate.py", "tools/h5_runner.py", "scripts/h6-repeat.sh"):
        require(h5.sha256_file(ROOT / name) == manifest["artifacts"]["source/" + name],
                "driver changed; use the frozen source/scripts/h6-repeat.sh")
    identity = resource_keys(args.host, args.port)
    if args.run > 1:
        first = h5.load_json(args.gate / "runs/001/attempt.json")
        require(first["resource_keys"] == identity and first["remote_image"] == args.remote_image,
                "board, serial port, or deployed image path changed during gate")
    directory = args.gate / "runs/{:03d}".format(args.run)
    directory.mkdir(parents=True, exist_ok=False)
    h5.write_json(directory / "attempt.json", {
        "run": args.run, "started_utc": utc_now(), "host": args.host, "port": args.port,
        "resource_keys": identity, "remote_image": args.remote_image,
        "trace_timeout": args.trace_timeout, "linux_timeout": args.linux_timeout,
        "image_sha256": manifest["image_sha256"], "flat_image_sha256": manifest["flat_image_sha256"],
        "driver_command": ["bash", str(ROOT / "scripts/h6-repeat.sh"), str(args.runs), str(args.gate)],
        "start_run": os.environ.get("START_RUN", "1"),
    })


def validate_deployment(directory, flat_sha, remote_image):
    remote = (directory / "remote-image.sha256").read_text().split()
    require(remote and remote[0] == flat_sha, "deployed flat image mismatch")
    image_path = Path(remote_image)
    require(image_path.parent == Path("/boot/firmware"), "remote image must be in /boot/firmware")
    settings = {}
    for line in (directory / "tryboot.txt").read_text().splitlines():
        line = line.split("#", 1)[0].strip()
        if not line:
            continue
        require("=" in line, "unsupported tryboot directive")
        key, value = line.split("=", 1)
        require(key not in settings, "duplicate tryboot setting")
        settings[key] = value
    require(settings == {"arm_64bit": "1", "enable_uart": "1", "dtoverlay": "disable-bt",
                         "init_uart_clock": "3000000", "kernel": image_path.name},
            "tryboot configuration does not select the verified image and UART settings")


def finish(args):
    manifest, scenario, baseline = frozen_baseline(args.gate)
    directory = args.gate / "runs/{:03d}".format(args.run)
    attempt = h5.load_json(directory / "attempt.json")
    require(not (directory / "result.json").exists(), "refusing to replace a run result")
    status, detail = args.status, args.detail
    if status == "PASS":
        try:
            validate_deployment(directory, manifest["flat_image_sha256"], attempt["remote_image"])
            trace = h5.import_trace(scenario, "pi400", args.gate / "image/gicv2-lab.elf",
                                    directory / "serial.log", directory / "h5", attempt["host"])
            comparison = h5.compare_traces(scenario, baseline, trace)
            h5.write_json(directory / "comparison.json", comparison)
            require(comparison["matches"], "trace differs from frozen QEMU baseline")
        except (h5.H5Error, OSError) as error:
            status, detail = "FAIL", str(error)
    inventory = {str(p.relative_to(directory)): h5.sha256_file(p) for p in files_below(directory)}
    h5.write_json(directory / "result.json", {"run": args.run, "status": status, "detail": detail,
                                            "finished_utc": utc_now(), "artifacts": inventory})
    print("run {:03d}: {} - {}".format(args.run, status, detail))
    return 0 if status == "PASS" else 1


def archive(args):
    verify(args.gate)
    require(not args.out.exists(), "refusing to overwrite archive")
    require(not args.out.resolve().is_relative_to(args.gate.resolve()), "archive must be outside gate")
    args.out.parent.mkdir(parents=True, exist_ok=True)
    with args.out.open("xb") as destination:
        with tarfile.open(fileobj=destination, mode="w:gz") as bundle:
            bundle.add(args.gate, arcname="gate", filter=lambda item:
                       None if Path(item.name).name == ".gate.lock" else item)
    print(h5.sha256_file(args.out) + "  " + str(args.out))


def main(argv):
    parser = argparse.ArgumentParser(description=__doc__)
    commands = parser.add_subparsers(dest="command", required=True)
    prepared = commands.add_parser("prepare", help="freeze source, build, and capture QEMU locally")
    prepared.add_argument("--out", required=True, type=Path)
    prepared.add_argument("--qemu", required=True, type=Path)
    prepared.add_argument("--llvm-bin", type=Path)
    prepared.add_argument("--scenario", type=Path, default=ROOT / "scenarios/h4i-context-save-restore-v1.json")
    prepared.add_argument("--runs", type=int, default=100)
    prepared.add_argument("--timeout", type=float, default=10)
    keys = commands.add_parser("resource-keys", help="derive local board/serial lock names")
    keys.add_argument("host")
    keys.add_argument("port")
    for name in ("verify", "start", "finish", "archive", "check-deployment"):
        command = commands.add_parser(name)
        command.add_argument("--gate", required=True, type=Path)
        if name == "start":
            command.add_argument("--runs", required=True, type=int)
            command.add_argument("--host", required=True)
            command.add_argument("--port", required=True)
            command.add_argument("--remote-image", required=True)
            command.add_argument("--trace-timeout", required=True, type=int)
            command.add_argument("--linux-timeout", required=True, type=int)
        if name in ("start", "finish", "check-deployment"):
            command.add_argument("--run", required=True, type=int)
        if name == "finish":
            command.add_argument("--status", choices=("PASS", "FAIL", "TIMEOUT", "INTERRUPTED"), required=True)
            command.add_argument("--detail", required=True)
        if name == "archive":
            command.add_argument("--out", required=True, type=Path)
    args = parser.parse_args(argv)
    try:
        if args.command == "prepare":
            prepare(args)
        elif args.command == "resource-keys":
            print("\n".join(resource_keys(args.host, args.port)))
        elif args.command == "verify":
            print(json.dumps(verify(args.gate), indent=2))
        elif args.command == "start":
            start(args)
        elif args.command == "finish":
            return finish(args)
        elif args.command == "check-deployment":
            manifest = h5.load_json(args.gate / "gate.json")
            directory = args.gate / "runs/{:03d}".format(args.run)
            attempt = h5.load_json(directory / "attempt.json")
            validate_deployment(directory, manifest["flat_image_sha256"], attempt["remote_image"])
        elif args.command == "archive":
            archive(args)
        return 0
    except (h5.H5Error, OSError, subprocess.SubprocessError, KeyError, TypeError, ValueError) as error:
        print("gicv2-lab H6: {}".format(error), file=sys.stderr)
        return 2


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
