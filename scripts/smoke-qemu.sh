#!/bin/sh
# SPDX-License-Identifier: GPL-2.0-or-later

set -eu

image=${1:?usage: smoke-qemu.sh IMAGE}
qemu=${QEMU:?set QEMU to qemu-system-aarch64}
log=build/smoke-qemu.log
stderr_log=build/smoke-qemu.stderr
marker='[gicv2-lab] H4e PASS'
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
    snapshot_apr=$3
    snapshot_elrsr0=$4

    require_line "GICH_HCR_${snapshot_suffix}=0x0000000000000001"
    require_line "GICH_VMCR_${snapshot_suffix}=0x00000000f85c0201"
    require_line "GICH_MISR_${snapshot_suffix}=0x0000000000000000"
    require_line "GICH_EISR0_${snapshot_suffix}=0x0000000000000000"
    require_line "GICH_EISR1_${snapshot_suffix}=0x0000000000000000"
    require_line "GICH_ELRSR0_${snapshot_suffix}=${snapshot_elrsr0}"
    require_line "GICH_ELRSR1_${snapshot_suffix}=0x0000000000000000"
    require_line "GICH_APR_${snapshot_suffix}=${snapshot_apr}"
    require_line "GICH_LR0_${snapshot_suffix}=${snapshot_lr0}"
    require_line "GICH_LR1_${snapshot_suffix}=0x0000000000000000"
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
        require_once '[gicv2-lab] H4e EL2 monitor'
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
        require_once '[gicv2-lab] active-plus-pending virtual IRQ pending'
        require_snapshot pending 0x000000001800002a \
            0x0000000000000000 0x000000000000000e
        require_once '[gicv2-lab] first virtual IRQ active'
        require_line 'GuestIAR_first_active=42'
        require_line 'GuestRPR_first_active=0x0000000000000080'
        require_line 'GuestHPPIR_first_active=1023'
        require_snapshot first_active 0x000000002800002a \
            0x0000000000010000 0x000000000000000e
        require_once '[gicv2-lab] virtual source re-pended while active'
        require_snapshot active_pending 0x000000003800002a \
            0x0000000000010000 0x000000000000000e
        require_once '[gicv2-lab] first EOIR priority drop'
        require_line 'GuestIAR_first_priority_drop=42'
        require_line 'GuestRPR_first_priority_drop=0x00000000000000ff'
        require_line 'GuestHPPIR_first_priority_drop=1023'
        require_snapshot first_priority_drop 0x000000003800002a \
            0x0000000000000000 0x000000000000000e
        require_once '[gicv2-lab] first DIR exposed pending redelivery'
        require_line 'GuestIAR_redelivery_pending=42'
        require_line 'GuestRPR_redelivery_pending=0x00000000000000ff'
        require_line 'GuestHPPIR_redelivery_pending=42'
        require_snapshot redelivery_pending 0x000000001800002a \
            0x0000000000000000 0x000000000000000e
        require_once '[gicv2-lab] virtual IRQ delivered again'
        require_line 'GuestIAR_second_active=42'
        require_line 'GuestRPR_second_active=0x0000000000000080'
        require_line 'GuestHPPIR_second_active=1023'
        require_snapshot second_active 0x000000002800002a \
            0x0000000000010000 0x000000000000000e
        require_once '[gicv2-lab] second EOIR priority drop'
        require_line 'GuestIAR_second_priority_drop=42'
        require_line 'GuestRPR_second_priority_drop=0x00000000000000ff'
        require_line 'GuestHPPIR_second_priority_drop=1023'
        require_snapshot second_priority_drop 0x000000002800002a \
            0x0000000000000000 0x000000000000000e
        require_once '[gicv2-lab] second DIR deactivated virtual IRQ'
        require_line 'GuestIAR_final_deactivate=42'
        require_line 'GuestRPR_final_deactivate=0x00000000000000ff'
        require_line 'GuestHPPIR_final_deactivate=1023'
        require_snapshot final_deactivated 0x000000000800002a \
            0x0000000000000000 0x000000000000000f
        require_once '[gicv2-lab] virtual interface restored'
        require_snapshot cleared 0x0000000000000000 \
            0x0000000000000000 0x000000000000000f
        reject_line '[gicv2-lab] H3 FAIL:'
        reject_line '[gicv2-lab] H4a FAIL:'
        reject_line '[gicv2-lab] H4b FAIL:'
        reject_line '[gicv2-lab] H4c FAIL:'
        reject_line '[gicv2-lab] H4d FAIL:'
        reject_line '[gicv2-lab] H4e FAIL:'
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
echo "gicv2-lab: QEMU smoke test did not observe the H4e marker" >&2
exit 1
