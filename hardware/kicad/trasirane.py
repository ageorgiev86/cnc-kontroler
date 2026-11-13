"""
Пробно автоматично трасиране с Freerouting (върху копие на проекта).
Copyright (c) 2026 ageorgiev86. Всички права запазени.

Пускане:
  "C:\\Program Files\\KiCad\\10.0\\bin\\python.exe" -I trasirane.py <проект> <копие> [проходи]

1. Копира проекта в папката <копие> (оригиналът не се пипа).
2. До всяка SMD площадка на маса (GND, GND_IO) слага преход към масата на другия слой.
3. Freerouting трасира всичко без масите (класът Ground се пропуска).
4. Масите стават заливка на двата слоя по зоните от zony.json (логика / изолирана част),
   която служи и като екран на платката.
Freerouting е от плъгина на KiCad; Java 25 — от кеша на плъгина.
"""
import glob
import json
import math
import os
import shutil
import subprocess
import sys

import pcbnew

HOME = os.path.expanduser('~')
JAR = os.path.join(HOME, 'Documents', 'KiCad', '10.0', '3rdparty', 'plugins', 'app_freerouting_kicad-plugin', 'jar',
                   'freerouting-2.5.0.jar')
# Freerouting 2.5 иска Java 25: плъгинът я сваля в своя кеш при първото пускане
JAVA = (sorted(glob.glob(os.path.join(HOME, 'AppData', 'Local', 'freerouting', 'cache', 'jre', '*', 'bin',
                                      'java.exe'))) or ['java'])[-1]
GROUND = ('GND', 'GND_IO')
VIA_D, VIA_DRILL = 1.2, 0.6


def mm(v):
    return pcbnew.FromMM(v)


def tomm(v):
    return pcbnew.ToMM(v)


def ground_vias(board):
    """Преход до всяка SMD площадка на маса, навън от центъра на елемента."""
    pads = [p for p in board.GetPads()]
    boxes = []
    for p in pads:
        b = p.GetBoundingBox()
        boxes.append((tomm(b.GetLeft()), tomm(b.GetTop()), tomm(b.GetRight()), tomm(b.GetBottom()), p.GetNetname()))
    vias = []
    n = 0
    for p in pads:
        net = p.GetNetname()
        if net not in GROUND or p.GetAttribute() != pcbnew.PAD_ATTRIB_SMD:
            continue
        fp = p.GetParentFootprint()
        cx, cy = tomm(fp.GetPosition().x), tomm(fp.GetPosition().y)
        px, py = tomm(p.GetPosition().x), tomm(p.GetPosition().y)
        b = p.GetBoundingBox()
        half = max(tomm(b.GetWidth()), tomm(b.GetHeight())) / 2
        dx, dy = px - cx, py - cy
        d = math.hypot(dx, dy) or 1.0
        placed = False
        for ang in (0, 30, -30, 60, -60, 90, -90, 180):
            a = math.radians(ang)
            ux = (dx * math.cos(a) - dy * math.sin(a)) / d
            uy = (dx * math.sin(a) + dy * math.cos(a)) / d
            vx, vy = px + ux * (half + VIA_D / 2 + 0.35), py + uy * (half + VIA_D / 2 + 0.35)
            r = VIA_D / 2 + 0.3
            ok = all(not (vx + r > l and vx - r < rr and vy + r > t and vy - r < bb) or nn == net
                     for (l, t, rr, bb, nn) in boxes)
            ok = ok and all(math.hypot(vx - ox, vy - oy) > VIA_D + 0.3 for ox, oy in vias)
            if ok:
                v = pcbnew.PCB_VIA(board)
                v.SetPosition(pcbnew.VECTOR2I(mm(vx), mm(vy)))
                v.SetDrill(mm(VIA_DRILL))
                v.SetWidth(mm(VIA_D))
                v.SetNet(p.GetNet())
                board.Add(v)
                vias.append((vx, vy))
                n += 1
                placed = True
                break
        if not placed:
            print('  без преход:', fp.GetReference(), p.GetNumber())
    return n


def ground_zones(board, zones):
    out = []
    for z in zones:
        net = board.FindNet(z['net'])
        for layer in (pcbnew.F_Cu, pcbnew.B_Cu):
            zone = pcbnew.ZONE(board)
            zone.SetLayer(layer)
            zone.SetNet(net)
            ol = zone.Outline()
            ol.NewOutline()
            for x, y in z['pts']:
                ol.Append(mm(x), mm(y))
            zone.SetLocalClearance(mm(0.4))
            zone.SetMinThickness(mm(0.3))
            zone.SetPadConnection(pcbnew.ZONE_CONNECTION_THERMAL)
            zone.SetThermalReliefGap(mm(0.4))
            zone.SetThermalReliefSpokeWidth(mm(0.5))
            zone.SetIslandRemovalMode(pcbnew.ISLAND_REMOVAL_MODE_ALWAYS)
            board.Add(zone)
            out.append(zone)
    pcbnew.ZONE_FILLER(board).Fill(board.Zones())
    return len(out), None


def main():
    src, dst = sys.argv[1], sys.argv[2]
    passes = sys.argv[3] if len(sys.argv) > 3 else '30'
    os.makedirs(dst, exist_ok=True)
    for f in os.listdir(src):
        if f.endswith(('.kicad_pcb', '.kicad_pro', '.kicad_sch', '.json')):
            shutil.copy(os.path.join(src, f), dst)
    pcb = [os.path.join(dst, f) for f in os.listdir(dst) if f.endswith('.kicad_pcb')][0]
    dsn, ses = pcb[:-10] + '.dsn', pcb[:-10] + '.ses'
    board = pcbnew.LoadBoard(pcb)
    print('преходи към масата:', ground_vias(board), flush=True)
    board.Save(pcb)
    board = pcbnew.LoadBoard(pcb)
    if not pcbnew.ExportSpecctraDSN(board, dsn):
        sys.exit('неуспешен износ на DSN')
    cmd = [JAVA, '-jar', JAR, '-de', dsn, '-do', ses, '-mp', passes, '-inc', 'Ground', '--gui.enabled=false']
    print(' '.join(cmd), flush=True)
    r = subprocess.run(cmd, capture_output=True, text=True, encoding='utf-8', errors='replace')
    tail = [ln for ln in (r.stdout + r.stderr).splitlines() if 'completed' in ln or 'ERROR' in ln or 'Error' in ln]
    print('\n'.join(tail[-8:]), flush=True)
    if not os.path.exists(ses):
        sys.exit('Freerouting не върна резултат')
    board = pcbnew.LoadBoard(pcb)
    if not pcbnew.ImportSpecctraSES(board, ses):
        sys.exit('неуспешен внос на SES')
    zf = os.path.join(dst, 'zony.json')
    if os.path.exists(zf):
        nz, _ = ground_zones(board, json.load(open(zf, encoding='utf-8')))
        print('заливки:', nz, flush=True)
    board.Save(pcb)
    tracks = list(board.GetTracks())
    vias = [t for t in tracks if t.GetClass() == 'PCB_VIA']
    print(f'готово: {len(tracks) - len(vias)} трасета, {len(vias)} прехода → {pcb}')


if __name__ == '__main__':
    main()
