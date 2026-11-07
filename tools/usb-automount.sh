#!/bin/bash
# Автоматично монтиране на USB флашки в /media/usb-<устройство> (Armbian Minimal няма автомонтиране).
# Интерфейсът ги показва в Програми → USB.
#
#   sudo bash tools/usb-automount.sh          включва
#   sudo bash tools/usb-automount.sh --off    изключва
#
# Флашката се монтира само за четене (безопасно при издърпване). FAT32 и exFAT получават собственик
# потребителя, който пуска скрипта; ext4 се чете с правата от самата флашка.
set -u

if [ "$(id -u)" -ne 0 ]; then
  echo "Пуснете скрипта със sudo:  sudo bash $0"
  exit 1
fi
USR="${SUDO_USER:-root}"
UID_N=$(id -u "$USR")
GID_N=$(id -g "$USR")
RULE=/etc/udev/rules.d/99-cnc-usb-automount.rules

if [ "${1:-}" = "--off" ]; then
  rm -f "$RULE"
  udevadm control --reload-rules
  echo "Автоматичното монтиране е изключено."
  exit 0
fi

MOUNT=$(command -v systemd-mount || echo /usr/bin/systemd-mount)
cat > "$RULE" <<EOF
# CNC: USB флашките се монтират в /media/usb-<устройство> само за четене (tools/usb-automount.sh)
ACTION=="add", SUBSYSTEMS=="usb", KERNEL=="sd[a-z]*", ENV{ID_FS_USAGE}=="filesystem", ENV{ID_FS_TYPE}=="vfat|exfat", RUN{program}+="$MOUNT --no-block --automount=yes --collect --options=ro,uid=$UID_N,gid=$GID_N,umask=022 \$devnode /media/usb-%k"
ACTION=="add", SUBSYSTEMS=="usb", KERNEL=="sd[a-z]*", ENV{ID_FS_USAGE}=="filesystem", ENV{ID_FS_TYPE}!="vfat|exfat", RUN{program}+="$MOUNT --no-block --automount=yes --collect --options=ro \$devnode /media/usb-%k"
ACTION=="remove", SUBSYSTEMS=="usb", KERNEL=="sd[a-z]*", RUN{program}+="$MOUNT --umount \$devnode"
EOF

udevadm control --reload-rules
# флашки, които вече са поставени
udevadm trigger --action=add --subsystem-match=block
sleep 2
echo "Готово. Поставените флашки:"
found=0
for d in /media/usb-*; do
  [ -d "$d" ] || continue
  found=1
  n=$(find "$d" -maxdepth 3 -type f \( -iname '*.nc' -o -iname '*.ngc' -o -iname '*.gcode' -o -iname '*.tap' -o -iname '*.cnc' -o -iname '*.txt' \) 2>/dev/null | wc -l)
  echo "  $d: $n файла с програми"
done
[ $found = 1 ] || echo "  няма (поставете флашка; FAT32 и exFAT се четат директно)"
lsblk -o NAME,FSTYPE,SIZE,MOUNTPOINT | grep -E 'sd[a-z]' || echo "  (lsblk не вижда USB диск: проверете флашката в друг порт)"
