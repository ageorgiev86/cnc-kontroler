"""
Основна платка на CNC контролера (версия 3): STM32H723ZGT6 директно на платката.
Copyright (c) 2026 ageorgiev86. Всички права запазени.

Пускане (затвори KiCad без запис преди това):
  "C:\\Program Files\\KiCad\\10.0\\bin\\python.exe" -I osnovna_platka.py "D:\\KiCad_project\\my_first+project"

Основа за процесорната част: схемата на WAVGAT LXB743ZI-P1 (кварци, USB, BOOT0, RESET, VCAP).
На платката: 7 енкодера (TTL, A и B), MPG пулт по RS-485 (гнездо 2 × 3), 6 драйвера (STEP, DIR, ENA, ALM),
крайни изключватели и HOME (6 + 6), пробник, toolsetter, авариен стоп, 8 входа и 8 изхода 24 V
(74HCT165 / 74HCT595 зад изолатор), шпиндел (0–10 V, FWD/REV, RS485), Orange Pi.
Каналите 1–6 са общи: кой е X, Y1, Y2, Z, A, C се избира в Настройки.

Всеки елемент има поле Role: по него platka.py го слага на точно място в плана на платката.
"""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import sch  # noqa: E402
from sch import Sheet, write_project  # noqa: E402
from sch import place as _place  # noqa: E402

PROJECT_TITLE = 'CNC контролер: основна платка (STM32H723ZGT6)'

# ---------------------------------------------------------------------------
# Изводите на STM32H723ZGT6 (LQFP144). Таблицата е и в docs/stm32-pinove.md.
# ---------------------------------------------------------------------------
MCU = {
    # енкодери (таймер в encoder mode: A = CH1, B = CH2)
    'PA0': 'ENC1_A', 'PA1': 'ENC1_B',          # TIM5
    'PA15': 'ENC2_A', 'PB3': 'ENC2_B',         # TIM2
    'PF0': 'ENC3_A', 'PF1': 'ENC3_B',          # TIM23
    'PB4': 'ENC4_A', 'PB5': 'ENC4_B',          # TIM3
    'PB6': 'ENC5_A', 'PB7': 'ENC5_B',          # TIM4
    'PA8': 'ENC6_A', 'PA9': 'ENC6_B',          # TIM1
    'PF11': 'ENC_SP_A', 'PF12': 'ENC_SP_B',    # TIM24
    # MPG пулт: отделен модул по RS-485 (UART4, AF8), посоката от PD3
    'PD0': 'MPG_RX', 'PD1': 'MPG_TX', 'PD3': 'MPG_DE',
    # стъпки и посока, канал 1–6 (един порт: DMA → GPIOD BSRR)
    'PD4': 'STEP1', 'PD5': 'DIR1', 'PD6': 'STEP2', 'PD7': 'DIR2', 'PD8': 'STEP3', 'PD9': 'DIR3',
    'PD10': 'STEP4', 'PD11': 'DIR4', 'PD12': 'STEP5', 'PD13': 'DIR5', 'PD14': 'STEP6', 'PD15': 'DIR6',
    'PB11': 'DRV_EN', 'PE3': 'DRV_FAULT', 'PE9': 'DRV_OE_N',
    # крайни изключватели и HOME, канал 1–6 (активни при 0 след оптрона)
    'PG8': 'LIM1', 'PG9': 'LIM2', 'PG10': 'LIM3', 'PG11': 'LIM4', 'PG12': 'LIM5', 'PG13': 'LIM6',
    'PG14': 'HOME1', 'PG15': 'HOME2', 'PF6': 'HOME3', 'PF7': 'HOME4', 'PF8': 'HOME5', 'PF9': 'HOME6',
    # връзка с Orange Pi (SPI4 slave)
    'PE11': 'LINK_NSS', 'PE12': 'LINK_SCK', 'PE13': 'LINK_MISO', 'PE14': 'LINK_MOSI',
    'PE15': 'LINK_READY', 'PE10': 'LINK_IRQ',
    # входове и изходи (SPI2 master към 74HCT595 / 74HCT165 през изолатора)
    'PB13': 'IO_SCK', 'PB14': 'IO_MISO', 'PB15': 'IO_MOSI', 'PB12': 'IO_LATCH', 'PE7': 'IO_LOAD',
    'PE8': 'IO_OE_N',
    # бързи входове
    'PA7': 'PROBE', 'PB1': 'TOOLSETTER', 'PC13': 'ESTOP_SENSE',
    # шпиндел
    'PB9': 'SPINDLE_PWM', 'PC4': 'SPINDLE_DIR', 'PC5': 'SPINDLE_EN',
    # процесорна част
    'PA11': '.usb_dm', 'PA12': '.usb_dp', 'PA13': 'SWDIO', 'PA14': 'SWCLK',
    'PH0': '.hse_in', 'PH1': '.hse_out', 'PC14': '.lse_in', 'PC15': '.lse_out',
    'PG7': '.led',
    'NRST': 'NRST', 'BOOT0': '.boot0', 'PDR_ON': '+3V3', 'VBAT': '.vbat',
    'VDD': '+3V3', 'VDD33USB': '+3V3', 'VDDA': '.vdda', 'VREF+': '.vdda', 'VSS': 'GND', 'VSSA': 'GND',
    71: '.vcap1', 106: '.vcap2',     # VCAP: всеки със свой кондензатор (не се свързват заедно)
}

FP_R = 'Resistor_SMD:R_0805_2012Metric'
FP_R1206 = 'Resistor_SMD:R_1206_3216Metric'
FP_C = 'Capacitor_SMD:C_0805_2012Metric'
FP_C1206 = 'Capacitor_SMD:C_1206_3216Metric'
FP_C1210 = 'Capacitor_SMD:C_1210_3225Metric'
FP_D_SMA = 'Diode_SMD:D_SMA'
FP_D_SMC = 'Diode_SMD:D_SMC'
FP_SOD = 'Diode_SMD:D_SOD-323'
FP_LED = 'LED_SMD:LED_0805_2012Metric'
FP_PTC = 'Fuse:Fuse_1206_3216Metric'
FP_SOIC14 = 'Package_SO:SOIC-14_3.9x8.7mm_P1.27mm'
FP_SOIC16 = 'Package_SO:SOIC-16_3.9x9.9mm_P1.27mm'
FP_SOIC8 = 'Package_SO:SOIC-8_3.9x4.9mm_P1.27mm'
FP_PC817 = 'Package_DIP:SMDIP-4_W7.62mm'
FP_6N137 = 'Package_DIP:SMDIP-8_W7.62mm'
FP_BTN = 'Button_Switch_SMD:SW_SPST_TL3342'
FP_IDC10 = 'Connector_IDC:IDC-Header_2x05_P2.54mm_Vertical'
FP_IDC14 = 'Connector_IDC:IDC-Header_2x07_P2.54mm_Vertical'


def place(sh, lib_id, prefix, value, x, y, conns, footprint='', unit=1, stub=5.08, ref=None, role=None):
    return _place(sh, lib_id, prefix, value, x, y, conns, footprint=footprint, unit=unit, stub=stub, ref=ref,
                  fields={'Role': role} if role else None)


def fp_term(n):
    """Щепселна клема Phoenix MSTB 2,5 със стъпка 5,08 мм (едноетажна)."""
    return f'Connector_Phoenix_MSTB:PhoenixContact_MSTBA_2,5_{n}-G-5,08_1x{n:02d}_P5.08mm_Horizontal'


def terminal(sh, x, y, value, nets, role):
    n = len(nets)
    return place(sh, f'Connector:Screw_Terminal_01x{n:02d}', 'X', value, x, y,
                 {i + 1: net for i, net in enumerate(nets)}, footprint=fp_term(n), stub=7.62, role=role)


def R(sh, x, y, value, a, b, role, fp=FP_R):
    return place(sh, 'Device:R', 'R', value, x, y, {1: a, 2: b}, footprint=fp, role=role)


def C(sh, x, y, value, a, b, role, fp=FP_C):
    return place(sh, 'Device:C', 'C', value, x, y, {1: a, 2: b}, footprint=fp, role=role)


def flag(sh, x, y, net):
    _place(sh, 'power:PWR_FLAG', '#FLG', 'PWR_FLAG', x, y, {1: net})


def grid(i, cols, x0, y0, dx, dy):
    return x0 + (i % cols) * dx, y0 + (i // cols) * dy


# ---------------------------------------------------------------------------
# Процесор
# ---------------------------------------------------------------------------
def sheet_mcu(project):
    sh = Sheet('mcu', 'STM32H723ZGT6', project, paper='A2')
    place(sh, 'MCU_ST_STM32H7:STM32H723ZGTx', 'U', 'STM32H723ZGT6', 200.66, 160.02, MCU, stub=7.62, role='mcu')

    for i in range(11):
        x, y = grid(i, 11, 25.4, 330.2, 12.7, 0)
        C(sh, x, y, '100nF', '+3V3', 'GND', 'mcu.cvdd')
    C(sh, 165.1, 330.2, '4.7uF', '+3V3', 'GND', 'mcu.cbulk', FP_C1206)
    C(sh, 190.5, 330.2, '4.7uF', '.vcap1', 'GND', 'mcu.cvcap1', FP_C1206)
    C(sh, 203.2, 330.2, '4.7uF', '.vcap2', 'GND', 'mcu.cvcap2', FP_C1206)
    # VBAT: от 3,3 V и от батерията CR2032 през BAT54C (общ катод). Пази backup SRAM (реда на
    # прекъснатата програма, resume.c) при спиране на тока.
    place(sh, 'Diode:BAT54C', 'D', 'BAT54C', 297.18, 330.2, {1: '+3V3', 2: '.bat', 3: '.vbat'}, role='mcu.dbat')
    place(sh, 'Device:Battery_Cell', 'BT', 'CR2032', 322.58, 330.2, {1: '.bat', 2: 'GND'},
          footprint='Battery:BatteryHolder_Keystone_3002_1x2032', role='mcu.bat')
    C(sh, 340.36, 330.2, '100nF', '.vbat', 'GND', 'mcu.cvbat')
    flag(sh, 355.6, 330.2, '.vbat')
    place(sh, 'Device:FerriteBead_Small', 'FB', '600R@100MHz', 228.6, 330.2, {1: '+3V3', 2: '.vdda'},
          footprint='Inductor_SMD:L_0805_2012Metric', role='mcu.fb')
    C(sh, 243.84, 330.2, '1uF', '.vdda', 'GND', 'mcu.cvdda1')
    C(sh, 256.54, 330.2, '100nF', '.vdda', 'GND', 'mcu.cvdda2')
    flag(sh, 271.78, 330.2, '.vdda')

    place(sh, 'Device:Crystal_GND24', 'Y', '25MHz', 360.68, 50.8,
          {1: '.hse_in', 3: '.hse_out', 2: 'GND', 4: 'GND'}, footprint='Crystal:Crystal_SMD_3225-4Pin_3.2x2.5mm',
          role='mcu.hse')
    C(sh, 381, 50.8, '22pF', '.hse_in', 'GND', 'mcu.chse1')
    C(sh, 393.7, 50.8, '22pF', '.hse_out', 'GND', 'mcu.chse2')
    place(sh, 'Device:Crystal', 'Y', '32.768kHz', 360.68, 81.28, {1: '.lse_in', 2: '.lse_out'},
          footprint='Crystal:Crystal_SMD_3215-2Pin_3.2x1.5mm', role='mcu.lse')
    C(sh, 381, 81.28, '6pF', '.lse_in', 'GND', 'mcu.clse1')
    C(sh, 393.7, 81.28, '6pF', '.lse_out', 'GND', 'mcu.clse2')

    R(sh, 360.68, 116.84, '10k', '.boot0', 'GND', 'mcu.rboot')
    place(sh, 'Switch:SW_Push', 'SW', 'BOOT0', 381, 116.84, {1: '+3V3', 2: '.boot0'}, footprint=FP_BTN,
          role='mcu.swboot')
    C(sh, 360.68, 142.24, '100nF', 'NRST', 'GND', 'mcu.crst')
    place(sh, 'Switch:SW_Push', 'SW', 'RESET', 381, 142.24, {1: 'NRST', 2: 'GND'}, footprint=FP_BTN,
          role='mcu.swrst')
    R(sh, 360.68, 170.18, '1k', '+3V3', '.led_a', 'mcu.rled')
    place(sh, 'Device:LED', 'D', 'син', 381, 182.88, {'A': '.led_a', 'K': '.led'}, footprint=FP_LED,
          role='mcu.led')

    place(sh, 'Connector:USB_B', 'J', 'USB', 360.68, 241.3, {
        'VBUS': 'VBUS', 'GND': 'GND', 'Shield': 'GND', 'D+': '.usb_dp_c', 'D-': '.usb_dm_c'},
        footprint='Connector_USB:USB_B_OST_USB-B1HSxx_Horizontal', stub=7.62, role='usb.j')
    place(sh, 'Power_Protection:USBLC6-2SC6', 'U', 'USBLC6-2SC6', 444.5, 241.3, {
        'VBUS': 'VBUS', 'GND': 'GND', 1: '.usb_dp_c', 6: '.usb_dp', 3: '.usb_dm_c', 4: '.usb_dm'}, stub=7.62,
        role='usb.esd')
    place(sh, 'Connector_Generic:Conn_01x05', 'J', 'SWD', 444.5, 50.8,
          {1: '+3V3', 2: 'SWCLK', 3: 'GND', 4: 'SWDIO', 5: 'NRST'},
          footprint='Connector_PinHeader_2.54mm:PinHeader_1x05_P2.54mm_Vertical', role='mcu.swd')

    sh.text('Процесорната част е по схемата на WAVGAT LXB743ZI-P1: ядро на LDO (VCAP 2 × 4,7 uF),\n'
            'HSE 25 MHz (22 pF), LSE 32,768 kHz (6 pF), BOOT0 с 10k към маса, PDR_ON към 3,3 V.\n'
            'VBAT: BAT54C от 3,3 V и от батерия CR2032 (пази backup SRAM при спиране на тока).\n'
            'Записване: дръж BOOT0, натисни RESET → USB DFU (кабел USB A–B). Или SWD конектора (ST-Link).\n'
            'LQFP144 със стъпка 0,5 мм: трасета и разстояния 0,2 мм само при изводите на чипа.',
            25.4, 355.6)
    return sh


# ---------------------------------------------------------------------------
# Захранване
# ---------------------------------------------------------------------------
def sheet_power(project):
    sh = Sheet('zahranvane', 'Захранване', project)
    y = 60.96
    terminal(sh, 30.48, y, '24V ВХОД', ['.vin', 'GND'], 'pwr.term')
    place(sh, 'Device:Fuse', 'F', '2A', 60.96, y, {1: '.vin', 2: '.vin_f'}, footprint='Fuse:Fuse_1812_4532Metric',
          role='pwr.fuse')
    place(sh, 'Device:D_Schottky', 'D', 'SS54', 91.44, y, {'A': '.vin_f', 'K': '+24V'}, footprint=FP_D_SMC,
          role='pwr.drev')
    place(sh, 'Device:D_TVS', 'D', 'SMAJ26CA', 121.92, y, {1: '+24V', 2: 'GND'}, footprint=FP_D_SMA, role='pwr.tvs')
    place(sh, 'Device:C_Polarized', 'C', '220uF 50V', 149.86, y, {1: '+24V', 2: 'GND'},
          footprint='Capacitor_SMD:CP_Elec_10x10.5', role='pwr.cbulk')
    sh.pwr_flag('+24V', 175.26, y + 10.16)

    # 24 V → 5 V: LM2596S-5.0 (до 3 A); 5 V от USB идва през диод
    C(sh, 30.48, 121.92, '100nF 50V', '+24V', 'GND', 'pwr.cin')
    place(sh, 'Regulator_Switching:LM2596S-5', 'U', 'LM2596S-5.0', 63.5, 121.92,
          {'VIN': '+24V', 'GND': 'GND', '~{ON}/OFF': 'GND', 'OUT': '.sw', 'FB': '.5v_mod'}, stub=7.62,
          role='pwr.reg')
    place(sh, 'Device:D_Schottky', 'D', 'SS54', 88.9, 139.7, {'K': '.sw', 'A': 'GND'}, footprint=FP_D_SMC,
          role='pwr.dcatch')
    place(sh, 'Device:L', 'L', '33uH 3A', 101.6, 121.92, {1: '.sw', 2: '.5v_mod'},
          footprint='Inductor_SMD:L_12x12mm_H8mm', role='pwr.ind')
    place(sh, 'Device:C_Polarized', 'C', '220uF 10V', 111.76, 139.7, {1: '.5v_mod', 2: 'GND'},
          footprint='Capacitor_SMD:CP_Elec_8x10', role='pwr.cout')
    place(sh, 'Device:D_Schottky', 'D', 'SS34', 127, 121.92, {'A': '.5v_mod', 'K': '+5V'}, footprint=FP_D_SMA,
          role='pwr.dor')
    place(sh, 'Device:D_Schottky', 'D', 'SS34', 127, 147.32, {'A': 'VBUS', 'K': '+5V'}, footprint=FP_D_SMA,
          role='pwr.dusb')
    C(sh, 152.4, 121.92, '10uF', '+5V', 'GND', 'pwr.c5', FP_C1206)
    sh.pwr_flag('+5V', 167.64, 134.62)
    place(sh, 'Regulator_Linear:AP2112K-3.3', 'U', 'AP2112K-3.3', 210.82, 121.92,
          {'VIN': '+5V', 'EN': '+5V', 'GND': 'GND', 'VOUT': '+3V3'}, stub=7.62, role='pwr.ldo')
    C(sh, 238.76, 121.92, '10uF', '+3V3', 'GND', 'pwr.c33', FP_C1206)
    R(sh, 287.02, 121.92, '1k', '+3V3', '.led3', 'pwr.rled')
    place(sh, 'Device:LED', 'D', 'зелен', 304.8, 134.62, {'A': '.led3', 'K': 'GND'}, footprint=FP_LED, role='pwr.led')

    # 24 V за датчиците и изходите (изолираната част) и 5 V от същата страна
    terminal(sh, 30.48, 190.5, '24V ДАТЧИЦИ', ['+24V_IO', 'GND_IO'], 'io24.term')
    place(sh, 'Device:Fuse', 'F', '3A', 60.96, 190.5, {1: '+24V_IO', 2: '.24io_f'},
          footprint='Fuse:Fuse_1812_4532Metric', role='io24.fuse')
    place(sh, 'Device:D_TVS', 'D', 'SMAJ26CA', 91.44, 190.5, {1: '+24V_IO', 2: 'GND_IO'}, footprint=FP_D_SMA,
          role='io24.tvs')
    C(sh, 116.84, 190.5, '10uF 50V', '.24io_f', 'GND_IO', 'io24.cin', FP_C1210)
    place(sh, 'Regulator_Linear:L78L05_SOT89', 'U', '78L05', 149.86, 190.5,
          {'IN': '.24io_f', 'GND': 'GND_IO', 'OUT': '+5V_IO'}, stub=7.62, role='io24.reg')
    C(sh, 180.34, 190.5, '10uF', '+5V_IO', 'GND_IO', 'io24.cout', FP_C1206)
    flag(sh, 205.74, 190.5, '+24V_IO')
    flag(sh, 220.98, 190.5, 'GND_IO')
    flag(sh, 236.22, 190.5, '.24io_f')

    terminal(sh, 30.48, 241.3, 'ЕКРАН / PE', ['SHIELD', 'SHIELD'], 'pe.term')
    C(sh, 60.96, 241.3, '10nF 1kV', 'SHIELD', 'GND', 'pe.c', FP_C1206)
    R(sh, 76.2, 241.3, '1M', 'SHIELD', 'GND', 'pe.r', FP_R1206)

    sh.text('Логиката: 24 V → LM2596S-5.0 (5 V, до 3 A) → AP2112K (3,3 V). При програмиране 5 V идва и от USB.\n'
            '24V ДАТЧИЦИ захранва датчиците, 8-те изхода и изолираната 5 V част (78L05, под 20 mA).\n'
            'Масите GND и GND_IO са разделени (изолатор ISO7761); може да е същият 24 V блок или отделен.',
            30.48, 266.7)
    return sh


# ---------------------------------------------------------------------------
# Стъпки, посока, разрешение и аларма (6 канала)
# ---------------------------------------------------------------------------
def sheet_stepdir(project):
    sh = Sheet('stapki', 'Стъпки и драйвери', project, paper='A2')
    for k in range(3):
        a, b = 2 * k + 1, 2 * k + 2
        x, y = 50.8 + k * 127, 63.5
        # 74ACT541: входовете A0–A7 от едната страна, изходите Y0–Y7 срещу тях от другата.
        # Изходите са по реда на изводите на клемите (ENA, DIR, STEP на канал a, после на канал b): без кръстосване.
        place(sh, '74xx:74AHCT541', 'U', '74ACT541', x, y, {
            'A0': 'DRV_EN', 'Y0': f'.o{a}_ena', 'A1': f'DIR{a}', 'Y1': f'.o{a}_dir', 'A2': f'STEP{a}', 'Y2': f'.o{a}_step',
            'A3': 'DRV_EN', 'Y3': f'.o{b}_ena', 'A4': f'DIR{b}', 'Y4': f'.o{b}_dir', 'A5': f'STEP{b}', 'Y5': f'.o{b}_step',
            'A6': 'GND', 'A7': 'GND', 'G1': 'DRV_OE_N', 'G2': 'DRV_OE_N', 'VCC': '+5V', 'GND': 'GND'},
            footprint='Package_SO:SOIC-20W_7.5x12.8mm_P1.27mm', stub=7.62, role=f'drv.buf{k + 1}')
        C(sh, x + 38.1, y, '100nF', '+5V', 'GND', f'drv.cbuf{k + 1}')
    R(sh, 431.8, 63.5, '10k', '+3V3', 'DRV_OE_N', 'drv.roe')
    R(sh, 152.4, 139.7, '10k', '+3V3', 'DRV_FAULT', 'drv.rflt')
    C(sh, 165.1, 139.7, '10nF', 'DRV_FAULT', 'GND', 'drv.cflt')
    for i in range(1, 7):
        place(sh, 'Device:D_Schottky', 'D', 'BAT54', 190.5 + (i - 1) * 25.4, 139.7,
              {'A': 'DRV_FAULT', 'K': f'.alm{i}'}, footprint=FP_SOD, role=f'drv{i}.dalm')
    for i in range(1, 7):
        terminal(sh, 38.1 + (i - 1) * 76.2, 203.2, f'ДРАЙВЕР {i}',
                 ['+5V', f'.o{i}_step', f'.o{i}_dir', f'.o{i}_ena', f'.alm{i}', 'GND'], f'drv{i}.term')
    sh.text('Клеми: 1 +5V  2 STEP  3 DIR  4 ENA  5 ALM  6 GND.\n'
            'Свързване (общ анод): PUL+, DIR+, ENA+ на драйвера → клема 1 (+5V); PUL−, DIR−, ENA− → клеми 2, 3, 4.\n'
            'Изход 0 включва оптрона в драйвера (firmware-ът задава полярността). ALM+ → клема 5, ALM− → клема 6.\n'
            'ALM: драйверът да е настроен изходът му да провежда при аларма; тогава DRV_FAULT = 0.\n'
            'DRV_OE_N = 1 (резистор 10k): при включване и рестарт изходите на 74ACT541 са изключени.',
            38.1, 254)
    return sh


# ---------------------------------------------------------------------------
# Енкодери TTL (A, B) и MPG
# ---------------------------------------------------------------------------
GATE_PINS = {1: (1, 2), 2: (3, 4), 3: (5, 6), 4: (9, 8), 5: (11, 10), 6: (13, 12)}


def sheet_encoders(project):
    sh = Sheet('enkoderi', 'Енкодери и MPG', project, paper='A1')
    chans = [(str(i), f'ENC{i}', f'ЕНКОДЕР {i}') for i in range(1, 7)] + [('sp', 'ENC_SP', 'ЕНКОДЕР ШПИНДЕЛ')]
    sigs = []
    for c, net, _ in chans:
        for s_ in 'AB':
            sigs.append((f'enc{c}.{s_.lower()}', f'.e{c}{s_.lower()}', f'{net}_{s_}', f'.e{c}_5v'))
    chips = []
    for k in range(3):
        ref = sch.next_ref('U')
        chips.append(ref)
        place(sh, '74xx:74HC14', 'U', '74LVC14A', 50.8 + k * 63.5, 40.64, {14: '+3V3', 7: 'GND'},
              footprint=FP_SOIC14, unit=7, ref=ref, role=f'enc.buf{k + 1}')
        C(sh, 66.04 + k * 63.5, 40.64, '100nF', '+3V3', 'GND', f'enc.cbuf{k + 1}')
    for n, (role, conn_net, out_net, v5) in enumerate(sigs):
        x, y = grid(n, 4, 25.4, 76.2, 104.14, 22.86)
        filt = conn_net + '_f'
        R(sh, x, y, '10k', v5, conn_net, role + '.pu')
        R(sh, x + 12.7, y, '1k', conn_net, filt, role + '.r')
        C(sh, x + 25.4, y, '100pF', filt, 'GND', role + '.c')
        gate = n % 6 + 1
        place(sh, '74xx:74HC14', 'U', '74LVC14A', x + 45.72, y, {GATE_PINS[gate][0]: filt, GATE_PINS[gate][1]: out_net},
              footprint=FP_SOIC14, unit=gate, ref=chips[n // 6])
    for gate in (3, 4, 5, 6):   # неизползваните вентили на последния чип: вход към маса
        place(sh, '74xx:74HC14', 'U', '74LVC14A', 330.2 + gate * 25.4, 40.64, {GATE_PINS[gate][0]: 'GND'},
              footprint=FP_SOIC14, unit=gate, ref=chips[2])

    # клеми: 1 +5V, 2 0V, 3 A, 4 B
    for i, (c, net, name) in enumerate(chans):
        x = 30.48 + i * 76.2
        place(sh, 'Device:Polyfuse', 'F', '0.25A', x + 30.48, 254, {1: '+5V', 2: f'.e{c}_5v'}, footprint=FP_PTC,
              role=f'enc{c}.ptc')
        terminal(sh, x, 292.1, name, [f'.e{c}_5v', 'GND', f'.e{c}a', f'.e{c}b'], f'enc{c}.term')

    # MPG пулт: отделен модул (собствен микроконтролер), връзка RS-485, гнездо 2 × 3:
    # 1 +24V (през предпазител)  2 0V  3 A  4 B  5–6 авариен стоп на пулта (NC, към клемата за релето)
    place(sh, 'Interface_UART:THVD1450D', 'U', 'THVD1450', 50.8, 381, {
        'VCC': '+3V3', 'GND': 'GND', 'DI': 'MPG_TX', 'RO': 'MPG_RX', 'DE': 'MPG_DE', '~{RE}': 'MPG_DE',
        'A': '.mpg_a', 'B': '.mpg_b'}, stub=7.62, role='mpg.rs.u')
    C(sh, 76.2, 365.76, '100nF', '+3V3', 'GND', 'mpg.rs.c')
    R(sh, 96.52, 381, '680', '+3V3', '.mpg_a', 'mpg.rs.rpu')
    R(sh, 109.22, 381, '680', '.mpg_b', 'GND', 'mpg.rs.rpd')
    R(sh, 121.92, 381, '120', '.mpg_a', '.mpg_b', 'mpg.rs.rt')
    place(sh, 'Device:Polyfuse', 'F', '0.25A', 152.4, 381, {1: '+24V', 2: '.mpg_24v'}, footprint=FP_PTC,
          role='mpg.ptc')
    place(sh, 'Connector_Generic:Conn_02x03_Odd_Even', 'J', 'MPG', 190.5, 381, {
        1: '.mpg_24v', 2: 'GND', 3: '.mpg_a', 4: '.mpg_b', 5: '.mpg_es1', 6: '.mpg_es2'},
        footprint='Connector_IDC:IDC-Header_2x03_P2.54mm_Vertical', stub=15.24, role='mpg.j')
    terminal(sh, 241.3, 381, 'АВ. СТОП ПУЛТ', ['.mpg_es1', '.mpg_es2'], 'mpg.es.term')

    sh.text('Енкодери TTL (едностранни, 5 V). Клеми: 1 +5V  2 0V  3 A  4 B.\n'
            'При енкодер с DB-9: 7 → +5V, 2 → 0V, 6 → A, 8 → B; извод 4 (екран) → PE в шкафа.\n'
            'Всеки сигнал: 10k към 5 V (за изходи с отворен колектор), 1k + 100 pF, Шмитов 74LVC14 на 3,3 V.\n'
            '74LVC14 обръща сигнала: firmware-ът задава обратна полярност на таймера. Кабели: екранирани, до 2–3 м.\n'
            'MPG пулт (гнездо 2 × 3): 1 +24V  2 0V  3 RS-485 A  4 RS-485 B  5–6 авариен стоп (NC).\n'
            'Пултът е отделен модул: на всяка 1 ms праща колело, ос, ×1/×10/×100, Старт, Пауза, Стоп. Без пакети 20 ms →\n'
            'плавно спиране. Аварийният стоп минава през клемата АВ. СТОП ПУЛТ към релето за безопасност.',
            30.48, 520.7)
    return sh


# ---------------------------------------------------------------------------
# Входове 24 V с оптрони: пробник, toolsetter, авариен стоп, крайни изключватели и HOME
# ---------------------------------------------------------------------------
def sheet_fast(project):
    sh = Sheet('barzi', 'Пробник, крайни, авариен стоп', project, paper='A2')
    # пробник и toolsetter: 6N137 (под 0,1 µs)
    for i, (name, net) in enumerate((('probe', 'PROBE'), ('tool', 'TOOLSETTER'))):
        x, y = 30.48 + i * 121.92, 50.8
        inn, a = f'.{name}_in', f'.{name}_a'
        R(sh, x, y, '3k3', inn, a, f'in.{name}.r', FP_R1206)
        place(sh, 'Device:D', 'D', '1N4148W', x + 15.24, y + 10.16, {'A': 'GND_IO', 'K': a}, footprint=FP_SOD,
              role=f'in.{name}.d')
        place(sh, 'Isolator:6N137', 'U', '6N137', x + 45.72, y, {'A': a, 'C': 'GND_IO', 'GND': 'GND', 'VO': net,
                                                                  'EN': '+5V', 'VCC': '+5V'},
              footprint=FP_6N137, stub=7.62, role=f'in.{name}.u')
        R(sh, x + 76.2, y, '1k', '+3V3', net, f'in.{name}.pu')
        C(sh, x + 88.9, y, '100nF', '+5V', 'GND', f'in.{name}.c')
    # авариен стоп (наблюдение): PC817
    x, y = 274.32, 50.8
    R(sh, x, y, '4k7', '.estop_in', '.estop_a', 'in.estop.r', FP_R1206)
    place(sh, 'Device:D', 'D', '1N4148W', x + 15.24, y + 10.16, {'A': 'GND_IO', 'K': '.estop_a'}, footprint=FP_SOD,
          role='in.estop.d')
    place(sh, 'Isolator:PC817', 'U', 'PC817', x + 40.64, y, {1: '.estop_a', 2: 'GND_IO', 3: 'GND', 4: 'ESTOP_SENSE'},
          footprint=FP_PC817, stub=7.62, role='in.estop.u')
    R(sh, x + 66.04, y, '10k', '+3V3', 'ESTOP_SENSE', 'in.estop.pu')
    C(sh, x + 78.74, y, '10nF', 'ESTOP_SENSE', 'GND', 'in.estop.c')
    terminal(sh, 401.32, 45.72, 'ПРОБНИК / АВ. СТОП', ['+24V_IO', 'GND_IO', '.probe_in', '.tool_in', '.estop_in'],
             'in.term')

    # крайни изключватели и HOME: 12 входа през 3 × LTV-847 (4 оптрона в корпус)
    sigs = [f'LIM{i}' for i in range(1, 7)] + [f'HOME{i}' for i in range(1, 7)]
    optos = [sch.next_ref('U') for _ in range(3)]
    for n, net in enumerate(sigs):
        x, y = grid(n, 4, 30.48, 101.6, 101.6, 30.48)
        k, u = n // 4, n % 4 + 1
        inn, a = f'.{net.lower()}_in', f'.{net.lower()}_a'
        role = f'lim.{net.lower()}'
        R(sh, x, y, '4k7', inn, a, role + '.r', FP_R1206)
        place(sh, 'Device:D', 'D', '1N4148W', x + 15.24, y + 10.16, {'A': 'GND_IO', 'K': a}, footprint=FP_SOD,
              role=role + '.d')
        an, ca, em, co = 2 * u - 1, 2 * u, 17 - 2 * u, 18 - 2 * u
        place(sh, 'Isolator:LTV-847', 'U', 'LTV-847', x + 40.64, y, {an: a, ca: 'GND_IO', em: 'GND', co: net},
              footprint='Package_DIP:SMDIP-16_W7.62mm', stub=7.62, unit=u, ref=optos[k], role=f'lim.opto{k + 1}')
        R(sh, x + 66.04, y, '10k', '+3V3', net, role + '.pu')
        C(sh, x + 78.74, y, '10nF', net, 'GND', role + '.c')
    terminal(sh, 401.32, 228.6, 'КРАЙНИ / HOME', ['+24V_IO'] + [f'.{n.lower()}_in' for n in sigs] + ['GND_IO'],
             'lim.term')

    sh.text('Клема ПРОБНИК / АВ. СТОП: 1 +24V  2 0V  3 пробник  4 toolsetter  5 авариен стоп (контрол).\n'
            'Клема КРАЙНИ / HOME: 1 +24V  2–7 LIM 1–6  8–13 HOME 1–6  14 0V.\n'
            'Датчиците са PNP 24 V. Крайните изключватели на една ос (+ и −) са NC, последователно.\n'
            'Изходът на оптрона е 0 при активен вход; firmware-ът обръща сигнала.\n'
            'Аварийният стоп е хардуерна верига (реле за безопасност); тук само се наблюдава.',
            30.48, 304.8)
    return sh


# ---------------------------------------------------------------------------
# Входове и изходи 8 + 8 (изолирани): 74HCT165, 74HCT595, TBD62783, разширения
# ---------------------------------------------------------------------------
def sheet_io(project):
    sh = Sheet('vhodove_izhodi', 'Входове и изходи 8 + 8', project, paper='A2')
    place(sh, 'Isolator:ISO7761DW', 'U', 'ISO7761', 76.2, 63.5, {
        'Vcc1': '+3V3', 'GND1': 'GND', 'Vcc2': '+5V_IO', 'GND2': 'GND_IO',
        'INA': 'IO_SCK', 'INB': 'IO_MOSI', 'INC': 'IO_LATCH', 'IND': 'IO_LOAD', 'INE': 'IO_OE_N',
        'OUTA': '.sck', 'OUTB': '.mosi', 'OUTC': '.latch', 'OUTD': '.load', 'OUTE': '.oe',
        'INF': '.miso', 'OUTF': 'IO_MISO'}, stub=7.62, role='io.iso')
    C(sh, 30.48, 101.6, '100nF', '+3V3', 'GND', 'io.ciso1')
    C(sh, 45.72, 101.6, '100nF', '+5V_IO', 'GND_IO', 'io.ciso2')
    R(sh, 60.96, 101.6, '10k', '+5V_IO', '.oe', 'io.roe')

    place(sh, '74xx:74HCT595', 'U', '74HCT595', 177.8, 63.5, {
        'SER': '.mosi', 'SRCLK': '.sck', 'RCLK': '.latch', '~{OE}': '.oe', '~{SRCLR}': '+5V_IO',
        **{f'Q{c}': f'.o{i + 1}' for i, c in enumerate('ABCDEFGH')}, "QH'": '.ser_out',
        'VCC': '+5V_IO', 'GND': 'GND_IO'}, footprint=FP_SOIC16, stub=7.62, role='io.595')
    C(sh, 205.74, 40.64, '100nF', '+5V_IO', 'GND_IO', 'io.c595')
    place(sh, 'Transistor_Array:TBD62783A', 'U', 'TBD62783A', 266.7, 63.5, {
        **{f'I{j}': f'.o{9 - j}' for j in range(1, 9)}, **{f'O{j}': f'.out{9 - j}' for j in range(1, 9)},
        'VCC': '+24V_IO', 'GND': 'GND_IO'}, footprint='Package_SO:SOIC-18W_7.5x11.6mm_P1.27mm', stub=7.62,
        role='io.tbd')
    C(sh, 294.64, 40.64, '100nF 50V', '+24V_IO', 'GND_IO', 'io.ctbd')
    terminal(sh, 345.44, 63.5, 'ИЗХОДИ 1-8', [f'.out{i}' for i in range(1, 9)] + ['GND_IO', 'GND_IO'], 'io.out.term')

    for i in range(1, 9):
        x, y = grid(i - 1, 4, 30.48, 160.02, 50.8, 30.48)
        R(sh, x, y, '22k', f'.in{i}_t', f'.in{i}', f'io.in{i}.r1', FP_R1206)
        R(sh, x + 12.7, y, '4k7', f'.in{i}', 'GND_IO', f'io.in{i}.r2')
        C(sh, x + 25.4, y, '100nF', f'.in{i}', 'GND_IO', f'io.in{i}.c')
    place(sh, '74xx:74HC165', 'U', '74HCT165', 266.7, 172.72, {
        **{'D6': '.in1', 'D7': '.in2', 'D0': '.in3', 'D1': '.in4', 'D2': '.in5', 'D3': '.in6', 'D4': '.in7', 'D5': '.in8'},  # клема k → D по физическия ред на изводите (без кръстосване) 'DS': '.ser_in', 'CP': '.sck', '~{PL}': '.load',
        '~{CE}': 'GND_IO', 'Q7': '.miso', 'VCC': '+5V_IO', 'GND': 'GND_IO'}, footprint=FP_SOIC16, stub=7.62,
        role='io.165')
    C(sh, 294.64, 147.32, '100nF', '+5V_IO', 'GND_IO', 'io.c165')
    R(sh, 213.36, 210.82, '10k', '.ser_in', 'GND_IO', 'io.rser')
    terminal(sh, 345.44, 172.72, 'ВХОДОВЕ 1-8', [f'.in{i}_t' for i in range(1, 9)] + ['+24V_IO', 'GND_IO'],
             'io.in.term')

    place(sh, 'Connector_Generic:Conn_02x05_Odd_Even', 'J', 'РАЗШИРЕНИЕ', 177.8, 254, {
        1: 'GND_IO', 2: '+5V_IO', 3: '.sck', 4: 'GND_IO', 5: '.latch', 6: '.load', 7: '.oe', 8: '.ser_out',
        9: '.ser_in', 10: 'GND_IO'}, footprint=FP_IDC10, stub=15.24, role='io.exp')

    sh.text('Изходи: 74HCT595 → TBD62783 (PNP, до 0,5 A на изход, 24 V). Клеми: 1–8 изходи, 9–10 0V.\n'
            'При включване OE = 1 (резистор 10k): всички изходи са изключени, докато firmware-ът не ги разреши.\n'
            'Индуктивни товари (релета, вентили): обратен диод на самия товар.\n'
            'Входове: PNP 24 V; делител 22k/4k7 и филтър 100 nF (~0,5 ms). Клеми: 1–8 входове, 9 +24V, 10 0V.\n'
            'Разширение: следващата платка взима .ser_out като SER и връща своя Q7 като .ser_in.',
            30.48, 279.4)
    return sh


# ---------------------------------------------------------------------------
# Orange Pi
# ---------------------------------------------------------------------------
def sheet_links(project):
    sh = Sheet('orangepi', 'Orange Pi', project)
    for i, (net, loc) in enumerate((('LINK_MISO', '.miso_r'), ('LINK_READY', '.ready_r'), ('LINK_IRQ', '.irq_r'))):
        R(sh, 45.72 + i * 15.24, 50.8, '33', net, loc, f'opi.r{i + 1}')
    place(sh, 'Connector_Generic:Conn_02x07_Odd_Even', 'J', 'ORANGE PI', 101.6, 101.6, {
        1: 'GND', 2: 'LINK_SCK', 3: 'GND', 4: 'LINK_MOSI', 5: 'GND', 6: '.miso_r', 7: 'LINK_NSS',
        8: '.ready_r', 9: '.irq_r', 10: 'GND', 11: 'OPI_TX', 12: 'OPI_RX', 13: 'OPI_RTS', 14: 'GND'},
        footprint=FP_IDC14, stub=15.24, role='opi.j')
    sh.text('Плосък кабел IDC 14 до Orange Pi: SPI (STM32 е slave), READY, IRQ, UART за RS485.\n'
            'Захранване между платките не се свързва, само GND.', 25.4, 152.4)
    return sh


# ---------------------------------------------------------------------------
# Шпиндел
# ---------------------------------------------------------------------------
def sheet_spindle(project):
    sh = Sheet('shpindel', 'Шпиндел', project)
    R(sh, 30.48, 50.8, '10k', 'SPINDLE_PWM', '.pwm_f', 'vfd.rf')
    C(sh, 45.72, 63.5, '1uF', '.pwm_f', 'GND', 'vfd.cf')
    ref = place(sh, 'Amplifier_Operational:LM358', 'U', 'LM358', 81.28, 55.88,
                {'+': '.pwm_f', '-': '.fb', 1: '.aout'}, footprint=FP_SOIC8, unit=1, role='vfd.op')
    place(sh, 'Amplifier_Operational:LM358', 'U', 'LM358', 81.28, 99.06,
          {'+': 'GND', '-': '.u2o', 7: '.u2o'}, footprint=FP_SOIC8, unit=2, ref=ref)
    place(sh, 'Amplifier_Operational:LM358', 'U', 'LM358', 127, 99.06,
          {'V+': '+24V', 'V-': 'GND'}, footprint=FP_SOIC8, unit=3, ref=ref)
    C(sh, 144.78, 99.06, '100nF 50V', '+24V', 'GND', 'vfd.cop')
    R(sh, 111.76, 50.8, '20k', '.aout', '.fb', 'vfd.rfb')
    R(sh, 111.76, 71.12, '10k', '.fb', 'GND', 'vfd.rg')
    R(sh, 129.54, 50.8, '100', '.aout', '.avi', 'vfd.rout')
    for i, (sig, out, nm) in enumerate((('SPINDLE_EN', '.fwd', 'fwd'), ('SPINDLE_DIR', '.rev', 'rev'))):
        x = 182.88 + i * 60.96
        R(sh, x, 50.8, '470', sig, f'{out}_a', f'vfd.{nm}.r')
        place(sh, 'Isolator:PC817', 'U', 'PC817', x + 27.94, 50.8,
              {1: f'{out}_a', 2: 'GND', 4: out, 3: '.vfd_com'}, footprint=FP_PC817, stub=7.62, role=f'vfd.{nm}.u')
    terminal(sh, 203.2, 109.22, 'ЧЕСТОТЕН РЕГУЛАТОР', ['.avi', 'GND', '.fwd', '.rev', '.vfd_com'], 'vfd.term')

    place(sh, 'Interface_UART:THVD1450D', 'U', 'THVD1450', 81.28, 162.56, {
        'VCC': '+3V3', 'GND': 'GND', 'DI': 'OPI_TX', 'RO': 'OPI_RX', 'DE': 'OPI_RTS', '~{RE}': 'OPI_RTS',
        'A': '.rs_a', 'B': '.rs_b'}, stub=7.62, role='rs.u')
    C(sh, 106.68, 147.32, '100nF', '+3V3', 'GND', 'rs.c')
    R(sh, 127, 162.56, '680', '+3V3', '.rs_a', 'rs.rpu')
    R(sh, 139.7, 162.56, '680', '.rs_b', 'GND', 'rs.rpd')
    R(sh, 152.4, 162.56, '120', '.rs_a', '.term', 'rs.rt')
    place(sh, 'Connector_Generic:Conn_01x02', 'JP', 'ТЕРМИНАТОР', 172.72, 162.56, {1: '.term', 2: '.rs_b'},
          footprint='Connector_PinHeader_2.54mm:PinHeader_1x02_P2.54mm_Vertical', role='rs.jp')
    terminal(sh, 203.2, 162.56, 'RS485', ['.rs_a', '.rs_b', 'GND'], 'rs.term')
    sh.text('Регулатор: 1 AVI (0–10 V)  2 ACM  3 FWD  4 REV  5 COM. 0–10 V не е изолиран.\n'
            'RS485 (Modbus от Orange Pi): джъмперът включва 120 Ω само на края на линията.', 25.4, 213.36)
    return sh


def main():
    folder = sys.argv[1] if len(sys.argv) > 1 else '.'
    name = [fn[:-len('.kicad_pro')] for fn in os.listdir(folder) if fn.endswith('.kicad_pro')]
    if not name:
        sys.exit('няма .kicad_pro в ' + folder)
    name = name[0]
    for old in ('wavgat', 'skali', 'vhodove', 'vrazki'):
        p = os.path.join(folder, old + '.kicad_sch')
        if os.path.exists(p):
            os.remove(p)
    sheets = [sheet_mcu(name), sheet_power(name), sheet_stepdir(name), sheet_encoders(name),
              sheet_fast(name), sheet_io(name), sheet_links(name), sheet_spindle(name)]
    write_project(folder, name, sheets, PROJECT_TITLE)
    print('записано в', folder, 'проект', name)


if __name__ == '__main__':
    main()
