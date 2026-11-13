"""
Генератор на схеми за KiCad 10 (.kicad_sch).
Copyright (c) 2026 ageorgiev86. Всички права запазени.

Символите се копират от библиотеките на KiCad. Всеки извод се свързва с
къс проводник и етикет (глобален етикет, захранване или „несвързан“),
затова листовете са подредени като таблици, а не като рисунка.
Пуска се с Python-а на KiCad:
  "C:\\Program Files\\KiCad\\10.0\\bin\\python.exe" -I osnovna_platka.py <папка на проекта>
"""
import os
import re
import uuid as _uuid

KICAD_SYM = r"C:\Program Files\KiCad\10.0\share\kicad\symbols"
POWER_NETS = ('+3V3', '+5V', 'GND', '+24V')


_NS = _uuid.UUID('6f1d3c52-8a4e-4b7a-9c1e-2d5f0a7b3c91')


def uid(key=None):
    """Случаен UUID, или постоянен (изведен от key), за да остане връзката схема ↔ платка."""
    return str(_uuid.uuid5(_NS, key)) if key else str(_uuid.uuid4())


def f(v):
    return f'{v:.4f}'.rstrip('0').rstrip('.')


# ---------- s-изрази ----------

def tokenize(text):
    return re.findall(r'"(?:\\.|[^"\\])*"|\(|\)|[^\s()"]+', text)


def parse(tokens, i=0):
    out = []
    while i < len(tokens):
        t = tokens[i]
        if t == '(':
            sub, i = parse(tokens, i + 1)
            out.append(sub)
        elif t == ')':
            return out, i + 1
        else:
            out.append(t)
            i += 1
    return out, i


def dump(e, ind=0):
    if not isinstance(e, list):
        return e
    if all(not isinstance(x, list) for x in e):
        return '(' + ' '.join(e) + ')'
    head = []
    for x in e:
        if isinstance(x, list):
            break
        head.append(x)
    s = '(' + ' '.join(head)
    pad = '\t' * (ind + 1)
    for x in e[len(head):]:
        s += '\n' + pad + dump(x, ind + 1)
    return s + '\n' + '\t' * ind + ')'


def q(s):
    return '"' + str(s).replace('\\', '\\\\').replace('"', '\\"').replace('\n', '\\n') + '"'


def font(hide=False, justify=None):
    e = ['effects', ['font', ['size', '1.27', '1.27']]]
    if justify:
        e.append(['justify'] + justify.split())
    if hide:
        e.append(['hide', 'yes'])
    return e


# ---------- библиотеки ----------

_libs = {}


def lib_symbol(lib_id):
    """Символът от библиотеката (с разгънато наследяване) и списък с изводите му."""
    lib, name = lib_id.split(':')
    if lib not in _libs:
        text = open(os.path.join(KICAD_SYM, lib + '.kicad_sym'), encoding='utf-8').read()
        tree, _ = parse(tokenize(text))
        _libs[lib] = {e[1].strip('"'): e for e in tree[0] if isinstance(e, list) and e and e[0] == 'symbol'}
    src = _libs[lib][name]
    ext = [x for x in src if isinstance(x, list) and x[0] == 'extends']
    if ext:
        bname = ext[0][1].strip('"')
        base, _ = lib_symbol(lib + ':' + bname)
        props = {x[1]: x for x in src if isinstance(x, list) and x[0] == 'property'}
        sym = ['symbol', q(lib_id)]
        for x in base[2:]:
            if isinstance(x, list) and x[0] == 'property' and x[1] in props:
                sym.append(props.pop(x[1]))
            elif isinstance(x, list) and x[0] == 'symbol':
                sub = list(x)
                sub[1] = q(sub[1].strip('"').replace(bname, name, 1))
                sym.append(sub)
            else:
                sym.append(x)
        # свойствата, които ги има само в наследника, идват след тези на основата
        last = max(i for i, x in enumerate(sym) if isinstance(x, list) and x[0] == 'property')
        sym[last + 1:last + 1] = list(props.values())
    else:
        sym = ['symbol', q(lib_id)] + list(src[2:])
    pins = []

    def walk(e, unit):
        for x in e:
            if isinstance(x, list):
                if x[0] == 'symbol':
                    m = re.search(r'_(\d+)_(\d+)"$', x[1])
                    walk(x, int(m.group(1)) if m else unit)
                elif x[0] == 'pin':
                    at = [y for y in x if isinstance(y, list) and y[0] == 'at'][0]
                    num = [y for y in x if isinstance(y, list) and y[0] == 'number'][0][1].strip('"')
                    nm = [y for y in x if isinstance(y, list) and y[0] == 'name'][0][1].strip('"')
                    pins.append(dict(num=num, name=nm, x=float(at[1]), y=float(at[2]),
                                     ang=int(float(at[3])), etype=x[1], unit=unit))
    walk(sym, 0)
    return sym, pins


# ---------- лист ----------

class Sheet:
    def __init__(self, name, title, project, paper='A3'):
        self.name = name
        self.title = title
        self.project = project
        self.paper = paper
        self.uuid = uid('sheet:' + name)
        self.items = []
        self.lib = {}
        self.path = None
        self.npwr = 0

    def _lib(self, lib_id):
        if lib_id not in self.lib:
            self.lib[lib_id] = lib_symbol(lib_id)
        return self.lib[lib_id]

    def _sym(self, lib_id, ref, value, x, y, rot, unit, footprint, fields, hide_ref=False, value_dy=-5.5, value_dx=0):
        sym, pins = self._lib(lib_id)
        if not footprint:
            for x_ in sym:
                if isinstance(x_, list) and x_[0] == 'property' and x_[1] == '"Footprint"':
                    footprint = x_[2].strip('"')
        rx, ry, rj, vj = x, y - 3, None, None
        if len(pins) == 2 and not hide_ref and rot == 0 and all(p_['y'] == 0 for p_ in pins):
            # хоризонтални (диоди): надписите отгоре и отдолу
            rx, ry = x, y - 2.54
            value_dx, value_dy = 0, 2.54
        elif len(pins) <= 3 and not hide_ref and rot == 0:
            # малки елементи (R, C, D): надписите встрани
            rx, ry, rj = x + 2.54, y - 1.27, 'left'
            value_dx, value_dy, vj = 2.54, 1.27, 'left'
        e = ['symbol', ['lib_id', q(lib_id)], ['at', f(x), f(y), str(rot)], ['unit', str(unit)],
             ['exclude_from_sim', 'no'], ['in_bom', 'yes'], ['on_board', 'yes'], ['dnp', 'no'],
             ['uuid', q(uid(f'sym:{self.name}:{ref}:{unit}'))],
             ['property', '"Reference"', q(ref), ['at', f(rx), f(ry), '0'], font(hide_ref, rj)],
             ['property', '"Value"', q(value), ['at', f(x + value_dx), f(y + value_dy), '0'], font(justify=vj)],
             ['property', '"Footprint"', q(footprint), ['at', f(x), f(y), '0'], font(True)],
             ['property', '"Datasheet"', '"~"', ['at', f(x), f(y), '0'], font(True)]]
        for k, v in (fields or {}).items():
            e.append(['property', q(k), q(v), ['at', f(x), f(y), '0'], font(True)])
        for p in pins:
            if p['unit'] in (0, unit):
                e.append(['pin', q(p['num']), ['uuid', q(uid())]])
        e.append('INSTANCES')
        self.items.append(dict(kind='symbol', e=e, ref=ref, unit=unit))
        return pins

    def symbol(self, lib_id, ref, value, x, y, footprint='', unit=1, fields=None, value_dy=-5.5):
        pins = self._sym(lib_id, ref, value, x, y, 0, unit, footprint, fields, value_dy=value_dy)
        return SymbolRef(self, pins, x, y, unit)

    def wire(self, x1, y1, x2, y2):
        self.items.append(dict(kind='raw', e=['wire', ['pts', ['xy', f(x1), f(y1)], ['xy', f(x2), f(y2)]],
                                               ['stroke', ['width', '0'], ['type', 'default']], ['uuid', q(uid())]]))

    def glabel(self, name, x, y, ang, shape='bidirectional'):
        just = 'right' if ang in (180, 270) else 'left'
        self.items.append(dict(kind='raw', e=[
            'global_label', q(name), ['shape', shape], ['at', f(x), f(y), str(ang)],
            ['fields_autoplaced', 'yes'], font(justify=just), ['uuid', q(uid())],
            ['property', '"Intersheetrefs"', '"${INTERSHEET_REFS}"', ['at', f(x), f(y), '0'], font(True)]]))

    def label(self, name, x, y, ang):
        just = ('right' if ang in (180, 270) else 'left') + ' bottom'
        self.items.append(dict(kind='raw', e=['label', q(name), ['at', f(x), f(y), str(ang)],
                                               ['fields_autoplaced', 'yes'], font(justify=just), ['uuid', q(uid())]]))

    def noconnect(self, x, y):
        self.items.append(dict(kind='raw', e=['no_connect', ['at', f(x), f(y)], ['uuid', q(uid())]]))

    def text(self, s, x, y, size=1.27):
        self.items.append(dict(kind='raw', e=['text', q(s), ['exclude_from_sim', 'no'], ['at', f(x), f(y), '0'],
                                               ['effects', ['font', ['size', f(size), f(size)]], ['justify', 'left', 'top']],
                                               ['uuid', q(uid())]]))

    # завъртане на захранващите символи според посоката навън (0 дясно, 90 горе, 180 ляво, 270 долу)
    _ROT_UP = {90: 0, 180: 90, 270: 180, 0: 270}      # символи с графика нагоре (+3V3, +5V, PWR_FLAG)
    _ROT_DOWN = {270: 0, 0: 90, 90: 180, 180: 270}    # символи с графика надолу (GND)
    _VAL = {0: (5.5, 0), 90: (0, -3.8), 180: (-5.5, 0), 270: (0, 3.8)}

    def power(self, net, x, y, d=90):
        """Захранващ символ в точка x, y; d е посоката, в която сочи графиката."""
        self.npwr += 1
        gnd = net.startswith('GND')
        rot = (self._ROT_DOWN if gnd else self._ROT_UP)[d]
        dx, dy = self._VAL[d]
        self._sym('power:' + net, f'#PWR{self.npwr:03d}', net, x, y, rot, 1, '', None,
                  hide_ref=True, value_dy=dy, value_dx=dx)

    def pwr_flag(self, net, x, y):
        """PWR_FLAG и захранващ символ в една точка (флагът сочи обратно на символа)."""
        gnd = net.startswith('GND')
        self.npwr += 1
        d = 90 if gnd else 270
        dx, dy = self._VAL[d]
        self._sym('power:PWR_FLAG', f'#FLG{self.npwr:03d}', 'PWR_FLAG', x, y, self._ROT_UP[d], 1, '', None,
                  hide_ref=True, value_dy=dy, value_dx=dx)
        self.power(net, x, y, 270 if gnd else 90)

    def render(self, extra=None):
        out = ['kicad_sch', ['version', '20260306'], ['generator', '"eeschema"'], ['generator_version', '"10.0"'],
               ['uuid', q(self.uuid)], ['paper', q(self.paper)]]
        out.append(['lib_symbols'] + [s for s, _ in self.lib.values()])
        for it in self.items:
            e = list(it['e'])
            if it['kind'] == 'symbol':
                e[e.index('INSTANCES')] = ['instances', ['project', q(self.project),
                                                         ['path', q(self.path), ['reference', q(it['ref'])],
                                                          ['unit', str(it['unit'])]]]]
            out.append(e)
        out += extra or []
        out.append(['embedded_fonts', 'no'])
        return dump(out) + '\n'


_refs = {}


def next_ref(prefix):
    _refs[prefix] = _refs.get(prefix, 0) + 1
    return f'{prefix}{_refs[prefix]}'


def place(sheet, lib_id, prefix, value, x, y, conns, footprint='', unit=1, stub=5.08, ref=None, fields=None):
    """Поставя символ и свързва изводите му: conns = {номер или име на извод: мрежа}.
    Изводите, които не са в conns, остават несвързани (с флаг NC)."""
    ref = ref or next_ref(prefix)
    sym = sheet.symbol(lib_id, ref, value, x, y, footprint=footprint, unit=unit, fields=fields)
    used = set()
    for key, net in conns.items():
        k = str(key)
        p = [pp for pp in sym.pins if pp['unit'] in (0, unit) and pp['num'] == k]
        if not p:
            # по име; всички изводи с това име (напр. VDD); „PA13“ намира и „PA13(JTMS/SWDIO)“
            p = [pp for pp in sym.pins if pp['unit'] in (0, unit)
                 and (pp['name'] == k or pp['name'].startswith(k + '('))]
        if not p:
            raise KeyError(f'{lib_id}: няма извод {key}')
        seen = set()
        for pp in p:
            used.add(pp['num'])
            if (pp['x'], pp['y']) in seen:          # изводи един върху друг са вече свързани
                continue
            seen.add((pp['x'], pp['y']))
            sym.connect(pp['num'], net, length=stub)
    for pp in sym.pins:
        if pp['unit'] in (0, unit) and pp['num'] not in used:
            sym.connect(pp['num'], None)
    return ref


class SymbolRef:
    def __init__(self, sheet, pins, x, y, unit):
        self.sheet, self.pins, self.x, self.y, self.unit = sheet, pins, x, y, unit

    def pin(self, num):
        for p in self.pins:
            if p['num'] == str(num) and p['unit'] in (0, self.unit):
                px, py = self.x + p['x'], self.y - p['y']          # библиотеката: y нагоре
                out = {0: 180, 180: 0, 90: 270, 270: 90}[p['ang']]  # посока навън от символа
                return px, py, out
        raise KeyError(num)

    def connect(self, num, net, length=7.62, shape='bidirectional'):
        """Свързва извод с глобален етикет, захранване или „несвързан“ (net = None)."""
        px, py, d = self.pin(num)
        if net is None:
            self.sheet.noconnect(px, py)
            return None
        dx, dy = {0: (1, 0), 180: (-1, 0), 90: (0, -1), 270: (0, 1)}[d]
        ex, ey = px + dx * length, py + dy * length
        self.sheet.wire(px, py, ex, ey)
        if net in POWER_NETS:
            self.sheet.power(net, ex, ey, d)
        elif net.startswith('.'):
            self.sheet.label(net[1:], ex, ey, d)          # локален етикет (само в този лист)
        else:
            self.sheet.glabel(net, ex, ey, d, shape)
        return ex, ey


def write_project(folder, name, sheets, title):
    """Главен лист с йерархични листове, по един файл за всеки."""
    root = Sheet(name, title, name)
    root.path = '/' + root.uuid
    extra = []
    x, y = 25.4, 50.8
    for i, sh in enumerate(sheets):
        su = uid('sheetinst:' + sh.name)
        sh.path = f'/{root.uuid}/{su}'
        fn = sh.name + '.kicad_sch'
        extra.append(['sheet', ['at', f(x), f(y)], ['size', '63.5', '17.78'], ['exclude_from_sim', 'no'],
                      ['in_bom', 'yes'], ['on_board', 'yes'], ['dnp', 'no'], ['fields_autoplaced', 'yes'],
                      ['stroke', ['width', '0.1524'], ['type', 'solid']], ['fill', ['color', '0', '0', '0', '0.0000']],
                      ['uuid', q(su)],
                      ['property', '"Sheetname"', q(sh.title), ['at', f(x), f(y - 0.7116), '0'],
                       font(justify='left bottom')],
                      ['property', '"Sheetfile"', q(fn), ['at', f(x), f(y + 18.3716), '0'],
                       font(justify='left top')],
                      ['instances', ['project', q(name), ['path', q('/' + root.uuid), ['page', q(str(i + 2))]]]]])
        x += 88.9
        if x > 300:
            x, y = 25.4, y + 33.02
        with open(os.path.join(folder, fn), 'w', encoding='utf-8', newline='\n') as fh:
            fh.write(sh.render())
    root.text(title, 25.4, 25.4, 3)
    extra.append(['sheet_instances', ['path', '"/"', ['page', '"1"']]])
    with open(os.path.join(folder, name + '.kicad_sch'), 'w', encoding='utf-8', newline='\n') as fh:
        fh.write(root.render(extra))
