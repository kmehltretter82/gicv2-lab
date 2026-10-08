#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""Exercise the H7 host boundary with fake ioctls and synthetic guest images."""
import argparse
import ctypes
import io
import json
import os
import struct
import sys
import tempfile
import unittest
from pathlib import Path
from unittest import mock

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
import h5_runner as h5
import h7_runner as h7
import kvm_arm64 as kvm


def elf_image(physical=kvm.RAM_BASE, entry=kvm.RAM_BASE, filesz=4, memsz=8):
    header = struct.pack("<16sHHIQQQIHHHHHH", b"\x7fELF\x02\x01\x01" + b"\0" * 9,
                         2, 183, 1, entry, 64, 0, 0, 64, 56, 1, 0, 0, 0)
    program = struct.pack("<IIQQQQQQ", 1, 5, 120, physical, physical, filesz, memsz, 4)
    return header + program + b"\x1f\x20\x03\xd5"  # one NOP; never executed by these tests


class Memory(bytearray):
    def __enter__(self):
        return self

    def __exit__(self, *error):
        pass


class H7Test(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.directory = Path(self.temporary.name)
        self.image = self.directory / "guest.elf"
        self.image.write_bytes(elf_image())

    def test_elf_load_and_zero_fill(self):
        memory = bytearray(b"\xff" * kvm.RAM_SIZE)
        self.assertEqual(kvm.load_elf(self.image, memory), kvm.RAM_BASE)
        self.assertEqual(memory[:8], b"\x1f\x20\x03\xd5\0\0\0\0")

    def test_elf_rejects_wrong_architecture_truncation_and_outside_ram(self):
        wrong_arch = bytearray(elf_image())
        struct.pack_into("<H", wrong_arch, 18, 62)
        for image in (b"ELF", elf_image()[:100], wrong_arch,
                      elf_image(physical=kvm.GICD), elf_image(entry=kvm.GICD),
                      elf_image(filesz=16), elf_image(memsz=kvm.RAM_SIZE)):
            with self.subTest(image=image[:20]):
                self.image.write_bytes(image)
                with self.assertRaises(ValueError):
                    kvm.load_elf(self.image, bytearray(kvm.RAM_SIZE))

    def mmio(self, address, data=0, length=4, write=1, reason=6):
        run = bytearray(256)
        struct.pack_into("<I", run, 8, reason)
        struct.pack_into("<QQIB", run, 32, address, data, length, write)
        return run

    def test_console_handles_only_guest_uart_accesses(self):
        stream = io.BytesIO()
        kvm.service_console(self.mmio(kvm.UART, ord("A")), stream)
        read = self.mmio(kvm.UART + 0x18, 0xffff, write=0)
        kvm.service_console(read, stream)
        self.assertEqual(stream.getvalue(), b"A")
        self.assertEqual(read[40:48], b"\0" * 8)
        for run in (self.mmio(kvm.GICC), self.mmio(kvm.UART, 255),
                    self.mmio(kvm.UART, length=8), self.mmio(kvm.UART, reason=9)):
            with self.assertRaises(RuntimeError):
                kvm.service_console(run, stream)

    def test_non_linux_capture_returns_skip_before_opening_any_device(self):
        with mock.patch.object(kvm.platform, "system", return_value="Darwin"), \
                mock.patch.object(kvm.os, "open") as opened, \
                mock.patch("sys.stderr", new_callable=io.StringIO):
            result = h7.main(["capture-kvm", "--bundle", str(self.directory),
                              "--out", str(self.directory / "capture"),
                              "--unit-tests", str(self.directory / "prerequisite")])
        self.assertEqual(result, 77)
        opened.assert_not_called()
        self.assertFalse((self.directory / "capture").exists())

    def test_kvm_setup_initializes_vcpu_before_vgic_and_negotiates_iidr(self):
        calls, registers = [], {}

        def ioctl(fd, operation, argument=0, *extra):
            calls.append((fd, operation, argument[:] if isinstance(argument, bytearray) else argument))
            if operation == kvm.GET_API_VERSION:
                return 12
            if operation == kvm.CHECK_EXTENSION:
                return 1
            if operation == kvm.CREATE_VM:
                return 11
            if operation == kvm.CREATE_VCPU:
                return 12
            if operation == kvm.ARM_PREFERRED_TARGET:
                struct.pack_into("<I", argument, 0, 5)
            if operation == kvm.CREATE_DEVICE and struct.unpack_from("<I", argument, 8)[0] == 0:
                struct.pack_into("<I", argument, 4, 13)
            if operation == kvm.GET_DEVICE_ATTR:
                _, group, attr, pointer = struct.unpack("<IIQQ", argument)
                self.assertEqual((group, attr), (1, 8))
                ctypes.c_uint32.from_address(pointer).value = 0x43b
            if operation == kvm.SET_DEVICE_ATTR:
                _, group, attr, pointer = struct.unpack("<IIQQ", argument)
                if group == 1:
                    self.assertEqual(attr, 8)
                    self.assertEqual(ctypes.c_uint32.from_address(pointer).value, 0x43b)
            if operation == kvm.GET_VCPU_MMAP_SIZE:
                return 4096
            if operation == kvm.SET_ONE_REG:
                identifier, pointer = struct.unpack("<QQ", argument)
                registers[identifier] = ctypes.c_uint64.from_address(pointer).value
            if operation == kvm.RUN:
                raise RuntimeError("end of fake execution")
            return 0

        with mock.patch.object(kvm, "require_host"), \
                mock.patch.object(kvm.os, "open", return_value=10), \
                mock.patch.object(kvm.os, "close") as closed, \
                mock.patch.object(kvm.fcntl, "ioctl", side_effect=ioctl), \
                mock.patch.object(kvm.mmap, "mmap", side_effect=lambda fd, size: Memory(size)):
            with kvm.VM() as vm:
                with self.assertRaisesRegex(RuntimeError, "end of fake execution"):
                    vm.execute(self.image, io.BytesIO())
        operations = [item[1] for item in calls]
        self.assertLess(operations.index(kvm.ARM_VCPU_INIT), operations.index(kvm.CREATE_DEVICE))
        self.assertEqual(registers, {kvm.REG_PC: kvm.RAM_BASE, kvm.REG_PSTATE: 0x3c5,
                                     kvm.REG_SP_EL1: kvm.RAM_BASE + kvm.RAM_SIZE})
        self.assertEqual([call.args[0] for call in closed.call_args_list], [13, 12, 11, 10])

    def test_partial_kvm_setup_closes_owned_descriptors(self):
        def ioctl(fd, operation, argument=0):
            if operation == kvm.GET_API_VERSION:
                return 12
            if operation == kvm.CHECK_EXTENSION:
                return 0
            raise AssertionError("unexpected ioctl")

        with mock.patch.object(kvm, "require_host"), \
                mock.patch.object(kvm.os, "open", return_value=10), \
                mock.patch.object(kvm.os, "close") as closed, \
                mock.patch.object(kvm.fcntl, "ioctl", side_effect=ioctl):
            with self.assertRaises(kvm.Unsupported):
                with kvm.VM():
                    self.fail("unsupported VM entered")
        closed.assert_called_once_with(10)

    def test_existing_unit_test_skip_is_not_a_pass(self):
        self.assertTrue(h7.unit_result_passed("PASS gicv2-mmio-up (10 tests)\n"))
        for text in ("", "SKIP gicv2-mmio-up\n", "PASS another-test\n",
                     "PASS gicv2-mmio-up\nFAIL gicv2-mmio-up\n"):
            self.assertFalse(h7.unit_result_passed(text))

    def test_prerequisite_requires_real_kvm_on_same_kernel(self):
        log = self.directory / "unit-tests.log"
        log.write_text("PASS gicv2-mmio-up (10 tests)\n")
        result = {"schema": h7.UNIT_SCHEMA, "status": "pass", "accel": "tcg",
                  "test": h7.TEST_NAME, "host": h7.host_identity(),
                  "artifacts": {"unit-tests.log": h5.sha256_file(log)}}
        manifest = self.directory / "unit-tests.json"
        h5.write_json(manifest, result)
        with self.assertRaisesRegex(h5.H5Error, "passing KVM"):
            h7.require_prerequisite(self.directory)
        result["accel"] = "kvm"
        h5.write_json(manifest, result, replace=True)
        h7.require_prerequisite(self.directory)
        result["host"]["release"] = "another-kernel"
        h5.write_json(manifest, result, replace=True)
        with self.assertRaisesRegex(h5.H5Error, "different host/kernel"):
            h7.require_prerequisite(self.directory)

    def test_scenario_rejects_shared_forbidden_values(self):
        for scenario_id, _ in h7.WORKLOADS.values():
            scenario = h5.load_scenario(ROOT / ("scenarios/" + scenario_id + ".json"))
            self.assertEqual(set(scenario["rules"]), scenario["expected_keys"])
            fields = [h5.field_record(i, name, rule["values"][0])
                      for i, (name, rule) in enumerate(scenario["rules"].items())]
            self.assertEqual(h5.compare_records(scenario, fields, fields), [])
            for field in fields:
                with self.subTest(scenario=scenario_id, field=field["name"]):
                    allowed = field["value"]
                    field["value"] = hex(int(allowed, 0) + 1)
                    differences = h5.compare_records(scenario, fields, fields)
                    self.assertEqual(differences[0]["kind"], "forbidden-outcome")
                    field["value"] = allowed

    def test_bundle_workload_binding_and_legacy_compatibility(self):
        scenario_path = self.directory / "scenario.json"
        manifest_path = self.directory / "build.json"
        for workload, (scenario_id, _) in h7.WORKLOADS.items():
            scenario_path.write_bytes((ROOT / ("scenarios/" + scenario_id + ".json")).read_bytes())
            manifest = {"schema": h7.BUILD_SCHEMA, "workload": workload,
                        "artifacts": {p.name: h5.sha256_file(p) for p in (self.image, scenario_path)}}
            h5.write_json(manifest_path, manifest, replace=True)
            self.assertEqual(h7.verify_bundle(self.directory)["workload"], workload)
            del manifest["workload"]
            h5.write_json(manifest_path, manifest, replace=True)
            if workload == "spi-lifecycle":
                self.assertEqual(h7.verify_bundle(self.directory)["workload"], workload)
            else:
                with self.assertRaisesRegex(h5.H5Error, "does not match"):
                    h7.verify_bundle(self.directory)
            manifest["workload"] = "unregistered"
            h5.write_json(manifest_path, manifest, replace=True)
            with self.assertRaisesRegex(h5.H5Error, "unsupported fixed workload"):
                h7.verify_bundle(self.directory)

    def test_capture_timeout_preserves_image_command_and_partial_serial(self):
        bundle = self.directory / "bundle"
        bundle.mkdir()
        (bundle / "guest.elf").write_bytes(elf_image())
        (bundle / "scenario.json").write_bytes((ROOT / "scenarios/h7-spi-lifecycle-v1.json").read_bytes())
        worker = bundle / "source/tools/h7_runner.py"
        worker.parent.mkdir(parents=True)
        worker.write_text('import time\nprint("partial unit trace", flush=True)\ntime.sleep(60)\n')
        h5.write_json(bundle / "build.json", {
            "schema": h7.BUILD_SCHEMA,
            "artifacts": {str(p.relative_to(bundle)): h5.sha256_file(p)
                          for p in (bundle / "guest.elf", bundle / "scenario.json", worker)}})
        prerequisite = self.directory / "prerequisite"
        prerequisite.mkdir()
        out = self.directory / "capture"
        with mock.patch.object(kvm, "require_host"), mock.patch.object(h7, "require_prerequisite"):
            with self.assertRaisesRegex(h5.H5Error, "complete passing trace"):
                h7.capture_kvm(argparse.Namespace(bundle=bundle, out=out,
                                                  unit_tests=prerequisite, timeout=0.3))
        result = h5.load_json(out / "run.json")
        self.assertEqual(result["status"], "no-pass-marker")
        self.assertEqual((out / "serial.log").read_bytes(), b"partial unit trace\n")
        self.assertEqual((out / "bundle/guest.elf").read_bytes(), elf_image())
        self.assertNotEqual(result["worker_exit_status"], 0)
        self.assertIn("worker", result["command"])
        self.assertFalse((out / "normalized.json").exists())


if __name__ == "__main__":
    unittest.main()
