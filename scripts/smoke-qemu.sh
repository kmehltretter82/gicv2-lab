#!/bin/sh
# SPDX-License-Identifier: GPL-2.0-or-later

set -eu

image=${1:?usage: smoke-qemu.sh IMAGE}
qemu=${QEMU:?set QEMU to qemu-system-aarch64}
log=build/smoke-qemu.log
marker='[gicv2-lab] H1 PASS'
deadline=$(( $(date +%s) + 10 ))
pid=

cleanup()
{
    if test -n "$pid" && kill -0 "$pid" 2>/dev/null; then
        kill "$pid" 2>/dev/null || true
    fi
    if test -n "$pid"; then
        wait "$pid" 2>/dev/null || true
    fi
}

trap cleanup EXIT INT TERM

rm -f "$log"
"$qemu" \
    -machine raspi400 \
    -cpu cortex-a72,has_el3=off \
    -kernel "$image" \
    -display none \
    -monitor none \
    -serial "file:$log" \
    -no-reboot &
pid=$!

while test "$(date +%s)" -lt "$deadline"; do
    if test -f "$log" && grep -Fq "$marker" "$log"; then
        cat "$log"
        exit 0
    fi
    if ! kill -0 "$pid" 2>/dev/null; then
        break
    fi
    sleep 1
done

test -f "$log" && cat "$log"
echo "gicv2-lab: QEMU smoke test did not observe the H1 marker" >&2
exit 1
