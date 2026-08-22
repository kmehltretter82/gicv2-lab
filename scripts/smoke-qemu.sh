#!/bin/sh
# SPDX-License-Identifier: GPL-2.0-or-later

set -eu

image=${1:?usage: smoke-qemu.sh IMAGE}
qemu=${QEMU:?set QEMU to qemu-system-aarch64}
log=build/smoke-qemu.log
stderr_log=build/smoke-qemu.stderr
marker='[gicv2-lab] H4b PASS'
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

reject_line()
{
    forbidden=$1

    if grep -Fq "$forbidden" "$log"; then
        echo "gicv2-lab: forbidden smoke-test line: $forbidden" >&2
        return 1
    fi
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
        require_line '[gicv2-lab] H4b EL2 monitor'
        require_line 'CurrentEL=2'
        require_line 'GICH_LRS=4'
        require_line '[gicv2-lab] H1 PASS'
        require_line 'stage2_guest_desc=0x00000000002007fd'
        require_line 'stage2_gicv_desc=0x00400000ff8467c7'
        require_line 'VTCR_EL2_enabled=0x0000000080003560'
        require_line 'HCR_EL2_enabled=0x0000000080000011'
        require_line 'GICC_PMR_enabled=0x00000000000000ff'
        require_line 'GICH_HCR_enabled=0x0000000000000001'
        require_line 'GICH_VMCR_enabled=0x00000000f8fc0001'
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
        require_line '[gicv2-lab] H2 PASS'
        require_line 'GuestGICV_CTLR=0x0000000000000001'
        require_line 'GuestGICV_PMR=0x00000000000000f8'
        require_line 'GICH_HCR_pending=0x0000000000000003'
        require_line 'GICH_VMCR_pending=0x00000000f8fc0001'
        require_line 'GICH_MISR_pending=0x0000000000000000'
        require_line 'GICH_EISR0_pending=0x0000000000000000'
        require_line 'GICH_EISR1_pending=0x0000000000000000'
        require_line 'GICH_ELRSR0_pending=0x000000000000000c'
        require_line 'GICH_ELRSR1_pending=0x0000000000000000'
        require_line 'GICH_APR_pending=0x0000000000000000'
        require_line 'GICH_LR0_injected=0x000000001200002a'
        require_line 'GICH_LR1_injected=0x000000001f80002b'
        require_line '[gicv2-lab] LR0 active with LR1 reserve'
        require_line 'GuestIAR_active=42'
        require_line 'GICH_HCR_active=0x0000000000000003'
        require_line 'GICH_VMCR_active=0x00000000f8fc0001'
        require_line 'GICH_MISR_active=0x0000000000000000'
        require_line 'GICH_EISR0_active=0x0000000000000000'
        require_line 'GICH_EISR1_active=0x0000000000000000'
        require_line 'GICH_ELRSR0_active=0x000000000000000c'
        require_line 'GICH_ELRSR1_active=0x0000000000000000'
        require_line 'GICH_APR_active=0x0000000000000001'
        require_line 'GICH_LR0_active=0x000000002200002a'
        require_line 'GICH_LR1_active=0x000000001f80002b'
        require_line 'GuestIAR=42'
        require_line '[gicv2-lab] GIC underflow maintenance interrupt'
        require_line 'maintenance_vector_slot=9'
        require_line 'PhysicalIAR=25'
        require_line 'GICH_HCR_maintenance=0x0000000000000003'
        require_line 'GICH_VMCR_maintenance=0x00000000f8fc0001'
        require_line 'GICH_MISR=0x0000000000000002'
        require_line 'GICH_EISR0=0x0000000000000000'
        require_line 'GICH_EISR1=0x0000000000000000'
        require_line 'GICH_ELRSR0=0x000000000000000d'
        require_line 'GICH_ELRSR1=0x0000000000000000'
        require_line 'GICH_APR=0x0000000000000000'
        require_line 'GICH_LR0_post_eoi=0x000000000200002a'
        require_line 'GICH_LR1_remaining=0x000000001f80002b'
        require_line 'GICH_HCR_cleared=0x0000000000000001'
        require_line 'GICH_VMCR_cleared=0x00000000f8fc0001'
        require_line 'GICH_MISR_cleared=0x0000000000000000'
        require_line 'GICH_EISR0_cleared=0x0000000000000000'
        require_line 'GICH_EISR1_cleared=0x0000000000000000'
        require_line 'GICH_ELRSR0_cleared=0x000000000000000f'
        require_line 'GICH_ELRSR1_cleared=0x0000000000000000'
        require_line 'GICH_APR_cleared=0x0000000000000000'
        require_line 'GICH_LR0_cleared=0x0000000000000000'
        require_line 'GICH_LR1_cleared=0x0000000000000000'
        require_line '[gicv2-lab] underflow maintenance PPI 25 acknowledged'
        reject_line '[gicv2-lab] H3 FAIL:'
        reject_line '[gicv2-lab] H4a FAIL:'
        reject_line '[gicv2-lab] H4b FAIL:'
        exit 0
    fi
    if ! kill -0 "$pid" 2>/dev/null; then
        break
    fi
    sleep "$poll_interval"
done

test -f "$log" && cat "$log"
test ! -s "$stderr_log" || cat "$stderr_log" >&2
echo "gicv2-lab: QEMU smoke test did not observe the H4b marker" >&2
exit 1
