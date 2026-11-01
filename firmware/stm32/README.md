# Firmware за STM32H723ZG

Изпълнителната част на CNC контролера: генериране на стъпки, енкодери, затворен контур, пробник, входове и изходи, безопасност. Архитектурата е в [`../../docs/arhitektura.md`](../../docs/arhitektura.md).

**Етап 0 (сега):** стартиране, такт 400 MHz от вътрешния осцилатор, кешове, MPU и мигащ светодиод. Проверява, че инструментите, програматорът и платката работят.

## Нужно на компютъра (Windows 10)

1. **STM32CubeCLT** (st.com): компилатор `arm-none-eabi-gcc`, CMake, Ninja, ST-LINK GDB сървър, STM32CubeProgrammer.
2. **VS Code** с разширенията:
   - **STM32Cube for Visual Studio Code** (STMicroelectronics);
   - **C/C++** и **CMake Tools** (Microsoft);
   - по желание **Cortex-Debug** (има готова конфигурация в `.vscode/launch.json`).
3. **Git** и външен **ST-LINK** (например ST-LINK V3MINIE), защото платката WAVGAT няма вграден.

## Сваляне

Библиотеките на ST (HAL, CMSIS) са git подмодули, затова хранилището се клонира с `--recursive`:

```bash
git clone --recursive https://github.com/ageorgiev86/cnc-kontroler
```

Ако вече е клонирано без тях:

```bash
git submodule update --init --recursive
```

## Компилиране

- **VS Code:** File → Open Folder → `firmware/stm32`. CMake Tools пита за конфигурация: избери **Debug**. Компилиране с **F7** или бутона Build.
- **Команден ред** (в папка `firmware/stm32`):

```bash
cmake --preset Debug
```

```bash
cmake --build --preset Debug
```

Резултатът е в `build/Debug/`: `cnc_mcu.elf` (за дебъг), `cnc_mcu.hex` и `cnc_mcu.bin` (за записване).

## Свързване на ST-LINK (SWD)

| ST-LINK | Платката |
|---|---|
| SWDIO | PA13 (SWDIO) |
| SWCLK | PA14 (SWCLK) |
| GND | GND |
| 3.3V (VTref) | 3.3V |
| NRST (по желание) | NRST |

## Преди първото записване: провери по платката

Стойностите са в [`Core/Inc/board.h`](Core/Inc/board.h) и **не са проверени** по схемата на WAVGAT:

1. **Светодиод:** по подразбиране PE3. Ако е на друг пин или свети при 0, смени `BOARD_LED_*`.
2. **Захранване на ядрото:** LDO (STM32H723 няма SMPS). Ако настройката не съвпада с платката, чипът спира веднага след старта.
3. **Кварц (HSE):** засега не се използва. Тактът е от вътрешния HSI, така че първата проба работи без значение какъв е кварцът.

Първо свържи с **STM32CubeProgrammer** и провери **Device ID = 0x483** и **flash = 1 MB**. Така се проверява и дали чипът е истински.

## Записване и дебъг

- **VS Code:** F5 (Cortex-Debug или конфигурацията на разширението STM32Cube). Програмата спира в `main()`; продължи и светодиодът трябва да мига 2 пъти в секунда.
- **Само записване:**

```bash
STM32_Programmer_CLI -c port=SWD -w build/Debug/cnc_mcu.hex -v -rst
```

## Ако платката „замръзне“

Ако след записване ST-LINK не се свързва: дръж **BOOT0 на 3.3V**, включи захранването и изтрий паметта:

```bash
STM32_Programmer_CLI -c port=SWD mode=UR -e all
```

После върни BOOT0 на 0 и поправи настройката (най-често захранването на ядрото или пина на светодиода).

## Структура

```
firmware/stm32/
├── CMakeLists.txt            проект
├── CMakePresets.json         конфигурации Debug и Release
├── cmake/                    настройка на компилатора arm-none-eabi
├── STM32H723ZGTX_FLASH.ld    разпределение на паметта
├── Core/Inc/board.h          всичко, зависимо от платката
├── Core/Inc/stm32h7xx_hal_conf.h  включени модули на HAL
├── Core/Src/main.c           старт, такт, MPU, светодиод
├── Core/Src/...              прекъсвания, MSP, системни функции
└── Drivers/                  библиотеки на ST (git подмодули, BSD-3 / Apache-2.0)
```

**Разпределение на паметта:** стекът е в DTCM (най-бърз, но недостъпен за DMA); данните и heap са в AXI SRAM (RAM_D1); буферите за DMA се слагат в RAM_D2 с `__attribute__((section(".dma_buffer")))`.

## Следващи стъпки

1. Проверка на платката (светодиод, кварц) → такт 550 MHz от HSE.
2. USB CDC за съобщения при отстраняване на грешки.
3. SPI връзка с Linux платката (DMA, CRC, heartbeat, watchdog).
4. Генериране на стъпки с таймер + DMA за 1 ос, после за 6.
5. Енкодери на таймерите, затворен контур, следяща грешка.
6. Input capture за пробника и toolsetter-а.
7. Входове и изходи 32 + 32 по SPI с DMA.
