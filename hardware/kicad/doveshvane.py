"""Довършва връзките, които Freerouting не е трасирал (след trasirane.py).

    python3.12 -I doveshvane.py <платка.kicad_pcb> [изход.kicad_pcb] [--rounds N]

Несвързаните двойки идват от DRC на KiCad (kicad-cli). Медта на платката и връзките отиват към doveshvane.c
(мрежа 0,05 мм, A* на двата слоя; компилира се при всяко пускане с cc/gcc). Новите трасета и преходи се
добавят, масите се пълнят наново и платката се записва (по подразбиране върху входа). Папката трябва да има
.kicad_pro и .kicad_dru със същото име (правилата и класовете).

На кръгове (по подразбиране до 12): връзка, която не става, минава през чужди трасета (не през площадките,
заключените трасета и масата) срещу висока цена; тези трасета се махат и мрежите им се трасират в следващия
кръг. Пази се платката с най-малко несвързани; спира при 0 или след 3 кръга без подобрение.
"""
import json
import math
import os
import shutil
import subprocess
import sys
import tempfile

import pcbnew

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))   # -I не слага папката на скрипта
from trasirane import GROUND, cleanup  # noqa: E402

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


def fix_uuids(pcb):
    """Площадки с еднакви UUID (стари версии на platka.py): DRC би посочил площадката на друг елемент."""
    board = pcbnew.LoadBoard(pcb)
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


def soft(tr):
    """Трасе, което може да се разкъса: не е заключено и не е маса."""
    return not tr.IsLocked() and tr.GetNetname() not in GROUND


def seg_dist(px, py, x1, y1, x2, y2):
    dx, dy = x2 - x1, y2 - y1
    L2 = dx * dx + dy * dy
    t = 0 if L2 == 0 else max(0.0, min(1.0, ((px - x1) * dx + (py - y1) * dy) / L2))
    return math.hypot(x1 + t * dx - px, y1 + t * dy - py)


def one_pass(pcb, exe, rip, final=False, only=None):
    """Един кръг: DRC → връзки → рутерът → нови трасета, разкъсване, почистване, заливки."""
    rep = drc(pcb, os.path.join(os.path.dirname(pcb), 'doveshvane_drc.json'))
    board = pcbnew.LoadBoard(pcb)
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

    # медта: трасета и преходи (Q: могат да се разкъсат), площадки и медни графики (P); без заливките
    for tr in board.GetTracks():
        head += poly_lines('Q' if soft(tr) else 'P', tr, tr.GetNetCode())
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

    # връзките: от най-късата (масата минава през заливката)
    jobs = []
    for i, u in enumerate(rep['unconnected_items']):
        items = [board.ResolveItem(pcbnew.KIID(it['uuid'])) for it in u['items']]
        items = [it.Cast() if it else None for it in items]
        if len(items) != 2 or not all(items) or any(it.GetClass() == 'ZONE' for it in items):
            continue
        a, c = items
        cls = a.GetNetClassName() if a.GetNetCode() else c.GetNetClassName()
        cls = cls.split(',')[0] if cls else 'Default'
        pa, pc = u['items'][0]['pos'], u['items'][1]['pos']
        jobs.append((math.hypot(pa['x'] - pc['x'], pa['y'] - pc['y']), i, a.GetNetCode(), cls, a, c, a.GetNetname()))
    if only is not None:
        jobs = [j for j in jobs if j[6] in only]
    jobs.sort(key=lambda j: j[0])
    body = []
    for dist, i, net, cls, a, c, name in jobs:
        vd, _ = VIA.get(cls, (1.2, 0.6))
        ws = WIDTHS.get(cls, WIDTHS['Default'])
        # 0,25 мм навсякъде (чуждата мед може да е от клас с 0,25); 0,2 мм само до изводите на U1 (ред F)
        body.append(f'J {i} {net} 0.25 {vd} 0.25 {rip} {len(ws)} ' + ' '.join(str(w) for w in ws))
        body += poly_lines('S', a)
        body += poly_lines('T', c)
        body.append('E')
    res = subprocess.run([exe], input='\n'.join(head + body) + '\n', capture_output=True, text=True)
    if res.returncode:
        sys.exit(res.stderr)
    if os.environ.get('DOVESHVANE_DEBUG'):
        print(res.stderr)

    nets = {j[1]: j[2] for j in jobs}
    names = {j[1]: j[6] for j in jobs}
    classes = {j[1]: j[3] for j in jobs}
    ok, failed, cross = 0, [], []
    cur = None
    for ln in res.stdout.splitlines():
        f = ln.split()
        if f[0] == 'OK':
            cur = int(f[1]); ok += 1
        elif f[0] == 'FAIL':
            failed.append(names[int(f[1])])
        elif f[0] == 'X':
            # половината от нашето: преход или най-тясното трасе на класа
            cls = classes[cur]
            ours = VIA.get(cls, (1.2, 0.6))[0] / 2 if f[1] == '2' else WIDTHS.get(cls, WIDTHS['Default'])[-1] / 2
            cross.append((int(f[1]), float(f[2]), float(f[3]), nets[cur], ours))
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

    # разкъсване: чуждите трасета до клетките X (с запас 0,1 мм)
    dead = {}
    if cross:
        for tr in board.GetTracks():
            if not soft(tr):
                continue
            via = tr.GetClass() == 'PCB_VIA'
            x1, y1 = tomm(tr.GetStart().x), tomm(tr.GetStart().y)
            x2, y2 = tomm(tr.GetEnd().x), tomm(tr.GetEnd().y)
            half = tomm(tr.GetWidth(pcbnew.F_Cu) if via else tr.GetWidth()) / 2
            for layer, px, py, net, ours in cross:
                if tr.GetNetCode() == net:
                    continue
                if not via and layer != 2 and tr.GetLayer() != (pcbnew.F_Cu if layer == 0 else pcbnew.B_Cu):
                    continue
                reach = 0.25 + half + ours + 0.02     # разстояние + двете половини
                if seg_dist(px, py, x1, y1, x2, y2) < reach:
                    dead[tr.m_Uuid.AsString()] = tr
                    break
        for tr in dead.values():
            board.Delete(tr)
    # при разкъсването остатъците на мрежата остават (следващият кръг свързва островчетата им);
    # висящите парчета се махат само в последния кръг
    removed = cleanup(board) if final else 0
    pcbnew.ZONE_FILLER(board).Fill(board.Zones())
    board.Save(pcb)
    print(f'  връзки {len(jobs)}: свързани {ok}, не стават {len(failed)}, разкъсани трасета {len(dead)}, '
          f'почистени {removed}', flush=True)
    if failed:
        print('  не стават:', ', '.join(sorted(set(failed))), flush=True)
    victims = sorted({tr.GetNetname() for tr in dead.values()}) if dead else []
    with open(pcb + '.victims.json', 'w', encoding='utf-8') as fv:
        json.dump({'victims': victims, 'ok': ok, 'failed': failed}, fv, ensure_ascii=False)


def count(pcb):
    rep = drc(pcb, os.path.join(os.path.dirname(pcb), 'doveshvane_drc.json'))
    errs = sum(1 for v in rep['violations'] if v['severity'] == 'error')
    return len(rep['unconnected_items']), errs


def evaluate(work, exe):
    """Почистено копие (<work>_c) и неговите несвързани / грешки."""
    base = work[:-len('.kicad_pcb')]
    for ext in ('.kicad_pcb', '.kicad_pro', '.kicad_dru'):
        if os.path.exists(base + ext):
            shutil.copy(base + ext, base + '_c' + ext)
    subprocess.run([sys.executable, '-I', os.path.abspath(__file__), '--clean', base + '_c.kicad_pcb'],
                   check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    return count(base + '_c.kicad_pcb')


def copy_board(src, dst):
    sb, db = src[:-len('.kicad_pcb')], dst[:-len('.kicad_pcb')]
    for ext in ('.kicad_pcb', '.kicad_pro', '.kicad_dru'):
        if os.path.exists(sb + ext):
            shutil.copy(sb + ext, db + ext)


def run_pass(pcb, exe, rip, nets):
    lst = pcb + '.nets.json'
    with open(lst, 'w', encoding='utf-8') as f:
        json.dump(sorted(nets), f, ensure_ascii=False)
    subprocess.run([sys.executable, '-I', os.path.abspath(__file__), '--pass', str(rip), pcb, exe, lst],
                   check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    return json.load(open(pcb + '.victims.json', encoding='utf-8'))


def greedy(args):
    """Връзка по връзка: разкъсване в разширяваща се зона около нея, после наново разкъсаните мрежи;
    приема се само ако несвързаните намалеят и DRC остане без грешки. Записва след всяко приемане."""
    pcb = os.path.abspath(args[0])
    out = os.path.abspath(args[1]) if len(args) > 1 else pcb
    exe = build(os.path.join(os.path.dirname(os.path.abspath(__file__)), 'doveshvane.c'))
    work = os.path.join(os.path.dirname(out), '_greedy.kicad_pcb')
    trial = os.path.join(os.path.dirname(out), '_greedy_t.kicad_pcb')
    copy_board(pcb, work)
    fix_uuids(work)
    cur, errs = count(work)
    print(f'начало: несвързани {cur}, други грешки {errs}', flush=True)
    rep = json.load(open(os.path.join(os.path.dirname(work), 'doveshvane_drc.json'), encoding='utf-8'))
    targets = []
    for u in rep['unconnected_items']:
        name = u['items'][0]['description'].split('[')[-1].split(']')[0]
        if name not in GROUND and name not in targets:
            targets.append(name)
    for name in targets:
        for r in (3, 6, 12, 25):
            copy_board(work, trial)
            v = run_pass(trial, exe, r, {name})
            if not v['ok']:
                continue
            if v['victims']:
                run_pass(trial, exe, 0, set(v['victims']) | {name})
            n, e = evaluate(trial, exe)
            print(f'  {name}, зона {r} мм: разкъсани мрежи {len(v["victims"])} → несвързани {n}, грешки {e}',
                  flush=True)
            if e == 0 and n < cur:
                copy_board(trial[:-len('.kicad_pcb')] + '_c.kicad_pcb', work)
                cur = n
                shutil.copy(work, out)
                print(f'приета: {name} → несвързани {cur}', flush=True)
                break
    shutil.copy(work, out)
    print(f'край: несвързани {cur} → {out}')


def main():
    args = sys.argv[1:]
    if args and args[0] == '--pass':            # един кръг в отделен процес (pcbnew не обича много Delete)
        # --pass <разкъсване мм | rip | plain> <платка> <рутер> [мрежи.json]
        r = {'rip': '3', 'plain': '0'}.get(args[1], args[1])
        only = set(json.load(open(args[4], encoding='utf-8'))) if len(args) > 4 else None
        one_pass(args[2], args[3], r, only=only)
        return
    if '--greedy' in args:
        args.remove('--greedy')
        return greedy(args)
    if args and args[0] == '--clean':           # само почистване и заливки
        board = pcbnew.LoadBoard(args[1])
        print('  почистени:', cleanup(board))
        pcbnew.ZONE_FILLER(board).Fill(board.Zones())
        board.Save(args[1])
        return
    rounds = 12
    if '--rounds' in args:
        k = args.index('--rounds'); rounds = int(args[k + 1]); del args[k:k + 2]
    pcb = os.path.abspath(args[0])
    out = os.path.abspath(args[1]) if len(args) > 1 else pcb
    here = os.path.dirname(os.path.abspath(__file__))
    exe = build(os.path.join(here, 'doveshvane.c'))
    work = os.path.join(os.path.dirname(pcb), '_doveshvane.kicad_pcb')
    for ext in ('.kicad_pro', '.kicad_dru'):
        src = pcb[:-len('.kicad_pcb')] + ext
        if os.path.exists(src):
            shutil.copy(src, work[:-len('.kicad_pcb')] + ext)
    shutil.copy(pcb, work)
    fix_uuids(work)
    best, best_n = None, None
    stale = 0
    for k in range(rounds):
        n, errs = evaluate(work, exe)
        print(f'кръг {k + 1}: несвързани {n}, други грешки {errs}', flush=True)
        if errs == 0 and (best_n is None or n < best_n):
            best_n, stale = n, 0
            best = work + '.best'
            shutil.copy(work[:-len('.kicad_pcb')] + '_c.kicad_pcb', best)
        else:
            stale += 1
        if n == 0 or stale >= 3:
            break
        # първият кръг без разкъсване, после с
        subprocess.run([sys.executable, '-I', os.path.abspath(__file__), '--pass', 'rip' if k else 'plain', work, exe],
                       check=True)
    else:
        n, errs = evaluate(work, exe)
        print(f'край: несвързани {n}, други грешки {errs}', flush=True)
        if errs == 0 and (best_n is None or n < best_n):
            best_n = n
            best = work + '.best'
            shutil.copy(work[:-len('.kicad_pcb')] + '_c.kicad_pcb', best)
    if best is None:
        sys.exit('няма резултат без грешки')
    shutil.copy(best, out)
    base = work[:-len('.kicad_pcb')]
    for b_ in (base, base + '_c'):
        for ext in ('.kicad_pcb', '.kicad_pro', '.kicad_dru', '.kicad_prl', '.kicad_pcb.best'):
            if os.path.exists(b_ + ext):
                os.remove(b_ + ext)
    if os.path.exists(work + '.best'):
        os.remove(work + '.best')
    print(f'най-добрият резултат: {best_n} несвързани → {out}')


if __name__ == '__main__':
    main()
