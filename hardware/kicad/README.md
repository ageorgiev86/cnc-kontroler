# Платките (KiCad 10)

Схемите и разположението се **генерират със скриптове** (Python-а на KiCad), а не се чертаят на ръка. Така всяка промяна е повторяема и се проверява автоматично (ERC, DRC, застъпвания).

| Папка / файл | Какво е |
|---|---|
| `osnovna_platka/` | основната платка: STM32H723ZGT6, драйвери, енкодери, входове и изходи 8 + 8, шпиндел, Orange Pi (338 × 136 мм) |
| `razshiritel/` | разширителен модул: 8 входа + 8 изхода 24 V, верижно (93 × 64 мм) |
| `osnovna_platka.py` | схемата на основната платка (речникът `MCU` = изводите на STM32, съвпада с `firmware/stm32/Core/Inc/board.h`) |
| `platka.py` | разположението на основната платка по план (роли на елементите от схемата) |
| `razshiritel.py` | схемата и разположението на модула |
| `pravila.py` | правилата: класове мрежи (сигнал 0,4 мм, STM32 0,3 мм, захранване 1 мм, маси), преходи 0,6/1,2 мм |
| `trasirane.py` | пробно трасиране с Freerouting върху копие на проекта + заливки GND и GND_IO на двата слоя |
| `sch.py` | общата част на генератора на схеми |

## Пускане

Затвори KiCad без запис преди това (скриптовете презаписват файловете).

```
"C:\Program Files\KiCad\10.0\bin\python.exe" -I osnovna_platka.py osnovna_platka
"C:\Program Files\KiCad\10.0\bin\python.exe" -I pravila.py osnovna_platka
"C:\Program Files\KiCad\10.0\bin\python.exe" -I platka.py osnovna_platka 136
"C:\Program Files\KiCad\10.0\bin\python.exe" -I razshiritel.py razshiritel 64
```

Ако вече си трасирал или местил нещо ръчно в KiCad, **не пускай `platka.py`** (ще го изтрие). Промените по схемата тогава се пренасят с F8 (Update PCB from Schematic): връзката схема ↔ платка е по постоянни UUID.

## На Linux (и трасиране)

KiCad 10 от PPA-то на KiCad (Ubuntu 24.04), Java 21 и Freerouting 2.1.0 (2.5 иска Java 25):

```
sudo add-apt-repository ppa:kicad/kicad-10.0-releases
sudo apt install --no-install-recommends kicad kicad-footprints kicad-symbols openjdk-21-jre-headless
sudo mkdir -p /opt/freerouting
sudo curl -L -o /opt/freerouting/freerouting-2.1.0.jar \
    https://github.com/freerouting/freerouting/releases/download/v2.1.0/freerouting-2.1.0.jar
```

Скриптовете се пускат с Python-а, с който е компилиран модулът `pcbnew` (на Ubuntu 24.04 — `python3.12`):

```
python3.12 -I osnovna_platka.py osnovna_platka
python3.12 -I pravila.py osnovna_platka
python3.12 -I platka.py osnovna_platka 136
python3.12 -I trasirane.py osnovna_platka /tmp/trasirane --minutes 90
kicad-cli pcb drc --severity-all /tmp/trasirane/osnovna_platka.kicad_pcb
```

`trasirane.py` търси Freerouting и Java така: аргументите `--jar`/`--java`, после променливите
`FREEROUTING_JAR`/`FREEROUTING_JAVA`, после (Linux) най-новия `/opt/freerouting/freerouting-*.jar` и `java` от PATH,
(Windows) плъгина на KiCad. Пътищата до KiCad (символи, отпечатъци, `kicad-cli`) се сменят с `KICAD_DIR`,
`KICAD_SYMBOL_DIR`, `KICAD_FOOTPRINT_DIR`, `KICAD_CLI`. Ако ERC/DRC се оплакват „library not included“, копирай
`/usr/share/kicad/template/{sym,fp}-lib-table` в `~/.config/kicad/10.0/`.

Freerouting 2.1 не спира по броя проходи (`-mp` се пренебрегва), а само по време: `--minutes N` (или
`FREEROUTING_MINUTES`, по подразбиране 60). Напредъкът е в `<копие>/freerouting.log` („pass #… (N unrouted)“).
Преди Freerouting `trasirane.py` слага преходите към масата (при QFP навътре под корпуса) и заключени изводи навън
по 1,5 мм от всеки сигнален извод на STM32 (fan-out: иначе рано сложено трасе покрай изводите запушва изхода на
останалите); след него маха висящите парчета и пълни масите (клемите — плътно към заливката).

След Freerouting остатъкът се довършва с `doveshvane.py` (собствен рутер: мрежа 0,05 мм, A* на двата слоя,
разкъсване на чужди трасета до 3 мм от краищата; `doveshvane.c` се компилира при пускане с `cc`/`gcc`):

```
python3.12 -I doveshvane.py /tmp/trasirane/osnovna_platka.kicad_pcb /tmp/trasirane/dovarshena.kicad_pcb
```

Пази платката с най-малко несвързани и без грешки в DRC. Резултатът е в `osnovna_platka/trasirana/`.

`pravila.py` записва и `osnovna_platka.kicad_dru`: 0,2 мм при всичко в U1 (изводите на STM32 са на 0,2 мм един от друг
и при масата и неизползваните изводи).

## Правила за домашно производство

- Двуслойна, фоторезист; метализирани отвори; най-малкото свредло 0,5 мм → преход с отвор 0,6 мм и площадка 1,2 мм.
- 0,2 мм разстояние само при изводите на STM32 (LQFP144, стъпка 0,5 мм), иначе 0,25 мм.
- Масите (GND за логиката, GND_IO за изолираната 24 V част) са заливка на двата слоя и служат и като екран; между тях 2 мм по изолационната граница (пунктир на копринения слой).

## План на основната платка

- **Горе:** всички изходи: 6 драйвера (клеми +5V, STEP, DIR, ENA, ALM, GND), честотен регулатор, RS485, изходи 1–8.
- **Долу:** всички входове: 7 енкодера (+5V, 0V, A, B), MPG пулт (гнездо 2 × 3), авариен стоп на пулта, пробник/toolsetter/авариен стоп, крайни изключватели и HOME (14 клеми).
- **Вляво:** 24 V, PE, USB-B и захранването (в посоката на тока). **Вдясно:** 24 V за датчиците, входове 1–8, гнездо за разширителните модули.
- Клемите са едноетажни щепселни 5,08 мм (Phoenix MSTBA), 1 мм една от друга.
- Изходите и входовете са свързани по физическия ред на изводите на чиповете (без кръстосване): буферите към драйверите са 74ACT541 (проходни изводи); при 74HCT165 таблицата „клема → бит“ е в `board.h`.

## Преди производство

Схемата е проектирана по документацията на чиповете, но преди поръчка/изработка трябва да я прегледа човек с опит в платки. Отворено: окончателното трасиране и модулът на MPG пулта (последен).
