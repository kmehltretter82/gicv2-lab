set -u
session=/home/karl/gicv2-lab-20260905-hardware
cd "$session"
k=$(uname -r)
case $k in 7.3.0-rc3-*) ;; *) echo "wrong kernel $k"; exit 1 ;; esac
ut="$session/h7-existing-tests-$k"
h7="$session/h8/split-eoi/bundle/source/tools/h7_runner.py"
if [ ! -e "$ut" ]; then
    sudo -n runuser -u karl -g kvm -- python3 -B "$h7" unit-tests --checkout "$session/kvm-unit-tests" --compiler /usr/bin/gcc --qemu /usr/bin/qemu-system-aarch64 --out "$ut" >/dev/null 2>&1
    echo "unit-tests rc=$?"; tail -25 "$ut/unit-tests.log"
fi
for variant in level edge; do
  for workload in split-eoi priority redelivery; do
    if [ $variant = level ]; then bundle="$session/h8/$workload/bundle"; base="$session/h8/$workload/qemu/run-001/normalized.json"
    else bundle="$session/edge/$workload/qemu/bundle"; base="$session/edge/$workload/qemu/qemu/run-001/normalized.json"; fi
    for run in $(seq 1 10); do
        printf -v tag '%03d' "$run"
        out="$session/h8-kvm-$k/$variant/$workload/run-$tag"
        sudo -n runuser -u karl -g kvm -- python3 -B "$bundle/source/tools/h7_runner.py" capture-kvm --bundle "$bundle" --unit-tests "$ut" --out "$out" >/dev/null 2>&1
        crc=$?
        cmp=skipped
        if [ -e "$out/normalized.json" ]; then
            python3 -B "$bundle/source/tools/h5_runner.py" compare --scenario "$bundle/scenario.json" --left "$base" --right "$out/normalized.json" --out "$out/comparison-qemu.json" >/dev/null 2>&1
            cmp=$?
        fi
        printf 'H8 %s %s %s run %s capture_rc=%s compare_rc=%s serial_sha256=%s\n' "$k" "$variant" "$workload" "$tag" "$crc" "$cmp" "$(sha256sum < "$out/serial.log" | cut -c1-16)"
    done
    echo "--- $variant $workload run-001 serial tail"; tail -6 "$out/../run-001/serial.log"
  done
done
tar -cJf "h8-kvm-$k-evidence.tar.xz" "h8-kvm-$k" "h7-existing-tests-$k" && sha256sum "h8-kvm-$k-evidence.tar.xz"
uname -a; df -h / | tail -1
