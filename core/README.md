# Ядро: интерпретатор и планер на G-code

Библиотека `cnc-core` на C11, без LVGL и без зависимост от интерфейса. Устройството и поддържаните кодове по стъпки са в [`../docs/interpretator-planer.md`](../docs/interpretator-planer.md).

## Състояние

- **Стъпка 1 (готова):** парсер и интерпретатор: G0–G3, нули G54–G59 и G154, G10, G43/G49, G28/G30, G53, G92, G4, M3–M9, M6, диалекти Mach3 и Haas.
- **Стъпка 2 (в работа):** подпрограми O / M98 P L / M99 и M97 на Haas (до 16 нива) и циклите G73, G81–G83, G85–G89 са готови. Остават разписанието на детайлите и „Изпълни от ред“.
- **Реални програми** в `tests/programs/` (FlatCAM и SolidCAM с постпроцесор за Mach3): обобщението на всяка (`gcsim --summary`) се сравнява с `.expect`. Heatsink и двете от FlatCAM минават изцяло; BAIL-3 чака G41/G42.

## Компилиране и тестове

```sh
cd core
cmake -B build                      # -DCNC_CORE_SANITIZE=ON за ASan + UBSan
cmake --build build
ctest --test-dir build --output-on-failure
```

## gcsim

```sh
./build/gcsim [--haas] [--axes XYZA] [--block-delete] [--summary] програма.nc
```

Извежда командите на интерпретатора, по една на ред, в машинни координати. Грешката отива на stderr с ред и колона.

```
13: SPINDLE CW S8000
14: TLO H1 149.95
15: FEED X-300 Y-200 Z-255.05 F500
16: ARC CW G17 X-290 Y-190 Z-255.05 C-290,-200,-255.05 F800
```

## Тестове с примерни програми

- `tests/gcode/*.nc`: всяка програма има очакван изход `*.out`.
- Първият ред `; test: haas axes=XYZA block-delete` задава настройките.
- Нулите и инструментите се задават в самата програма с G10.
- Нов тест: напишете `.nc` и пуснете `CNC_UPDATE=1 ./build/test_golden tests/gcode`. **Проверете `.out` на ръка** преди да го качите.
