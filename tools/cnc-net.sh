#!/bin/bash
# Мрежата на Orange Pi / Raspberry Pi от интерфейса (Настройки → Мрежа). Netplan + systemd-networkd (Armbian).
# Copyright (c) 2026 ageorgiev86. Всички права запазени.
#
#   sudo bash tools/cnc-net.sh --install     еднократно: слага командата cnc-net и разрешава на
#                                            потребителя да я вика без парола (само нея)
#   sudo cnc-net status                      режим и текущ адрес (/usr/local/sbin не е в пътя без sudo)
#   cnc-net dhcp                             адресът от рутера (DHCP)
#   cnc-net static IP МАСКА ШЛЮЗ [DNS]       статичен адрес, напр. 192.168.0.50 255.255.255.0 192.168.0.1
#   cnc-net hostname ИМЕ                     име на машината
#
# Статичният адрес е в /etc/netplan/90-cnc.yaml: той заменя DHCP на Ethernet от файла на Armbian
# (същото име all-eth-interfaces), без да го пипа. DHCP = този файл се изтрива.
# В режим „само за четене“ (tools/cnc-readonly.sh) промените се записват и на картата.
# Новата мрежа се прилага 2 s след командата, за да стигне отговорът до интерфейса.
set -u

YAML=/etc/netplan/90-cnc.yaml
SELF=/usr/local/sbin/cnc-net
SUDOERS=/etc/sudoers.d/cnc-net

die() { echo "ГРЕШКА: $*"; exit 1; }
need_root() { [ "$(id -u)" -eq 0 ] || die "трябват права на root (sudo)"; }
ro_now() { [ "$(findmnt -n -o FSTYPE /)" = "overlay" ] && mountpoint -q /media/root-ro; }

# файл: временен → sync → преименуване; в режим само за четене и на картата
put_file() {   # $1 път, $2 права, съдържанието от stdin
  local data; data=$(cat)
  local dirs="/"
  ro_now && dirs="/ /media/root-ro/"
  ro_now && mount -o remount,bind,rw /media/root-ro
  for d in $dirs; do
    local p="${d%/}$1"
    printf '%s\n' "$data" > "$p.tmp" && chmod "$2" "$p.tmp" && sync "$p.tmp" && mv -f "$p.tmp" "$p"
  done
  sync
  ro_now && mount -o remount,bind,ro /media/root-ro
  return 0
}
del_file() {
  ro_now && mount -o remount,bind,rw /media/root-ro
  rm -f "$1"
  ro_now && rm -f "/media/root-ro$1"
  sync
  ro_now && mount -o remount,bind,ro /media/root-ro
  return 0
}

valid_ip() {
  [[ "$1" =~ ^([0-9]{1,3})\.([0-9]{1,3})\.([0-9]{1,3})\.([0-9]{1,3})$ ]] || return 1
  for i in 1 2 3 4; do [ "${BASH_REMATCH[$i]}" -le 255 ] || return 1; done
  return 0
}
ip2int() { local IFS=.; set -- $1; echo $(( ($1 << 24) | ($2 << 16) | ($3 << 8) | $4 )); }
mask2prefix() {   # само непрекъснати маски (255.255.255.0 → 24)
  local m; m=$(ip2int "$1") n=0 seen0=0
  for ((b = 31; b >= 0; b--)); do
    if (( (m >> b) & 1 )); then (( seen0 )) && return 1; n=$((n + 1)); else seen0=1; fi
  done
  (( n >= 8 && n <= 30 )) || return 1
  echo "$n"
}

# прилагане след 2 s; после пинг до шлюза ($1), за да научи рутерът новия адрес веднага (ARP),
# вместо да чака старият запис в таблицата му да изтече
apply_later() {
  if ! netplan generate 2>/tmp/cnc-net.err; then return 1; fi
  local cmd="/usr/sbin/netplan apply"
  [ -n "${1:-}" ] && cmd="$cmd; sleep 3; ping -c 3 -W 1 $1 >/dev/null 2>&1; true"
  systemd-run --quiet --on-active=2 --unit="cnc-net-apply-$$" /bin/sh -c "$cmd" >/dev/null 2>&1 \
    || ( sleep 2; sh -c "$cmd" ) >/dev/null 2>&1 &
  return 0
}

cur_addr() { ip -4 -o addr show scope global 2>/dev/null | awk '$2 ~ /^(e|lan|wan)/ { print $2 " " $4; exit }'; }

case "${1:-}" in
--install)
  need_root
  ro_now && die "системата е само за четене. Първо: sudo bash $(dirname "$0")/cnc-readonly.sh --off и рестарт"
  USR="${SUDO_USER:-}"
  [ -n "$USR" ] && [ "$USR" != root ] || die "пуснете със sudo от обикновения потребител"
  command -v netplan >/dev/null || die "няма netplan (Armbian с systemd-networkd?)"
  install -m 755 "$0" "$SELF" || die "не може да се копира в $SELF"
  printf '%s ALL=(root) NOPASSWD: %s\n' "$USR" "$SELF" > "$SUDOERS.tmp"
  chmod 440 "$SUDOERS.tmp"
  visudo -cf "$SUDOERS.tmp" >/dev/null || { rm -f "$SUDOERS.tmp"; die "правилото за sudo не е валидно"; }
  mv -f "$SUDOERS.tmp" "$SUDOERS"
  sync
  echo "ОК: $SELF; $USR може да го вика без парола (само него)."
  echo "Сега „Запиши“ в Настройки → Мрежа прилага адреса и името на машината."
  ;;
status)
  if [ -f "$YAML" ]; then echo "режим: статичен"; else echo "режим: DHCP"; fi
  echo "адрес: $(cur_addr)"
  echo "име: $(hostname)"
  ;;
dhcp)
  need_root
  if [ ! -f "$YAML" ]; then echo "ОК: адресът вече е от рутера (DHCP)"; exit 0; fi
  del_file "$YAML"
  apply_later || die "netplan: $(head -c 150 /tmp/cnc-net.err)"
  echo "ОК: адресът ще дойде от рутера (DHCP) след 2 s"
  ;;
static)
  need_root
  IP="${2:-}" MASK="${3:-}" GW="${4:-}" DNS="${5:-}"
  valid_ip "$IP" || die "невалиден IP адрес „$IP“"
  valid_ip "$MASK" || die "невалидна маска „$MASK“"
  PFX=$(mask2prefix "$MASK") || die "невалидна маска „$MASK“ (пример: 255.255.255.0)"
  valid_ip "$GW" || die "невалиден шлюз „$GW“"
  [ -z "$DNS" ] && DNS="$GW"
  valid_ip "$DNS" || die "невалиден DNS „$DNS“"
  m=$(ip2int "$MASK")
  (( ($(ip2int "$IP") & m) == ($(ip2int "$GW") & m) )) || die "шлюзът $GW не е в мрежата на $IP/$PFX"
  h=$(( $(ip2int "$IP") & ~m & 0xFFFFFFFF )) hm=$(( ~m & 0xFFFFFFFF ))
  (( h != 0 && h != hm )) || die "$IP е адрес на мрежата или broadcast за /$PFX"
  OLD=""; [ -f "$YAML" ] && OLD=$(cat "$YAML")
  put_file "$YAML" 600 <<EOF
# Записан от интерфейса на CNC (Настройки → Мрежа, tools/cnc-net.sh). DHCP = изтриване на файла.
network:
  version: 2
  ethernets:
    all-eth-interfaces:
      dhcp4: false
      dhcp6: false
      addresses: [$IP/$PFX]
      routes:
        - to: default
          via: $GW
      nameservers:
        addresses: [$DNS]
EOF
  if ! apply_later "$GW"; then
    if [ -n "$OLD" ]; then printf '%s\n' "$OLD" | put_file "$YAML" 600; else del_file "$YAML"; fi
    die "netplan отказа настройката: $(head -c 150 /tmp/cnc-net.err)"
  fi
  echo "ОК: адресът става $IP/$PFX (шлюз $GW) след 2 s; SSH: ssh ${SUDO_USER:-потребител}@$IP"
  ;;
hostname)
  need_root
  N="${2:-}"
  [[ "$N" =~ ^[A-Za-z0-9]([A-Za-z0-9-]{0,30}[A-Za-z0-9])?$ ]] || die "невалидно име „$N“ (латински букви, цифри, тире)"
  [ "$(hostname)" = "$N" ] && { echo "ОК: името вече е $N"; exit 0; }
  OLDN=$(hostname)
  printf '%s\n' "$N" | put_file /etc/hostname 644
  sed "s/^127\.0\.1\.1[[:space:]].*/127.0.1.1\t$N/" /etc/hosts > /tmp/cnc-hosts
  grep -q '^127\.0\.1\.1' /tmp/cnc-hosts || printf '127.0.1.1\t%s\n' "$N" >> /tmp/cnc-hosts
  put_file /etc/hosts 644 < /tmp/cnc-hosts
  rm -f /tmp/cnc-hosts
  hostname "$N"
  echo "ОК: името е $N (беше $OLDN)"
  ;;
*)
  echo "Употреба: cnc-net status | dhcp | static IP МАСКА ШЛЮЗ [DNS] | hostname ИМЕ"
  echo "Еднократно: sudo bash tools/cnc-net.sh --install"
  exit 2
  ;;
esac
