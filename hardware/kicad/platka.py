"""
Основна платка: разполагане на елементите в KiCad (.kicad_pcb) по ръчно проектиран план.
Copyright (c) 2026 ageorgiev86. Всички права запазени.

Пускане (KiCad затворен):
  "C:\\Program Files\\KiCad\\10.0\\bin\\python.exe" -I platka.py "D:\\KiCad_project\\my_first+project"

План (гледано отгоре):
  - горе: изходите (6 драйвера, честотен регулатор, RS485 | изходи 1–8 в изолираната зона);
  - долу: входовете (7 енкодера, MPG | пробник/авариен стоп, крайни/HOME, входове 1–8 в изолираната зона);
  - вляво: 24 V, PE, USB и захранването (в посоката на тока: предпазител → диод → TVS → LM2596 → AP2112);
  - вдясно: 24 V за датчиците; изолираната част (GND_IO) е „Г“ отдясно и отдолу, с граница на копринения слой;
  - зад всяка клема — нейната верига на точно място спрямо изводите ѝ (по полето Role от схемата);
  - процесорът в средата, кондензатор до всеки извод VDD, кварците до изводите си.
Връзката със схемата е по пътя (UUID) на символите: F8 в KiCad работи нормално.
"""
import os
import re
import subprocess
import sys

import pcbnew

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from sch import parse, tokenize  # noqa: E402

KICAD = r"C:\Program Files\KiCad\10.0"
FPDIR = os.path.join(KICAD, 'share', 'kicad', 'footprints')
GAP = 1.0             # между съседни клеми (корпусите почти един до друг)
BARRIER = 8.0         # изолационна граница: разстояние между зоните
X0, Y0 = 30.0, 30.0   # горен ляв ъгъл на платката в листа
CORNER = 10.0

TOP = [f'drv{i}.term' for i in range(1, 7)] + ['vfd.term', 'rs.term', '|', 'io.out.term']
BOTTOM = [f'enc{i}.term' for i in range(1, 7)] + ['encsp.term', 'mpg.j', 'mpg.es.term', '|', 'in.term', 'lim.term']
LEFT = ['pwr.term', 'pe.term', 'usb.j']
RIGHT = ['io24.term', 'io.in.term', 'io.exp']
WANT = {'right': 0, 'top': 90, 'left': 180, 'bottom': 270}

# надписи на клемите (по ред на изводите)
LABELS = {
    'drv': ['+5V', 'STEP', 'DIR', 'ENA', 'ALM', 'GND'],
    'enc': ['+5V', '0V', 'A', 'B'],
    'vfd.term': ['AVI', 'ACM', 'FWD', 'REV', 'COM'],
    'rs.term': ['A', 'B', 'GND'],
    'io.out.term': [str(i) for i in range(1, 9)] + ['0V', '0V'],
    'io.in.term': [str(i) for i in range(1, 9)] + ['+24', '0V'],
    'in.term': ['+24', '0V', 'PRB', 'TOOL', 'ESTP'],
    'lim.term': ['+24'] + [f'L{i}' for i in range(1, 7)] + [f'H{i}' for i in range(1, 7)] + ['0V'],
    'pwr.term': ['+24', '0V'],
    'io24.term': ['+24', '0V'],
    'pe.term': ['PE', 'PE'],
}


def mm(v):
    return pcbnew.FromMM(v)


def tomm(v):
    return pcbnew.ToMM(v)


def child(e, name):
    for x in e:
        if isinstance(x, list) and x and x[0] == name:
            return x
    return None


def text(e):
    return e[1].strip('"') if e and len(e) > 1 else ''


def read_netlist(sch_path, out):
    subprocess.run([os.path.join(KICAD, 'bin', 'kicad-cli.exe'), 'sch', 'export', 'netlist',
                    '--format', 'kicadsexpr', '-o', out, sch_path], check=True, capture_output=True)
    tree, _ = parse(tokenize(open(out, encoding='utf-8').read()))
    root = tree[0]
    comps = []
    for c in child(root, 'components')[1:]:
        if not isinstance(c, list) or c[0] != 'comp':
            continue
        sp = child(c, 'sheetpath')
        role = ''
        fields = child(c, 'fields')
        for f in (fields or [])[1:]:
            if isinstance(f, list) and f[0] == 'field' and text(child(f, 'name')) == 'Role':
                role = f[2].strip('"') if len(f) > 2 else ''
        comps.append(dict(ref=text(child(c, 'ref')), value=text(child(c, 'value')),
                          footprint=text(child(c, 'footprint')), role=role,
                          sheet=text(child(sp, 'names')).strip('/'),
                          path=text(child(sp, 'tstamps')) + text(child(c, 'tstamps')),
                          sheetfile=next((text(child(p, 'value')) for p in c if isinstance(p, list)
                                          and p[0] == 'property' and text(child(p, 'name')) == 'Sheetfile'), '')))
    nets = {}
    for n in child(root, 'nets')[1:]:
        if not isinstance(n, list) or n[0] != 'net':
            continue
        name = text(child(n, 'name'))
        for node in n:
            if isinstance(node, list) and node[0] == 'node':
                nets.setdefault(text(child(node, 'ref')), {})[text(child(node, 'pin'))] = name
    return comps, nets


_fp_cache = {}


def load_fp(fpid):
    if fpid not in _fp_cache:
        lib, name = fpid.split(':')
        fp = pcbnew.FootprintLoad(os.path.join(FPDIR, lib + '.pretty'), name)
        if fp is None:
            raise RuntimeError('няма посадъчно място ' + fpid)
        fp.SetFPID(pcbnew.LIB_ID(lib, name))
        _fp_cache[fpid] = fp
    return pcbnew.FOOTPRINT(_fp_cache[fpid])


def bbox(fp):
    b = fp.GetBoundingBox(False)
    return (tomm(b.GetLeft()), tomm(b.GetTop()), tomm(b.GetRight()), tomm(b.GetBottom()))


def size(fp, rot=0):
    fp.SetOrientationDegrees(rot)
    fp.SetPosition(pcbnew.VECTOR2I(0, 0))
    l, t, r, b = bbox(fp)
    return r - l, b - t


def at(fp, cx, cy, rot=0):
    """Центърът на рамката в cx, cy (мрежа 0,25 мм)."""
    fp.SetOrientationDegrees(rot)
    fp.SetPosition(pcbnew.VECTOR2I(0, 0))
    l, t, r, b = bbox(fp)
    ox, oy = (l + r) / 2, (t + b) / 2
    fp.SetPosition(pcbnew.VECTOR2I(mm(round((cx - ox) * 4) / 4), mm(round((cy - oy) * 4) / 4)))


def corner(fp, x, y, rot=0):
    """Горният ляв ъгъл на рамката в x, y."""
    fp.SetOrientationDegrees(rot)
    fp.SetPosition(pcbnew.VECTOR2I(0, 0))
    l, t, _, _ = bbox(fp)
    fp.SetPosition(pcbnew.VECTOR2I(mm(round((x - l) * 4) / 4), mm(round((y - t) * 4) / 4)))


def pad_xy(fp, num):
    for p in fp.Pads():
        if p.GetNumber() == str(num):
            return tomm(p.GetPosition().x), tomm(p.GetPosition().y)
    raise KeyError(num)


def opening_rot(fp, edge):
    fp.SetOrientationDegrees(0)
    fp.SetPosition(pcbnew.VECTOR2I(0, 0))
    l, t, r, b = bbox(fp)
    xs = [tomm(p.GetPosition().x) for p in fp.Pads()]
    ys = [tomm(p.GetPosition().y) for p in fp.Pads()]
    side = max(((r - max(xs)), 0), ((min(ys) - t), 90), ((min(xs) - l), 180), ((b - max(ys)), 270))[1]
    return (WANT[edge] - side) % 360


def edge_rot(fp, edge):
    """Клемите: отворът навън. Гнездата IDC: дългата страна по ръба, ключът навън."""
    if 'IDC' not in fp.GetFPIDAsString():
        return opening_rot(fp, edge)
    horiz = edge in ('top', 'bottom')
    for rot in (0, 90, 180, 270):
        w, h = size(fp, rot)
        if (w > h) == horiz:
            return rot
    return 0


class Layout:
    def __init__(self, folder):
        self.folder = folder
        self.name = [f[:-10] for f in os.listdir(folder) if f.endswith('.kicad_pro')][0]
        self.pcb_path = os.path.join(folder, self.name + '.kicad_pcb')
        self.comps, self.pinnets = read_netlist(os.path.join(folder, self.name + '.kicad_sch'),
                                                os.path.join(os.environ.get('TEMP', folder), 'cnc_netlist.net'))

    def new_board(self):
        with open(self.pcb_path, 'w', encoding='utf-8') as fh:
            fh.write('(kicad_pcb (version 20260206) (generator "pcbnew") (generator_version "10.0"))\n')
        board = pcbnew.LoadBoard(self.pcb_path)
        netinfo = {}
        for pins in self.pinnets.values():
            for net in pins.values():
                if net not in netinfo:
                    ni = pcbnew.NETINFO_ITEM(board, net)
                    board.Add(ni)
                    netinfo[net] = ni
        self.fp = {}
        self.role = {}
        self.byrole = {}
        for c in self.comps:
            fp = load_fp(c['footprint'])
            fp.SetReference(c['ref'])
            fp.SetValue(c['value'])
            fp.SetPath(pcbnew.KIID_PATH(c['path']))
            fp.SetSheetname(c['sheet'])
            fp.SetSheetfile(c['sheetfile'])
            for pad in fp.Pads():
                net = self.pinnets.get(c['ref'], {}).get(pad.GetNumber())
                if net:
                    pad.SetNet(netinfo[net])
            if 'MSTB' not in c['footprint']:
                rt = fp.Reference()
                rt.SetTextSize(pcbnew.VECTOR2I(mm(0.8), mm(0.8)))
                rt.SetTextThickness(mm(0.12))
            self.fp[c['ref']] = fp
            self.role[c['ref']] = c['role']
            self.byrole.setdefault(c['role'], []).append(fp)
        self.board = board
        self.placed = set()

    def R(self, role, i=0):
        lst = self.byrole.get(role, [])
        if not lst:
            raise KeyError('няма елемент с роля ' + role)
        return lst[i]

    def add(self, fp):
        if fp.GetReference() not in self.placed:
            self.board.Add(fp)
            self.placed.add(fp.GetReference())

    def put(self, role, cx, cy, rot=0, i=0):
        fp = self.R(role, i)
        at(fp, cx, cy, rot)
        self.add(fp)
        return fp

    def row(self, roles, x, cy, rot=0, gap=1.5, direction=1):
        """Ред от елементи по x, центрирани по cy; връща x след последния."""
        for r in roles:
            fp = self.R(r) if isinstance(r, str) else r
            w, _ = size(fp, rot)
            cx = x + direction * w / 2
            at(fp, cx, cy, rot)
            self.add(fp)
            x += direction * (w + gap)
        return x

    def col(self, roles, cx, y, rot=90, gap=1.2, direction=1):
        """Колона от елементи по y, центрирани по cx; връща y след последния."""
        for r in roles:
            fp = self.R(r) if isinstance(r, str) else r
            _, h = size(fp, rot)
            cy = y + direction * h / 2
            at(fp, cx, cy, rot)
            self.add(fp)
            y += direction * (h + gap)
        return y

    # ---------------------------------------------------------------- клеми
    def edge_row(self, items, edge, L, H, start=None):
        """Слага клемите по края; връща {роля: (рамка, [x или y на изводите])}."""
        out = {}
        sizes = []
        for it in items:
            if it == '|':
                sizes.append(('|', BARRIER))
                continue
            fp = self.R(it)
            rot = edge_rot(fp, edge)
            w, h = size(fp, rot)
            sizes.append((it, rot, w, h))
        horiz = edge in ('top', 'bottom')
        cur = start if start is not None else (X0 if horiz else Y0) + CORNER
        for s in sizes:
            if s[0] == '|':
                cur += s[1] - GAP
                out['|'] = cur - s[1] / 2 + GAP / 2
                continue
            it, rot, w, h = s
            fp = self.R(it)
            x = {'top': cur, 'bottom': cur, 'left': X0, 'right': X0 + L - w}[edge]
            y = {'top': Y0, 'bottom': Y0 + H - h, 'left': cur, 'right': cur}[edge]
            corner(fp, x, y, rot)
            self.add(fp)
            pads = sorted(fp.Pads(), key=lambda p: int(p.GetNumber()) if p.GetNumber().isdigit() else 99)
            pos = [tomm(p.GetPosition().x if horiz else p.GetPosition().y) for p in pads]
            out[it] = (bbox(fp), pos)
            cur += (w if horiz else h) + GAP
        out['_end'] = cur - GAP
        return out

    @staticmethod
    def edge_len(self_, items, edge):
        total = 0
        for it in items:
            if it == '|':
                total += BARRIER
                continue
            fp = self_.R(it)
            w, h = size(fp, edge_rot(fp, edge))
            total += (w if edge in ('top', 'bottom') else h) + GAP
        return total - GAP

    # ---------------------------------------------------------------- шаблони
    def build(self, H):
        self.new_board()
        L = round(max(self.edge_len(self, TOP, 'top'), self.edge_len(self, BOTTOM, 'bottom')) + 2 * CORNER)
        top = self.edge_row(TOP, 'top', L, H)
        bot = self.edge_row(BOTTOM, 'bottom', L, H)
        self.L, self.H = L, H
        t_in = max(v[0][3] for k, v in top.items() if k not in ('|', '_end'))      # долен край на горните клеми
        b_in = min(v[0][1] for k, v in bot.items() if k not in ('|', '_end'))      # горен край на долните клеми
        left = self.edge_row(LEFT, 'left', L, H, start=t_in + 22)
        right = self.edge_row(RIGHT, 'right', L, H, start=t_in + 4)

        # ---- драйвери: ALM диод под извод 5, буферите между двойките канали ----
        for i in range(1, 7):
            bb, pos = top[f'drv{i}.term']
            self.put(f'drv{i}.dalm', pos[4], t_in + 3.0, 90)
        for k in range(1, 4):
            c1 = sum(top[f'drv{2 * k - 1}.term'][0][::2]) / 2
            c2 = sum(top[f'drv{2 * k}.term'][0][::2]) / 2
            buf = self.put(f'drv.buf{k}', (c1 + c2) / 2, t_in + 13, 90)
            bl, bt, br, bb_ = bbox(buf)
            self.put(f'drv.cbuf{k}', bl - 2.0, (bt + bb_) / 2, 90)   # до VCC (извод 20)
        b1 = self.R('drv.buf1')
        l1, t1, r1, bb1 = bbox(b1)
        self.col(['drv.roe', 'drv.rflt', 'drv.cflt'], l1 - 6, t1, 90)

        # ---- честотен регулатор и RS485 под клемите им ----
        vbb, vpos = top['vfd.term']
        f_u = self.put('vfd.fwd.u', vpos[2], t_in + 6, 90)
        r_u = self.put('vfd.rev.u', vpos[2] + 2 * (vpos[3] - vpos[2]), t_in + 6, 90)
        self.put('vfd.fwd.r', vpos[2], bbox(f_u)[3] + 2.5, 90)
        self.put('vfd.rev.r', vpos[2] + 2 * (vpos[3] - vpos[2]), bbox(r_u)[3] + 2.5, 90)
        op = self.put('vfd.op', vpos[0] + 1, t_in + 7, 0)
        ol, ot, orr, ob = bbox(op)
        # пасивните на LM358 в две колони точно под него
        self.col(['vfd.rout', 'vfd.rfb', 'vfd.rg'], ol + 1.5, ob + 1.5, 90, gap=0.8)
        self.col(['vfd.cop', 'vfd.rf', 'vfd.cf'], orr - 1.5, ob + 1.5, 90, gap=0.8)
        rbb, rpos = top['rs.term']
        u = self.put('rs.u', (rpos[0] + rpos[2]) / 2, t_in + 6, 0)
        ul, ut, ur, ub = bbox(u)
        self.row(['rs.c', 'rs.rpu', 'rs.rpd'], rbb[0] + 0.5, ub + 2.5, 0, gap=1.5)
        self.row(['rs.rt', 'rs.jp'], rbb[0] + 0.5, ub + 7.0, 0, gap=2.0)

        # ---- изходи 1–8 (изолирана зона): TBD62783 под клемата, 74HCT595 под него ----
        obb, opos = top['io.out.term']
        ocx = (opos[0] + opos[7]) / 2
        tbd = self.put('io.tbd', ocx, t_in + 7, 90)
        tl, tt, tr, tb = bbox(tbd)
        self.put('io.ctbd', tr + 2.5, (tt + tb) / 2, 0)
        s595 = self.put('io.595', ocx, tb + 7, 270)
        sl, st, sr, sb = bbox(s595)
        self.put('io.c595', sr + 2.5, (st + sb) / 2, 0)
        self.put('io.roe', sl - 2.5, (st + sb) / 2, 0)

        # ---- енкодери: над всяка клема — предпазител над +5V, RC филтрите над A и B ----
        for c in [str(i) for i in range(1, 7)] + ['sp']:
            bb, pos = bot[f'enc{c}.term']
            y = b_in - 2.5
            self.put(f'enc{c}.ptc', pos[0], y - 1.5, 90)
            for s_, p in (('a', pos[2]), ('b', pos[3])):
                self.col([f'enc{c}.{s_}.pu', f'enc{c}.{s_}.r', f'enc{c}.{s_}.c'], p, y, 90, direction=-1)
        # 74LVC14: чип 1 за енк. 1–3, чип 2 за 4–6, чип 3 за шпиндела и MPG
        groups = [('1', '3'), ('4', '6'), ('sp', 'sp')]
        for k, (a, b) in enumerate(groups, 1):
            x1 = bot[f'enc{a}.term'][0][0]
            x2 = bot[f'enc{b}.term'][0][2]
            buf = self.put(f'enc.buf{k}', (x1 + x2) / 2, b_in - 24, 90)
            px, py = pad_xy(buf, 14)
            self.put(f'enc.cbuf{k}', px - 1.0, py - 3.0, 0)

        # ---- MPG пулт (RS-485): трансивърът и резисторите точно над гнездото 2 × 3 ----
        mbb, _ = bot['mpg.j']
        ml, mt, mr, mb = mbb
        u = self.put('mpg.rs.u', (ml + mr) / 2, mt - 5.5, 0)
        ul, ut, ur, ub = bbox(u)
        self.put('mpg.rs.c', ul - 2.0, (ut + ub) / 2, 90)
        self.row(['mpg.rs.rpu', 'mpg.rs.rt', 'mpg.rs.rpd'], ul - 1, ut - 2.0, 0, gap=1.2)
        self.put('mpg.ptc', ur + 2.5, (ut + ub) / 2, 90)

        # ---- входове 24 V с оптрони (изолирана зона долу) ----
        # над всеки извод: резистор и диод (страна 24 V); оптроните на границата; издърпване над тях
        ibb, ipos = bot['in.term']
        ybar = b_in - 22          # хоризонталната изолационна граница над долната изолирана зона
        for name, p in (('probe', ipos[2]), ('tool', ipos[3]), ('estop', ipos[4])):
            self.col([f'in.{name}.r', f'in.{name}.d'], p, b_in - 2.5, 90, direction=-1)
        self.put('in.probe.u', ipos[1] - 1.0, ybar, 90)
        self.put('in.tool.u', ipos[3] + 2.0, ybar, 90)
        self.put('in.estop.u', ipos[4] + 6.0, ybar, 90)
        for name, p in (('probe', ipos[1] - 3), ('tool', ipos[3]), ('estop', ipos[4] + 6)):
            self.row([f'in.{name}.pu', f'in.{name}.c'], p - 3, ybar - 9, 90)

        lbb, lpos = bot['lim.term']
        sig = [f'lim{i}' for i in range(1, 7)] + [f'home{i}' for i in range(1, 7)]
        for n, s_ in enumerate(sig):
            p = lpos[n + 1]
            self.col([f'lim.{s_}.r', f'lim.{s_}.d'], p, b_in - 2.5, 90, direction=-1)
        for k in range(3):
            cx = (lpos[1 + 4 * k] + lpos[4 + 4 * k]) / 2 + (k - 1) * 2.0
            self.put(f'lim.opto{k + 1}', cx, ybar, 90)
        for n, s_ in enumerate(sig):
            p = lpos[n + 1]
            self.col([f'lim.{s_}.pu', f'lim.{s_}.c'], p, ybar - 7.5, 90, direction=-1)

        nbb, npos = right['io.in.term']
        for i in range(1, 9):
            self.row([f'io.in{i}.r1', f'io.in{i}.r2', f'io.in{i}.c'], nbb[0] - 2.5, npos[i - 1], 0, gap=1.5,
                     direction=-1)
        xr_div = min(bbox(self.R(f'io.in{i}.c'))[0] for i in range(1, 9))
        s165 = self.put('io.165', xr_div - 6, (npos[0] + npos[7]) / 2, 0)
        hl, ht, hr, hb = bbox(s165)
        self.put('io.c165', (hl + hr) / 2, ht - 2.5, 0)
        self.put('io.rser', (hl + hr) / 2, hb + 2.5, 0)
        self.x_strip = hl - 3

        # ---- десен коридор (изолиран): 24 V за датчиците, 78L05, ISO7761, гнездо за разширения ----
        rbb, rpos = right['io24.term']
        xr = rbb[0] - 3
        y = (rbb[1] + rbb[3]) / 2
        # колона вляво от клемата: предпазител → TVS → C → 78L05 → C (надолу)
        self.col(['io24.fuse', 'io24.tvs', 'io24.cin', 'io24.reg', 'io24.cout'], xr - 3, rbb[1], 0, gap=1.5)
        iso = self.put('io.iso', top['|'], t_in + 34, 0)
        il, it_, ir, ib = bbox(iso)
        self.put('io.ciso1', il - 2.5, it_ + 3, 0)
        self.put('io.ciso2', ir + 2.5, it_ + 3, 0)

        # ---- захранване вляво (по посоката на тока, в три реда) ----
        pbb, ppos = left['pwr.term']
        x0 = pbb[2] + 3
        yp = (pbb[1] + pbb[3]) / 2
        # ред 1: предпазител → диод за поляритет → TVS → буферен кондензатор
        self.row(['pwr.fuse', 'pwr.drev', 'pwr.tvs', 'pwr.cbulk'], x0, yp, 0, gap=2.5)
        # ред 2: импулсният стабилизатор с късата си верига: C вход → LM2596 → диод → бобина → C изход
        yr = yp + 17
        x = self.row(['pwr.cin'], x0 + 12, yr, 90, gap=2.0)
        reg = self.put('pwr.reg', x + 7, yr, 0)
        rl, rt_, rr, rb = bbox(reg)
        self.put('pwr.dcatch', rr + 4, yr, 90)
        ind = self.put('pwr.ind', rr + 16, yr, 0)
        il2, it2, ir2, ib2 = bbox(ind)
        self.put('pwr.cout', ir2 + 6, yr, 0)
        # ред 3: 5 V → диоди ИЛИ → 3,3 V → светодиод
        x = self.row(['pwr.dor', 'pwr.c5', 'pwr.ldo', 'pwr.c33', 'pwr.rled', 'pwr.led'], x0 + 12, ib2 + 5, 0, gap=2.5)
        ebb, _ = left['pe.term']
        self.row(['pe.c', 'pe.r'], ebb[2] + 3, (ebb[1] + ebb[3]) / 2, 0, gap=2.0)
        ubb, _ = left['usb.j']
        self.put('pwr.dusb', ubb[2] + 5, (ubb[1] + ubb[3]) / 2 - 4, 90)
        self.put('usb.esd', ubb[2] + 5, (ubb[1] + ubb[3]) / 2 + 4, 0)

        # ---- процесорът и всичко около него ----
        mcu = self.R('mcu')
        mcx = X0 + L * 0.40
        mcy = (t_in + 26 + ybar - 12) / 2
        at(mcu, mcx, mcy, 0)
        self.add(mcu)
        self.mcu_ring(mcu)

        # Orange Pi: гнездо и серийните резистори до изводите LINK (лявата страна на чипа, долу)
        ml_, mt_, mr_, mb_ = bbox(mcu)
        opi = self.put('opi.j', mr_ + 52, t_in + 30, 0)
        ol2, ot2, or2, ob2 = bbox(opi)
        self.row(['opi.r1', 'opi.r2', 'opi.r3'], ol2 + 4, ot2 - 2.5, 0, gap=2.0)
        swd = self.put('mcu.swd', mr_ + 12, mt_ - 1, 90)
        x = self.row(['mcu.swboot', 'mcu.swrst'], bbox(swd)[2] + 3, mt_ - 1, 0, gap=3)
        # надписи на бутоните: за какво е всеки
        for role, txt in (('mcu.swboot', 'BOOT0: запис по USB'), ('mcu.swrst', 'RESET: рестарт')):
            l_, t_, r_, b_ = bbox(self.R(role))
            self.txt(txt, (l_ + r_) / 2, b_ + 1.3, 0.9)
        self.row(['mcu.rled', 'mcu.led'], bbox(swd)[2] + 3, mt_ - 7, 0, gap=1.5)

        # ---- изолационна граница и надписи ----
        self.barrier(obb, nbb, ibb, ybar, top, bot)
        self.labels(top, bot, left, right)

        # ---- отвори и контур ----
        for i, (x, y) in enumerate(((X0 + 4.5, Y0 + 4.5), (X0 + L - 4.5, Y0 + 4.5),
                                    (X0 + 4.5, Y0 + H - 4.5), (X0 + L - 4.5, Y0 + H - 4.5))):
            h_ = load_fp('MountingHole:MountingHole_3.2mm_M3')
            h_.SetReference(f'H{i + 1}')
            h_.SetValue('M3')
            h_.SetPosition(pcbnew.VECTOR2I(mm(x), mm(y)))
            self.board.Add(h_)
        self.line(X0, Y0, X0 + L, Y0, pcbnew.Edge_Cuts, 0.1)
        self.line(X0 + L, Y0, X0 + L, Y0 + H, pcbnew.Edge_Cuts, 0.1)
        self.line(X0 + L, Y0 + H, X0, Y0 + H, pcbnew.Edge_Cuts, 0.1)
        self.line(X0, Y0 + H, X0, Y0, pcbnew.Edge_Cuts, 0.1)
        missing = [r for r in self.fp if r not in self.placed]
        return missing

    def mcu_ring(self, mcu):
        """Кондензатор до всеки извод VDD; кварци, VCAP, VDDA, BOOT0 и NRST до изводите си."""
        l, t, r, b = bbox(mcu)
        cx, cy = (l + r) / 2, (t + b) / 2
        pads = {}
        for p in mcu.Pads():
            pads.setdefault(p.GetNetname(), []).append((p.GetNumber(), tomm(p.GetPosition().x),
                                                        tomm(p.GetPosition().y)))

        def outside(px, py, d):
            dx, dy = px - cx, py - cy
            if abs(dx) > abs(dy):
                return (px + (d if dx > 0 else -d), py, 0)
            return (px, py + (d if dy > 0 else -d), 90)

        used = []

        def free(x, y, fp, rot):
            w, h = size(fp, rot)
            box = (x - w / 2, y - h / 2, x + w / 2, y + h / 2)
            for u in used:
                if box[0] < u[2] + 0.4 and box[2] > u[0] - 0.4 and box[1] < u[3] + 0.4 and box[3] > u[1] - 0.4:
                    return False
            return True

        def near(fp, px, py, d=3.2):
            x, y, rot = outside(px, py, d)
            tang = (0, 1) if rot == 0 else (1, 0)
            for k in range(0, 40):
                s = (k + 1) // 2 * (1 if k % 2 else -1) * 1.0
                for dd in (0, 2.6, 5.2):
                    xx, yy, _ = outside(px + tang[0] * s, py + tang[1] * s, d + dd)
                    if free(xx, yy, fp, rot):
                        at(fp, xx, yy, rot)
                        self.add(fp)
                        used.append(bbox(fp))
                        return
            at(fp, x, y, rot)
            self.add(fp)
            used.append(bbox(fp))

        vdd = sorted(pads.get('+3V3', []), key=lambda p: int(p[0]))
        caps = self.byrole.get('mcu.cvdd', [])
        for fp, (_, px, py) in zip(caps, vdd):
            near(fp, px, py)
        near(self.R('mcu.cbulk'), *pads['+3V3'][0][1:], d=8)

        def padnet(net):
            for k, v in pads.items():
                if k.endswith('/' + net) or k == net:
                    return v[0][1:]
            raise KeyError(net)

        near(self.R('mcu.cvcap1'), *padnet('vcap1'))
        near(self.R('mcu.cvcap2'), *padnet('vcap2'))
        vx, vy = padnet('vdda')
        near(self.R('mcu.cvdda2'), vx, vy)
        near(self.R('mcu.cvdda1'), vx, vy, d=6)
        near(self.R('mcu.fb'), vx, vy, d=9)
        hx, hy = padnet('hse_in')
        near(self.R('mcu.hse'), hx, hy + 1, d=5)
        near(self.R('mcu.chse1'), hx, hy - 2, d=9)
        near(self.R('mcu.chse2'), hx, hy + 3, d=9)
        lx, ly = padnet('lse_in')
        near(self.R('mcu.lse'), lx, ly, d=5)
        near(self.R('mcu.clse1'), lx, ly - 2, d=9)
        near(self.R('mcu.clse2'), lx, ly + 2, d=9)
        bx, by = padnet('boot0')
        near(self.R('mcu.rboot'), bx, by)
        nx, ny = pads['NRST'][0][1:]
        near(self.R('mcu.crst'), nx, ny)
        bx2, by2 = padnet('vbat')
        near(self.R('mcu.cvbat'), bx2, by2)
        near(self.R('mcu.dbat'), bx2, by2, d=6)
        near(self.R('mcu.bat'), bx2, by2 - 6, d=16)

    def line(self, x1, y1, x2, y2, layer, w):
        s = pcbnew.PCB_SHAPE(self.board)
        s.SetShape(pcbnew.SHAPE_T_SEGMENT)
        s.SetStart(pcbnew.VECTOR2I(mm(x1), mm(y1)))
        s.SetEnd(pcbnew.VECTOR2I(mm(x2), mm(y2)))
        s.SetLayer(layer)
        s.SetWidth(mm(w))
        self.board.Add(s)

    def txt(self, s, x, y, size_=1.0, layer=pcbnew.F_SilkS, rot=0):
        t = pcbnew.PCB_TEXT(self.board)
        t.SetText(s)
        t.SetLayer(layer)
        t.SetTextSize(pcbnew.VECTOR2I(mm(size_), mm(size_)))
        t.SetTextThickness(mm(max(0.12, size_ * 0.15)))
        t.SetPosition(pcbnew.VECTOR2I(mm(x), mm(y)))
        t.SetTextAngleDegrees(rot)
        self.board.Add(t)

    def barrier(self, obb, nbb, ibb, ybar, top, bot):  # noqa: C901
        """Изолационната граница (пунктир на копринения слой). Изолираната зона (GND_IO):
        горе вдясно (изходи 1–8, ISO7761, 78L05, разширения), тясна ивица над входовете 1–8
        и долу вдясно (клемите на датчиците и входните резистори). Оптроните стъпват на границата."""
        xb_top, xb_bot = top['|'], bot['|']
        x_in = self.x_strip
        y_mid = ybar - 20
        pts = [(xb_top, Y0), (xb_top, y_mid), (x_in, y_mid), (x_in, ybar), (xb_bot, ybar), (xb_bot, Y0 + self.H)]
        for (x1, y1), (x2, y2) in zip(pts, pts[1:]):
            n = max(2, int(max(abs(x2 - x1), abs(y2 - y1)) / 1.5))
            for k in range(0, n, 2):
                a, b = k / n, min(1, (k + 1) / n)
                self.line(x1 + (x2 - x1) * a, y1 + (y2 - y1) * a, x1 + (x2 - x1) * b, y1 + (y2 - y1) * b,
                          pcbnew.F_SilkS, 0.2)
        # заливките: логика (GND) вляво и изолирана част (GND_IO) вдясно, 2 мм между тях по границата
        L, H, g = self.L, self.H, 1.0
        self.zones = [
            {'net': 'GND', 'pts': [(X0, Y0), (xb_top - g, Y0), (xb_top - g, y_mid + g), (x_in - g, y_mid + g),
                                   (x_in - g, ybar - g), (xb_bot - g, ybar - g), (xb_bot - g, Y0 + H), (X0, Y0 + H)]},
            {'net': 'GND_IO', 'pts': [(xb_top + g, Y0), (X0 + L, Y0), (X0 + L, Y0 + H), (xb_bot + g, Y0 + H),
                                      (xb_bot + g, ybar + g), (x_in + g, ybar + g), (x_in + g, y_mid - g),
                                      (xb_top + g, y_mid - g)]},
        ]
        self.txt('GND_IO (24 V)', xb_top + 12, y_mid - 2, 1.0)
        self.txt('GND (логика)', xb_top - 12, y_mid + 3, 1.0)
        self.y_mid = y_mid

    def labels(self, top, bot, left, right):
        """Надписи на изводите на всяка клема (копринен слой, до клемата откъм платката)."""
        def lab(role):
            for k, v in LABELS.items():
                if role == k or (role.startswith(k) and role[len(k):len(k) + 1].isdigit()) or \
                        (k == 'enc' and role.startswith('enc')):
                    return v
            return None

        for side, d in (('top', top), ('bottom', bot), ('left', left), ('right', right)):
            for role, v in d.items():
                if role in ('|', '_end'):
                    continue
                names = lab(role)
                if not names:
                    continue
                (l, t, r, b), pos = v
                for n, p in zip(names, pos):
                    if side == 'top':
                        self.txt(n, p, b + 0.0, 0.8, rot=90 if len(n) > 2 else 0)
                    elif side == 'bottom':
                        self.txt(n, p, t - 0.0, 0.8, rot=90 if len(n) > 2 else 0)
                    elif side == 'left':
                        self.txt(n, r + 1.5, p, 0.8)
                    else:
                        self.txt(n, l - 1.5, p, 0.8)
                name = self.fp_value(role)
                if side == 'top':
                    self.txt(name, (l + r) / 2, t + 1.2, 1.0)
                elif side == 'bottom':
                    self.txt(name, (l + r) / 2, b - 1.2, 1.0)

    def fp_value(self, role):
        return self.R(role).GetValue()

    def check(self):
        rects = []
        for fp in self.board.GetFootprints():
            l, t, r, b = bbox(fp)
            rects.append((l, t, r, b, fp.GetReference()))
        bad = []
        for i in range(len(rects)):
            a = rects[i]
            for j in range(i + 1, len(rects)):
                c = rects[j]
                if a[0] < c[2] and a[2] > c[0] and a[1] < c[3] and a[3] > c[1]:
                    bad.append((a[4], c[4]))
        out = [x[4] for x in rects if x[0] < X0 - 0.6 or x[1] < Y0 - 0.6 or x[2] > X0 + self.L + 0.6
               or x[3] > Y0 + self.H + 0.6]
        return bad, out

    def save(self):
        tb = self.board.GetTitleBlock()
        tb.SetTitle('CNC контролер: основна платка')
        tb.SetComment(0, 'Изходи горе, входове долу; изолираната зона (GND_IO) вдясно и долу вдясно')
        self.board.Save(self.pcb_path)
        t = open(self.pcb_path, encoding='utf-8').read()
        t = re.sub(r'\(paper "[^"]*"\)', '(paper "A3")', t, count=1)
        open(self.pcb_path, 'w', encoding='utf-8').write(t)


def main():
    lay = Layout(sys.argv[1])
    H = float(sys.argv[2]) if len(sys.argv) > 2 else 130
    missing = lay.build(H)
    bad, out = lay.check()
    lay.save()
    import json
    json.dump(lay.zones, open(os.path.join(sys.argv[1], 'zony.json'), 'w', encoding='utf-8'))
    print(f'платка {lay.L} × {H} мм')
    if missing:
        print('неразположени:', ', '.join(missing))
    if bad:
        print('застъпвания:', len(bad), bad[:30])
    if out:
        print('извън платката:', out)


if __name__ == '__main__':
    main()
