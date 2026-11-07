#!/bin/sh
# Генерира шрифтовете за LVGL от TTF файловете в ttf/ (OFL лиценз).
# Шрифтове: Roboto Condensed (надписи), Roboto (текст), JetBrains Mono (числа).
# Barlow няма кирилица, затова е заменен с Roboto (виж README.md).
# Резултатът (gen/*.c) е в git, така че на Orange Pi не е нужен Node.js.
# Пускане при промяна на размерите или знаците:
#   npm i -g lv_font_conv@1.5.3   (или npx)
#   sh gen_fonts.sh
#
# Размерите са в две степени според базата em (както в ui/maket.html: em = min(W,H)/800*16):
#   t16 — екрани с по-късата страна 800 (800×1280, 1280×800)
#   t24 — екрани с по-късата страна 1200 (1920×1200), и 1080 (1920×1080) с малко по-едри знаци
set -eu
cd "$(dirname "$0")"
CONV="${LV_FONT_CONV:-npx lv_font_conv}"
mkdir -p gen

# Латиница, кирилица и знаците, които се ползват в интерфейса (° ± µ · × Ø ø – — „ “ … ″ −)
FULL="0x20-0x7E,0xB0-0xB3,0xB5,0xB7,0xD7,0xD8,0xF8,0x401,0x410-0x44F,0x451,0x2013,0x2014,0x201C,0x201E,0x2026,0x2033,0x2212"

# име  шрифт  em-множител  знаци
ROLES="
ui_xs  RobotoCondensed-SemiBold.ttf 0.82 full
ui_sm  RobotoCondensed-SemiBold.ttf 0.92 full
ui_md  RobotoCondensed-Bold.ttf     1.05 full
ui_ax  RobotoCondensed-Bold.ttf     1.95 axis
txt    Roboto-Medium.ttf            0.95 full
num_sm JetBrainsMono-Regular.ttf    0.74 full
num_md JetBrainsMono-Medium.ttf     0.95 full
num_lg JetBrainsMono-Medium.ttf     1.85 digits
num_gc JetBrainsMono-Regular.ttf    0.82 full
num_ov JetBrainsMono-Medium.ttf     1.55 digits
num_pg JetBrainsMono-Medium.ttf     1.15 full
txt_sm Roboto-Medium.ttf            0.74 full
ui_lg  RobotoCondensed-Bold.ttf     1.45 full
"

for tier in 16 24; do
  echo "$ROLES" | while read -r name ttf mul set; do
    [ -n "$name" ] || continue
    px=$(awk "BEGIN{printf \"%d\", $tier*$mul+0.5}")
    case $set in
      full)   R="-r $FULL" ;;
      ascii)  R="-r 0x20-0x7E" ;;
      axis)   R="--symbols XYZABCUVWØ" ;;
      digits) R="-r 0x20,0x25,0x2B-0x3A" ;;   # интервал % + , - . / 0-9 :
    esac
    out="gen/font_${name}_t${tier}.c"
    echo "$out ($px px)"
    # shellcheck disable=SC2086
    $CONV --font "ttf/$ttf" $R --size "$px" --bpp 4 --format lvgl --no-compress \
      --lv-include lvgl.h --lv-font-name "font_${name}_t${tier}" -o "$out"
  done
done
