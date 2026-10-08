set -eu
session=/home/karl/gicv2-lab-20260905-hardware
cd "$session"
export PYTHONDONTWRITEBYTECODE=1
du -sh h7-existing-tests h7-kvm
df -h .
for run in $(seq 2 20); do
    printf -v tag '%03d' "$run"
    sudo -n runuser -u karl -g kvm -- python3 h7/bundle/source/tools/h7_runner.py capture-kvm --bundle "$session/h7/bundle" --unit-tests "$session/h7-existing-tests" --out "$session/h7-kvm/run-$tag"
    python3 h7/bundle/source/tools/h5_runner.py compare --scenario h7/bundle/scenario.json --left h7/qemu/run-001/normalized.json --right "h7-kvm/run-$tag/normalized.json" --out "h7-kvm/run-$tag/comparison-qemu.json"
    printf 'H7 KVM run %s passed and matched QEMU\n' "$tag"
done
tar -czf h7-kvm-evidence.tar.gz h7-existing-tests h7-kvm h7-native-abi-check.log
sha256sum h7-kvm-evidence.tar.gz
