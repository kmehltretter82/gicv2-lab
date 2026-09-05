/* SPDX-License-Identifier: GPL-2.0-or-later */
/* Compile with Linux arm64 UAPI headers; execution is unnecessary. */
#include <stddef.h>
#include <linux/kvm.h>
#define CHECK(name, value) _Static_assert((name) == (value), #name)
CHECK(KVM_GET_API_VERSION, 0xae00);
CHECK(KVM_CREATE_VM, 0xae01);
CHECK(KVM_CHECK_EXTENSION, 0xae03);
CHECK(KVM_GET_VCPU_MMAP_SIZE, 0xae04);
CHECK(KVM_CREATE_VCPU, 0xae41);
CHECK(KVM_SET_USER_MEMORY_REGION, 0x4020ae46);
CHECK(KVM_RUN, 0xae80);
CHECK(KVM_SET_ONE_REG, 0x4010aeac);
CHECK(KVM_ARM_VCPU_INIT, 0x4020aeae);
CHECK(KVM_ARM_PREFERRED_TARGET, 0x8020aeaf);
CHECK(KVM_CREATE_DEVICE, 0xc00caee0);
CHECK(KVM_SET_DEVICE_ATTR, 0x4018aee1);
CHECK(KVM_GET_DEVICE_ATTR, 0x4018aee2);
CHECK(KVM_DEV_TYPE_ARM_VGIC_V2, 5);
CHECK(KVM_CREATE_DEVICE_TEST, 1);
CHECK(KVM_DEV_ARM_VGIC_GRP_NR_IRQS, 3);
CHECK(KVM_DEV_ARM_VGIC_GRP_ADDR, 0);
CHECK(KVM_DEV_ARM_VGIC_GRP_DIST_REGS, 1);
CHECK(KVM_DEV_ARM_VGIC_GRP_CTRL, 4);
CHECK(KVM_DEV_ARM_VGIC_CTRL_INIT, 0);
CHECK(KVM_VGIC_V2_ADDR_TYPE_DIST, 0);
CHECK(KVM_VGIC_V2_ADDR_TYPE_CPU, 1);
CHECK(KVM_CAP_IRQCHIP, 0);
CHECK(KVM_CAP_USER_MEMORY, 3);
CHECK(KVM_CAP_ONE_REG, 70);
CHECK(KVM_CAP_DEVICE_CTRL, 89);
CHECK(KVM_EXIT_MMIO, 6);
CHECK(KVM_REG_ARM64 | KVM_REG_SIZE_U64 | KVM_REG_ARM_CORE | KVM_REG_ARM_CORE_REG(regs.pc), 0x6030000000100040ULL);
CHECK(KVM_REG_ARM64 | KVM_REG_SIZE_U64 | KVM_REG_ARM_CORE | KVM_REG_ARM_CORE_REG(regs.pstate), 0x6030000000100042ULL);
CHECK(KVM_REG_ARM64 | KVM_REG_SIZE_U64 | KVM_REG_ARM_CORE | KVM_REG_ARM_CORE_REG(sp_el1), 0x6030000000100044ULL);
CHECK(sizeof(struct kvm_userspace_memory_region), 32);
CHECK(sizeof(struct kvm_vcpu_init), 32);
CHECK(sizeof(struct kvm_create_device), 12);
CHECK(sizeof(struct kvm_device_attr), 24);
CHECK(sizeof(struct kvm_one_reg), 16);
CHECK(offsetof(struct kvm_run, exit_reason), 8);
CHECK(offsetof(struct kvm_run, mmio.phys_addr), 32);
CHECK(offsetof(struct kvm_run, mmio.data), 40);
CHECK(offsetof(struct kvm_run, mmio.len), 48);
CHECK(offsetof(struct kvm_run, mmio.is_write), 52);
