# SPDX-License-Identifier: GPL-2.0-or-later
"""The small Linux arm64 KVM ABI subset used by the fixed H7 guest.

Constants/layouts are checked against Linux UAPI by h7/kvm_abi_check.c.
No ioctl is issued at import time. This module does not expose arbitrary
device registers or guest-controlled addresses to the host.
"""

import ctypes
import fcntl
import mmap
import os
import platform
import struct
from contextlib import ExitStack


GET_API_VERSION = 0xAE00
CREATE_VM = 0xAE01
CHECK_EXTENSION = 0xAE03
GET_VCPU_MMAP_SIZE = 0xAE04
CREATE_VCPU = 0xAE41
SET_USER_MEMORY_REGION = 0x4020AE46
RUN = 0xAE80
SET_ONE_REG = 0x4010AEAC
ARM_VCPU_INIT = 0x4020AEAE
ARM_PREFERRED_TARGET = 0x8020AEAF
CREATE_DEVICE = 0xC00CAEE0
SET_DEVICE_ATTR = 0x4018AEE1
GET_DEVICE_ATTR = 0x4018AEE2
REG_PC = 0x6030000000100040
REG_PSTATE = 0x6030000000100042
REG_SP_EL1 = 0x6030000000100044
RAM_BASE = 0x40000000
RAM_SIZE = 0x100000
GICD = 0x08000000
GICC = 0x08010000
UART = 0x09000000


class Unsupported(RuntimeError):
    pass


def require_host():
    if platform.system() != "Linux" or platform.machine() not in ("aarch64", "arm64"):
        raise Unsupported("KVM execution requires Linux arm64; this host is {} {}".format(
            platform.system(), platform.machine()))
    if not os.path.exists("/dev/kvm"):
        raise Unsupported("/dev/kvm is unavailable")


def load_elf(image, memory):
    """Load only an ELF64 little-endian AArch64 executable in the fixed RAM slot."""
    data = image.read_bytes()
    if len(data) < 64 or data[:7] != b"\x7fELF\x02\x01\x01":
        raise ValueError("expected ELF64 little-endian image")
    _, kind, machine, version, entry, phoff, _, _, ehsize, phsize, count, *_ = \
        struct.unpack_from("<16sHHIQQQIHHHHHH", data)
    if (kind, machine, version, ehsize, phsize) != (2, 183, 1, 64, 56) or count > 32:
        raise ValueError("unsupported AArch64 ELF header")
    if phoff + count * phsize > len(data):
        raise ValueError("truncated ELF program headers")
    ranges = []
    executable_entry = False
    for number in range(count):
        ptype, flags, offset, virtual, physical, filesz, memsz, _ = struct.unpack_from(
            "<IIQQQQQQ", data, phoff + number * phsize)
        if ptype != 1 or memsz == 0:
            continue
        if virtual != physical or filesz > memsz or offset + filesz > len(data):
            raise ValueError("invalid ELF load segment")
        start, end = physical - RAM_BASE, physical - RAM_BASE + memsz
        if start < 0 or end > RAM_SIZE - 0x4000:
            raise ValueError("ELF segment outside guest image RAM or overlapping stack")
        if any(start < previous_end and previous_start < end for previous_start, previous_end in ranges):
            raise ValueError("overlapping ELF segments")
        ranges.append((start, end))
        memory[start:end] = data[offset:offset + filesz] + b"\0" * (memsz - filesz)
        executable_entry |= bool(flags & 1) and physical <= entry < physical + filesz
    if not executable_entry or entry & 3:
        raise ValueError("ELF entry is not an aligned executable instruction")
    return entry


def service_console(run, stream):
    """Handle only the two PL011 accesses made by h7/guest.c."""
    reason = struct.unpack_from("<I", run, 8)[0]
    if reason != 6:  # KVM_EXIT_MMIO
        raise RuntimeError("unexpected KVM exit reason {}".format(reason))
    address, data, length, write = struct.unpack_from("<Q8sIB", run, 32)
    if address == UART and length == 4 and write == 1:
        value = struct.unpack("<I", data[:4])[0]
        if value > 127 or (value < 32 and value not in (10, 13, 9)):
            raise RuntimeError("unexpected console byte")
        stream.write(bytes([value]))
        stream.flush()
    elif address == UART + 0x18 and length == 4 and write == 0:
        run[40:48] = b"\0" * 8  # TXFF clear: transmitter can accept one byte.
    else:
        raise RuntimeError("unexpected MMIO: address={:#x}, length={}, write={}".format(
            address, length, write))


class VM:
    def __init__(self):
        self.resources = ExitStack()

    def fd(self, descriptor):
        self.resources.callback(os.close, descriptor)
        return descriptor

    def attribute(self, group, attr, value=None, get=False):
        pointer = ctypes.addressof(value) if value is not None else 0
        request = struct.pack("<IIQQ", 0, group, attr, pointer)
        fcntl.ioctl(self.gic, GET_DEVICE_ATTR if get else SET_DEVICE_ATTR, request)

    def register(self, identifier, value):
        cell = ctypes.c_uint64(value)
        fcntl.ioctl(self.vcpu, SET_ONE_REG, struct.pack("<QQ", identifier, ctypes.addressof(cell)))

    def __enter__(self):
        require_host()
        try:
            self.kvm = self.fd(os.open("/dev/kvm", os.O_RDWR | os.O_CLOEXEC))
            if fcntl.ioctl(self.kvm, GET_API_VERSION, 0) != 12:
                raise Unsupported("unsupported KVM API version")
            for capability in (0, 3, 70, 89):  # IRQCHIP, USER_MEMORY, ONE_REG, DEVICE_CTRL
                if fcntl.ioctl(self.kvm, CHECK_EXTENSION, capability) <= 0:
                    raise Unsupported("missing KVM capability {}".format(capability))
            self.vm = self.fd(fcntl.ioctl(self.kvm, CREATE_VM, 0))
            self.vcpu = self.fd(fcntl.ioctl(self.vm, CREATE_VCPU, 0))
            preferred = bytearray(32)
            fcntl.ioctl(self.vm, ARM_PREFERRED_TARGET, preferred, True)
            fcntl.ioctl(self.vcpu, ARM_VCPU_INIT, preferred)
            self.memory = self.resources.enter_context(mmap.mmap(-1, RAM_SIZE))
            address = ctypes.addressof(ctypes.c_char.from_buffer(self.memory))
            fcntl.ioctl(self.vm, SET_USER_MEMORY_REGION,
                        struct.pack("<IIQQQ", 0, 0, RAM_BASE, RAM_SIZE, address))
            # Test support before creating the sole in-kernel interrupt controller.
            probe = bytearray(struct.pack("<III", 5, 0, 1))
            try:
                fcntl.ioctl(self.vm, CREATE_DEVICE, probe, True)
            except OSError as error:
                import errno
                if error.errno in (errno.ENODEV, errno.ENXIO, errno.EINVAL):
                    raise Unsupported("KVM VGICv2 is unavailable") from error
                raise
            device = bytearray(struct.pack("<III", 5, 0, 0))
            fcntl.ioctl(self.vm, CREATE_DEVICE, device, True)
            self.gic = self.fd(struct.unpack_from("<I", device, 4)[0])
            self.attribute(3, 0, ctypes.c_uint32(64))
            self.attribute(0, 0, ctypes.c_uint64(GICD))
            self.attribute(0, 1, ctypes.c_uint64(GICC))
            self.attribute(4, 0)  # KVM_DEV_ARM_VGIC_CTRL_INIT, after vCPU creation.
            iidr = ctypes.c_uint32()
            self.attribute(1, 8, iidr, get=True)
            self.attribute(1, 8, iidr)  # acknowledge the kernel's register ABI revision
            size = fcntl.ioctl(self.kvm, GET_VCPU_MMAP_SIZE, 0)
            if size < 256:
                raise RuntimeError("invalid KVM vCPU mapping size")
            self.run = self.resources.enter_context(mmap.mmap(self.vcpu, size))
            return self
        except BaseException:
            self.resources.close()
            raise

    def execute(self, image, stream):
        entry = load_elf(image, self.memory)
        self.register(REG_PC, entry)
        self.register(REG_PSTATE, 0x3c5)  # EL1h, DAIF masked
        self.register(REG_SP_EL1, RAM_BASE + RAM_SIZE)
        while True:
            fcntl.ioctl(self.vcpu, RUN, 0)
            service_console(self.run, stream)

    def __exit__(self, *error):
        self.resources.close()
