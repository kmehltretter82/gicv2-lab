set -eu
cd /home/karl/gicv2-lab-20260905-hardware
sha256sum -c boot-backup/SHA256SUMS
test -f boot-backup/image-absent
test ! -e /boot/firmware/gicv2-lab-h6.bin
printf '%s\n' 'bf78cbdcd4af2008d342a7c85da5040c6c36b4fe6bccf83b54cff4a7216a4950  gicv2-lab.bin' | sha256sum -c -
sudo -n install -m 755 gicv2-lab.bin /boot/firmware/gicv2-lab-h6.bin
sudo -n cp -- tryboot-h6.txt /boot/firmware/tryboot.txt
sudo -n sync /boot/firmware/tryboot.txt /boot/firmware/gicv2-lab-h6.bin
cmp gicv2-lab.bin /boot/firmware/gicv2-lab-h6.bin
cmp tryboot-h6.txt /boot/firmware/tryboot.txt
sha256sum /boot/firmware/config.txt /boot/firmware/tryboot.txt /boot/firmware/gicv2-lab-h6.bin
cat /boot/firmware/tryboot.txt
uname -a
date -u
