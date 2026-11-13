"""
Разширителен модул: 8 входа + 8 изхода 24 V, верижно след основната платка.
Copyright (c) 2026 ageorgiev86. Всички права запазени.

Пускане (KiCad затворен):
  "C:\\Program Files\\KiCad\\10.0\\bin\\python.exe" -I razshiritel.py "D:\\KiCad_project\\razshiritel"

Схема: IDC вход (от предишната платка) → 74HCT244 (усилва SCK, LATCH, LOAD, OE за следващата) →
74HCT595 → TBD62783 → 8 изхода; 8 входа → делители → 74HCT165; IDC изход (към следващата).
Разводката на IDC е същата като гнездото „РАЗШИРЕНИЕ“ на основната платка:
  1 GND_IO  2 (не се ползва)  3 SCK  4 GND_IO  5 LATCH  6 LOAD  7 OE  8 данни към 595  9 данни от 165  10 GND_IO
Захранването е собствено: 24 V на датчиците → 78L05 → +5V_IO (5 V от кабела не се ползва).
"""
import json
import os
import shutil
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import pcbnew  # noqa: E402
import sch  # noqa: E402
from sch import Sheet, write_project  # noqa: E402
from osnovna_platka import place, terminal, R, C, flag, grid, FP_R1206, FP_C1206, FP_C1210, FP_D_SMA, FP_SOIC16, \
    FP_IDC10  # noqa: E402
import platka as pl  # noqa: E402

TITLE = 'CNC контролер: разширителен модул 8 входа + 8 изхода'


def sheet(project):
    sh = Sheet('modul', 'Разширителен модул', project, paper='A3')
    # захранване от 24 V на датчиците
    terminal(sh, 30.48, 50.8, '24V ДАТЧИЦИ', ['+24V_IO', 'GND_IO'], 'p.term')
    place(sh, 'Device:Fuse', 'F', '3A', 60.96, 50.8, {1: '+24V_IO', 2: '.24f'}, footprint='Fuse:Fuse_1812_4532Metric',
          role='p.fuse')
    place(sh, 'Device:D_TVS', 'D', 'SMAJ26CA', 86.36, 50.8, {1: '+24V_IO', 2: 'GND_IO'}, footprint=FP_D_SMA,
          role='p.tvs')
    C(sh, 106.68, 50.8, '10uF 50V', '.24f', 'GND_IO', 'p.cin', FP_C1210)
    place(sh, 'Regulator_Linear:L78L05_SOT89', 'U', '78L05', 132.08, 50.8,
          {'IN': '.24f', 'GND': 'GND_IO', 'OUT': '+5V_IO'}, stub=7.62, role='p.reg')
    C(sh, 157.48, 50.8, '10uF', '+5V_IO', 'GND_IO', 'p.cout', FP_C1206)
    flag(sh, 177.8, 50.8, '+24V_IO')
    flag(sh, 193.04, 50.8, 'GND_IO')
    flag(sh, 208.28, 50.8, '.24f')

    # вход от предишната платка
    place(sh, 'Connector_Generic:Conn_02x05_Odd_Even', 'J', 'ВХОД (от предишния)', 50.8, 101.6, {
        1: 'GND_IO', 3: '.sck_in', 4: 'GND_IO', 5: '.latch_in', 6: '.load_in', 7: '.oe_in', 8: '.ser_in',
        9: '.q7', 10: 'GND_IO'}, footprint=FP_IDC10, stub=15.24, role='j.in')
    R(sh, 30.48, 132.08, '10k', '+5V_IO', '.oe_in', 'b.roe')
    # буфер: тактовете към собствените чипове и към следващата платка
    place(sh, '74xx:74AHCT244', 'U', '74HCT244', 116.84, 101.6, {
        '1A0': '.sck_in', '1Y0': '.sck', '1A1': '.latch_in', '1Y1': '.latch', '1A2': '.load_in', '1Y2': '.load',
        '1A3': '.oe_in', '1Y3': '.oe', '2A0': 'GND_IO', '2A1': 'GND_IO', '2A2': 'GND_IO', '2A3': 'GND_IO',
        '1OE': 'GND_IO', '2OE': 'GND_IO', 'VCC': '+5V_IO', 'GND': 'GND_IO'},
        footprint='Package_SO:SOIC-20W_7.5x12.8mm_P1.27mm', stub=7.62, role='b.buf')
    C(sh, 152.4, 88.9, '100nF', '+5V_IO', 'GND_IO', 'b.cbuf')

    # изходи
    place(sh, '74xx:74HCT595', 'U', '74HCT595', 203.2, 101.6, {
        'SER': '.ser_in', 'SRCLK': '.sck', 'RCLK': '.latch', '~{OE}': '.oe', '~{SRCLR}': '+5V_IO',
        **{f'Q{c}': f'.o{i + 1}' for i, c in enumerate('ABCDEFGH')}, "QH'": '.ser_out',
        'VCC': '+5V_IO', 'GND': 'GND_IO'}, footprint=FP_SOIC16, stub=7.62, role='o.595')
    C(sh, 231.14, 78.74, '100nF', '+5V_IO', 'GND_IO', 'o.c595')
    place(sh, 'Transistor_Array:TBD62783A', 'U', 'TBD62783A', 284.48, 101.6, {
        **{f'I{j}': f'.o{9 - j}' for j in range(1, 9)}, **{f'O{j}': f'.out{9 - j}' for j in range(1, 9)},
        'VCC': '+24V_IO', 'GND': 'GND_IO'}, footprint='Package_SO:SOIC-18W_7.5x11.6mm_P1.27mm', stub=7.62,
        role='o.tbd')
    C(sh, 312.42, 78.74, '100nF 50V', '+24V_IO', 'GND_IO', 'o.ctbd')
    terminal(sh, 363.22, 101.6, 'ИЗХОДИ 1-8', [f'.out{i}' for i in range(1, 9)] + ['GND_IO', 'GND_IO'], 'o.term')

    # входове
    for i in range(1, 9):
        x, y = grid(i - 1, 4, 30.48, 177.8, 50.8, 25.4)
        R(sh, x, y, '22k', f'.in{i}_t', f'.in{i}', f'i.in{i}.r1', FP_R1206)
        R(sh, x + 12.7, y, '4k7', f'.in{i}', 'GND_IO', f'i.in{i}.r2')
        C(sh, x + 25.4, y, '100nF', f'.in{i}', 'GND_IO', f'i.in{i}.c')
    place(sh, '74xx:74HC165', 'U', '74HCT165', 284.48, 190.5, {
        **{'D2': '.in1', 'D3': '.in2', 'D4': '.in3', 'D5': '.in4', 'D6': '.in5', 'D7': '.in6', 'D0': '.in7', 'D1': '.in8'},  # клема k → D по физическия ред на изводите (без кръстосване) 'DS': '.ds', 'CP': '.sck', '~{PL}': '.load',
        '~{CE}': 'GND_IO', 'Q7': '.q7', 'VCC': '+5V_IO', 'GND': 'GND_IO'}, footprint=FP_SOIC16, stub=7.62,
        role='i.165')
    C(sh, 312.42, 165.1, '100nF', '+5V_IO', 'GND_IO', 'i.c165')
    R(sh, 233.68, 223.52, '10k', '.ds', 'GND_IO', 'i.rds')
    terminal(sh, 363.22, 190.5, 'ВХОДОВЕ 1-8', [f'.in{i}_t' for i in range(1, 9)] + ['+24V_IO', 'GND_IO'], 'i.term')

    # изход към следващата платка
    place(sh, 'Connector_Generic:Conn_02x05_Odd_Even', 'J', 'ИЗХОД (към следващия)', 203.2, 254, {
        1: 'GND_IO', 3: '.sck', 4: 'GND_IO', 5: '.latch', 6: '.load', 7: '.oe', 8: '.ser_out', 9: '.ds',
        10: 'GND_IO'}, footprint=FP_IDC10, stub=15.24, role='j.out')

    sh.text('Верига: основна платка → модул 1 → модул 2 → ... с лентов кабел 10 жила (IDC).\n'
            '74HCT244 усилва тактовете за следващия модул. OE е издърпан към 1: без кабел изходите са изключени.\n'
            'Изходи: PNP 24 V, до 0,5 A на изход (клеми 1–8, 9–10 0V). Входове: PNP 24 V (клеми 1–8, 9 +24V, 10 0V).\n'
            'Всеки модул има собствено захранване 24 V; 5 V от кабела (извод 2) не се свързва.',
            30.48, 266.7)
    return sh


class ExpLayout(pl.Layout):
    """Компактно, по посоката на сигнала: IDC вход вляво → 74HCT244 → 595/165 → IDC изход вдясно;
    изходите горе (TBD62783 точно под изводите), входовете долу (делителите точно над изводите);
    захранването в колона вляво. Цялото фолио извън трасетата е GND_IO на двата слоя."""

    def build(self, H):
        self.new_board()
        X0, Y0 = pl.X0, pl.Y0
        tlen = self.edge_len(self, ['o.term'], 'top')
        margin_l, margin_r = 26.0, 13.0          # вляво: 24 V, захранване, IDC вход и буфер; вдясно: IDC изход
        L = round(tlen + margin_l + margin_r)
        self.L, self.H = L, H
        top = self.edge_row(['o.term'], 'top', L, H, start=X0 + margin_l)
        bot = self.edge_row(['i.term'], 'bottom', L, H, start=X0 + margin_l)
        left = self.edge_row(['p.term', 'j.in'], 'left', L, H, start=Y0 + 8)
        jo = self.R('j.out')
        w, h = pl.size(jo, pl.edge_rot(jo, 'right'))
        right = self.edge_row(['j.out'], 'right', L, H, start=Y0 + (H - h) / 2)
        t_in = top['o.term'][0][3]
        b_in = bot['i.term'][0][1]
        obb, opos = top['o.term']
        ibb, ipos = bot['i.term']
        cx = (opos[0] + opos[7]) / 2

        # изходи: TBD62783 точно под изводите 1–8, 74HCT595 непосредствено под него
        tbd = self.put('o.tbd', cx, t_in + 7.0, 90)
        tl, tt, tr, tb = pl.bbox(tbd)
        self.put('o.ctbd', tr + 2.0, (tt + tb) / 2, 90)
        s595 = self.put('o.595', cx - 9, tb + 5.0, 270)
        sl, st, sr, sb = pl.bbox(s595)
        self.put('o.c595', sl - 2.0, (st + sb) / 2, 90)

        # входове: делителите в колони точно над изводите, 74HCT165 непосредствено над тях
        for i in range(1, 9):
            self.col([f'i.in{i}.r1', f'i.in{i}.r2', f'i.in{i}.c'], ipos[i - 1], b_in - 1.5, 90, gap=0.8,
                     direction=-1)
        div_top = min(pl.bbox(self.R(f'i.in{i}.c'))[1] for i in range(1, 9))
        s165 = self.put('i.165', cx + 9, div_top - 5.0, 90)
        hl, ht, hr, hb = pl.bbox(s165)
        self.put('i.c165', hr + 2.0, (ht + hb) / 2, 90)
        self.put('i.rds', hr + 5.0, (ht + hb) / 2, 90)

        # захранване: колона под клемата за 24 V
        pbb, _ = left['p.term']
        self.col(['p.fuse', 'p.tvs', 'p.cin', 'p.reg', 'p.cout'], pbb[2] + 4.5, Y0 + 2.5, 0, gap=0.8)
        # буферът между входния IDC и 595/165
        jbb, _ = left['j.in']
        buf = self.put('b.buf', jbb[2] + 9.5, (jbb[1] + jbb[3]) / 2 + 2, 0)
        bl, bt, br, bb_ = pl.bbox(buf)
        self.put('b.cbuf', (bl + br) / 2, bb_ + 2.0, 0)
        self.put('b.roe', (bl + br) / 2, bb_ + 5.0, 0)

        for i, (x, y) in enumerate(((X0 + 4, Y0 + 4), (X0 + L - 4, Y0 + 4),
                                    (X0 + 4, Y0 + H - 4), (X0 + L - 4, Y0 + H - 4))):
            h_ = pl.load_fp('MountingHole:MountingHole_3.2mm_M3')
            h_.SetReference(f'H{i + 1}')
            h_.SetValue('M3')
            h_.SetPosition(pcbnew.VECTOR2I(pl.mm(x), pl.mm(y)))
            self.board.Add(h_)
        for a, b in (((0, 0), (L, 0)), ((L, 0), (L, H)), ((L, H), (0, H)), ((0, H), (0, 0))):
            self.line(X0 + a[0], Y0 + a[1], X0 + b[0], Y0 + b[1], pcbnew.Edge_Cuts, 0.1)
        self.labels(top, bot, left, right)
        # заливката: цялата платка е GND_IO (двата слоя)
        self.zones = [{'net': 'GND_IO', 'pts': [(X0, Y0), (X0 + L, Y0), (X0 + L, Y0 + H), (X0, Y0 + H)]}]
        return [r for r in self.fp if r not in self.placed]

    def fp_value(self, role):
        return self.R(role).GetValue()


pl.LABELS.update({'o.term': [str(i) for i in range(1, 9)] + ['0V', '0V'],
                  'i.term': [str(i) for i in range(1, 9)] + ['+24', '0V'],
                  'p.term': ['+24', '0V']})


def main():
    folder = sys.argv[1]
    os.makedirs(folder, exist_ok=True)
    name = 'razshiritel'
    pro = os.path.join(folder, name + '.kicad_pro')
    if not os.path.exists(pro):
        src = os.path.join(os.path.dirname(folder.rstrip('\\/')), 'my_first+project', 'my_first+project.kicad_pro')
        if os.path.exists(src):
            shutil.copy(src, pro)
            d = json.load(open(pro, encoding='utf-8'))
            d.setdefault('meta', {})['filename'] = name + '.kicad_pro'
            json.dump(d, open(pro, 'w', encoding='utf-8'), indent=2, ensure_ascii=False)
        else:
            open(pro, 'w').write('{}')
    write_project(folder, name, [sheet(name)], TITLE)
    pcb = os.path.join(folder, name + '.kicad_pcb')
    if not os.path.exists(pcb):
        open(pcb, 'w').write('(kicad_pcb (version 20260206) (generator "pcbnew") (generator_version "10.0"))\n')
    lay = ExpLayout(folder)
    H = float(sys.argv[2]) if len(sys.argv) > 2 else 70
    missing = lay.build(H)
    bad, out = lay.check()
    lay.save()
    json.dump(lay.zones, open(os.path.join(folder, 'zony.json'), 'w', encoding='utf-8'))
    print(f'модул {lay.L} × {H} мм')
    if missing:
        print('неразположени:', ', '.join(missing))
    if bad:
        print('застъпвания:', bad)
    if out:
        print('извън платката:', out)


if __name__ == '__main__':
    main()
