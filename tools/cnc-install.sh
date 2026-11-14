#!/bin/bash
# Инсталиране на целия контролер на чист Armbian (Orange Pi) или Raspberry Pi OS Lite.
# Copyright (c) 2026 ageorgiev86. Всички права запазени.
#
# На чиста система, като обикновения потребител (не root), с интернет:
#   sudo apt-get update && sudo apt-get install -y git
#   git clone https://github.com/ageorgiev86/cnc-kontroler.git
#   cd cnc-kontroler
#   sudo bash tools/cnc-install.sh              # всичко, после: sudo reboot
#   sudo bash tools/cnc-install.sh --readonly   # и системата само за четене (за работа на машината)
#   sudo bash tools/cnc-install.sh --no-splash  # без началния екран с логото
#
# По ред (всяка стъпка е отделен скрипт в tools/ и може да се пусне и сама):
#   1. пакети, права за екрана и входа, конзолата   orangepi-setup.sh
#   2. библиотеките (LVGL) и компилиране: интерфейсът и ядрото (интерпретатор и планер)
#   3. мрежата от интерфейса                        cnc-net.sh --install
#   4. USB флашки                                   usb-automount.sh
#   5. интерфейсът при старт (systemd)               cnc-ui-autostart.sh
#   6. начален екран с логото                       cnc-splash.sh
#   7. (по избор) система само за четене            cnc-readonly.sh --install
# Може да се пуска многократно: компилира само промененото и не повтаря направеното.
# Мрежовият диск се добавя отделно (иска адрес и парола): tools/net-share.sh.
set -u
export DEBIAN_FRONTEND=noninteractive

REPO=$(cd "$(dirname "$0")/.." && pwd)
RO=0 SPLASH=1
for a in "$@"; do
  case "$a" in
    --readonly) RO=1 ;;
    --no-splash) SPLASH=0 ;;
    *) echo "Непозната опция: $a"; sed -n '6,11p' "$0"; exit 2 ;;
  esac
done

if [ "$(id -u)" -ne 0 ]; then echo "Пуснете със sudo:  sudo bash $0"; exit 1; fi
USR="${SUDO_USER:-}"
if [ -z "$USR" ] || [ "$USR" = root ]; then
  echo "Пуснете го със sudo от обикновения потребител (не като root): интерфейсът върви като него."
  exit 1
fi
if [ "$(findmnt -n -o FSTYPE /)" = overlay ]; then
  echo "Системата е в режим само за четене: apt не може да инсталира."
  echo "  sudo bash $REPO/tools/cnc-readonly.sh --off && sudo reboot"
  echo "после пуснете инсталирането отново."
  exit 1
fi
case "$REPO" in /home/*) ;; *) echo "Внимание: хранилището не е в /home ($REPO). В режим само за четене само /home остава записваемо." ;; esac

LOG="$REPO/install.log"
: > "$LOG" && chown "$USR": "$LOG"
T0=$(date +%s)
FAILED=()
step() { echo; echo "===== $* ====="; echo "===== $* =====" >> "$LOG"; }
as_user() { sudo -u "$USR" -H "$@"; }
run() {   # run ИМЕ команда…: изходът в дневника, в конзолата само ОК / ГРЕШКА
  local n="$1"; shift
  if "$@" >>"$LOG" 2>&1; then echo "ОК: $n"; else echo "ГРЕШКА: $n (вижте $LOG)"; FAILED+=("$n"); return 1; fi
}

step "1. Пакети, права, конзола"
run "пакети и права" bash "$REPO/tools/orangepi-setup.sh"

step "2. Компилиране на интерфейса и ядрото"
run "библиотеките (LVGL)" as_user git -C "$REPO" submodule update --init ui/lvgl/lvgl
B="$REPO/ui/lvgl/build"
JOBS=$(nproc); MEM=$(awk '/MemTotal/{print int($2/1024)}' /proc/meminfo)
[ "$MEM" -lt 400 ] && JOBS=2   # при малко памет по-малко едновременни компилации
echo "Компилиране с $JOBS нишки (първия път на Orange Pi One: около 10–15 минути)…"
if run "конфигуриране" as_user cmake -S "$REPO/ui/lvgl" -B "$B" -DCMAKE_BUILD_TYPE=Release &&
   run "компилиране" as_user cmake --build "$B" -j "$JOBS"; then
  [ -x "$B/cnc-ui" ] || { echo "ГРЕШКА: няма $B/cnc-ui (липсват libdrm-dev / libinput-dev?)"; FAILED+=("cnc-ui"); }
fi
# проверка на ядрото: тестовете на интерпретатора и планера (бързи, без ASan)
CB="$REPO/core/build"
if run "ядро: конфигуриране" as_user cmake -S "$REPO/core" -B "$CB" -DCMAKE_BUILD_TYPE=Release &&
   run "ядро: компилиране" as_user cmake --build "$CB" -j "$JOBS"; then
  run "ядро: тестове" as_user ctest --test-dir "$CB" --output-on-failure
fi

step "3. Мрежа от интерфейса"
if command -v netplan >/dev/null; then run "cnc-net" bash "$REPO/tools/cnc-net.sh" --install
else echo "Пропуснато: няма netplan (не е Armbian). Мрежата се сменя от системата, не от интерфейса."; fi

step "4. USB флашки"
run "автомонтиране" bash "$REPO/tools/usb-automount.sh"

step "5. Интерфейсът при старт"
if [ -x "$B/cnc-ui" ]; then run "услуга cnc-ui" bash "$REPO/tools/cnc-ui-autostart.sh"
else echo "Пропуснато: интерфейсът не е компилиран"; fi

step "6. Начален екран"
if [ "$SPLASH" = 1 ]; then
  if [ -f /boot/armbianEnv.txt ]; then run "логото при старта" bash "$REPO/tools/cnc-splash.sh"
  else echo "Пропуснато: началният екран е само за Armbian"; fi
else echo "Пропуснато (--no-splash)"; fi

step "7. Система само за четене"
if [ "$RO" = 1 ]; then
  if [ ${#FAILED[@]} -eq 0 ]; then run "защита от спиране на тока" bash "$REPO/tools/cnc-readonly.sh" --install
  else echo "Пропуснато: първо оправете грешките отгоре (в режим само за четене apt не работи)"; fi
else
  echo "Не е включена. За работа на машината (спирането на тока не поврежда системата):"
  echo "  sudo bash $REPO/tools/cnc-readonly.sh --install && sudo reboot"
fi

step "Готово за $(( ($(date +%s) - T0) / 60 )) мин"
if [ ${#FAILED[@]} -gt 0 ]; then
  echo "Неуспешни стъпки: ${FAILED[*]}"
  echo "Подробностите са в $LOG (пратете края му в чата). Скриптът може да се пусне отново."
  exit 1
fi
echo "Всичко е инсталирано. Рестартирайте:  sudo reboot"
echo "Мрежов диск с програмите (по желание):  sudo bash $REPO/tools/net-share.sh //сървър/дял [потребител парола]"
