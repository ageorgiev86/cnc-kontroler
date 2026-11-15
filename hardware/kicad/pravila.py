"""
Правила за проектиране на платката (домашно производство: фоторезист, двуслойна).
Copyright (c) 2026 ageorgiev86. Всички права запазени.

Пускане (KiCad затворен):
  "C:\\Program Files\\KiCad\\10.0\\bin\\python.exe" -I pravila.py "D:\\KiCad_project\\my_first+project"

Записва в .kicad_pro: минималните размери, класовете мрежи (сигнал, захранване) и преходите.
Най-малкото свредло е 0,5 мм → преход с отвор 0,6 мм и площадка 1,2 мм.
"""
import json
import os
import sys

RULES = {
    'min_clearance': 0.2,               # само при изводите на STM32 (LQFP144, стъпка 0,5 мм)
    'min_track_width': 0.2,
    'min_via_diameter': 1.0,
    'min_via_annular_width': 0.2,
    'min_through_hole_diameter': 0.5,
    'min_hole_clearance': 0.25,
    'min_hole_to_hole': 0.5,
    'min_copper_edge_clearance': 0.5,
    'min_text_height': 0.8,
}

CLASSES = [
    # име, разстояние, трасе, преход (площадка, отвор)
    ('Default', 0.25, 0.4, 1.2, 0.6),    # сигнали
    ('MCU', 0.2, 0.2, 1.2, 0.6),         # сигналите от STM32 и +3V3: трасе 0,2 мм, за да излиза между изводите
                                         # (стъпка 0,5 мм, площадка 0,3 мм → 0,25 мм до съседния извод)
    ('Power', 0.25, 1.0, 1.4, 0.7),      # 24 V, 5 V, импулсната част
    ('Ground', 0.25, 0.6, 1.2, 0.6),     # масите: заливка на двата слоя, не се трасират
]
POWER_PATTERNS = ['+24V', '+24V_IO', '+5V', '+5V_IO', 'VBUS',
                  '*/5v_mod', '*/sw', '*/vin*', '*/24io_f', '*/24f', '*/e*_5v', '*/mpg_5v', '*/mpg_24v']
GROUND_PATTERNS = ['GND', 'GND_IO']

# Изводите на STM32 (LQFP144, стъпка 0,5 мм) са на 0,2 мм един от друг, каквато и да е мрежата им
# (маса, неизползван извод): 0,2 мм важи за всичко, което е в U1 или влиза в корпуса му.
DRU = """(version 1)

(rule "stm32_lqfp144_0.2mm"
	(constraint clearance (min 0.2mm))
	(condition "A.memberOfFootprint('U1') || B.memberOfFootprint('U1') || A.intersectsCourtyard('U1') || B.intersectsCourtyard('U1')"))
"""


def mcu_patterns():
    """Мрежите, които излизат от STM32 (от речника MCU на схемата), и +3V3."""
    try:
        sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
        from osnovna_platka import MCU
    except Exception:
        return []
    nets = {n for n in MCU.values() if not n.startswith('.') and n not in ('GND', '+3V3')}
    return sorted(nets) + ['+3V3', '/STM32H723ZGT6/*']


def main():
    folder = sys.argv[1]
    name = [f for f in os.listdir(folder) if f.endswith('.kicad_pro')][0]
    path = os.path.join(folder, name)
    pro = json.load(open(path, encoding='utf-8'))
    ds = pro.setdefault('board', {}).setdefault('design_settings', {})
    ds.setdefault('rules', {}).update(RULES)
    ds['track_widths'] = [0.0, 0.3, 0.5, 0.8, 1.2]
    ds['via_dimensions'] = [{'diameter': 0.0, 'drill': 0.0}, {'diameter': 1.2, 'drill': 0.6},
                            {'diameter': 1.4, 'drill': 0.7}]
    ns = pro.setdefault('net_settings', {})
    old = {c.get('name'): c for c in ns.get('classes', [])}
    classes = []
    for i, (cname, clr, tw, vd, vdr) in enumerate(CLASSES):
        c = dict(old.get('Default', {}))
        c.update({'name': cname, 'clearance': clr, 'track_width': tw, 'via_diameter': vd, 'via_drill': vdr,
                  'diff_pair_width': 0.3, 'diff_pair_gap': 0.25,
                  'priority': 2147483647 if cname == 'Default' else i})
        classes.append(c)
    ns['classes'] = classes
    ns['netclass_patterns'] = ([{'netclass': 'Power', 'pattern': p} for p in POWER_PATTERNS] +
                               [{'netclass': 'Ground', 'pattern': p} for p in GROUND_PATTERNS] +
                               [{'netclass': 'MCU', 'pattern': p} for p in mcu_patterns()])
    ds['track_widths'] = [0.0, 0.3, 0.4, 0.6, 1.0]
    json.dump(pro, open(path, 'w', encoding='utf-8'), indent=2, ensure_ascii=False)
    with open(path[:-len('.kicad_pro')] + '.kicad_dru', 'w', encoding='utf-8', newline='\n') as fdru:
        fdru.write(DRU)
    print('правилата са записани в', path)


if __name__ == '__main__':
    main()
