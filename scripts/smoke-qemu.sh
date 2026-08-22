#!/bin/sh
# SPDX-License-Identifier: GPL-2.0-or-later

set -eu

image=${1:?usage: smoke-qemu.sh IMAGE}
qemu=${QEMU:?set QEMU to qemu-system-aarch64}
log=build/smoke-qemu.log
stderr_log=build/smoke-qemu.stderr
marker='[gicv2-lab] H4g PASS'
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
        require_once '[gicv2-lab] H4g EL2 monitor'
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
        require_once '[gicv2-lab] all four List Registers occupied'
        require_snapshot all_pending 0x0000000012000028 \
            0x0000000014000029 0x000000001600002a \
            0x000000001800002b 0x0000000000000000 \
            0x0000000000000000
        require_once '[gicv2-lab] fifth virtual interrupt queued in EL2'
        require_line 'EL2SoftwareQueue_entry=0x000000001a00002c'
        require_line 'GuestHPPIR_all_pending_observed=0x0000000000000028'
        require_once '[gicv2-lab] highest-priority resident LR visible'
        require_line 'GuestHPPIR_all_pending=0x0000000000000028'

        require_count 5 '[gicv2-lab] refill delivery active'
        require_count 5 '[gicv2-lab] refill delivery priority dropped'
        require_count 5 '[gicv2-lab] refill delivery deactivated'
        require_count 3 'RefillDelivery=0'
        require_count 3 'RefillDelivery=1'
        require_count 3 'RefillDelivery=2'
        require_count 3 'RefillDelivery=3'
        require_count 3 'RefillDelivery=4'
        require_count 5 'RefillPhase=0'
        require_count 5 'RefillPhase=1'
        require_count 5 'RefillPhase=2'
        require_count 3 'GuestIAR_refill=0x0000000000000028'
        require_count 3 'GuestIAR_refill=0x0000000000000029'
        require_count 3 'GuestIAR_refill=0x000000000000002a'
        require_count 3 'GuestIAR_refill=0x000000000000002b'
        require_count 3 'GuestIAR_refill=0x000000000000002c'
        require_count 10 'GuestRPR_refill=0x00000000000000ff'
        require_count 3 'GuestHPPIR_refill=0x0000000000000029'
        require_count 3 'GuestHPPIR_refill=0x000000000000002a'
        require_count 3 'GuestHPPIR_refill=0x000000000000002b'
        require_count 3 'GuestHPPIR_refill=0x000000000000002c'
        require_count 3 'GuestHPPIR_refill=0x00000000000003ff'

        require_line 'GuestIRQState_delivery0_active=0x0000292000000028'
        require_snapshot delivery0_active 0x0000000022000028 \
            0x0000000014000029 0x000000001600002a \
            0x000000001800002b 0x0000000000000010 \
            0x0000000000000000
        require_line 'GuestIRQState_delivery0_drop=0x000029ff00000028'
        require_snapshot delivery0_drop 0x0000000022000028 \
            0x0000000014000029 0x000000001600002a \
            0x000000001800002b 0x0000000000000000 \
            0x0000000000000000
        require_line 'GuestIRQState_delivery0_deactivated=0x000029ff00000028'
        require_snapshot delivery0_deactivated 0x0000000002000028 \
            0x0000000014000029 0x000000001600002a \
            0x000000001800002b 0x0000000000000000 \
            0x0000000000000001
        require_once '[gicv2-lab] freed LR0 refilled from EL2 queue'
        require_line 'EL2SoftwareQueue_after_refill=0x0000000000000000'
        require_snapshot refilled 0x000000001a00002c \
            0x0000000014000029 0x000000001600002a \
            0x000000001800002b 0x0000000000000000 \
            0x0000000000000000

        require_line 'GuestIRQState_delivery1_active=0x00002a4000000029'
        require_snapshot delivery1_active 0x000000001a00002c \
            0x0000000024000029 0x000000001600002a \
            0x000000001800002b 0x0000000000000100 \
            0x0000000000000000
        require_line 'GuestIRQState_delivery1_drop=0x00002aff00000029'
        require_snapshot delivery1_drop 0x000000001a00002c \
            0x0000000024000029 0x000000001600002a \
            0x000000001800002b 0x0000000000000000 \
            0x0000000000000000
        require_line 'GuestIRQState_delivery1_deactivated=0x00002aff00000029'
        require_snapshot delivery1_deactivated 0x000000001a00002c \
            0x0000000004000029 0x000000001600002a \
            0x000000001800002b 0x0000000000000000 \
            0x0000000000000002

        require_line 'GuestIRQState_delivery2_active=0x00002b600000002a'
        require_snapshot delivery2_active 0x000000001a00002c \
            0x0000000004000029 0x000000002600002a \
            0x000000001800002b 0x0000000000001000 \
            0x0000000000000002
        require_line 'GuestIRQState_delivery2_drop=0x00002bff0000002a'
        require_snapshot delivery2_drop 0x000000001a00002c \
            0x0000000004000029 0x000000002600002a \
            0x000000001800002b 0x0000000000000000 \
            0x0000000000000002
        require_line 'GuestIRQState_delivery2_deactivated=0x00002bff0000002a'
        require_snapshot delivery2_deactivated 0x000000001a00002c \
            0x0000000004000029 0x000000000600002a \
            0x000000001800002b 0x0000000000000000 \
            0x0000000000000006

        require_line 'GuestIRQState_delivery3_active=0x00002c800000002b'
        require_snapshot delivery3_active 0x000000001a00002c \
            0x0000000004000029 0x000000000600002a \
            0x000000002800002b 0x0000000000010000 \
            0x0000000000000006
        require_line 'GuestIRQState_delivery3_drop=0x00002cff0000002b'
        require_snapshot delivery3_drop 0x000000001a00002c \
            0x0000000004000029 0x000000000600002a \
            0x000000002800002b 0x0000000000000000 \
            0x0000000000000006
        require_line 'GuestIRQState_delivery3_deactivated=0x00002cff0000002b'
        require_snapshot delivery3_deactivated 0x000000001a00002c \
            0x0000000004000029 0x000000000600002a \
            0x000000000800002b 0x0000000000000000 \
            0x000000000000000e

        require_line 'GuestIRQState_delivery4_active=0x0003ffa00000002c'
        require_snapshot delivery4_active 0x000000002a00002c \
            0x0000000004000029 0x000000000600002a \
            0x000000000800002b 0x0000000000100000 \
            0x000000000000000e
        require_line 'GuestIRQState_delivery4_drop=0x0003ffff0000002c'
        require_snapshot delivery4_drop 0x000000002a00002c \
            0x0000000004000029 0x000000000600002a \
            0x000000000800002b 0x0000000000000000 \
            0x000000000000000e
        require_line 'GuestIRQState_delivery4_deactivated=0x0003ffff0000002c'
        require_snapshot delivery4_deactivated 0x000000000a00002c \
            0x0000000004000029 0x000000000600002a \
            0x000000000800002b 0x0000000000000000 \
            0x000000000000000f
        require_once '[gicv2-lab] exhausted LR set restored'
        require_snapshot cleared 0x0000000000000000 \
            0x0000000000000000 0x0000000000000000 \
            0x0000000000000000 0x0000000000000000 \
            0x000000000000000f
        reject_line '[gicv2-lab] H3 FAIL:'
        reject_line '[gicv2-lab] H4a FAIL:'
        reject_line '[gicv2-lab] H4b FAIL:'
        reject_line '[gicv2-lab] H4c FAIL:'
        reject_line '[gicv2-lab] H4d FAIL:'
        reject_line '[gicv2-lab] H4e FAIL:'
        reject_line '[gicv2-lab] H4f FAIL:'
        reject_line '[gicv2-lab] H4g FAIL:'
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
echo "gicv2-lab: QEMU smoke test did not observe the H4g marker" >&2
exit 1
