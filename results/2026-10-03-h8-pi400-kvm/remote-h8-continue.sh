# Continuation after remote-h8-first.sh stopped at split-eoi run-001 (no PASS
# marker). Captures every workload without stopping at a failed capture.
set -u
session=/home/karl/gicv2-lab-20260905-hardware
cd "$session"
for workload in split-eoi priority redelivery; do
    bundle="$session/h8/$workload/bundle"
    python3 -B "$bundle/source/tools/h7_runner.py" verify --bundle "$bundle" >/dev/null || exit 1
    for run in $(seq 1 20); do
        printf -v tag '%03d' "$run"
        out="$session/h8-kvm/$workload/run-$tag"
        [ -e "$out" ] && continue
        sudo -n runuser -u karl -g kvm -- python3 -B "$bundle/source/tools/h7_runner.py" capture-kvm --bundle "$bundle" --unit-tests "$session/h7-existing-tests" --out "$out"
        crc=$?
        cmp=skipped
        if [ -e "$out/normalized.json" ]; then
            python3 -B "$bundle/source/tools/h5_runner.py" compare --scenario "$bundle/scenario.json" --left "$session/h8/$workload/qemu/run-001/normalized.json" --right "$out/normalized.json" --out "$out/comparison-qemu.json" >/dev/null 2>&1
            cmp=$?
        fi
        printf 'H8 %s KVM run %s capture_rc=%s compare_rc=%s serial_sha256=%s\n' "$workload" "$tag" "$crc" "$cmp" "$(sha256sum < "$out/serial.log" | cut -c1-16)"
    done
    echo "--- $workload run-001 serial"; cat "$session/h8-kvm/$workload/run-001/serial.log"
done
uname -a
