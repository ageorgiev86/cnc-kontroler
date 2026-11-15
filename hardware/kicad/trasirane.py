"""
Пробно автоматично трасиране с Freerouting (върху копие на проекта).
Copyright (c) 2026 ageorgiev86. Всички права запазени.

Пускане:
  "C:\\Program Files\\KiCad\\10.0\\bin\\python.exe" -I trasirane.py <проект> <копие> [проходи]
  python3 -I trasirane.py <проект> <копие> [проходи] [--minutes N] [--jar <freerouting.jar>] [--java <java>]   (Linux)

Freerouting 2.1 не спира по броя проходи (цикълът му не ги чете), а по времето: --minutes
(или FREEROUTING_MINUTES, по подразбиране 60); тогава спира меко и записва най-добрия резултат.

1. Копира проекта в папката <копие> (оригиналът не се пипа).
2. До всяка SMD площадка на маса (GND, GND_IO) слага преход към масата на другия слой.
3. Freerouting трасира всичко без масите (класът Ground се пропуска).
4. Масите стават заливка на двата слоя по зоните от zony.json (логика / изолирана част),
   която служи и като екран на платката.
Къде са Freerouting и Java (първото намерено):
  1. аргументите --jar и --java;
  2. променливите на средата FREEROUTING_JAR и FREEROUTING_JAVA;
  3. Windows: jar-ът от плъгина на KiCad и Java 25 от кеша на плъгина;
     Linux: най-новият /opt/freerouting/freerouting-*.jar и java от PATH.
Freerouting 2.5 иска Java 25; 2.1.0 върви с Java 21.
"""
import glob
import json
import math
import os
import re
import shutil
import subprocess
import sys

import pcbnew

HOME = os.path.expanduser('~')


def default_jar():
    if os.name == 'nt':
        return os.path.join(HOME, 'Documents', 'KiCad', '10.0', '3rdparty', 'plugins',
                            'app_freerouting_kicad-plugin', 'jar', 'freerouting-2.5.0.jar')
    jars = sorted(glob.glob('/opt/freerouting/freerouting-*.jar'), key=_version)
    return jars[-1] if jars else 'freerouting.jar'


def default_java():
    if os.name == 'nt':
        # Freerouting 2.5 иска Java 25: плъгинът я сваля в своя кеш при първото пускане
        return (sorted(glob.glob(os.path.join(HOME, 'AppData', 'Local', 'freerouting', 'cache', 'jre', '*', 'bin',
                                              'java.exe'))) or ['java'])[-1]
    return 'java'


def _version(path):
    nums = re.findall(r'\d+', os.path.basename(path))
    return [int(n) for n in nums]


GROUND = ('GND', 'GND_IO')
VIA_D, VIA_DRILL = 1.2, 0.6
QFP_PADS = 40      # от толкова изводи нагоре преходът към масата е навътре под корпуса
QFP_IN = 1.6       # колко навътре от края на площадката
QFP_OUT = 1.5      # заключен извод навън от всеки сигнален извод на QFP (fan-out)


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
        if len(fp.Pads()) >= QFP_PADS:
            # QFP: навън преходът би затворил изхода на съседните изводи → късо трасе навътре под корпуса
            if abs(px - cx) > abs(py - cy):
                ux, uy = (-1.0 if px > cx else 1.0), 0.0
            else:
                ux, uy = 0.0, (-1.0 if py > cy else 1.0)
            # навътре на няколко дълбочини; никога навън (там са изводите навън на сигналите)
            for depth in (QFP_IN, QFP_IN + 1.6, QFP_IN + 3.2, QFP_IN + 4.8):
                vx, vy = px + ux * (half + depth), py + uy * (half + depth)
                if all(math.hypot(vx - ox, vy - oy) > VIA_D + 0.3 for ox, oy in vias):
                    break
            else:
                print('  без преход към масата:', fp.GetReference(), p.GetNumber())
                continue
            t = pcbnew.PCB_TRACK(board)
            t.SetStart(p.GetPosition())
            t.SetEnd(pcbnew.VECTOR2I(mm(vx), mm(vy)))
            t.SetWidth(mm(0.3))
            t.SetLayer(pcbnew.F_Cu if p.IsOnLayer(pcbnew.F_Cu) else pcbnew.B_Cu)
            t.SetNet(p.GetNet())
            board.Add(t)
            v = pcbnew.PCB_VIA(board)
            v.SetPosition(pcbnew.VECTOR2I(mm(vx), mm(vy)))
            v.SetDrill(mm(VIA_DRILL))
            v.SetWidth(mm(VIA_D))
            v.SetNet(p.GetNet())
            board.Add(v)
            vias.append((vx, vy))
            n += 1
            continue
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


def qfp_fanout(board):
    """Заключено трасе навън от всеки сигнален извод на QFP.

    Без него Freerouting слага рано трасе покрай редицата изводи и то запушва изхода на всички
    останали (стъпка 0,5 мм: между края на изводите и трасето преход не се побира)."""
    count = {}
    for p in board.GetPads():
        count[p.GetNetname()] = count.get(p.GetNetname(), 0) + 1
    others = []
    for p in board.GetPads():
        b = p.GetBoundingBox()
        others.append((tomm(b.GetLeft()), tomm(b.GetTop()), tomm(b.GetRight()), tomm(b.GetBottom()),
                       p.GetNetname(), p.GetParentFootprint().GetReference()))
    for v in board.GetTracks():   # и преходите към масата, сложени преди това
        if v.GetClass() == 'PCB_VIA':
            x, y, r = tomm(v.GetPosition().x), tomm(v.GetPosition().y), tomm(v.GetWidth()) / 2
            others.append((x - r, y - r, x + r, y + r, v.GetNetname(), ''))
    n = 0
    for fp in board.GetFootprints():
        if len(fp.Pads()) < QFP_PADS:
            continue
        cx, cy = tomm(fp.GetPosition().x), tomm(fp.GetPosition().y)
        for p in fp.Pads():
            net = p.GetNetname()
            if not net or net in GROUND or net.startswith('unconnected-') or count.get(net, 0) < 2:
                continue
            px, py = tomm(p.GetPosition().x), tomm(p.GetPosition().y)
            b = p.GetBoundingBox()
            half = max(tomm(b.GetWidth()), tomm(b.GetHeight())) / 2
            if abs(px - cx) > abs(py - cy):
                ux, uy = (1.0 if px > cx else -1.0), 0.0
            else:
                ux, uy = 0.0, (1.0 if py > cy else -1.0)
            w = 0.2 if p.GetNetClassName() == 'MCU' else 0.25   # класът MCU е 0,2 мм (pravila.py)
            length = half + QFP_OUT
            while length > half:
                ex, ey = px + ux * length, py + uy * length
                r = w / 2 + 0.3
                if all(not (ex + r > l and ex - r < rr and ey + r > t and ey - r < bb) or nn == net or ref == fp.GetReference()
                       for (l, t, rr, bb, nn, ref) in others):
                    break
                length -= 0.25
            if length <= half:
                continue
            t = pcbnew.PCB_TRACK(board)
            t.SetStart(p.GetPosition())
            t.SetEnd(pcbnew.VECTOR2I(mm(px + ux * length), mm(py + uy * length)))
            t.SetWidth(mm(w))
            t.SetLayer(pcbnew.F_Cu if p.IsOnLayer(pcbnew.F_Cu) else pcbnew.B_Cu)
            t.SetNet(p.GetNet())
            t.SetLocked(True)
            board.Add(t)
            n += 1
    return n


def cleanup(board):
    """Маха висящите парчета трасе и преходите на сигнали с по-малко от две трасета (като Cleanup Tracks)."""
    removed = 0
    while True:
        board.BuildConnectivity()
        conn = board.GetConnectivity()
        dead = [t for t in board.GetTracks()
                if t.GetClass() == 'PCB_TRACK' and conn.TestTrackEndpointDangling(t, False)]
        ends = {}
        for t in board.GetTracks():
            if t.GetClass() == 'PCB_TRACK':
                for p in (t.GetStart(), t.GetEnd()):
                    k = (p.x, p.y, t.GetNetCode())
                    ends[k] = ends.get(k, 0) + 1
        for v in board.GetTracks():
            if v.GetClass() == 'PCB_VIA' and v.GetNetname() not in GROUND:
                p = v.GetPosition()
                n = ends.get((p.x, p.y, v.GetNetCode()), 0)
                if n < 2:   # преход в площадка на същата мрежа: площадката е втората връзка
                    n += any(pd.GetNetCode() == v.GetNetCode() and pd.HitTest(p) for pd in board.GetPads())
                if n < 2:
                    dead.append(v)
        if not dead:
            return removed
        for t in dead:
            board.Delete(t)
        removed += len(dead)


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
            zone.SetPadConnection(pcbnew.ZONE_CONNECTION_THT_THERMAL)   # SMD плътно, изводите с отвор — с термо
            zone.SetThermalReliefGap(mm(0.4))
            zone.SetThermalReliefSpokeWidth(mm(0.5))
            zone.SetIslandRemovalMode(pcbnew.ISLAND_REMOVAL_MODE_ALWAYS)
            board.Add(zone)
            out.append(zone)
    # клемите и гнездата: изводите на маса плътно към заливката (иначе термото им остава с едно
    # мостче между съседните изводи, а през тях минава токът на машината)
    for fp in board.GetFootprints():
        if fp.GetReference()[:1] in ('X', 'J'):
            for p in fp.Pads():
                if p.GetNetname() in GROUND and p.GetAttribute() == pcbnew.PAD_ATTRIB_PTH:
                    p.SetLocalZoneConnection(pcbnew.ZONE_CONNECTION_FULL)
    pcbnew.ZONE_FILLER(board).Fill(board.Zones())
    return len(out), None


def parse_args(argv):
    """<проект> <копие> [проходи] [--jar път] [--java път]"""
    opts = {'jar': os.environ.get('FREEROUTING_JAR') or None, 'java': os.environ.get('FREEROUTING_JAVA') or None,
            'minutes': os.environ.get('FREEROUTING_MINUTES') or '60'}
    pos = []
    it = iter(argv)
    for a in it:
        if a in ('--jar', '--java', '--minutes'):
            opts[a[2:]] = next(it, None)
        else:
            pos.append(a)
    if len(pos) < 2:
        sys.exit(__doc__)
    return (pos[0], pos[1], (pos[2] if len(pos) > 2 else '30'), opts['jar'] or default_jar(),
            opts['java'] or default_java(), int(opts['minutes']))


def main():
    src, dst, passes, jar, java, minutes = parse_args(sys.argv[1:])
    if not os.path.exists(jar):
        sys.exit(f'няма Freerouting: {jar} (задай --jar или FREEROUTING_JAR)')
    os.makedirs(dst, exist_ok=True)
    for f in os.listdir(src):
        if f.endswith(('.kicad_pcb', '.kicad_pro', '.kicad_sch', '.kicad_dru', '.json')):
            shutil.copy(os.path.join(src, f), dst)
    pcb = [os.path.join(dst, f) for f in os.listdir(dst) if f.endswith('.kicad_pcb')][0]
    dsn, ses = pcb[:-10] + '.dsn', pcb[:-10] + '.ses'
    board = pcbnew.LoadBoard(pcb)
    print('преходи към масата:', ground_vias(board), flush=True)
    print('изводи навън от QFP:', qfp_fanout(board), flush=True)
    board.Save(pcb)
    board = pcbnew.LoadBoard(pcb)
    if not pcbnew.ExportSpecctraDSN(board, dsn):
        sys.exit('неуспешен износ на DSN')
    # Freerouting 2.x взима броя проходи от freerouting.json (по подразбиране 9999), затова и --router.max_passes;
    # без телеметрия
    cmd = [java, '-jar', jar, '-de', dsn, '-do', ses, '-mp', passes, '-inc', 'Ground', '--gui.enabled=false',
           f'--router.max_passes={passes}', f'--router.job_timeout={minutes // 60:02d}:{minutes % 60:02d}:00',
           '--usage_and_diagnostic_data.disable_analytics=true',
           '--profile.allow_telemetry=false']
    print(' '.join(cmd), flush=True)
    log = os.path.join(dst, 'freerouting.log')
    with open(log, 'w', encoding='utf-8', errors='replace') as flog:   # напредъкът се вижда в лога
        subprocess.run(cmd, stdout=flog, stderr=subprocess.STDOUT)
    lines = open(log, encoding='utf-8', errors='replace').read().splitlines()
    tail = [ln for ln in lines if 'completed' in ln or 'ERROR' in ln or 'Error' in ln]
    print('\n'.join(tail[-8:]), flush=True)
    if not os.path.exists(ses):
        sys.exit('Freerouting не върна резултат')
    board = pcbnew.LoadBoard(pcb)
    if not pcbnew.ImportSpecctraSES(board, ses):
        sys.exit('неуспешен внос на SES')
    print('почистени висящи парчета:', cleanup(board), flush=True)
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
