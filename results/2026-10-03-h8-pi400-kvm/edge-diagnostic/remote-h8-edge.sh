set -u
session=/home/karl/gicv2-lab-20260905-hardware
cd "$session"
test ! -e edge || exit 1
tar -xJf h8-edge-diagnostic.tar.xz
for workload in split-eoi priority redelivery; do
    bundle="$session/edge/$workload/qemu/bundle"
    python3 -B "$bundle/source/tools/h7_runner.py" verify --bundle "$bundle" >/dev/null || exit 1
    for run in $(seq 1 20); do
        printf -v tag '%03d' "$run"
        out="$session/h8-edge-kvm/$workload/run-$tag"
        sudo -n runuser -u karl -g kvm -- python3 -B "$bundle/source/tools/h7_runner.py" capture-kvm --bundle "$bundle" --unit-tests "$session/h7-existing-tests" --out "$out" >/dev/null 2>&1
        crc=$?
        cmp=skipped
        if [ -e "$out/normalized.json" ]; then
            python3 -B "$bundle/source/tools/h5_runner.py" compare --scenario "$bundle/scenario.json" --left "$session/edge/$workload/qemu/qemu/run-001/normalized.json" --right "$out/normalized.json" --out "$out/comparison-qemu.json" >/dev/null 2>&1
            cmp=$?
        fi
        printf 'H8-edge %s KVM run %s capture_rc=%s compare_rc=%s serial_sha256=%s\n' "$workload" "$tag" "$crc" "$cmp" "$(sha256sum < "$out/serial.log" | cut -c1-16)"
    done
    echo "--- $workload run-001 serial"; cat "$out/../run-001/serial.log"
done
tar -cJf h8-edge-kvm-evidence.tar.xz h8-edge-kvm && sha256sum h8-edge-kvm-evidence.tar.xz
