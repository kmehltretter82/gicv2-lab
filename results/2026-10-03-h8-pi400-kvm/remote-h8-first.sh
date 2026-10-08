set -eu
session=/home/karl/gicv2-lab-20260905-hardware
cd "$session"
printf '%s\n' 'ea9fbac61d3a12162dfb3928dc6649b2450477540873c045cd7b67174ae43f79  h8-qemu-campaign.tar.xz' | sha256sum -c -
test ! -e h8
tar -xJf h8-qemu-campaign.tar.xz
for workload in split-eoi priority redelivery; do
    bundle="$session/h8/$workload/bundle"
    python3 -B "$bundle/source/tools/h7_runner.py" verify --bundle "$bundle"
    sudo -n runuser -u karl -g kvm -- python3 -B "$bundle/source/tools/h7_runner.py" capture-kvm --bundle "$bundle" --unit-tests "$session/h7-existing-tests" --out "$session/h8-kvm/$workload/run-001"
    python3 -B "$bundle/source/tools/h5_runner.py" compare --scenario "$bundle/scenario.json" --left "$session/h8/$workload/qemu/run-001/normalized.json" --right "$session/h8-kvm/$workload/run-001/normalized.json" --out "$session/h8-kvm/$workload/run-001/comparison-qemu.json"
    cat "$session/h8-kvm/$workload/run-001/serial.log"
done
