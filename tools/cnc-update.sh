#!/bin/bash
# Обновяване на интерфейса (вика го cnc-ui от прозореца „Обновяване“; може и ръчно).
#
#   cnc-update.sh --bin ПЪТ [--ch stable|test] status|check|download|install|rollback
#
# Изходът е по един ред за интерфейса:
#   CUR <версия> / PREV <версия>     (status)
#   NEW <версия> <0|1 важна> / NONE   (check)
#   P <0..100>                         (напредък при download и install)
#   OK / ERR <текст>
#
# Два режима:
#   сървър — ако има /etc/cnc/update.conf (URL=…, PUB=…). Машината тегли от сървъра на производителя
#            манифест и архив; манифестът е подписан (Ed25519), архивът е проверен по SHA256.
#            Така се обновяват клиентските машини: те не знаят нищо за изходния код.
#   git    — ако програмата е компилирана от копие на хранилището (машината за разработка).
#            „stable“ следи клона stable (или последния таг vX.Y, ако няма такъв клон), „test“ следи main.
#            Компилира се в отделна папка (build-upd).
#
# Инсталирането не иска root: новата програма се записва до старата, fsync и се преименува върху нея
# (атомарно, безопасно при спиране на тока). Старата остава като <bin>.prev за „Върни предишната версия“.
# Рестартът го прави самият интерфейс: излиза и systemd го пуска отново (Restart=always).
set -u
export LC_ALL=C

BIN="" CH=stable CMD=""
while [ $# -gt 0 ]; do
  case "$1" in
    --bin) BIN="$2"; shift 2 ;;
    --ch) CH="$2"; shift 2 ;;
    *) CMD="$1"; shift ;;
  esac
done
[ -n "$BIN" ] && [ -n "$CMD" ] || { echo "ERR употреба: $0 --bin ПЪТ [--ch stable|test] status|check|download|install|rollback"; exit 2; }
[ "$CH" = test ] || CH=stable

STATE="${XDG_STATE_HOME:-$HOME/.local/state}/cnc-update"
mkdir -p "$STATE" || { echo "ERR няма място за временните файлове"; exit 1; }
LOG="$STATE/last.log"

fail() { echo "ERR $*"; exit 1; }
say() { echo "$@"; }

# етикетът и идентификаторът на инсталираната програма: <bin>.ver = „етикет идентификатор“
ver_of() { [ -f "$1.ver" ] && cut -d' ' -f"$2" "$1.ver" || true; }

# атомарна замяна на един файл: нов → временен до целта → fsync → преименуване → fsync на папката
put_file() {
  local src="$1" dst="$2" tmp
  tmp="$(dirname "$dst")/.$(basename "$dst").new.$$"
  cp -f "$src" "$tmp" && chmod --reference="$src" "$tmp" 2>/dev/null; sync "$tmp" 2>/dev/null
  mv -f "$tmp" "$dst" || { rm -f "$tmp"; return 1; }
  sync "$(dirname "$dst")" 2>/dev/null
  return 0
}
write_ver() {   # write_ver ФАЙЛ „етикет ид“
  local tmp="$1.new.$$"
  printf '%s\n' "$2" > "$tmp" && sync "$tmp" 2>/dev/null && mv -f "$tmp" "$1"
}

# новата програма става текуща, текущата става предишна
install_bin() {   # install_bin НОВ_ФАЙЛ „етикет ид“
  local d; d=$(dirname "$BIN")
  if [ -f "$BIN" ]; then
    ln -f "$BIN" "$d/.cnc-prev.$$" 2>/dev/null || cp -f "$BIN" "$d/.cnc-prev.$$"
    sync "$d/.cnc-prev.$$" 2>/dev/null
    mv -f "$d/.cnc-prev.$$" "$BIN.prev"
    # етикетът на старата: от <bin>.ver или (първото обновяване от git) запомненият при изтеглянето
    if [ -f "$BIN.ver" ]; then cp -f "$BIN.ver" "$BIN.prev.ver"
    elif [ -f "$STATE/was" ]; then cp -f "$STATE/was" "$BIN.prev.ver"
    else rm -f "$BIN.prev.ver"; fi
  fi
  say "P 60"
  put_file "$1" "$BIN" || fail "програмата не може да бъде записана"
  say "P 90"
  write_ver "$BIN.ver" "$2"
  sync "$d" 2>/dev/null
  say "P 100"
}

# ---------- режим „сървър“ ----------
CONF="${CNC_UPDATE_CONF:-/etc/cnc/update.conf}"   # другият път е само за проба
URL="" PUB=/etc/cnc/update.pub
if [ -f "$CONF" ]; then . "$CONF"; fi

arch() {
  case "$(uname -m)" in
    armv7l|armv6l|armhf) echo armhf ;;
    aarch64|arm64) echo arm64 ;;
    x86_64) echo amd64 ;;
    *) uname -m ;;
  esac
}

srv_manifest() {   # тегли и проверява манифеста → $STATE/manifest
  curl -fsS --max-time 20 -o "$STATE/manifest.new" "$URL/$CH/manifest" 2>>"$LOG" &&
  curl -fsS --max-time 20 -o "$STATE/manifest.sig" "$URL/$CH/manifest.sig" 2>>"$LOG" || fail "няма връзка"
  openssl pkeyutl -verify -pubin -inkey "$PUB" -rawin -in "$STATE/manifest.new" -sigfile "$STATE/manifest.sig" >>"$LOG" 2>&1 ||
    fail "подписът не е верен"
  mv -f "$STATE/manifest.new" "$STATE/manifest"
}
mf() { awk -v k="$1" '$1==k{$1="";sub(/^ /,"");print;exit}' "$STATE/manifest"; }

srv_check() {
  srv_manifest
  local v cur crit; v=$(mf version); cur=$(ver_of "$BIN" 1); crit=$(mf crit)
  [ -n "$v" ] || fail "празен манифест"
  # по-нова ли е (сортиране по версии); същата или по-стара = няма обновление
  if [ "$v" != "$cur" ] && [ "$(printf '%s\n%s\n' "$cur" "$v" | sort -V | tail -1)" = "$v" ]; then
    say "NEW $v ${crit:-0}"
  else
    say "NONE"
  fi
}

srv_download() {
  [ -f "$STATE/manifest" ] || srv_manifest
  local a line file sum size v
  a=$(arch); line=$(mf "$a"); v=$(mf version)
  [ -n "$line" ] || fail "няма версия за тази платка ($a)"
  read -r file sum size <<<"$line"
  rm -rf "$STATE/pkg" "$STATE/ready"; mkdir -p "$STATE/pkg"
  curl -fsS --max-time 1800 -o "$STATE/pkg.tar.gz" "$URL/$CH/$file" 2>>"$LOG" &
  local pid=$! n
  trap 'kill $pid 2>/dev/null; exit 1' TERM INT
  while kill -0 $pid 2>/dev/null; do
    n=$(stat -c %s "$STATE/pkg.tar.gz" 2>/dev/null || echo 0)
    [ "${size:-0}" -gt 0 ] && say "P $(( n * 95 / size ))"
    sleep 0.3
  done
  wait $pid || fail "изтеглянето е прекъснато"
  echo "$sum  $STATE/pkg.tar.gz" | sha256sum -c --status || fail "файлът е повреден"
  tar -xzf "$STATE/pkg.tar.gz" -C "$STATE/pkg" || fail "архивът е повреден"
  [ -x "$STATE/pkg/cnc-ui" ] || fail "в архива няма програма"
  rm -f "$STATE/pkg.tar.gz"
  echo "$v $v" > "$STATE/ready"
  say "P 100"; say "OK"
}

srv_install() {
  [ -f "$STATE/ready" ] || fail "първо изтеглете"
  local lbl; lbl=$(cat "$STATE/ready")
  say "P 10"
  # самият скрипт също се обновява, ако е в архива
  [ -f "$STATE/pkg/cnc-update" ] && put_file "$STATE/pkg/cnc-update" "$(dirname "$BIN")/cnc-update"
  install_bin "$STATE/pkg/cnc-ui" "$lbl"
  rm -rf "$STATE/pkg" "$STATE/ready"
  say "OK"
}

# ---------- режим „git“ (машината за разработка) ----------
REPO=""
git_init() {
  REPO=$(git -C "$(dirname "$BIN")" rev-parse --show-toplevel 2>/dev/null) || fail "няма откъде да се обнови"
}
git_label() {   # етикет за даден commit: тагът vX.Y, VERSION (върхът на stable) или VERSION-кратък хеш
  local c="$1" t v
  c=$(git -C "$REPO" rev-parse "$c")
  t=$(git -C "$REPO" tag --points-at "$c" -l 'v*' | sort -V | tail -1)
  if [ -n "$t" ]; then echo "${t#v}"; return; fi
  v=$(git -C "$REPO" show "$c:ui/lvgl/VERSION" 2>/dev/null | head -1); v=${v:-0}
  if [ "$c" = "$(git -C "$REPO" rev-parse -q --verify origin/stable 2>/dev/null)" ]; then echo "$v"; return; fi
  echo "$v-$(git -C "$REPO" rev-parse --short "$c")"
}
git_cur() { local c; c=$(ver_of "$BIN" 2); [ -n "$c" ] && git -C "$REPO" cat-file -e "$c^{commit}" 2>/dev/null && echo "$c" || git -C "$REPO" rev-parse HEAD; }
git_target() {
  if [ "$CH" = test ]; then git -C "$REPO" rev-parse origin/main 2>/dev/null
  elif git -C "$REPO" rev-parse -q --verify origin/stable >/dev/null 2>&1; then git -C "$REPO" rev-parse origin/stable
  else
    local t; t=$(git -C "$REPO" tag -l 'v*' | sort -V | tail -1)
    [ -n "$t" ] && git -C "$REPO" rev-parse "$t^{commit}"
  fi
}

git_check() {
  git_init
  timeout 60 git -C "$REPO" fetch -q --tags origin >>"$LOG" 2>&1 || fail "няма връзка"
  local cur tg; cur=$(git_cur); tg=$(git_target)
  if [ -n "$tg" ] && [ "$tg" != "$cur" ] && git -C "$REPO" merge-base --is-ancestor "$cur" "$tg"; then
    local crit=0
    git -C "$REPO" log --format='%B' "$cur..$tg" | grep -qi '\[важно\]' && crit=1
    say "NEW $(git_label "$tg") $crit"
  else
    say "NONE"
  fi
}

git_download() {
  git_init
  local tg; tg=$(git_target)
  [ -n "$tg" ] || fail "няма версия"
  say "P 2"
  # коя е програмата сега (ако няма <bin>.ver): след merge HEAD вече е новият
  [ -f "$BIN.ver" ] || { local c0; c0=$(git_cur); echo "$(git_label "$c0") $c0" > "$STATE/was"; }
  # изходният код се премества напред само ако няма локални промени, които биха се загубили
  if [ "$(git -C "$REPO" rev-parse HEAD)" != "$tg" ]; then
    git -C "$REPO" merge -q --ff-only "$tg" >>"$LOG" 2>&1 || fail "в хранилището има локални промени"
  fi
  # ако хранилището вече е напред (ръчен git pull на main), merge не прави нищо и би се компилирал
  # по-новият код под етикета на избраната версия: по-добре отказ
  [ "$(git -C "$REPO" rev-parse HEAD)" = "$tg" ] || fail "хранилището е по-напред от версията (ръчен git pull); изберете „Тестови“"
  git -C "$REPO" submodule -q update --init >>"$LOG" 2>&1 || fail "библиотеките не могат да се изтеглят"
  say "P 5"
  local src="$REPO/ui/lvgl" b="$REPO/ui/lvgl/build-upd" tgt; tgt=$(basename "$BIN")
  rm -f "$STATE/ready"
  if [ ! -f "$b/CMakeCache.txt" ]; then
    cmake -S "$src" -B "$b" -DCMAKE_BUILD_TYPE=Release >>"$LOG" 2>&1 || fail "компилирането не тръгна"
  fi
  say "P 8"
  # „[ 45%]“ от make → 8..98 %
  cmake --build "$b" --target "$tgt" -j "$(nproc)" 2>&1 | tee -a "$LOG" |
    sed -un 's/^\[ *\([0-9]*\)%\].*/\1/p' | while read -r p; do echo "P $(( 8 + p * 90 / 100 ))"; done
  [ "${PIPESTATUS[0]}" -eq 0 ] && [ -x "$b/$tgt" ] || fail "грешка при компилиране (вижте $LOG)"
  echo "$(git_label "$tg") $tg" > "$STATE/ready"
  say "P 100"; say "OK"
}

git_install() {
  git_init
  [ -f "$STATE/ready" ] || fail "първо изтеглете"
  local lbl; lbl=$(cat "$STATE/ready")
  say "P 10"
  install_bin "$REPO/ui/lvgl/build-upd/$(basename "$BIN")" "$lbl"
  rm -f "$STATE/ready" "$STATE/was"
  say "OK"
}

# ---------- общи ----------
rollback() {
  [ -f "$BIN.prev" ] || fail "няма предишна версия"
  local d; d=$(dirname "$BIN")
  ln -f "$BIN" "$d/.cnc-swap.$$" 2>/dev/null || cp -f "$BIN" "$d/.cnc-swap.$$"
  [ -f "$BIN.ver" ] && cp -f "$BIN.ver" "$d/.cnc-swap.ver.$$"
  put_file "$BIN.prev" "$BIN" || fail "програмата не може да бъде записана"
  if [ -f "$BIN.prev.ver" ]; then write_ver "$BIN.ver" "$(cat "$BIN.prev.ver")"; else rm -f "$BIN.ver"; fi
  mv -f "$d/.cnc-swap.$$" "$BIN.prev"
  if [ -f "$d/.cnc-swap.ver.$$" ]; then mv -f "$d/.cnc-swap.ver.$$" "$BIN.prev.ver"; else rm -f "$BIN.prev.ver"; fi
  sync "$d" 2>/dev/null
  rm -f "$STATE/ready"
  say "OK"
}

status() {
  local cur; cur=$(ver_of "$BIN" 1)
  if [ -z "$cur" ] && [ ! -f "$CONF" ]; then REPO=$(git -C "$(dirname "$BIN")" rev-parse --show-toplevel 2>/dev/null) && cur=$(git_label HEAD); fi
  say "CUR ${cur:-?}"
  say "PREV $(ver_of "$BIN.prev" 1)"
  [ -f "$STATE/ready" ] && say "READY $(cut -d' ' -f1 "$STATE/ready")"
  return 0
}

# ---------- проба за симулатора: CNC_UPDATE_FAKE=1 ----------
if [ -n "${CNC_UPDATE_FAKE:-}" ]; then
  F="$STATE/fake"; [ -f "$F" ] || echo "0.5 0.4" > "$F"
  read -r cur prev < "$F"
  case "$CMD" in
    status) say "CUR $cur"; say "PREV $prev" ;;
    check) sleep 1; if [ "$cur" = 0.5 ]; then say "NEW 0.6 ${CNC_UPDATE_FAKE_CRIT:-0}"; else say NONE; fi ;;
    download) for i in $(seq 0 4 100); do say "P $i"; sleep 0.12; [ -n "${CNC_UPDATE_FAKE_FAIL:-}" ] && [ "$i" -gt 44 ] && fail "изтеглянето е прекъснато"; done; say OK ;;
    install) for i in $(seq 0 10 100); do say "P $i"; sleep 0.1; done; echo "0.6 $cur" > "$F"; say OK ;;
    rollback) echo "$prev $cur" > "$F"; say OK ;;
  esac
  exit 0
fi

MODE=git; [ -n "$URL" ] && MODE=srv
case "$CMD" in
  status) status ;;
  check) ${MODE}_check ;;
  download) ${MODE}_download ;;
  install) ${MODE}_install ;;
  rollback) rollback ;;
  *) fail "непозната команда $CMD" ;;
esac
