"""Довършва връзките, които Freerouting не е трасирал (след trasirane.py).

    python3.12 -I doveshvane.py <платка.kicad_pcb> [изход.kicad_pcb]

Несвързаните двойки идват от DRC на KiCad (kicad-cli). Медта на платката и връзките отиват към doveshvane.c
(мрежа 0,05 мм, A* на двата слоя; компилира се при всяко пускане с cc/gcc). Новите трасета и преходи се
добавят, масите се пълнят наново и платката се записва (по подразбиране върху входа). Папката трябва да има
.kicad_pro и .kicad_dru със същото име (правилата и класовете).
"""
import json
import math
import os
import shutil
import subprocess
import sys
import tempfile

import pcbnew

RES = 0.05
WIDTHS = {'MCU': [0.2], 'Power': [1.0, 0.6, 0.4], 'Ground': [0.6, 0.4], 'Default': [0.4, 0.3, 0.25]}
VIA = {'Power': (1.4, 0.7)}                # иначе 1,2 / 0,6
FINE = 'U1'                                # около изводите му разстоянието е 0,2 мм (.kicad_dru)


def mm(v):
    return pcbnew.FromMM(v)


def tomm(v):
    return v / 1e6


def polys(item, layer):
    """Медта на елемента в слоя като списък от многоъгълници (мм)."""
    ps = pcbnew.SHAPE_POLY_SET()
    item.TransformShapeToPolygon(ps, layer, 0, mm(0.005), pcbnew.ERROR_OUTSIDE)
    out = []
    for i in range(ps.OutlineCount()):
        ol = ps.Outline(i)
        pts = [ol.CPoint(j) for j in range(ol.PointCount())]
        if len(pts) >= 3:
            out.append([(tomm(p.x), tomm(p.y)) for p in pts])
    return out


def poly_lines(tag, item, net=None):
    """Редове P/S/T за елемента на двата медни слоя."""
    lines = []
    for li, layer in ((0, pcbnew.F_Cu), (1, pcbnew.B_Cu)):
        if not item.IsOnLayer(layer):
            continue
        for pl in polys(item, layer):
            head = f'{tag} {li} {net} {len(pl)}' if net is not None else f'{tag} {li} {len(pl)}'
            lines.append(head + ' ' + ' '.join(f'{x:.4f} {y:.4f}' for x, y in pl))
    return lines


def drc(pcb, out):
    subprocess.run(['kicad-cli', 'pcb', 'drc', '--format', 'json', '--severity-all', '-o', out, pcb],
                   stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    return json.load(open(out, encoding='utf-8'))


def build(src):
    tmp = tempfile.mkdtemp(prefix='doveshvane')
    exe = os.path.join(tmp, 'doveshvane.exe' if os.name == 'nt' else 'doveshvane')
    cc = shutil.which('cc') or shutil.which('gcc')
    if not cc:
        sys.exit('няма C компилатор (cc/gcc)')
    subprocess.run([cc, '-O2', '-o', exe, src, '-lm'], check=True)
    return exe


def main():
    pcb = os.path.abspath(sys.argv[1])
    out = os.path.abspath(sys.argv[2]) if len(sys.argv) > 2 else pcb
    here = os.path.dirname(os.path.abspath(__file__))
    exe = build(os.path.join(here, 'doveshvane.c'))
    board = pcbnew.LoadBoard(pcb)
    # площадки с еднакви UUID (стари версии на platka.py): DRC би посочил площадката на друг елемент
    seen, dup = set(), 0
    for p in board.GetPads():
        u = p.m_Uuid.AsString()
        if u in seen:
            p.ResetUuidDirect()
            dup += 1
        seen.add(u)
    if dup:
        print('нови UUID на площадки:', dup)
        board.Save(pcb)
        board = pcbnew.LoadBoard(pcb)
    rep = drc(pcb, os.path.join(os.path.dirname(pcb), 'doveshvane_drc.json'))

    e = board.GetBoardEdgesBoundingBox()
    l, t, r, b = tomm(e.GetLeft()), tomm(e.GetTop()), tomm(e.GetRight()), tomm(e.GetBottom())
    gw, gh = int(math.ceil((r + 1) / RES)), int(math.ceil((b + 1) / RES))
    head = [f'G {gw} {gh} {RES}', f'B {l + 0.05:.4f} {t + 0.05:.4f} {r - 0.05:.4f} {b - 0.05:.4f} 0.5']
    u1 = board.FindFootprintByReference(FINE)
    if u1:
        cb = u1.GetCourtyard(pcbnew.F_CrtYd).BBox()
        head.append(f'F {tomm(cb.GetLeft()):.4f} {tomm(cb.GetTop()):.4f} {tomm(cb.GetRight()):.4f} '
                    f'{tomm(cb.GetBottom()):.4f} 0.2')
    else:
        head.append('F 0 0 0 0 0.25')

    # медта: трасета, преходи, площадки, медни графики (без заливките: пълнят се после)
    for tr in board.GetTracks():
        head += poly_lines('P', tr, tr.GetNetCode())
        if tr.GetClass() == 'PCB_VIA':
            head.append(f'H {tomm(tr.GetPosition().x):.4f} {tomm(tr.GetPosition().y):.4f} {tomm(tr.GetDrill()) / 2:.4f}')
    for fp in board.GetFootprints():
        for p in fp.Pads():
            if p.GetAttribute() == pcbnew.PAD_ATTRIB_NPTH:
                # отвор без мед: пречка на двата слоя (номер 32000 = ничий)
                x, y = tomm(p.GetPosition().x), tomm(p.GetPosition().y)
                rr = tomm(max(p.GetDrillSize().x, p.GetDrillSize().y)) / 2 + 0.05
                pts = ' '.join(f'{x + rr * math.cos(a * math.pi / 12):.4f} {y + rr * math.sin(a * math.pi / 12):.4f}'
                               for a in range(24))
                head.append(f'P 2 32000 24 {pts}')
                head.append(f'H {x:.4f} {y:.4f} {rr - 0.05:.4f}')
                continue
            head += poly_lines('P', p, p.GetNetCode() or 32000)
            if p.GetAttribute() == pcbnew.PAD_ATTRIB_PTH:
                head.append(f'H {tomm(p.GetPosition().x):.4f} {tomm(p.GetPosition().y):.4f} '
                            f'{tomm(max(p.GetDrillSize().x, p.GetDrillSize().y)) / 2:.4f}')
        for g in fp.GraphicalItems():
            if g.IsOnLayer(pcbnew.F_Cu) or g.IsOnLayer(pcbnew.B_Cu):
                head += poly_lines('P', g, 32000)

    # връзките: от най-късата
    jobs = []
    for i, u in enumerate(rep['unconnected_items']):
        items = [board.ResolveItem(pcbnew.KIID(it['uuid'])) for it in u['items']]
        items = [it.Cast() if it else None for it in items]
        if len(items) != 2 or not all(items) or any(it.GetClass() == 'ZONE' for it in items):
            print('  пропусната (заливка):', u['items'][0]['description'])
            continue
        a, c = items
        cls = a.GetNetClassName() if a.GetNetCode() else c.GetNetClassName()
        cls = cls.split(',')[0] if cls else 'Default'
        net = a.GetNetCode()
        pa, pc = u['items'][0]['pos'], u['items'][1]['pos']
        jobs.append((math.hypot(pa['x'] - pc['x'], pa['y'] - pc['y']), i, net, cls, a, c,
                     a.GetNetname()))
    jobs.sort(key=lambda j: j[0])
    body = []
    for dist, i, net, cls, a, c, name in jobs:
        vd, _ = VIA.get(cls, (1.2, 0.6))
        ws = WIDTHS.get(cls, WIDTHS['Default'])
        # 0,25 мм навсякъде (чуждата мед може да е от клас с 0,25); 0,2 мм само до изводите на U1 (ред F)
        body.append(f'J {i} {net} 0.25 {vd} 0.25 {len(ws)} ' +
                    ' '.join(str(w) for w in ws))
        body += poly_lines('S', a)
        body += poly_lines('T', c)
        body.append('E')
    print(f'връзки за довършване: {len(jobs)}', flush=True)
    res = subprocess.run([exe], input='\n'.join(head + body) + '\n', capture_output=True, text=True)
    if res.returncode:
        sys.exit(res.stderr)
    if os.environ.get('DOVESHVANE_DEBUG'):
        print(res.stderr)

    names = {j[1]: j[6] for j in jobs}
    nets = {j[1]: j[2] for j in jobs}
    ok = fail = 0
    cur = None
    for ln in res.stdout.splitlines():
        f = ln.split()
        if f[0] == 'OK':
            cur = int(f[1]); ok += 1
        elif f[0] == 'FAIL':
            fail += 1
            print('  не става:', names[int(f[1])])
        elif f[0] == 'L':
            x1, y1, x2, y2 = map(float, f[3:7])
            if (x1, y1) == (x2, y2):
                continue
            tr = pcbnew.PCB_TRACK(board)
            tr.SetStart(pcbnew.VECTOR2I(mm(x1), mm(y1)))
            tr.SetEnd(pcbnew.VECTOR2I(mm(x2), mm(y2)))
            tr.SetWidth(mm(float(f[2])))
            tr.SetLayer(pcbnew.F_Cu if f[1] == '0' else pcbnew.B_Cu)
            tr.SetNetCode(nets[cur])
            board.Add(tr)
        elif f[0] == 'V':
            x, y, d = map(float, f[1:4])
            v = pcbnew.PCB_VIA(board)
            v.SetPosition(pcbnew.VECTOR2I(mm(x), mm(y)))
            v.SetWidth(mm(d))
            v.SetDrill(mm(0.7 if d >= 1.4 else 0.6))
            v.SetNetCode(nets[cur])
            board.Add(v)
    print(f'довършени: {ok}, не стават: {fail}', flush=True)
    pcbnew.ZONE_FILLER(board).Fill(board.Zones())
    board.Save(out)


if __name__ == '__main__':
    main()
