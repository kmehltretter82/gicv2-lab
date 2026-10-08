set -eu
session=/home/karl/gicv2-lab-20260905-hardware
cd "$session"
for workload in split-eoi priority redelivery; do
    bundle="$session/h8/$workload/bundle"
    for run in $(seq 2 20); do
        printf -v tag '%03d' "$run"
        sudo -n runuser -u karl -g kvm -- python3 -B "$bundle/source/tools/h7_runner.py" capture-kvm --bundle "$bundle" --unit-tests "$session/h7-existing-tests" --out "$session/h8-kvm/$workload/run-$tag"
        python3 -B "$bundle/source/tools/h5_runner.py" compare --scenario "$bundle/scenario.json" --left "$session/h8/$workload/qemu/run-001/normalized.json" --right "$session/h8-kvm/$workload/run-$tag/normalized.json" --out "$session/h8-kvm/$workload/run-$tag/comparison-qemu.json"
        printf 'H8 %s KVM run %s passed and matched QEMU\n' "$workload" "$tag"
    done
done
tar -cJf h8-kvm-evidence.tar.xz h8-kvm
sha256sum h8-kvm-evidence.tar.xz
df -h .
