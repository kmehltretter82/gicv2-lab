#!/bin/sh
# SPDX-License-Identifier: GPL-2.0-or-later

set -eu

image=${1:?usage: smoke-qemu.sh IMAGE}
qemu=${QEMU:?set QEMU to qemu-system-aarch64}
log=build/smoke-qemu.log
stderr_log=build/smoke-qemu.stderr
marker='[gicv2-lab] H4h PASS'
deadline=$(( $(date +%s) + 10 ))
poll_interval=${SMOKE_POLL_INTERVAL:-0.1}
pid=

# Invoked indirectly by the trap below.
# shellcheck disable=SC2329
cleanup()
{
    if test -n "$pid" && kill -0 "$pid" 2>/dev/null; then
        kill "$pid" 2>/dev/null || true
    fi
    if test -n "$pid"; then
        wait "$pid" 2>/dev/null || true
    fi
}

require_line()
{
    expected=$1

    if ! grep -Fq "$expected" "$log"; then
        echo "gicv2-lab: missing smoke-test line: $expected" >&2
        return 1
    fi
}

require_once()
{
    expected=$1
    actual=$(grep -Fc "$expected" "$log" || true)

    if test "$actual" -ne 1; then
        echo "gicv2-lab: expected one smoke-test line, found $actual: $expected" >&2
        return 1
    fi
}

require_count()
{
    expected_count=$1
    expected=$2
    actual=$(grep -Fc "$expected" "$log" || true)

    if test "$actual" -ne "$expected_count"; then
        echo "gicv2-lab: expected $expected_count smoke-test lines, found $actual: $expected" >&2
        return 1
    fi
}

reject_line()
{
    forbidden=$1

    if grep -Fq "$forbidden" "$log"; then
        echo "gicv2-lab: forbidden smoke-test line: $forbidden" >&2
        return 1
    fi
}

require_snapshot()
{
    snapshot_suffix=$1
    snapshot_lr0=$2
    snapshot_lr1=$3
    snapshot_lr2=$4
    snapshot_lr3=$5
    snapshot_apr=$6
    snapshot_elrsr0=$7

    require_line "GICH_HCR_${snapshot_suffix}=0x0000000000000001"
    require_line "GICH_VMCR_${snapshot_suffix}=0x00000000f85c0201"
    require_line "GICH_MISR_${snapshot_suffix}=0x0000000000000000"
    require_line "GICH_EISR0_${snapshot_suffix}=0x0000000000000000"
    require_line "GICH_EISR1_${snapshot_suffix}=0x0000000000000000"
    require_line "GICH_ELRSR0_${snapshot_suffix}=${snapshot_elrsr0}"
    require_line "GICH_ELRSR1_${snapshot_suffix}=0x0000000000000000"
    require_line "GICH_APR_${snapshot_suffix}=${snapshot_apr}"
    require_line "GICH_LR0_${snapshot_suffix}=${snapshot_lr0}"
    require_line "GICH_LR1_${snapshot_suffix}=${snapshot_lr1}"
    require_line "GICH_LR2_${snapshot_suffix}=${snapshot_lr2}"
    require_line "GICH_LR3_${snapshot_suffix}=${snapshot_lr3}"
}

trap cleanup EXIT INT TERM

rm -f "$log" "$stderr_log"
"$qemu" \
    -machine raspi400 \
    -cpu cortex-a72,has_el3=off \
    -kernel "$image" \
    -display none \
    -monitor none \
    -serial "file:$log" \
    -no-reboot 2>"$stderr_log" &
pid=$!

while test "$(date +%s)" -lt "$deadline"; do
    if test -f "$log" && grep -Fq "$marker" "$log"; then
        if test "${SMOKE_QUIET:-0}" != 1; then
            cat "$log"
        fi
        require_once '[gicv2-lab] H4h EL2 monitor'
        require_once "$marker"
        require_line 'CurrentEL=2'
        require_line 'GICH_LRS=4'
        require_line 'GICH_PREBITS=5'
        require_line 'GICH_PRIBITS=5'
        require_once '[gicv2-lab] H1 PASS'
        require_line 'stage2_guest_desc=0x00000000002007fd'
        require_line 'stage2_gicv_desc=0x00400000ff8467c7'
        require_line 'stage2_gicv_dir_desc=0x00400000ff8477c7'
        require_line 'VTCR_EL2_enabled=0x0000000080003560'
        require_line 'HCR_EL2_enabled=0x0000000080000011'
        require_line 'GICC_PMR_enabled=0x00000000000000ff'
        require_line 'GICD_ISENABLER0_enabled=0x000000000400ffff'
        require_line 'GICD_ICFGR1_enabled=0x0000000000000000'
        require_line 'GICD_IPRIORITYR24_enabled=0x0000000000200000'
        require_line 'CNTHP_CTL_initial=0x0000000000000000'
        require_line 'GICH_HCR_enabled=0x0000000000000001'
        require_line 'GICH_VMCR_enabled=0x00000000f85c0201'
        require_line 'GICH_MISR_initial=0x0000000000000000'
        require_line 'GICH_EISR0_initial=0x0000000000000000'
        require_line 'GICH_EISR1_initial=0x0000000000000000'
        require_line 'GICH_ELRSR0_initial=0x000000000000000f'
        require_line 'GICH_ELRSR1_initial=0x0000000000000000'
        require_line 'GICH_APR_initial=0x0000000000000000'
        require_line 'GICH_LR0_initial=0x0000000000000000'
        require_line 'GICH_LR1_initial=0x0000000000000000'
        require_line 'GICH_LR2_initial=0x0000000000000000'
        require_line 'GICH_LR3_initial=0x0000000000000000'
        require_line 'GuestCurrentEL=1'
        require_line '[gicv2-lab] expected stage-2 translation fault'
        require_line 'FAR_EL2=0x0000000000080000'
        require_line 'HPFAR_EL2=0x0000000000000800'
        require_line 'stage2_fsc=6'
        require_line '[gicv2-lab] EL1 guest resumed after stage-2 fault'
        require_once '[gicv2-lab] H2 PASS'
        require_line 'GuestGICV_CTLR=0x0000000000000201'
        require_line 'GuestGICV_PMR=0x00000000000000f8'
        require_line 'GuestGICV_BPR=0x0000000000000002'
        require_once '[gicv2-lab] virtual interface empty before WFI'
        require_snapshot before_wfi \
            0x0000000000000000 0x0000000000000000 \
            0x0000000000000000 0x0000000000000000 \
            0x0000000000000000 0x000000000000000f
        require_line 'GuestHPPIR_before_wfi=0x00000000000003ff'
        require_once '[gicv2-lab] guest WFI trap armed'
        require_line 'HCR_EL2_before_wfi=0x0000000080000011'
        require_line 'HCR_EL2_wfi_trap_armed=0x0000000080002011'
        require_snapshot wfi_trap_armed \
            0x0000000000000000 0x0000000000000000 \
            0x0000000000000000 0x0000000000000000 \
            0x0000000000000000 0x000000000000000f
        require_once '[gicv2-lab] expected trapped guest WFI'
        require_count 2 'vector_slot=8'
        require_line 'ESR_EL2=0x0000000007e00000'
        require_line 'ELR_EL2=0x0000000000200074'
        require_line 'SPSR_EL2=0x0000000000000345'
        require_line 'HCR_EL2_wfi_trap=0x0000000080002011'
        require_line 'HCR_EL2_wfi_reexecute=0x0000000080000011'
        require_line 'CNTHP_delay_ticks=0x00000000002932e0'
        require_once 'CNTHP_start_count='
        require_line 'CNTHP_CTL_armed=0x0000000000000001'
        require_once '[gicv2-lab] physical IRQ observed'
        require_line 'physical_irq_vector_slot=9'
        require_line 'PhysicalIAR_hyp_timer=26'
        require_line 'CNTHP_CTL_expired=0x0000000000000005'
        require_once 'CNTHP_elapsed_ticks='
        require_line 'HCR_EL2_timer_irq=0x0000000080000011'
        require_line 'ELR_EL2_timer_irq=0x0000000000200078'
        require_line 'SPSR_EL2_timer_irq=0x0000000000000345'
        require_once '[gicv2-lab] expected hypervisor timer IRQ'
        require_snapshot timer_injected_pending \
            0x0000000014000030 0x0000000000000000 \
            0x0000000000000000 0x0000000000000000 \
            0x0000000000000000 0x000000000000000e
        require_once '[gicv2-lab] timer-injected virtual IRQ active'
        require_line 'GuestIAR_wake_active=0x0000000000000030'
        require_line 'GuestRPR_wake_active=0x0000000000000040'
        require_line 'GuestHPPIR_wake_active=0x00000000000003ff'
        require_line 'GuestIRQState_wake_active=0x0003ff4000000030'
        require_snapshot wake_active \
            0x0000000024000030 0x0000000000000000 \
            0x0000000000000000 0x0000000000000000 \
            0x0000000000000100 0x000000000000000e
        require_once '[gicv2-lab] wake IRQ priority dropped'
        require_line 'GuestIAR_wake_drop=0x0000000000000030'
        require_line 'GuestRPR_wake_drop=0x00000000000000ff'
        require_line 'GuestHPPIR_wake_drop=0x00000000000003ff'
        require_line 'GuestIRQState_wake_drop=0x0003ffff00000030'
        require_snapshot wake_drop \
            0x0000000024000030 0x0000000000000000 \
            0x0000000000000000 0x0000000000000000 \
            0x0000000000000000 0x000000000000000e
        require_once '[gicv2-lab] wake IRQ deactivated'
        require_line 'GuestIAR_wake_deactivated=0x0000000000000030'
        require_line 'GuestRPR_wake_deactivated=0x00000000000000ff'
        require_line 'GuestHPPIR_wake_deactivated=0x00000000000003ff'
        require_line 'GuestIRQState_wake_deactivated=0x0003ffff00000030'
        require_snapshot wake_deactivated \
            0x0000000004000030 0x0000000000000000 \
            0x0000000000000000 0x0000000000000000 \
            0x0000000000000000 0x000000000000000f
        require_once '[gicv2-lab] wake LR cleared'
        require_snapshot cleared \
            0x0000000000000000 0x0000000000000000 \
            0x0000000000000000 0x0000000000000000 \
            0x0000000000000000 0x000000000000000f
        require_once '[gicv2-lab] guest resumed after timer-woken WFI'
        require_line 'GuestIRQCount_after_wfi=1'
        reject_line '[gicv2-lab] H3 FAIL:'
        reject_line '[gicv2-lab] H4a FAIL:'
        reject_line '[gicv2-lab] H4b FAIL:'
        reject_line '[gicv2-lab] H4c FAIL:'
        reject_line '[gicv2-lab] H4d FAIL:'
        reject_line '[gicv2-lab] H4e FAIL:'
        reject_line '[gicv2-lab] H4f FAIL:'
        reject_line '[gicv2-lab] H4g FAIL:'
        reject_line '[gicv2-lab] H4h FAIL:'
        reject_line '[gicv2-lab] unexpected physical IRQ'
        exit 0
    fi
    if ! kill -0 "$pid" 2>/dev/null; then
        break
    fi
    sleep "$poll_interval"
done

test -f "$log" && cat "$log"
test ! -s "$stderr_log" || cat "$stderr_log" >&2
echo "gicv2-lab: QEMU smoke test did not observe the H4h marker" >&2
exit 1
