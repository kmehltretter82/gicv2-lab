set -eu
session=/home/karl/gicv2-lab-20260905-hardware
out="$session/host-kernel"
test ! -e "$out"
mkdir "$out"
kernel_release=$(uname -r)
test "$kernel_release" = '6.18.39+rpt-rpi-v8'
uname -a > "$out/uname.txt"
cat /proc/version > "$out/proc-version.txt"
cat /proc/cmdline > "$out/proc-cmdline.txt"
cat /sys/kernel/notes > "$out/kernel-notes.bin"
date -u > "$out/collected-utc.txt"
dpkg-query -W -f='${binary:Package}\t${Version}\t${source:Package}\t${source:Version}\n' \
    "linux-image-$kernel_release" linux-libc-dev python3.13 qemu-system-arm > "$out/packages.tsv"
dpkg-query -s "linux-image-$kernel_release" > "$out/kernel-package.txt"
dpkg -V "linux-image-$kernel_release" > "$out/kernel-package-verification.txt"
cp -p /boot/firmware/kernel8.img "$out/kernel8.img"
sha256sum /boot/firmware/kernel8.img > "$out/boot-kernel.sha256"
if test -f "/boot/vmlinuz-$kernel_release"; then
    cmp /boot/firmware/kernel8.img "/boot/vmlinuz-$kernel_release"
    sha256sum "/boot/vmlinuz-$kernel_release" >> "$out/boot-kernel.sha256"
fi
if test -f "/boot/config-$kernel_release"; then
    cp -p "/boot/config-$kernel_release" "$out/kernel.config"
fi
cc --version > "$out/cc-version.txt"
python3 --version > "$out/python-version.txt"
vcgencmd version > "$out/firmware-version.txt"
cd "$session"
tar -cJf host-kernel-provenance.tar.xz host-kernel
sha256sum host-kernel-provenance.tar.xz
