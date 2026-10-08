set -eu
session=/home/karl/gicv2-lab-20260905-hardware
sudo -n runuser -u karl -g kvm -- id
sudo -n runuser -u karl -g kvm -- test -w /dev/kvm
git clone --depth 1 https://gitlab.com/kvm-unit-tests/kvm-unit-tests.git "$session/kvm-unit-tests"
cd "$session/kvm-unit-tests"
git rev-parse HEAD
git status --short
./configure --arch=arm64
make -j2 arm/gic.flat
