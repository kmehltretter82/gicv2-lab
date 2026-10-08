set -eu
session=/home/karl/gicv2-lab-20260905-hardware
cd "$session"
printf '%s\n' 'e6b22403d067612ecdb3e106945fe0533dfc3f6d2fc6974832f2ab774885ea28  qemu-campaign.tar.gz' 'bf78cbdcd4af2008d342a7c85da5040c6c36b4fe6bccf83b54cff4a7216a4950  gicv2-lab.bin' | sha256sum -c -
test ! -e h7
tar -xzf qemu-campaign.tar.gz
export PYTHONDONTWRITEBYTECODE=1
python3 h7/bundle/source/tools/h7_runner.py verify --bundle h7/bundle
cc -fsyntax-only -Wall -Wextra -Werror h7/bundle/source/h7/kvm_abi_check.c > h7-native-abi-check.log 2>&1
sudo -n runuser -u karl -g kvm -- python3 h7/bundle/source/tools/h7_runner.py unit-tests --checkout "$session/kvm-unit-tests" --compiler /usr/bin/gcc --qemu /usr/bin/qemu-system-aarch64 --out "$session/h7-existing-tests"
cat h7-existing-tests/unit-tests.log
sudo -n runuser -u karl -g kvm -- python3 h7/bundle/source/tools/h7_runner.py capture-kvm --bundle "$session/h7/bundle" --unit-tests "$session/h7-existing-tests" --out "$session/h7-kvm/run-001"
python3 h7/bundle/source/tools/h5_runner.py compare --scenario h7/bundle/scenario.json --left h7/qemu/run-001/normalized.json --right h7-kvm/run-001/normalized.json --out h7-kvm/run-001/comparison-qemu.json
cat h7-kvm/run-001/serial.log
