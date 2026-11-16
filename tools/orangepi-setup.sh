#!/bin/bash
# Настройка на Orange Pi (Armbian) за разработка на интерфейса на LVGL.
# Copyright (c) 2026 ageorgiev86. Всички права запазени.
#
# Пуска се така:   sudo bash tools/orangepi-setup.sh
# Прави: пакети за компилиране, права за екрана и мишката/тъча, настройка на
# конзолата и отчет за системата, екрана и входните устройства.
# Може да се пуска многократно: не повтаря вече направеното.

set -u

if [ "$(id -u)" -ne 0 ]; then
  echo "Пуснете скрипта със sudo:  sudo bash $0"
  exit 1
fi
USR="${SUDO_USER:-}"

step() { echo; echo "===== $* ====="; }

step "1. Пакети за разработка"
export DEBIAN_FRONTEND=noninteractive
if apt-get update -qq && apt-get install -y -qq \
     build-essential cmake ninja-build git gdb pkg-config \
     libdrm-dev libinput-dev libevdev-dev libudev-dev \
     libdrm-tests evtest libinput-tools >/dev/null; then
  echo "ОК: пакетите са инсталирани"
else
  echo "ГРЕШКА: инсталирането на пакетите не мина (проверете мрежата)"
fi

step "2. Права за екрана и входните устройства"
if [ -n "$USR" ] && [ "$USR" != "root" ]; then
  for g in video input render; do
    if getent group "$g" >/dev/null; then
      usermod -aG "$g" "$USR" && echo "ОК: $USR е в група $g"
    else
      echo "Няма група $g (не е проблем, ако /dev/dri е достъпен)"
    fi
  done
  echo "Важи след излизане и ново влизане (или рестарт)."
else
  echo "Пропуснато: скриптът не е пуснат със sudo от обикновен потребител"
fi

step "2а. SPI към STM32 (/dev/spidev0.0)"
# Armbian: overlay spi-spidev на SPI0 (пинове 19 MOSI, 21 MISO, 23 SCLK, 24 CE0); Raspberry Pi: dtparam=spi=on
if [ -f /boot/armbianEnv.txt ]; then
  E=/boot/armbianEnv.txt
  [ -f "$E.cnc-backup" ] || cp "$E" "$E.cnc-backup"
  if grep -q '^overlays=' "$E"; then
    grep -q '^overlays=.*\bspi-spidev\b' "$E" || sed -i 's/^overlays=.*/& spi-spidev/' "$E"
  else
    echo "overlays=spi-spidev" >> "$E"
  fi
  if grep -q '^param_spidev_spi_bus=' "$E"; then sed -i 's/^param_spidev_spi_bus=.*/param_spidev_spi_bus=0/' "$E"
  else echo "param_spidev_spi_bus=0" >> "$E"; fi
  echo "ОК: $(grep -E '^(overlays|param_spidev_spi_bus)=' "$E" | tr '\n' ' ')"
else
  for C in /boot/firmware/config.txt /boot/config.txt; do
    [ -f "$C" ] || continue
    grep -q '^dtparam=spi=on' "$C" || echo "dtparam=spi=on" >> "$C"
    echo "ОК: dtparam=spi=on в $C"
    break
  done
fi
# правата: група spi за /dev/spidev* (иначе само root)
getent group spi >/dev/null || groupadd -r spi
echo 'SUBSYSTEM=="spidev", GROUP="spi", MODE="0660"' > /etc/udev/rules.d/90-cnc-spi.rules
[ -n "$USR" ] && [ "$USR" != "root" ] && usermod -aG spi "$USR" && echo "ОК: $USR е в група spi"
echo "SPI се появява след рестарт: ls -l /dev/spidev0.0"

step "3. Конзолата да не пречи на интерфейса"
ENV=/boot/armbianEnv.txt
ARGS="consoleblank=0 vt.global_cursor_default=0"
if [ -f "$ENV" ]; then
  [ -f "$ENV.cnc-backup" ] || cp "$ENV" "$ENV.cnc-backup"
  if grep -q '^extraargs=' "$ENV"; then
    for a in $ARGS; do
      grep -q "^extraargs=.*\b${a%%=*}=" "$ENV" || sed -i "s|^extraargs=.*|& $a|" "$ENV"
    done
  else
    echo "extraargs=$ARGS" >> "$ENV"
  fi
  echo "ОК: $(grep '^extraargs=' "$ENV")"
  echo "(резервно копие: $ENV.cnc-backup)"
else
  echo "Няма $ENV (не е Armbian?). Пропуснато."
fi

step "4. Система"
uname -srm
[ -f /etc/armbian-release ] && grep -E '^(BOARD|VERSION|DISTRIBUTION_CODENAME|BRANCH)=' /etc/armbian-release
echo "Памет:"; free -h | head -2
echo "Карта:"; df -h / | tail -1
echo "Мрежа:"; ip -4 -br addr | grep -v '^lo'

step "5. Екран (DRM)"
ls -l /dev/dri 2>/dev/null || echo "НЯМА /dev/dri: драйверът на дисплея не е зареден"
for c in /sys/class/drm/card*-*; do
  [ -e "$c/status" ] || continue
  st=$(cat "$c/status")
  echo "$(basename "$c"): $st"
  if [ "$st" = "connected" ]; then
    echo "  режими: $(head -8 "$c/modes" | tr '\n' ' ')"
  fi
done

step "6. Мишка, клавиатура, тъч"
if command -v libinput >/dev/null; then
  libinput list-devices 2>/dev/null | grep -E '^(Device|Capabilities):' | paste - - | sed 's/\t/  |  /'
else
  echo "libinput-tools не е инсталиран"
fi

step "Готово"
echo "Копирайте целия отчет отгоре и го пратете в чата."
echo "След това рестартирайте:  sudo reboot"
