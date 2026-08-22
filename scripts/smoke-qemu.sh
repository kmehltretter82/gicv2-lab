#!/bin/sh
# SPDX-License-Identifier: GPL-2.0-or-later

set -eu

image=${1:?usage: smoke-qemu.sh IMAGE}
qemu=${QEMU:?set QEMU to qemu-system-aarch64}
log=build/smoke-qemu.log
stderr_log=build/smoke-qemu.stderr
marker='[gicv2-lab] H2 PASS'
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
        require_line 'CurrentEL=2'
        require_line 'GICH_LRS=4'
        require_line '[gicv2-lab] H1 PASS'
        require_line 'stage2_guest_desc=0x00000000002007fd'
        require_line 'VTCR_EL2_enabled=0x0000000080003560'
        require_line 'HCR_EL2_enabled=0x0000000080000001'
        require_line 'GuestCurrentEL=1'
        require_line '[gicv2-lab] expected stage-2 translation fault'
        require_line 'FAR_EL2=0x0000000000080000'
        require_line 'HPFAR_EL2=0x0000000000000800'
        require_line 'stage2_fsc=6'
        require_line '[gicv2-lab] EL1 guest resumed after stage-2 fault'
        reject_line '[gicv2-lab] H2 FAIL:'
        exit 0
    fi
    if ! kill -0 "$pid" 2>/dev/null; then
        break
    fi
    sleep "$poll_interval"
done

test -f "$log" && cat "$log"
test ! -s "$stderr_log" || cat "$stderr_log" >&2
echo "gicv2-lab: QEMU smoke test did not observe the H2 marker" >&2
exit 1
