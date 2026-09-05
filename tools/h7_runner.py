#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""Build H7 locally, preserve existing unit-test evidence, and compare QEMU/KVM.

Only unit-tests --accel kvm and capture-kvm execute KVM. Both require Linux
arm64. Build, verify, and capture-qemu work without hardware access.
"""

import argparse
import json
import os
import platform
import re
import shutil
import signal
import subprocess
import sys
import time
from pathlib import Path

import h5_runner as h5
import h6_gate as evidence
import kvm_arm64 as kvm

ROOT = Path(__file__).resolve().parents[1]
BUILD_SCHEMA = "gicv2-lab.h7-build.v1"
UNIT_SCHEMA = "gicv2-lab.h7-unit-tests.v1"
TEST_NAME = "gicv2-mmio-up"


def host_identity():
    return dict(zip(("system", "node", "release", "version", "machine", "processor"), platform.uname()))


def stop_process(process):
    """Stop the process group created for this invocation, including children."""
    if process.poll() is None:
        try:
            os.killpg(process.pid, signal.SIGTERM)
            process.wait(timeout=2)
        except subprocess.TimeoutExpired:
            pass
        except ProcessLookupError:
            pass
        finally:
            try:
                os.killpg(process.pid, signal.SIGKILL)
            except ProcessLookupError:
                pass
            process.wait()


def build(args):
    bundle = args.out.resolve()
    evidence.require(not bundle.exists(), "refusing to reuse build bundle")
    tools = evidence.llvm_tools(args.llvm_bin)
    bundle.mkdir(parents=True)
    evidence.source_snapshot(bundle / "source")
    shutil.copyfile(bundle / "source/scenarios/h7-spi-lifecycle-v1.json", bundle / "scenario.json")
    (bundle / "source.diff").write_bytes(subprocess.check_output(["git", "diff", "--binary", "HEAD"], cwd=ROOT))
    command = [tools["clang"]["path"], "--target=aarch64-none-elf", "-mcpu=cortex-a72",
               "-ffreestanding", "-fno-builtin", "-fno-pic", "-fno-stack-protector",
               "-mgeneral-regs-only", "-ffunction-sections", "-fdata-sections",
               "-Wall", "-Wextra", "-Werror", "-O2", "-g", "-Iinclude",
               "-nostdlib", "--ld-path=" + tools["ld.lld"]["path"],
               "-Wl,-T,h7/linker.ld", "-Wl,--build-id=none",
               "-Wl,--gc-sections", "-Wl,-Map," + str(bundle / "guest.map"),
               "h7/start.S", "h7/guest.c", "src/print.c", "-o", str(bundle / "guest.elf")]
    commands = [command,
                [tools["llvm-objcopy"]["path"], "-O", "binary", str(bundle / "guest.elf"), str(bundle / "guest.bin")],
                [tools["llvm-objdump"]["path"], "-d", str(bundle / "guest.elf")]]
    with (bundle / "build.log").open("xb") as log:
        for command in commands[:2]:
            subprocess.run(command, cwd=bundle / "source", stdout=log, stderr=subprocess.STDOUT, check=True)
        with (bundle / "guest.dis").open("xb") as disassembly:
            subprocess.run(commands[2], stdout=disassembly, stderr=log, check=True)
    kvm.load_elf(bundle / "guest.elf", bytearray(kvm.RAM_SIZE))
    h5.write_json(bundle / "build.json", {
        "schema": BUILD_SCHEMA, "created_utc": evidence.utc_now(), "host": host_identity(),
        "source_commit": evidence.output(["git", "rev-parse", "HEAD"], ROOT),
        "source_status": evidence.output(["git", "status", "--porcelain=v1", "--untracked-files=all"], ROOT),
        "source_note": "source/ preserves working files, including uncommitted changes",
        "toolchain": tools, "commands": commands, "build_cwd": str(bundle / "source"),
        "artifacts": {str(p.relative_to(bundle)): h5.sha256_file(p) for p in evidence.files_below(bundle)}})
    print(json.dumps(verify_bundle(bundle), indent=2))


def verify_bundle(bundle):
    manifest = h5.load_json(bundle / "build.json")
    evidence.require(manifest.get("schema") == BUILD_SCHEMA, "not an H7 build bundle")
    for name, digest in manifest["artifacts"].items():
        evidence.pinned_file(bundle, name, digest)
    scenario = h5.load_scenario(bundle / "scenario.json")
    evidence.require(scenario["id"] == "h7-spi-lifecycle-v1", "unsupported H7 scenario")
    entry = kvm.load_elf(bundle / "guest.elf", bytearray(kvm.RAM_SIZE))
    return {"image_sha256": h5.sha256_file(bundle / "guest.elf"),
            "scenario_sha256": scenario["sha256"], "entry": hex(entry)}


def copy_bundle(bundle, out):
    verify_bundle(bundle)
    evidence.require(not out.exists(), "refusing to overwrite result directory")
    out.mkdir(parents=True)
    shutil.copytree(bundle, out / "bundle")
    verify_bundle(out / "bundle")
    return out / "bundle"


def capture_qemu(args):
    evidence.require(args.runs >= 1, "runs must be positive")
    bundle = copy_bundle(args.bundle.resolve(), args.out.resolve())
    scenario = h5.load_scenario(bundle / "scenario.json")
    if args.runs == 1:
        trace = h5.capture_qemu(scenario, bundle / "guest.elf", args.qemu,
                                args.out / "qemu", args.timeout)
    else:
        h5.repeat_qemu(scenario, bundle / "guest.elf", args.qemu,
                       args.out / "qemu", args.timeout, args.runs)
        trace = h5.load_trace(args.out / "qemu/run-001/normalized.json")
    evidence.require(h5.compare_traces(scenario, trace, trace)["matches"],
                     "QEMU trace violates the architectural contract")


def unit_result_passed(text):
    text = re.sub(r"\x1b\[[0-9;]*m", "", text)
    return bool(re.search(r"(?m)^PASS\s+" + TEST_NAME + r"(?:\s|$)", text)) and not \
        re.search(r"(?m)^(?:FAIL|SKIP)\s+" + TEST_NAME + r"(?:\s|$)", text)


def unit_tests(args):
    if args.accel == "kvm":
        kvm.require_host()
    checkout, out = args.checkout.resolve(), args.out.resolve()
    evidence.require(not out.exists(), "refusing to overwrite unit-test result")
    evidence.require(not evidence.output(["git", "diff", "HEAD", "--name-only"], checkout),
                     "unit-test source must match its recorded commit")
    for name in ("arm/gic.flat", "config.mak", "arm/unittests.cfg", "run_tests.sh"):
        evidence.require((checkout / name).is_file(), "build kvm-unit-tests for arm64 first: " + name)
    out.mkdir(parents=True)
    for name in ("arm/gic.flat", "config.mak", "arm/unittests.cfg"):
        shutil.copyfile(checkout / name, out / Path(name).name)
    (out / "source.tar").write_bytes(subprocess.check_output(["git", "archive", "HEAD"], cwd=checkout))
    command = [str(checkout / "run_tests.sh"), "-v", TEST_NAME]
    env = {**os.environ, "ACCEL": args.accel, "QEMU": str(args.qemu.resolve()),
           "MACHINE": "virt", "TIMEOUT": "30s", "unittest_log_dir": str(out / "logs")}
    (out / "logs").mkdir()
    result = {"schema": UNIT_SCHEMA, "host": host_identity(), "accel": args.accel,
              "test": TEST_NAME, "source_commit": evidence.output(["git", "rev-parse", "HEAD"], checkout),
              "compiler_version": evidence.output([str(args.compiler), "--version"]),
              "qemu": {**h5.artifact_metadata(args.qemu.resolve()), "version": h5.qemu_version(args.qemu)},
              "command": command, "cwd": str(checkout),
              "environment": {key: env[key] for key in ("ACCEL", "QEMU", "MACHINE", "TIMEOUT", "unittest_log_dir")}}
    status = 1
    with (out / "unit-tests.log").open("xb") as log:
        process = subprocess.Popen(command, cwd=checkout, env=env, stdout=log,
                                   stderr=subprocess.STDOUT, start_new_session=True)
        try:
            status = process.wait(timeout=120)
        except subprocess.TimeoutExpired:
            result["reason"] = "unit-test wrapper timed out"
        except KeyboardInterrupt:
            result["reason"] = "unit-test wrapper interrupted"
        finally:
            stop_process(process)
    text = (out / "unit-tests.log").read_text(errors="replace")
    result["status"] = "pass" if status == 0 and unit_result_passed(text) else "not-passed"
    result["exit_status"] = status
    result["artifacts"] = {str(p.relative_to(out)): h5.sha256_file(p) for p in evidence.files_below(out)}
    h5.write_json(out / "unit-tests.json", result)
    evidence.require(result["status"] == "pass", "existing GICv2 unit test did not pass; see " + str(out))


def require_prerequisite(directory):
    result = h5.load_json(directory / "unit-tests.json")
    evidence.require(result.get("schema") == UNIT_SCHEMA and result.get("status") == "pass"
                     and result.get("accel") == "kvm" and result.get("test") == TEST_NAME,
                     "a passing KVM gicv2-mmio-up result is required")
    evidence.require(result["host"] == host_identity(), "unit-test evidence is from a different host/kernel")
    for name, digest in result["artifacts"].items():
        evidence.pinned_file(directory, name, digest)
    evidence.require(unit_result_passed((directory / "unit-tests.log").read_text(errors="replace")),
                     "unit-test log has no passing prerequisite")


def worker(args):
    kvm.require_host()
    verify_bundle(args.bundle)
    with kvm.VM() as vm:
        vm.execute(args.bundle / "guest.elf", sys.stdout.buffer)


def capture_kvm(args):
    kvm.require_host()
    require_prerequisite(args.unit_tests)
    evidence.require(args.timeout > 0, "timeout must be positive")
    bundle = copy_bundle(args.bundle.resolve(), args.out.resolve())
    shutil.copytree(args.unit_tests, args.out / "unit-tests")
    scenario = h5.load_scenario(bundle / "scenario.json")
    # Execute the runner preserved with the image, so later worktree changes
    # cannot silently change the tested host implementation.
    command = [sys.executable, str(bundle / "source/tools/h7_runner.py"),
               "worker", "--bundle", str(bundle)]
    serial, stderr = args.out / "serial.log", args.out / "stderr.log"
    observed = False
    interrupted = False
    run = {"schema": h5.RUN_SCHEMA, "runner_version": h5.RUNNER_VERSION, "backend": "kvm",
           "backend_id": platform.node(), "host": host_identity(), "command": command,
           "image": h5.artifact_metadata(bundle / "guest.elf"),
           "scenario_path": str(scenario["path"]), "scenario_sha256": scenario["sha256"],
           "timeout_seconds": args.timeout, "status": "starting"}
    h5.write_json(args.out / "run.json", run)
    with serial.open("xb") as log, stderr.open("xb") as errors:
        process = subprocess.Popen(command, stdin=subprocess.DEVNULL, stdout=log, stderr=errors,
                                   env={**os.environ, "PYTHONDONTWRITEBYTECODE": "1"},
                                   start_new_session=True)
        try:
            deadline = time.monotonic() + args.timeout
            while time.monotonic() < deadline:
                text = serial.read_bytes().decode("utf-8", errors="replace")
                if any(failure in text for failure in scenario["failure_substrings"]):
                    break
                if scenario["success_marker"] in text:
                    observed = True
                    break
                if process.poll() is not None:
                    break
                time.sleep(0.05)
        except KeyboardInterrupt:
            interrupted = True
        finally:
            stop_process(process)
    run.update({"serial": h5.artifact_metadata(serial), "stderr": h5.artifact_metadata(stderr),
                "worker_exit_status": process.returncode,
                "status": "pass" if observed else "no-pass-marker"})
    if interrupted:
        run["status"] = "interrupted"
    if process.returncode == 77:
        run["status"] = "unsupported"
    try:
        evidence.require(observed and not interrupted, "KVM did not produce a complete passing trace")
        trace = h5.normalize_trace(scenario, serial, run)
        evidence.require(h5.compare_traces(scenario, trace, trace)["matches"], "KVM trace violates the contract")
    except h5.H5Error:
        if observed and not interrupted:
            run["status"] = "invalid-trace"
        h5.write_json(args.out / "run.json", run, replace=True)
        raise
    h5.write_json(args.out / "run.json", run, replace=True)
    h5.write_json(args.out / "normalized.json", trace)


def main(argv):
    parser = argparse.ArgumentParser(description=__doc__)
    commands = parser.add_subparsers(dest="command", required=True)
    build_parser = commands.add_parser("build")
    build_parser.add_argument("--out", required=True, type=Path)
    build_parser.add_argument("--llvm-bin", type=Path)
    commands.add_parser("check-host", help="check local OS and /dev/kvm presence without opening it")
    unit = commands.add_parser("unit-tests")
    unit.add_argument("--checkout", required=True, type=Path)
    unit.add_argument("--qemu", required=True, type=Path)
    unit.add_argument("--compiler", required=True, type=Path)
    unit.add_argument("--out", required=True, type=Path)
    unit.add_argument("--accel", choices=("kvm", "tcg"), default="kvm")
    for name in ("verify", "worker", "capture-qemu", "capture-kvm"):
        command = commands.add_parser(name)
        command.add_argument("--bundle", required=True, type=Path)
        if name.startswith("capture-"):
            command.add_argument("--out", required=True, type=Path)
            command.add_argument("--timeout", type=float, default=10)
        if name == "capture-qemu":
            command.add_argument("--qemu", required=True, type=Path)
            command.add_argument("--runs", type=int, default=20)
        if name == "capture-kvm":
            command.add_argument("--unit-tests", required=True, type=Path)
    args = parser.parse_args(argv)
    try:
        if args.command == "build":
            build(args)
        elif args.command == "verify":
            print(json.dumps(verify_bundle(args.bundle), indent=2))
        elif args.command == "check-host":
            kvm.require_host()
            print("Linux arm64 and /dev/kvm are present; VGICv2 support is checked when creating the VM")
        elif args.command == "unit-tests":
            unit_tests(args)
        elif args.command == "capture-qemu":
            capture_qemu(args)
        elif args.command == "capture-kvm":
            capture_kvm(args)
        elif args.command == "worker":
            worker(args)
        return 0
    except kvm.Unsupported as error:
        print("gicv2-lab H7: SKIP: {}".format(error), file=sys.stderr)
        return 77
    except (h5.H5Error, OSError, subprocess.SubprocessError, ValueError, RuntimeError, KeyError) as error:
        print("gicv2-lab H7: {}".format(error), file=sys.stderr)
        return 2


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
