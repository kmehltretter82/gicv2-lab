#!/bin/bash
# SPDX-License-Identifier: GPL-2.0-or-later
# Opt-in hardware execution of a gate prepared by tools/h6_gate.py.
# Usage: PI_HOST=user@board PORT=/dev/cu.serial h6-repeat.sh <total-runs> <gate>
set -euo pipefail

runs=${1:?usage: h6-repeat.sh <total-runs> <prepared-gate>}
gate=${2:?usage: h6-repeat.sh <total-runs> <prepared-gate>}
: "${PI_HOST:?set PI_HOST to the approved board}"
: "${PORT:?set PORT to its serial device}"
START_RUN=${START_RUN:-1}
TRACE_TIMEOUT=${TRACE_TIMEOUT:-120}
LINUX_TIMEOUT=${LINUX_TIMEOUT:-180}
REMOTE_IMAGE=${REMOTE_IMAGE:-/boot/firmware/gicv2-lab-h6.bin}
PYTHON=${PYTHON:-python3}
export PYTHONDONTWRITEBYTECODE=1

for value in "$runs" "$START_RUN" "$TRACE_TIMEOUT" "$LINUX_TIMEOUT"; do
    if [[ ! "$value" =~ ^[1-9][0-9]*$ ]]; then
        echo "ERROR: counts and timeouts must be positive integers" >&2
        exit 2
    fi
done
if (( START_RUN > runs )); then
    echo "ERROR: START_RUN exceeds the requested total" >&2
    exit 2
fi

script_dir=$(cd -- "$(dirname -- "$0")" && pwd)
helper="$script_dir/../tools/h6_gate.py"
gate=$(cd -- "$gate" && pwd)
command -v lsof >/dev/null
ssh_command=(ssh -o ConnectTimeout=10 -o BatchMode=yes "$PI_HOST")
remote_quoted=$("$PYTHON" -c 'import shlex,sys; print(shlex.quote(sys.argv[1]))' "$REMOTE_IMAGE")
image_check="sha256sum -- $remote_quoted"
reboot_command="sudo reboot '0 tryboot'"
locks=()
capture_pid=
active_run=

stop_capture() {
    if [[ -n "$capture_pid" ]]; then
        kill "$capture_pid" 2>/dev/null || true
        wait "$capture_pid" 2>/dev/null || true
        capture_pid=
    fi
}

cleanup() {
    local status=$?
    trap - EXIT INT TERM
    stop_capture
    if [[ -n "$active_run" ]]; then
        "$PYTHON" "$helper" finish --gate "$gate" --run "$active_run" \
            --status INTERRUPTED --detail "driver exited before completion (status $status)" || true
    fi
    for lock in "${locks[@]-}"; do
        [[ -n "$lock" ]] || continue
        rm -f -- "$lock/pid"
        rmdir -- "$lock"
    done
    exit "$status"
}
trap cleanup EXIT
trap 'exit 130' INT
trap 'exit 143' TERM

# One namespace per user, independent of the result directory. Tests override
# the root inside their temporary directory. Board aliases must be consistent.
lock_root=${H6_LOCK_ROOT:-/tmp/gicv2-lab-h6-$(id -u)}
( umask 077; mkdir -p -- "$lock_root" )
keys=$("$PYTHON" "$helper" resource-keys "$PI_HOST" "$PORT")
while IFS= read -r key; do
    lock="$lock_root/$key"
    if ! mkdir -- "$lock" 2>/dev/null; then
        echo "ERROR: board or serial port is locked: $lock" >&2
        exit 2
    fi
    locks+=("$lock")
    echo "$$" > "$lock/pid"
done <<< "$keys"
if ! mkdir -- "$gate/.gate.lock" 2>/dev/null; then
    echo "ERROR: gate is already locked: $gate" >&2
    exit 2
fi
locks+=("$gate/.gate.lock")
echo "$$" > "$gate/.gate.lock/pid"

finish_run() {
    local status=$1 detail=$2 result=0
    "$PYTHON" "$helper" finish --gate "$gate" --run "$active_run" \
        --status "$status" --detail "$detail" || result=$?
    active_run=
    return "$result"
}

for (( run=START_RUN; run<=runs; run++ )); do
    "$PYTHON" "$helper" start --gate "$gate" --run "$run" --runs "$runs" \
        --host "$PI_HOST" --port "$PORT" --remote-image "$REMOTE_IMAGE" \
        --trace-timeout "$TRACE_TIMEOUT" --linux-timeout "$LINUX_TIMEOUT"
    active_run=$run
    printf -v tag '%03d' "$run"
    directory="$gate/runs/$tag"
    # These are the exact command arguments, shell-quoted for later inspection.
    for command in true "$image_check" 'uname -a && cat /proc/cpuinfo && vcgencmd version' \
                   'cat /boot/firmware/tryboot.txt' "$reboot_command"; do
        printf '%q ' "${ssh_command[@]}" "$command" >> "$directory/commands.sh"
        printf '\n' >> "$directory/commands.sh"
    done
    printf 'stty 115200 cs8 -parenb -cstopb raw -echo; exec cat < %q > %q\n' \
        "$PORT" "$directory/serial.log" >> "$directory/commands.sh"

    # Never kill an unrelated reader. Its owner must release the port first.
    if [[ ! -c "$PORT" ]]; then
        finish_run FAIL "serial path is not a character device"
        exit 1
    fi
    busy=0
    lsof -t "$PORT" > "$directory/serial-holders.txt" 2> "$directory/lsof-stderr.log" || busy=$?
    if (( busy != 1 )); then
        finish_run FAIL "serial port occupied or lsof check failed"
        exit 1
    fi

    deadline=$((SECONDS + LINUX_TIMEOUT))
    reachable=0
    while (( SECONDS < deadline )); do
        if "${ssh_command[@]}" true >> "$directory/ssh.log" 2>&1; then reachable=1; break; fi
        sleep 3
    done
    if (( ! reachable )); then
        finish_run TIMEOUT "vendor kernel unreachable before boot"
        exit 1
    fi
    if ! "${ssh_command[@]}" "$image_check" > "$directory/remote-image.sha256" 2>> "$directory/ssh.log"; then
        finish_run FAIL "cannot verify deployed flat image"
        exit 1
    fi
    expected=$("$PYTHON" -c 'import json,sys; print(json.load(open(sys.argv[1]))["flat_image_sha256"])' "$gate/gate.json")
    read -r observed _ < "$directory/remote-image.sha256"
    if [[ "$observed" != "$expected" ]]; then
        finish_run FAIL "deployed flat image differs from frozen gate"
        exit 1
    fi
    if ! "${ssh_command[@]}" 'uname -a && cat /proc/cpuinfo && vcgencmd version' \
            > "$directory/board-environment.log" 2>> "$directory/ssh.log"; then
        finish_run FAIL "cannot capture board environment"
        exit 1
    fi
    if ! "${ssh_command[@]}" 'cat /boot/firmware/tryboot.txt' > "$directory/tryboot.txt" 2>> "$directory/ssh.log"; then
        finish_run FAIL "cannot capture boot configuration"
        exit 1
    fi
    if ! "$PYTHON" "$helper" check-deployment --gate "$gate" --run "$run" \
            > "$directory/deployment-check.log" 2>&1; then
        finish_run FAIL "boot configuration does not select the verified image"
        exit 1
    fi

    ( stty 115200 cs8 -parenb -cstopb raw -echo; exec cat ) \
        < "$PORT" > "$directory/serial.log" 2> "$directory/capture-stderr.log" &
    capture_pid=$!
    sleep 1
    if ! kill -0 "$capture_pid" 2>/dev/null; then
        stop_capture
        finish_run FAIL "serial reader exited before reboot"
        exit 1
    fi
    "${ssh_command[@]}" "$reboot_command" >> "$directory/ssh.log" 2>&1 || true

    deadline=$((SECONDS + TRACE_TIMEOUT))
    result=TIMEOUT
    while (( SECONDS < deadline )); do
        if grep -qa ' FAIL:' "$directory/serial.log"; then result=FAIL; break; fi
        if grep -qa '^\[gicv2-lab\] H4i PASS' "$directory/serial.log"; then result=PASS; break; fi
        if ! kill -0 "$capture_pid" 2>/dev/null; then result=FAIL; break; fi
        sleep 2
    done
    sleep 2
    stop_capture
    finish_run "$result" "serial capture finished; semantic comparison required for PASS"
done

"$PYTHON" "$helper" verify --gate "$gate"
