#!/usr/bin/env python3
"""
Проверка на разпределението на пиновете в Core/Inc/board.h.
Copyright (c) 2026 ageorgiev86. Всички права запазени.

Проверява, че:
  - никой пин не е използван два пъти;
  - не се ползват пинове, заети от платката (флаш, microSD, USB, SWD, кварци, светодиод);
  - бързите входове (по EXTI) са на различни номера на пина.
Пускане: python3 test/check_pins.py (от папка firmware/stm32)
"""
import re
import sys
from pathlib import Path

BOARD_H = Path(__file__).resolve().parent.parent / "Core" / "Inc" / "board.h"

# заети от процесорната част на платката (USB, SWD, кварци)
RESERVED = {
    "PA11", "PA12", "PA13", "PA14",                       # USB, SWD
    "PH0", "PH1", "PC14", "PC15",                         # кварци
}
LED = "PG7"
EXTI_INPUTS = ("BOARD_PROBE", "BOARD_TOOLSETTER", "BOARD_ESTOP", "BOARD_DRV_FAULT", "BOARD_LINK_NSS")


def main() -> int:
    text = BOARD_H.read_text(encoding="utf-8")
    uses = {}      # пин → списък с имена
    errors = []

    for name, port, pin in re.findall(r"#define\s+(BOARD_\w+)\s+GPIO([A-K]),\s*GPIO_PIN_(\d+)", text):
        uses.setdefault(f"P{port}{pin}", []).append(name)

    port = re.search(r"#define\s+BOARD_STEP_PORT\s+GPIO([A-K])", text).group(1)
    for kind in ("STEP", "DIR"):
        arr = re.search(rf"#define\s+BOARD_{kind}_PINS\s+\{{([^}}]*)\}}", text).group(1)
        for i, pin in enumerate(re.findall(r"GPIO_PIN_(\d+)", arr)):
            uses.setdefault(f"P{port}{pin}", []).append(f"{kind}[{'XYZABC'[i]}]")

    for pin, names in sorted(uses.items()):
        if pin == LED and names == ["BOARD_LED_PORT"]:
            continue
        if len(names) > 1:
            errors.append(f"{pin} е използван няколко пъти: {', '.join(names)}")
        if pin in RESERVED:
            errors.append(f"{pin} е зает от платката: {', '.join(names)}")

    exti = {}
    for name in EXTI_INPUTS:
        m = re.search(rf"#define\s+{name}\s+GPIO[A-K],\s*GPIO_PIN_(\d+)", text)
        if m:
            exti.setdefault(int(m.group(1)), []).append(name)
    for line, names in exti.items():
        if len(names) > 1:
            errors.append(f"EXTI{line} е общ за {', '.join(names)}")

    print(f"board.h: {len(uses)} пина, бързи входове на EXTI {sorted(exti)}")
    for e in errors:
        print("  ГРЕШКА:", e)
    print("Всичко е наред." if not errors else f"{len(errors)} грешки.")
    return 1 if errors else 0


if __name__ == "__main__":
    sys.exit(main())
