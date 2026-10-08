set -u
session=/home/karl/gicv2-lab-20260905-hardware
cd "$session"
k=$(uname -r)
[ -e observe ] || tar -xJf h8-observe-diagnostic.tar.xz
ut="$session/h7-existing-tests"; case $k in 7.3*) ut="$session/h7-existing-tests-$k" ;; esac
for workload in split-eoi priority redelivery; do
    bundle="$session/observe/$workload/qemu/bundle"
    for run in 1 2 3 4 5; do
        out="$session/h8-observe-kvm-$k/$workload/run-00$run"
        sudo -n runuser -u karl -g kvm -- python3 -B "$bundle/source/tools/h7_runner.py" capture-kvm --bundle "$bundle" --unit-tests "$ut" --out "$out" >/dev/null 2>&1
        echo "observe $k $workload run $run rc=$? sha=$(sha256sum < "$out/serial.log" | cut -c1-16)"
    done
    echo "--- $workload ($k) run-001 full serial"; cat "$session/h8-observe-kvm-$k/$workload/run-001/serial.log"
done
tar -cJf "h8-observe-kvm-$k.tar.xz" "h8-observe-kvm-$k" && sha256sum "h8-observe-kvm-$k.tar.xz" && rm -rf "h8-observe-kvm-$k"
