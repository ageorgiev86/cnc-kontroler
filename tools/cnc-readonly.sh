#!/bin/bash
# Система само за четене на Orange Pi (Armbian): спирането на тока не поврежда системата.
# Copyright (c) 2026 ageorgiev86. Всички права запазени.
#
#   sudo bash tools/cnc-readonly.sh --install   слага защитата (еднократно), после: sudo reboot
#   sudo bash tools/cnc-readonly.sh --off       изключва я от следващия старт (за apt и промени по системата)
#   sudo bash tools/cnc-readonly.sh --on        включва я отново от следващия старт
#   bash tools/cnc-readonly.sh                  показва състоянието
#
# Как работи (docs/arhitektura.md, раздел 6):
#   - при старта initramfs слага над картата слой в RAM (overlayfs): всичко, което системата пише,
#     отива в паметта и изчезва при изключване; картата остава непроменена;
#   - /home остава записваема (хранилището, компилираното, настройките, програмите):
#     git pull и cmake --build работят както досега;
#   - записите в /home се пращат на картата до 5 s (commit=5), а не до 10 мин, както е в Armbian;
#   - ако нещо не мине при старта, системата тръгва нормално (без защита), а не спира.
set -u

HOOK=/etc/initramfs-tools/scripts/init-bottom/cnc-overlay
UNIT=/etc/systemd/system/cnc-ro-home.service
FLAG=cncro=1

ro_now() { [ "$(findmnt -n -o FSTYPE /)" = "overlay" ] && mountpoint -q /media/root-ro; }

# armbianEnv.txt е на картата: в режим само за четене се пише през /media/root-ro
env_edit() {   # $1 = on | off
  local base="" envf
  if ro_now; then
    base=/media/root-ro
    mount -o remount,bind,rw /media/root-ro || { echo "ГРЕШКА: картата не може да се отвори за запис"; return 1; }
  fi
  envf="$base/boot/armbianEnv.txt"
  if [ ! -f "$envf" ]; then
    echo "ГРЕШКА: няма $envf"
    [ -n "$base" ] && mount -o remount,bind,ro /media/root-ro
    return 1
  fi
  [ -f "$envf.cnc-ro-backup" ] || cp "$envf" "$envf.cnc-ro-backup"
  if ! grep -q '^extraargs=' "$envf"; then echo "extraargs=" >> "$envf"; fi
  sed -i "s/ *\b$FLAG\b//g" "$envf"
  [ "$1" = on ] && sed -i "s|^extraargs=.*|& $FLAG|" "$envf"
  sed -i 's/^extraargs= */extraargs=/' "$envf"
  sync
  [ -n "$base" ] && mount -o remount,bind,ro /media/root-ro
  echo "ОК: $(grep '^extraargs=' "$envf")"
  return 0
}

status() {
  if ro_now; then echo "Сега: САМО ЗА ЧЕТЕНЕ (системата е защитена, /home се записва)"
  else echo "Сега: нормален режим (системата се записва на картата)"; fi
  local envf=/boot/armbianEnv.txt
  ro_now && envf=/media/root-ro/boot/armbianEnv.txt
  if grep -q "^extraargs=.*\b$FLAG\b" "$envf" 2>/dev/null; then echo "От следващия старт: само за четене"
  else echo "От следващия старт: нормален режим"; fi
  [ -x "$HOOK" ] || echo "(защитата не е инсталирана: sudo bash $0 --install)"
}

need_root() {
  if [ "$(id -u)" -ne 0 ]; then echo "Пуснете скрипта със sudo:  sudo bash $0 $*"; exit 1; fi
}

install_ro() {
  if [ ! -f /boot/armbianEnv.txt ] && ! ro_now; then
    echo "Няма /boot/armbianEnv.txt: това не е Armbian."
    echo "На Raspberry Pi OS защитата е вградена: sudo raspi-config → Performance Options → Overlay File System."
    exit 1
  fi
  if ro_now; then echo "Защитата е включена в момента. Първо: sudo bash $0 --off и рестарт."; exit 1; fi
  command -v update-initramfs >/dev/null || { echo "ГРЕШКА: няма update-initramfs (пакет initramfs-tools)"; exit 1; }
  [ -f /boot/uInitrd ] || { echo "ГРЕШКА: няма /boot/uInitrd: системата не тръгва с initramfs"; exit 1; }
  if ! grep -qw overlay /proc/filesystems && ! modprobe overlay 2>/dev/null; then
    echo "ГРЕШКА: ядрото няма overlayfs"; exit 1
  fi

  echo "1. Скрипт в initramfs: $HOOK"
  cat > "$HOOK" <<'EOF'
#!/bin/sh
# CNC: слой в RAM над картата (tools/cnc-readonly.sh). При всяка грешка: нормален старт.
case "$1" in prereqs) exit 0 ;; esac
# само вградени команди на shell + mount/mkdir/cat (в initramfs може да няма grep)
on=0
for x in $(cat /proc/cmdline); do [ "$x" = cncro=1 ] && on=1; done
[ $on = 1 ] || exit 0
msg() { echo "cnc-overlay: $*" > /dev/kmsg 2>/dev/null || echo "cnc-overlay: $*"; }
modprobe overlay 2>/dev/null
ovl=0
while read -r a b; do [ "$a" = overlay ] || [ "$b" = overlay ] && ovl=1; done < /proc/filesystems
[ $ovl = 1 ] || { msg "няма overlayfs, нормален старт"; exit 0; }
mkdir -p /cnc-lower /cnc-rw
mount -t tmpfs -o mode=0755,size=25% cnc-rw /cnc-rw || { msg "няма tmpfs, нормален старт"; exit 0; }
mkdir -p /cnc-rw/upper /cnc-rw/work
if ! mount -n -o move "${rootmnt}" /cnc-lower; then umount /cnc-rw; msg "нормален старт"; exit 0; fi
# картата се записва само през /home (виж cnc-ro-home.service); commit=5 вместо 600
if ! mount -o remount,rw,noatime,commit=5 /cnc-lower; then
  mount -n -o move /cnc-lower "${rootmnt}"; umount /cnc-rw; msg "картата не е за запис, нормален старт"; exit 0
fi
if ! mount -t overlay -o lowerdir=/cnc-lower,upperdir=/cnc-rw/upper,workdir=/cnc-rw/work cnc-overlay "${rootmnt}"; then
  mount -n -o move /cnc-lower "${rootmnt}"; umount /cnc-rw; msg "overlay не мина, нормален старт"; exit 0
fi
mkdir -p "${rootmnt}/media/root-ro" "${rootmnt}/media/root-rw"
mount -n -o move /cnc-lower "${rootmnt}/media/root-ro"
mount -n -o move /cnc-rw "${rootmnt}/media/root-rw"
# fstab в RAM без реда за / (иначе systemd се опитва да провери и премонтира картата)
if [ -f "${rootmnt}/media/root-ro/etc/fstab" ]; then
  while IFS= read -r l; do
    case "$l" in
      \#*|"") echo "$l" ;;
      *) set -- $l; if [ "$2" = "/" ]; then echo "#cnc-ro $l"; else echo "$l"; fi ;;
    esac
  done < "${rootmnt}/media/root-ro/etc/fstab" > "${rootmnt}/etc/fstab"
fi
msg "системата е само за четене"
exit 0
EOF
  chmod 755 "$HOOK"
  grep -qx overlay /etc/initramfs-tools/modules 2>/dev/null || echo overlay >> /etc/initramfs-tools/modules

  echo "2. /home остава записваема: $UNIT"
  cat > "$UNIT" <<'EOF'
[Unit]
Description=CNC: /home се записва на картата при система само за четене
DefaultDependencies=no
ConditionPathIsMountPoint=/media/root-ro
After=local-fs-pre.target
Before=local-fs.target

[Service]
Type=oneshot
RemainAfterExit=yes
ExecStart=/bin/mount --bind /media/root-ro/home /home
ExecStart=/bin/mount -o remount,bind,ro /media/root-ro

[Install]
WantedBy=local-fs.target
EOF
  systemctl daemon-reload
  systemctl enable cnc-ro-home.service >/dev/null 2>&1 && echo "ОК: услугата е включена"

  echo "3. Нов initramfs (около минута)"
  local before
  before=$(stat -c %Y /boot/uInitrd)
  if ! update-initramfs -u >/dev/null; then echo "ГРЕШКА: update-initramfs не мина"; exit 1; fi
  if [ "$(stat -c %Y /boot/uInitrd)" = "$before" ]; then
    echo "ГРЕШКА: /boot/uInitrd не е обновен. Защитата НЕ е включена."; exit 1
  fi
  if command -v lsinitramfs >/dev/null && ! lsinitramfs "/boot/initrd.img-$(uname -r)" 2>/dev/null | grep -q cnc-overlay; then
    echo "ГРЕШКА: скриптът не е влязъл в initramfs. Защитата НЕ е включена."; exit 1
  fi
  echo "ОК: /boot/uInitrd е обновен"

  echo "4. Включване от следващия старт"
  env_edit on || exit 1
  echo
  echo "Готово. Рестартирайте:  sudo reboot"
  echo "След това проверете:    bash $0"
}

case "${1:-}" in
  --install) need_root "$@"; install_ro ;;
  --on)  need_root "$@"; [ -x "$HOOK" ] || { echo "Първо: sudo bash $0 --install"; exit 1; }; env_edit on && echo "Рестартирайте: sudo reboot" ;;
  --off) need_root "$@"; env_edit off && echo "Рестартирайте: sudo reboot" ;;
  ""|--status) status ;;
  *) echo "Употреба: sudo bash $0 --install | --on | --off   (без опция: състояние)"; exit 2 ;;
esac
