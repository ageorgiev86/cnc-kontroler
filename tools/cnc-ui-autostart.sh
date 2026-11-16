#!/bin/bash
# Автоматично пускане на интерфейса (ui/lvgl/build/cnc-ui) при старт на Orange Pi / Raspberry Pi.
#
#   sudo bash tools/cnc-ui-autostart.sh          включва (systemd услуга cnc-ui)
#   sudo bash tools/cnc-ui-autostart.sh --off    изключва и връща входа в конзолата
#
# Услугата върви като обикновения потребител (този, който пуска sudo), с права за екрана и входа.
# Конзолата на tty1 се изключва, за да не пречи (SSH остава). Ако програмата спре, тръгва отново след 3 s.
set -u

if [ "$(id -u)" -ne 0 ]; then
  echo "Пуснете скрипта със sudo:  sudo bash $0"
  exit 1
fi
USR="${SUDO_USER:-}"
if [ -z "$USR" ] || [ "$USR" = "root" ]; then
  echo "Пуснете го със sudo от обикновения потребител (не като root)."
  exit 1
fi
HOME_DIR=$(getent passwd "$USR" | cut -d: -f6)
REPO=$(cd "$(dirname "$0")/.." && pwd)
BIN="$REPO/ui/lvgl/build/cnc-ui"
UNIT=/etc/systemd/system/cnc-ui.service

if [ "${1:-}" = "--off" ]; then
  systemctl disable --now cnc-ui.service 2>/dev/null
  rm -f "$UNIT"
  systemctl daemon-reload
  systemctl enable --now getty@tty1.service 2>/dev/null
  echo "Изключено. При старт ще има отново вход в конзолата."
  exit 0
fi

if [ ! -x "$BIN" ]; then
  echo "Няма $BIN"
  echo "Първо компилирайте:  cd $REPO/ui/lvgl && cmake -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build"
  exit 1
fi

cat > "$UNIT" <<EOF
[Unit]
Description=CNC интерфейс (LVGL)
After=systemd-user-sessions.service systemd-udev-settle.service
Conflicts=getty@tty1.service

[Service]
User=$USR
SupplementaryGroups=video input render
Environment=HOME=$HOME_DIR
WorkingDirectory=$REPO/ui/lvgl
ExecStart=$BIN
Restart=always
RestartSec=3
# нишката на движението (1 ms) е с приоритет на реално време (SCHED_FIFO)
LimitRTPRIO=60

[Install]
WantedBy=multi-user.target
EOF

systemctl daemon-reload
systemctl disable getty@tty1.service 2>/dev/null
systemctl enable cnc-ui.service
systemctl restart cnc-ui.service
sleep 2
if systemctl is-active --quiet cnc-ui.service; then
  echo "Готово: интерфейсът работи и ще тръгва при всеки старт."
else
  echo "Услугата не тръгна. Вижте:  journalctl -u cnc-ui -n 30"
fi
echo
echo "Полезни команди:"
echo "  sudo systemctl restart cnc-ui     след git pull и cmake --build build"
echo "  sudo systemctl stop cnc-ui        преди ръчно пускане (./build/cnc-ui)"
echo "  journalctl -u cnc-ui -f           какво пише програмата"
echo "  sudo bash tools/cnc-ui-autostart.sh --off   изключване"
