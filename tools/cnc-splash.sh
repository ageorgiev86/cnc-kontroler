#!/bin/bash
# Начален екран при старта на Orange Pi (Armbian): логото и лента за напредъка вместо текстовете.
# Copyright (c) 2026 ageorgiev86. Всички права запазени.
#
#   sudo bash tools/cnc-splash.sh            слага началния екран, после: sudo reboot
#   sudo bash tools/cnc-splash.sh --off      връща текстовете при старта
#
# Пуска се в нормален режим (не в „само за четене“): инсталира Plymouth с apt.
# Как работи:
#   - конзолата на Linux е само на серийния порт (console=serial): на екрана няма текстове;
#   - quiet, systemd.show_status=false: без съобщения от ядрото и от systemd;
#   - Plymouth с темата tools/splash/ang: логото и лента; лентата се движи по времето на предишния старт;
#   - cnc-ui спира Plymouth, без да изчиства екрана (--retain-splash), и интерфейсът се появява направо.
# Първите 1–2 s (U-Boot) остават до собствения образ от armbian/build.
set -u

if [ "$(id -u)" -ne 0 ]; then echo "Пуснете скрипта със sudo:  sudo bash $0 $*"; exit 1; fi
REPO=$(cd "$(dirname "$0")/.." && pwd)
THEME_SRC="$REPO/tools/splash/ang"
THEME_DIR=/usr/share/plymouth/themes/ang
ENVF=/boot/armbianEnv.txt
DROPIN=/etc/systemd/system/cnc-ui.service.d/splash.conf
OURS="quiet splash plymouth.ignore-serial-consoles systemd.show_status=false rd.udev.log_level=3 udev.log_priority=3"
ALSO="vt.global_cursor_default=0 consoleblank=0"   # от orangepi-setup.sh; при --off остават

if [ "$(findmnt -n -o FSTYPE /)" = "overlay" ]; then
  echo "Системата е в режим „само за четене“. Първо:"
  echo "  sudo bash $REPO/tools/cnc-readonly.sh --off && sudo reboot"
  echo "после пуснете този скрипт отново."
  exit 1
fi
if [ ! -f "$ENVF" ]; then
  echo "Няма $ENVF: това не е Armbian."
  echo "На Raspberry Pi: темата е в $THEME_SRC; настройва се с plymouth-set-default-theme (идва с CM4)."
  exit 1
fi

set_kv() {   # ключ=стойност в armbianEnv.txt
  if grep -q "^$1=" "$ENVF"; then sed -i "s|^$1=.*|$1=$2|" "$ENVF"; else echo "$1=$2" >> "$ENVF"; fi
}
add_arg() { grep -q '^extraargs=' "$ENVF" || echo "extraargs=" >> "$ENVF"
            grep -q "^extraargs=\(.* \)\?$1\( \|$\)" "$ENVF" || sed -i "s|^extraargs=.*|& $1|" "$ENVF"; }
del_arg() { sed -i "/^extraargs=/ s/\([= ]\)$1\( \|$\)/\1/g; /^extraargs=/ s/  */ /g; s/^extraargs= /extraargs=/; /^extraargs=/ s/ $//" "$ENVF"; }

if [ "${1:-}" = "--off" ]; then
  old=$(grep '^console=' "$ENVF.cnc-splash-backup" 2>/dev/null | cut -d= -f2)
  set_kv console "${old:-both}"
  for a in $OURS; do del_arg "$a"; done
  rm -f "$DROPIN"
  systemctl daemon-reload
  sync
  echo "ОК: $(grep -E '^(console|extraargs)=' "$ENVF" | tr '\n' ' ')"
  echo "Текстовете при старта са върнати. Рестартирайте: sudo reboot"
  exit 0
fi

echo "1. Plymouth (apt): на Orange Pi One отнема няколко минути, изходът е отдолу"
export DEBIAN_FRONTEND=noninteractive
if ! apt-get install -y plymouth plymouth-themes; then
  echo "ГРЕШКА: apt не мина (проверете мрежата)"; exit 1
fi

echo "2. Темата ANG systems"
[ -f "$THEME_SRC/ang.script" ] || { echo "ГРЕШКА: няма $THEME_SRC"; exit 1; }
rm -rf "$THEME_DIR"
cp -r "$THEME_SRC" "$THEME_DIR"
plymouth-set-default-theme ang || { echo "ГРЕШКА: темата не се избра"; exit 1; }

echo "3. Нов initramfs: 2–5 минути на Orange Pi One, не прекъсвайте"
before=$(stat -c %Y /boot/uInitrd 2>/dev/null || echo 0)
printf "   "
update-initramfs -u -v 2>&1 | awk 'NR % 40 == 0 { printf "."; fflush() } END { print " готово" }'   # точка = напредък
[ "${PIPESTATUS[0]}" -eq 0 ] || { echo "ГРЕШКА: update-initramfs не мина"; exit 1; }
[ "$(stat -c %Y /boot/uInitrd 2>/dev/null || echo 0)" != "$before" ] || echo "Внимание: /boot/uInitrd не е обновен (екранът ще се появи малко по-късно)"

echo "4. Без текстове при старта ($ENVF)"
[ -f "$ENVF.cnc-splash-backup" ] || cp "$ENVF" "$ENVF.cnc-splash-backup"
set_kv console serial
set_kv bootlogo false
for a in $OURS $ALSO; do add_arg "$a"; done
sync
echo "ОК: $(grep -E '^(console|extraargs)=' "$ENVF" | tr '\n' ' ')"

echo "5. Интерфейсът поема екрана без мигане"
mkdir -p "$(dirname "$DROPIN")"
cat > "$DROPIN" <<'EOF'
# tools/cnc-splash.sh: cnc-ui спира Plymouth, без да изчиства логото, и рисува направо върху него
[Unit]
After=plymouth-start.service
Before=plymouth-quit.service plymouth-quit-wait.service

[Service]
ExecStartPre=-+/usr/bin/plymouth quit --retain-splash
EOF
systemctl daemon-reload
echo "ОК"

echo
echo "Готово. Рестартирайте:  sudo reboot"
echo "Ако ползвате защитата от спиране на тока, след като видите логото:"
echo "  sudo bash $REPO/tools/cnc-readonly.sh --on && sudo reboot"
