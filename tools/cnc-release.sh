#!/bin/bash
# Издаване на версия за сървъра за обновления (пуска се при производителя, не на машината).
#
#   Ключовете (веднъж; update.key се пази в тайна, update.pub отива на всяка машина в /etc/cnc/):
#     bash tools/cnc-release.sh --keygen ПАПКА
#
#   Версия: архив за всяка платка + подписан манифест в ИЗХОД/<канал>/
#     bash tools/cnc-release.sh --key update.key --ver 0.6 [--ch stable|test] [--crit] --out ИЗХОД \
#          armhf=път/до/cnc-ui arm64=път/до/cnc-ui
#   После съдържанието на ИЗХОД се качва на сървъра (rclone, scp…), така че да се вижда като
#   https://сървър/<канал>/manifest. Машината пита само за manifest и manifest.sig.
#
# Манифестът:  version 0.6 / crit 0|1 / <платка> <файл> <sha256> <размер>
set -eu
export LC_ALL=C
DIR=$(cd "$(dirname "$0")" && pwd)

if [ "${1:-}" = "--keygen" ]; then
  out="${2:?папка}"; mkdir -p "$out"
  [ -e "$out/update.key" ] && { echo "Вече има $out/update.key — няма да бъде презаписан."; exit 1; }
  openssl genpkey -algorithm ed25519 -out "$out/update.key"
  chmod 600 "$out/update.key"
  openssl pkey -in "$out/update.key" -pubout -out "$out/update.pub"
  echo "Готово: $out/update.key (ТАЕН, пазете го извън хранилището) и $out/update.pub (за машините: /etc/cnc/update.pub)"
  exit 0
fi

KEY="" VER="" CH=stable CRIT=0 OUT=""
BINS=()
while [ $# -gt 0 ]; do
  case "$1" in
    --key) KEY="$2"; shift 2 ;;
    --ver) VER="$2"; shift 2 ;;
    --ch) CH="$2"; shift 2 ;;
    --crit) CRIT=1; shift ;;
    --out) OUT="$2"; shift 2 ;;
    *=*) BINS+=("$1"); shift ;;
    *) echo "Непознат аргумент: $1"; exit 2 ;;
  esac
done
[ -n "$KEY" ] && [ -n "$VER" ] && [ -n "$OUT" ] && [ ${#BINS[@]} -gt 0 ] || { sed -n '2,13p' "$0"; exit 2; }

D="$OUT/$CH"; mkdir -p "$D"
M="$D/manifest.new"
{ echo "version $VER"; echo "crit $CRIT"; } > "$M"
for b in "${BINS[@]}"; do
  arch="${b%%=*}" bin="${b#*=}"
  [ -x "$bin" ] || { echo "Няма програма: $bin"; exit 1; }
  tmp=$(mktemp -d)
  cp "$bin" "$tmp/cnc-ui"; cp "$DIR/cnc-update.sh" "$tmp/cnc-update"; chmod 755 "$tmp/cnc-ui" "$tmp/cnc-update"
  f="cnc-ui-$VER-$arch.tar.gz"
  tar -czf "$D/$f" -C "$tmp" cnc-ui cnc-update
  rm -rf "$tmp"
  echo "$arch $f $(sha256sum "$D/$f" | cut -d' ' -f1) $(stat -c %s "$D/$f")" >> "$M"
done
openssl pkeyutl -sign -inkey "$KEY" -rawin -in "$M" -out "$D/manifest.sig.new"
mv "$M" "$D/manifest"; mv "$D/manifest.sig.new" "$D/manifest.sig"
echo "Готово: $D"; cat "$D/manifest"
