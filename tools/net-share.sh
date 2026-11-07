#!/bin/bash
# Мрежов диск (SMB/CIFS: споделена папка в Windows или флашка на рутера) в /mnt/cnc-net.
# Интерфейсът показва програмите от него в Програми → Мрежа.
#
#   sudo bash tools/net-share.sh --list 192.168.1.1                 какви дялове има сървърът
#   sudo bash tools/net-share.sh //192.168.1.1/usb                  без парола (гост)
#   sudo bash tools/net-share.sh //192.168.1.1/usb потребител парола
#   sudo bash tools/net-share.sh --off                              изключва
#
# Дялът се монтира само за четене и се свързва при първото отваряне (x-systemd.automount),
# така че стартът не чака мрежата. Паролата се пази в /etc/cnc-net.cred (чете я само root).
set -u
MNT=/mnt/cnc-net
CRED=/etc/cnc-net.cred

if [ "$(id -u)" -ne 0 ]; then echo "Пуснете със sudo:  sudo bash $0 ..."; exit 1; fi
USR="${SUDO_USER:-root}"

if [ "${1:-}" = "--off" ]; then
  umount "$MNT" 2>/dev/null
  sed -i "\| $MNT cifs |d" /etc/fstab
  rm -f "$CRED"
  systemctl daemon-reload
  echo "Мрежовият диск е изключен."
  exit 0
fi

if [ "${1:-}" = "--list" ]; then
  command -v smbclient >/dev/null || apt-get install -y -qq smbclient >/dev/null
  echo "Дялове на ${2:?адрес на сървъра}:"
  smbclient -L "//$2" -N -g 2>/dev/null | awk -F'|' '$1=="Disk"{print "  //'"$2"'/"$2}' \
    || echo "  няма отговор без парола; пробвайте: smbclient -L //$2 -U потребител"
  exit 0
fi

SHARE="${1:-}"
if [ -z "$SHARE" ] || [ "${SHARE:0:2}" != "//" ]; then
  echo "Употреба: sudo bash $0 //сървър/дял [потребител] [парола]   (или --list сървър, --off)"
  exit 1
fi
NUSER="${2:-}"
NPASS="${3:-}"

command -v mount.cifs >/dev/null || { echo "Инсталиране на cifs-utils…"; apt-get install -y -qq cifs-utils >/dev/null; }
mkdir -p "$MNT"
umask 077
if [ -n "$NUSER" ]; then
  printf 'username=%s\npassword=%s\n' "$NUSER" "$NPASS" > "$CRED"
  AUTH="credentials=$CRED"
else
  rm -f "$CRED"
  AUTH="guest"
fi
umask 022
BASE="$AUTH,uid=$(id -u "$USR"),gid=$(id -g "$USR"),ro,iocharset=utf8,file_mode=0444,dir_mode=0555"

# коя версия на SMB поддържа сървърът (рутерите често са само със SMB 1.0)
umount "$MNT" 2>/dev/null
VER=""
for v in 3.0 2.1 2.0 1.0; do
  if mount -t cifs "$SHARE" "$MNT" -o "$BASE,vers=$v" 2>/dev/null; then VER=$v; break; fi
done
if [ -z "$VER" ]; then
  echo "Не може да се свърже с $SHARE."
  echo "Проверете адреса и името на дяла (sudo bash $0 --list АДРЕС), потребителя и паролата."
  echo "Подробно:  sudo mount -t cifs $SHARE $MNT -o $BASE,vers=2.0 ; dmesg | tail"
  exit 1
fi
N=$(find "$MNT" -maxdepth 3 -type f \( -iname '*.nc' -o -iname '*.ngc' -o -iname '*.gcode' -o -iname '*.tap' -o -iname '*.cnc' -o -iname '*.txt' \) 2>/dev/null | wc -l)
umount "$MNT"

sed -i "\| $MNT cifs |d" /etc/fstab
echo "$SHARE $MNT cifs $BASE,vers=$VER,nofail,_netdev,x-systemd.automount,x-systemd.idle-timeout=300 0 0" >> /etc/fstab
systemctl daemon-reload
systemctl restart remote-fs.target 2>/dev/null
echo "Готово: $SHARE (SMB $VER) е в $MNT, $N файла с програми."
echo "В интерфейса: Програми → Мрежа. Свързва се сам и след рестарт."
