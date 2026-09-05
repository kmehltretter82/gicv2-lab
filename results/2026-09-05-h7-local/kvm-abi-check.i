typedef long int ptrdiff_t;
typedef long unsigned int size_t;
typedef unsigned int wchar_t;
typedef struct {
  long long __clang_max_align_nonce1
      __attribute__((__aligned__(__alignof__(long long))));
  long double __clang_max_align_nonce2
      __attribute__((__aligned__(__alignof__(long double))));
} max_align_t;
typedef __signed__ char __s8;
typedef unsigned char __u8;
typedef __signed__ short __s16;
typedef unsigned short __u16;
typedef __signed__ int __s32;
typedef unsigned int __u32;
__extension__ typedef __signed__ long long __s64;
__extension__ typedef unsigned long long __u64;
typedef struct {
 unsigned long fds_bits[1024 / (8 * sizeof(long))];
} __kernel_fd_set;
typedef void (*__kernel_sighandler_t)(int);
typedef int __kernel_key_t;
typedef int __kernel_mqd_t;
typedef unsigned short __kernel_old_uid_t;
typedef unsigned short __kernel_old_gid_t;
typedef long __kernel_long_t;
typedef unsigned long __kernel_ulong_t;
typedef __kernel_ulong_t __kernel_ino_t;
typedef unsigned int __kernel_mode_t;
typedef int __kernel_pid_t;
typedef int __kernel_ipc_pid_t;
typedef unsigned int __kernel_uid_t;
typedef unsigned int __kernel_gid_t;
typedef __kernel_long_t __kernel_suseconds_t;
typedef int __kernel_daddr_t;
typedef unsigned int __kernel_uid32_t;
typedef unsigned int __kernel_gid32_t;
typedef unsigned int __kernel_old_dev_t;
typedef __kernel_ulong_t __kernel_size_t;
typedef __kernel_long_t __kernel_ssize_t;
typedef __kernel_long_t __kernel_ptrdiff_t;
typedef struct {
 int val[2];
} __kernel_fsid_t;
typedef __kernel_long_t __kernel_off_t;
typedef long long __kernel_loff_t;
typedef unsigned long long __kernel_uoff_t;
typedef __kernel_long_t __kernel_old_time_t;
typedef __kernel_long_t __kernel_time_t;
typedef long long __kernel_time64_t;
typedef __kernel_long_t __kernel_clock_t;
typedef int __kernel_timer_t;
typedef int __kernel_clockid_t;
typedef char * __kernel_caddr_t;
typedef unsigned short __kernel_uid16_t;
typedef unsigned short __kernel_gid16_t;
typedef __signed__ __int128 __s128 __attribute__((aligned(16)));
typedef unsigned __int128 __u128 __attribute__((aligned(16)));
typedef __u16 __le16;
typedef __u16 __be16;
typedef __u32 __le32;
typedef __u32 __be32;
typedef __u64 __le64;
typedef __u64 __be64;
typedef __u16 __sum16;
typedef __u32 __wsum;
typedef unsigned __poll_t;

struct user_pt_regs {
 __u64 regs[31];
 __u64 sp;
 __u64 pc;
 __u64 pstate;
};
struct user_fpsimd_state {
 __u128 vregs[32];
 __u32 fpsr;
 __u32 fpcr;
 __u32 __reserved[2];
};
struct user_hwdebug_state {
 __u32 dbg_info;
 __u32 pad;
 struct {
  __u64 addr;
  __u32 ctrl;
  __u32 pad;
 } dbg_regs[16];
};
struct user_sve_header {
 __u32 size;
 __u32 max_size;
 __u16 vl;
 __u16 max_vl;
 __u16 flags;
 __u16 __reserved;
};
struct user_pac_mask {
 __u64 data_mask;
 __u64 insn_mask;
};
struct user_pac_address_keys {
 __u128 apiakey;
 __u128 apibkey;
 __u128 apdakey;
 __u128 apdbkey;
};
struct user_pac_generic_keys {
 __u128 apgakey;
};
struct user_za_header {
 __u32 size;
 __u32 max_size;
 __u16 vl;
 __u16 max_vl;
 __u16 flags;
 __u16 __reserved;
};
struct user_gcs {
 __u64 features_enabled;
 __u64 features_locked;
 __u64 gcspr_el0;
};
struct kvm_regs {
 struct user_pt_regs regs;
 __u64 sp_el1;
 __u64 elr_el1;
 __u64 spsr[5];
 struct user_fpsimd_state fp_regs;
};
struct kvm_vcpu_init {
 __u32 target;
 __u32 features[7];
};
struct kvm_sregs {
};
struct kvm_fpu {
};
struct kvm_guest_debug_arch {
 __u64 dbg_bcr[16];
 __u64 dbg_bvr[16];
 __u64 dbg_wcr[16];
 __u64 dbg_wvr[16];
};
struct kvm_debug_exit_arch {
 __u32 hsr;
 __u32 hsr_high;
 __u64 far;
};
struct kvm_sync_regs {
 __u64 device_irq_level;
};
struct kvm_pmu_event_filter {
 __u16 base_event;
 __u16 nevents;
 __u8 action;
 __u8 pad[3];
};
struct kvm_vcpu_events {
 struct {
  __u8 serror_pending;
  __u8 serror_has_esr;
  __u8 ext_dabt_pending;
  __u8 pad[5];
  __u64 serror_esr;
 } exception;
 __u32 reserved[12];
};
struct kvm_arm_copy_mte_tags {
 __u64 guest_ipa;
 __u64 length;
 void *addr;
 __u64 flags;
 __u64 reserved[2];
};
struct kvm_arm_counter_offset {
 __u64 counter_offset;
 __u64 reserved;
};
enum {
 KVM_REG_ARM_STD_BIT_TRNG_V1_0 = 0,
};
enum {
 KVM_REG_ARM_STD_HYP_BIT_PV_TIME = 0,
};
enum {
 KVM_REG_ARM_VENDOR_HYP_BIT_FUNC_FEAT = 0,
 KVM_REG_ARM_VENDOR_HYP_BIT_PTP = 1,
};
enum {
 KVM_REG_ARM_VENDOR_HYP_BIT_DISCOVER_IMPL_VER = 0,
 KVM_REG_ARM_VENDOR_HYP_BIT_DISCOVER_IMPL_CPUS = 1,
};
enum kvm_smccc_filter_action {
 KVM_SMCCC_FILTER_HANDLE = 0,
 KVM_SMCCC_FILTER_DENY,
 KVM_SMCCC_FILTER_FWD_TO_USER,
};
struct kvm_smccc_filter {
 __u32 base;
 __u32 nr_functions;
 __u8 action;
 __u8 pad[15];
};
struct reg_mask_range {
 __u64 addr;
 __u32 range;
 __u32 reserved[13];
};
struct kvm_userspace_memory_region {
 __u32 slot;
 __u32 flags;
 __u64 guest_phys_addr;
 __u64 memory_size;
 __u64 userspace_addr;
};
struct kvm_userspace_memory_region2 {
 __u32 slot;
 __u32 flags;
 __u64 guest_phys_addr;
 __u64 memory_size;
 __u64 userspace_addr;
 __u64 guest_memfd_offset;
 __u32 guest_memfd;
 __u32 pad1;
 __u64 pad2[14];
};
struct kvm_irq_level {
 union {
  __u32 irq;
  __s32 status;
 };
 __u32 level;
};
struct kvm_irqchip {
 __u32 chip_id;
 __u32 pad;
        union {
  char dummy[512];
 } chip;
};
struct kvm_pit_config {
 __u32 flags;
 __u32 pad[15];
};
struct kvm_hyperv_exit {
 __u32 type;
 __u32 pad1;
 union {
  struct {
   __u32 msr;
   __u32 pad2;
   __u64 control;
   __u64 evt_page;
   __u64 msg_page;
  } synic;
  struct {
   __u64 input;
   __u64 result;
   __u64 params[2];
  } hcall;
  struct {
   __u32 msr;
   __u32 pad2;
   __u64 control;
   __u64 status;
   __u64 send_page;
   __u64 recv_page;
   __u64 pending_page;
  } syndbg;
 } u;
};
struct kvm_xen_exit {
 __u32 type;
 union {
  struct {
   __u32 longmode;
   __u32 cpl;
   __u64 input;
   __u64 result;
   __u64 params[6];
  } hcall;
 } u;
};
struct kvm_exit_snp_req_certs {
 __u64 gpa;
 __u64 npages;
 __u64 ret;
};
struct kvm_run {
 __u8 request_interrupt_window;
 __u8 immediate_exit;
 __u8 padding1[6];
 __u32 exit_reason;
 __u8 ready_for_interrupt_injection;
 __u8 if_flag;
 __u16 flags;
 __u64 cr8;
 __u64 apic_base;
 union {
  struct {
   __u64 hardware_exit_reason;
  } hw;
  struct {
   __u64 hardware_entry_failure_reason;
   __u32 cpu;
  } fail_entry;
  struct {
   __u32 exception;
   __u32 error_code;
  } ex;
  struct {
   __u8 direction;
   __u8 size;
   __u16 port;
   __u32 count;
   __u64 data_offset;
  } io;
  struct {
   struct kvm_debug_exit_arch arch;
  } debug;
  struct {
   __u64 phys_addr;
   __u8 data[8];
   __u32 len;
   __u8 is_write;
  } mmio;
  struct {
   __u64 phys_addr;
   __u8 data[8];
   __u32 len;
   __u8 is_write;
  } iocsr_io;
  struct {
   __u64 nr;
   __u64 args[6];
   __u64 ret;
   union {
    __u32 longmode;
    __u64 flags;
   };
  } hypercall;
  struct {
   __u64 rip;
   __u32 is_write;
   __u32 pad;
  } tpr_access;
  struct {
   __u8 icptcode;
   __u16 ipa;
   __u32 ipb;
  } s390_sieic;
  __u64 s390_reset_flags;
  struct {
   __u64 trans_exc_code;
   __u32 pgm_code;
  } s390_ucontrol;
  struct {
   __u32 dcrn;
   __u32 data;
   __u8 is_write;
  } dcr;
  struct {
   __u32 suberror;
   __u32 ndata;
   __u64 data[16];
  } internal;
  struct {
   __u32 suberror;
   __u32 ndata;
   __u64 flags;
   union {
    struct {
     __u8 insn_size;
     __u8 insn_bytes[15];
    };
   };
  } emulation_failure;
  struct {
   __u64 gprs[32];
  } osi;
  struct {
   __u64 nr;
   __u64 ret;
   __u64 args[9];
  } papr_hcall;
  struct {
   __u16 subchannel_id;
   __u16 subchannel_nr;
   __u32 io_int_parm;
   __u32 io_int_word;
   __u32 ipb;
   __u8 dequeued;
  } s390_tsch;
  struct {
   __u32 epr;
  } epr;
  struct {
   __u32 type;
   __u32 ndata;
   union {
    __u64 flags;
    __u64 data[16];
   };
  } system_event;
  struct {
   __u64 addr;
   __u8 ar;
   __u8 reserved;
   __u8 fc;
   __u8 sel1;
   __u16 sel2;
  } s390_stsi;
  struct {
   __u8 vector;
  } eoi;
  struct kvm_hyperv_exit hyperv;
  struct {
   __u64 esr_iss;
   __u64 fault_ipa;
  } arm_nisv;
  struct {
   __u8 error;
   __u8 pad[7];
   __u32 reason;
   __u32 index;
   __u64 data;
  } msr;
  struct kvm_xen_exit xen;
  struct {
   unsigned long extension_id;
   unsigned long function_id;
   unsigned long args[6];
   unsigned long ret[2];
  } riscv_sbi;
  struct {
   unsigned long csr_num;
   unsigned long new_value;
   unsigned long write_mask;
   unsigned long ret_value;
  } riscv_csr;
  struct {
   __u32 flags;
  } notify;
  struct {
   __u64 flags;
   __u64 gpa;
   __u64 size;
  } memory_fault;
  struct {
   __u64 flags;
   __u64 nr;
   union {
    struct {
     __u64 ret;
     __u64 data[5];
    } unknown;
    struct {
     __u64 ret;
     __u64 gpa;
     __u64 size;
    } get_quote;
    struct {
     __u64 ret;
     __u64 leaf;
     __u64 r11, r12, r13, r14;
    } get_tdvmcall_info;
    struct {
     __u64 ret;
     __u64 vector;
    } setup_event_notify;
   };
  } tdx;
  struct {
   __u64 flags;
   __u64 esr;
   __u64 gva;
   __u64 gpa;
  } arm_sea;
  struct kvm_exit_snp_req_certs snp_req_certs;
  char padding[256];
 };
 __u64 kvm_valid_regs;
 __u64 kvm_dirty_regs;
 union {
  struct kvm_sync_regs regs;
  char padding[2048];
 } s;
};
struct kvm_coalesced_mmio_zone {
 __u64 addr;
 __u32 size;
 union {
  __u32 pad;
  __u32 pio;
 };
};
struct kvm_coalesced_mmio {
 __u64 phys_addr;
 __u32 len;
 union {
  __u32 pad;
  __u32 pio;
 };
 __u8 data[8];
};
struct kvm_coalesced_mmio_ring {
 __u32 first, last;
 struct { struct { } __empty_coalesced_mmio; struct kvm_coalesced_mmio coalesced_mmio[]; };
};
struct kvm_translation {
 __u64 linear_address;
 __u64 physical_address;
 __u8 valid;
 __u8 writeable;
 __u8 usermode;
 __u8 pad[5];
};
struct kvm_interrupt {
 __u32 irq;
};
struct kvm_dirty_log {
 __u32 slot;
 __u32 padding1;
 union {
  void *dirty_bitmap;
  __u64 padding2;
 };
};
struct kvm_clear_dirty_log {
 __u32 slot;
 __u32 num_pages;
 __u64 first_page;
 union {
  void *dirty_bitmap;
  __u64 padding2;
 };
};
struct kvm_signal_mask {
 __u32 len;
 struct { struct { } __empty_sigset; __u8 sigset[]; };
};
struct kvm_tpr_access_ctl {
 __u32 enabled;
 __u32 flags;
 __u32 reserved[8];
};
struct kvm_vapic_addr {
 __u64 vapic_addr;
};
struct kvm_mp_state {
 __u32 mp_state;
};
struct kvm_guest_debug {
 __u32 control;
 __u32 pad;
 struct kvm_guest_debug_arch arch;
};
enum {
 kvm_ioeventfd_flag_nr_datamatch,
 kvm_ioeventfd_flag_nr_pio,
 kvm_ioeventfd_flag_nr_deassign,
 kvm_ioeventfd_flag_nr_virtio_ccw_notify,
 kvm_ioeventfd_flag_nr_fast_mmio,
 kvm_ioeventfd_flag_nr_max,
};
struct kvm_ioeventfd {
 __u64 datamatch;
 __u64 addr;
 __u32 len;
 __s32 fd;
 __u32 flags;
 __u8 pad[36];
};
struct kvm_enable_cap {
 __u32 cap;
 __u32 flags;
 __u64 args[4];
 __u8 pad[64];
};
struct kvm_irq_routing_irqchip {
 __u32 irqchip;
 __u32 pin;
};
struct kvm_irq_routing_msi {
 __u32 address_lo;
 __u32 address_hi;
 __u32 data;
 union {
  __u32 pad;
  __u32 devid;
 };
};
struct kvm_irq_routing_s390_adapter {
 __u64 ind_addr;
 __u64 summary_addr;
 __u64 ind_offset;
 __u32 summary_offset;
 __u32 adapter_id;
};
struct kvm_irq_routing_hv_sint {
 __u32 vcpu;
 __u32 sint;
};
struct kvm_irq_routing_xen_evtchn {
 __u32 port;
 __u32 vcpu;
 __u32 priority;
};
struct kvm_irq_routing_entry {
 __u32 gsi;
 __u32 type;
 __u32 flags;
 __u32 pad;
 union {
  struct kvm_irq_routing_irqchip irqchip;
  struct kvm_irq_routing_msi msi;
  struct kvm_irq_routing_s390_adapter adapter;
  struct kvm_irq_routing_hv_sint hv_sint;
  struct kvm_irq_routing_xen_evtchn xen_evtchn;
  __u32 pad[8];
 } u;
};
struct kvm_irq_routing {
 __u32 nr;
 __u32 flags;
 struct { struct { } __empty_entries; struct kvm_irq_routing_entry entries[]; };
};
struct kvm_irqfd {
 __u32 fd;
 __u32 gsi;
 __u32 flags;
 __u32 resamplefd;
 __u8 pad[16];
};
struct kvm_clock_data {
 __u64 clock;
 __u32 flags;
 __u32 pad0;
 __u64 realtime;
 __u64 host_tsc;
 __u32 pad[4];
};
struct kvm_config_tlb {
 __u64 params;
 __u64 array;
 __u32 mmu_type;
 __u32 array_len;
};
struct kvm_dirty_tlb {
 __u64 bitmap;
 __u32 num_dirty;
};
struct kvm_reg_list {
 __u64 n;
 struct { struct { } __empty_reg; __u64 reg[]; };
};
struct kvm_one_reg {
 __u64 id;
 __u64 addr;
};
struct kvm_msi {
 __u32 address_lo;
 __u32 address_hi;
 __u32 data;
 __u32 flags;
 __u32 devid;
 __u8 pad[12];
};
struct kvm_arm_device_addr {
 __u64 id;
 __u64 addr;
};
struct kvm_create_device {
 __u32 type;
 __u32 fd;
 __u32 flags;
};
struct kvm_device_attr {
 __u32 flags;
 __u32 group;
 __u64 attr;
 __u64 addr;
};
enum kvm_device_type {
 KVM_DEV_TYPE_FSL_MPIC_20 = 1,
 KVM_DEV_TYPE_FSL_MPIC_42,
 KVM_DEV_TYPE_XICS,
 KVM_DEV_TYPE_VFIO,
 KVM_DEV_TYPE_ARM_VGIC_V2,
 KVM_DEV_TYPE_FLIC,
 KVM_DEV_TYPE_ARM_VGIC_V3,
 KVM_DEV_TYPE_ARM_VGIC_ITS,
 KVM_DEV_TYPE_XIVE,
 KVM_DEV_TYPE_ARM_PV_TIME,
 KVM_DEV_TYPE_RISCV_AIA,
 KVM_DEV_TYPE_LOONGARCH_IPI,
 KVM_DEV_TYPE_LOONGARCH_EIOINTC,
 KVM_DEV_TYPE_LOONGARCH_PCHPIC,
 KVM_DEV_TYPE_LOONGARCH_DMSINTC,
 KVM_DEV_TYPE_ARM_VGIC_V5,
 KVM_DEV_TYPE_MAX,
};
struct kvm_vfio_spapr_tce {
 __s32 groupfd;
 __s32 tablefd;
};
struct kvm_s390_keyop {
 __u64 guest_addr;
 __u8 key;
 __u8 operation;
 __u8 pad[6];
};
struct kvm_enc_region {
 __u64 addr;
 __u64 size;
};
struct kvm_dirty_gfn {
 __u32 flags;
 __u32 slot;
 __u64 offset;
};
struct kvm_stats_header {
 __u32 flags;
 __u32 name_size;
 __u32 num_desc;
 __u32 id_offset;
 __u32 desc_offset;
 __u32 data_offset;
};
struct kvm_stats_desc {
 __u32 flags;
 __s16 exponent;
 __u16 size;
 __u32 offset;
 __u32 bucket_size;
 struct { struct { } __empty_name; char name[]; };
};
struct kvm_memory_attributes {
 __u64 address;
 __u64 size;
 __u64 attributes;
 __u64 flags;
};
struct kvm_create_guest_memfd {
 __u64 size;
 __u64 flags;
 __u64 reserved[6];
};
struct kvm_pre_fault_memory {
 __u64 gpa;
 __u64 size;
 __u64 flags;
 __u64 padding[5];
};

_Static_assert(((((0U) << (((0 +8)+8)+14)) | (((0xAE)) << (0 +8)) | (((0x00)) << 0) | ((0) << ((0 +8)+8)))) == (0xae00), "KVM_GET_API_VERSION");
_Static_assert(((((0U) << (((0 +8)+8)+14)) | (((0xAE)) << (0 +8)) | (((0x01)) << 0) | ((0) << ((0 +8)+8)))) == (0xae01), "KVM_CREATE_VM");
_Static_assert(((((0U) << (((0 +8)+8)+14)) | (((0xAE)) << (0 +8)) | (((0x03)) << 0) | ((0) << ((0 +8)+8)))) == (0xae03), "KVM_CHECK_EXTENSION");
_Static_assert(((((0U) << (((0 +8)+8)+14)) | (((0xAE)) << (0 +8)) | (((0x04)) << 0) | ((0) << ((0 +8)+8)))) == (0xae04), "KVM_GET_VCPU_MMAP_SIZE");
_Static_assert(((((0U) << (((0 +8)+8)+14)) | (((0xAE)) << (0 +8)) | (((0x41)) << 0) | ((0) << ((0 +8)+8)))) == (0xae41), "KVM_CREATE_VCPU");
_Static_assert(((((1U) << (((0 +8)+8)+14)) | (((0xAE)) << (0 +8)) | (((0x46)) << 0) | ((((sizeof(struct kvm_userspace_memory_region)))) << ((0 +8)+8)))) == (0x4020ae46), "KVM_SET_USER_MEMORY_REGION");
_Static_assert(((((0U) << (((0 +8)+8)+14)) | (((0xAE)) << (0 +8)) | (((0x80)) << 0) | ((0) << ((0 +8)+8)))) == (0xae80), "KVM_RUN");
_Static_assert(((((1U) << (((0 +8)+8)+14)) | (((0xAE)) << (0 +8)) | (((0xac)) << 0) | ((((sizeof(struct kvm_one_reg)))) << ((0 +8)+8)))) == (0x4010aeac), "KVM_SET_ONE_REG");
_Static_assert(((((1U) << (((0 +8)+8)+14)) | (((0xAE)) << (0 +8)) | (((0xae)) << 0) | ((((sizeof(struct kvm_vcpu_init)))) << ((0 +8)+8)))) == (0x4020aeae), "KVM_ARM_VCPU_INIT");
_Static_assert(((((2U) << (((0 +8)+8)+14)) | (((0xAE)) << (0 +8)) | (((0xaf)) << 0) | ((((sizeof(struct kvm_vcpu_init)))) << ((0 +8)+8)))) == (0x8020aeaf), "KVM_ARM_PREFERRED_TARGET");
_Static_assert(((((2U|1U) << (((0 +8)+8)+14)) | (((0xAE)) << (0 +8)) | (((0xe0)) << 0) | ((((sizeof(struct kvm_create_device)))) << ((0 +8)+8)))) == (0xc00caee0), "KVM_CREATE_DEVICE");
_Static_assert(((((1U) << (((0 +8)+8)+14)) | (((0xAE)) << (0 +8)) | (((0xe1)) << 0) | ((((sizeof(struct kvm_device_attr)))) << ((0 +8)+8)))) == (0x4018aee1), "KVM_SET_DEVICE_ATTR");
_Static_assert(((((1U) << (((0 +8)+8)+14)) | (((0xAE)) << (0 +8)) | (((0xe2)) << 0) | ((((sizeof(struct kvm_device_attr)))) << ((0 +8)+8)))) == (0x4018aee2), "KVM_GET_DEVICE_ATTR");
_Static_assert((KVM_DEV_TYPE_ARM_VGIC_V2) == (5), "KVM_DEV_TYPE_ARM_VGIC_V2");
_Static_assert((1) == (1), "KVM_CREATE_DEVICE_TEST");
_Static_assert((3) == (3), "KVM_DEV_ARM_VGIC_GRP_NR_IRQS");
_Static_assert((0) == (0), "KVM_DEV_ARM_VGIC_GRP_ADDR");
_Static_assert((1) == (1), "KVM_DEV_ARM_VGIC_GRP_DIST_REGS");
_Static_assert((4) == (4), "KVM_DEV_ARM_VGIC_GRP_CTRL");
_Static_assert((0) == (0), "KVM_DEV_ARM_VGIC_CTRL_INIT");
_Static_assert((0) == (0), "KVM_VGIC_V2_ADDR_TYPE_DIST");
_Static_assert((1) == (1), "KVM_VGIC_V2_ADDR_TYPE_CPU");
_Static_assert((0) == (0), "KVM_CAP_IRQCHIP");
_Static_assert((3) == (3), "KVM_CAP_USER_MEMORY");
_Static_assert((70) == (70), "KVM_CAP_ONE_REG");
_Static_assert((89) == (89), "KVM_CAP_DEVICE_CTRL");
_Static_assert((6) == (6), "KVM_EXIT_MMIO");
_Static_assert((0x6000000000000000ULL | 0x0030000000000000ULL | (0x0010 << 16) | (__builtin_offsetof(struct kvm_regs, regs.pc) / sizeof(__u32))) == (0x6030000000100040ULL), "KVM_REG_ARM64 | KVM_REG_SIZE_U64 | KVM_REG_ARM_CORE | KVM_REG_ARM_CORE_REG(regs.pc)");
_Static_assert((0x6000000000000000ULL | 0x0030000000000000ULL | (0x0010 << 16) | (__builtin_offsetof(struct kvm_regs, regs.pstate) / sizeof(__u32))) == (0x6030000000100042ULL), "KVM_REG_ARM64 | KVM_REG_SIZE_U64 | KVM_REG_ARM_CORE | KVM_REG_ARM_CORE_REG(regs.pstate)");
_Static_assert((0x6000000000000000ULL | 0x0030000000000000ULL | (0x0010 << 16) | (__builtin_offsetof(struct kvm_regs, sp_el1) / sizeof(__u32))) == (0x6030000000100044ULL), "KVM_REG_ARM64 | KVM_REG_SIZE_U64 | KVM_REG_ARM_CORE | KVM_REG_ARM_CORE_REG(sp_el1)");
_Static_assert((sizeof(struct kvm_userspace_memory_region)) == (32), "sizeof(struct kvm_userspace_memory_region)");
_Static_assert((sizeof(struct kvm_vcpu_init)) == (32), "sizeof(struct kvm_vcpu_init)");
_Static_assert((sizeof(struct kvm_create_device)) == (12), "sizeof(struct kvm_create_device)");
_Static_assert((sizeof(struct kvm_device_attr)) == (24), "sizeof(struct kvm_device_attr)");
_Static_assert((sizeof(struct kvm_one_reg)) == (16), "sizeof(struct kvm_one_reg)");
_Static_assert((__builtin_offsetof(struct kvm_run, exit_reason)) == (8), "offsetof(struct kvm_run, exit_reason)");
_Static_assert((__builtin_offsetof(struct kvm_run, mmio.phys_addr)) == (32), "offsetof(struct kvm_run, mmio.phys_addr)");
_Static_assert((__builtin_offsetof(struct kvm_run, mmio.data)) == (40), "offsetof(struct kvm_run, mmio.data)");
_Static_assert((__builtin_offsetof(struct kvm_run, mmio.len)) == (48), "offsetof(struct kvm_run, mmio.len)");
_Static_assert((__builtin_offsetof(struct kvm_run, mmio.is_write)) == (52), "offsetof(struct kvm_run, mmio.is_write)");
