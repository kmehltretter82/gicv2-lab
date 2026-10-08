set -eu
cd /home/karl/gicv2-lab-20260905-hardware
cmp boot-backup/config.txt /boot/firmware/config.txt
if cmp -s tryboot-h6.txt /boot/firmware/tryboot.txt; then
    sudo -n cp -p -- boot-backup/tryboot.txt /boot/firmware/tryboot.txt
elif cmp -s boot-backup/tryboot.txt /boot/firmware/tryboot.txt; then
    :
else
    printf '%s\n' 'Boot configuration changed externally; refusing to overwrite it.' >&2
    exit 2
fi
test -f boot-backup/image-absent
if test -e /boot/firmware/gicv2-lab-h6.bin; then
    cmp gicv2-lab.bin /boot/firmware/gicv2-lab-h6.bin
    sudo -n rm -- /boot/firmware/gicv2-lab-h6.bin
fi
sudo -n sync /boot/firmware
sha256sum -c boot-backup/SHA256SUMS
test ! -e /boot/firmware/gicv2-lab-h6.bin
hostname
uname -a
ip -br addr
uptime
pinctrl get 14,15
vcgencmd get_throttled
vcgencmd measure_temp
date -u
