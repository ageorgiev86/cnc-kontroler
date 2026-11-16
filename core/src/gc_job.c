/* Изпълнителят: редовете на програмата с подпрограмите, няколко детайла и „Изпълни от ред“
 * (docs/interpretator-planer.md, раздел 5). Интерпретаторът (gc_interp.c) прави един ред;
 * тук е редът на редовете: подпрограми O / M98 / M97 / M99, блоковете между M6 по детайлите,
 * мълчаливото минаване до избрания ред и безопасният подход оттам. */
#include "gc_internal.h"

#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct { int ret, start, count; } frame_t;

typedef struct {
    const char **ln;
    int n;
    int pc, depth;
    frame_t st[GC_SUB_DEPTH];
    bool code, pct, done;
    long jumps;            /* скокове назад (GOTO, END, M99 с L) срещу безкраен цикъл */
} run_t;

static int seterr(gc_err_t *e, int line, const char *fmt, ...)
{
    e->line = line;
    e->col = -1;
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(e->msg, sizeof e->msg, fmt, ap);
    va_end(ap);
    return -1;
}

#define MAX_JUMPS 2000000L

static int jump(run_t *R, int tgt, gc_err_t *e)
{
    if (tgt <= R->pc && ++R->jumps > MAX_JUMPS)
        return seterr(e, R->pc, "Повече от %ld повторения: безкраен цикъл (WHILE, GOTO, M99)?", MAX_JUMPS);
    R->pc = tgt;
    return 0;
}

/* номерът в началото на реда след буквата (O123, N45) или -1 */
static long lead_num(const char *s, char letter)
{
    while (*s == ' ' || *s == '\t') s++;
    if ((*s & ~0x20) != letter) return -1;
    s++;
    if (*s < '0' || *s > '9') return -1;
    long v = 0;
    while (*s >= '0' && *s <= '9' && v < 100000000) v = v * 10 + (*s++ - '0');
    return v;
}

/* в реда има M-код m (без редовете с '/', ако се пропускат) */
static bool line_has_m(const gc_interp_t *in, const run_t *R, int pc, int m)
{
    if (pc < 0 || pc >= R->n) return false;
    gc_block_t b;
    gc_err_t e;
    if (gc_parse_line(R->ln[pc], &b, &e)) return false;
    if (b.block_delete && in->cfg.block_delete) return false;
    for (int k = 0; k < b.nm; k++)
        if (b.m[k] == m) return true;
    return false;
}

/* един ред: 0 = продължава, 1 = край на програмата, -1 = грешка */
static int step(gc_interp_t *in, run_t *R, gc_err_t *e)
{
    if (R->pc >= R->n) { R->done = true; return 1; }
    if (in->cancel) return seterr(e, R->pc, "Програмата е спряна");
    gc_block_t b;
    const char *s = R->ln[R->pc];
    if (gc_parse_line(s, &b, e) == 0 && b.percent) {
        if (R->code || R->pct) { R->done = true; return 1; }   /* вторият % е краят на програмата */
        R->pct = true;
        R->pc++;
        return 0;
    }
    if (gc_interp_line(in, s, R->pc, e)) return -1;
    if (!b.empty) R->code = true;
    if (in->ended) { R->done = true; return 1; }
    int f = in->flow;
    if (f == GC_FLOW_CALL || f == GC_FLOW_CALL_N) {
        int tgt = -1;
        for (int i = 0; i < R->n && tgt < 0; i++)
            if (lead_num(R->ln[i], f == GC_FLOW_CALL ? 'O' : 'N') == in->flow_p) tgt = i;
        if (tgt < 0)
            return seterr(e, R->pc, f == GC_FLOW_CALL ? "Подпрограмата O%d не е намерена" : "Редът N%d не е намерен (M97)", in->flow_p);
        if (R->depth >= GC_SUB_DEPTH) return seterr(e, R->pc, "Повече от %d вложени подпрограми", GC_SUB_DEPTH);
        if (in->flow_l == 0) { R->pc++; return 0; }
        int start = f == GC_FLOW_CALL ? tgt + 1 : tgt;
        R->st[R->depth].ret = R->pc + 1;
        R->st[R->depth].start = start;
        R->st[R->depth].count = in->flow_l;
        R->depth++;
        R->pc = start;
        return 0;
    }
    if (f == GC_FLOW_GOTO) {   /* редът N: напред, после от началото */
        int tgt = -1;
        for (int k = 1; k <= R->n && tgt < 0; k++) {
            int i = (R->pc + k) % R->n;
            if (lead_num(R->ln[i], 'N') == in->flow_p) tgt = i;
        }
        if (tgt < 0) return seterr(e, R->pc, "GOTO %d: редът N%d не е намерен", in->flow_p, in->flow_p);
        return jump(R, tgt, e);
    }
    if (f == GC_FLOW_WHILE) {
        int depth = 0, id;
        for (int i = R->pc + 1; i < R->n; i++) {   /* съответният END (изпълнено: влиза, иначе след END) */
            id = -1;
            gc_mac_kind_t k = gc_macro_scan(R->ln[i], &id);
            if (id != in->flow_p) continue;
            if (k == GC_MAC_WHILE) depth++;
            else if (k == GC_MAC_END && depth-- == 0) { R->pc = in->flow_cond ? R->pc + 1 : i + 1; return 0; }
        }
        return seterr(e, R->pc, "WHILE … DO%d без END%d", in->flow_p, in->flow_p);
    }
    if (f == GC_FLOW_END) {   /* обратно към съответния WHILE (условието се проверява пак) */
        int depth = 0, id;
        for (int i = R->pc - 1; i >= 0; i--) {
            id = -1;
            gc_mac_kind_t k = gc_macro_scan(R->ln[i], &id);
            if (id != in->flow_p) continue;
            if (k == GC_MAC_END) depth++;
            else if (k == GC_MAC_WHILE && depth-- == 0) return jump(R, i, e);
        }
        return seterr(e, R->pc, "END%d без WHILE … DO%d", in->flow_p, in->flow_p);
    }
    if (f == GC_FLOW_RET) {
        if (!R->depth) return seterr(e, R->pc, "M99 без извикване на подпрограма (M98)");
        if (--R->st[R->depth - 1].count > 0) return jump(R, R->st[R->depth - 1].start, e);
        else R->pc = R->st[--R->depth].ret;
        return 0;
    }
    R->pc++;
    return 0;
}

/* безопасен подход до началото на избрания ред (in->pos е програмираната позиция там) */
static int approach(gc_interp_t *in, const double *real, const gc_job_t *job, int line, bool m6_line, int part, bool multi, gc_err_t *e)
{
    if (in->comp_side)
        return seterr(e, line, "Изпълни от ред: редът е при включена компенсация G41/G42. Изберете ред преди G41/G42.");
    in->silent = false;
    gc_cmd_t c;
    memset(&c, 0, sizeof c);
    c.line = line;
    double safe = job->safe_z;
    /* 1. Z нагоре там, където е машината */
    memcpy(c.tgt, real, sizeof c.tgt);
    if (c.tgt[GC_Z] < safe) { c.kind = GC_RAPID; c.tgt[GC_Z] = safe; gc_out(in, &c); }
    /* 2. състоянието отпреди реда: детайл, нула, инструмент, корекция, шпиндел, охлаждане */
    if (multi) gc_emit_simple(in, GC_PART, part, job->parts[part], line);
    gc_emit_simple(in, GC_WCS_SELECT, in->wcs, 0, line);
    if (!m6_line && in->tool > 0) gc_emit_simple(in, GC_TOOL_CHANGE, in->tool, 0, line);
    if (in->tlo_on) gc_emit_simple(in, GC_TLO, in->tlo_h, in->tlo, line);
    if (in->sp_dir) gc_emit_simple(in, GC_SPINDLE, in->sp_dir, in->s, line);
    if (in->cool) gc_emit_simple(in, GC_COOLANT, in->cool, 0, line);
    /* 3. XY (и въртящите оси) на безопасната височина */
    memcpy(c.tgt, in->pos, sizeof c.tgt);
    c.tgt[GC_Z] = in->comp_out[GC_Z] > safe ? in->comp_out[GC_Z] : safe;
    c.kind = GC_RAPID;
    gc_out(in, &c);
    /* 4. бързо до approach мм над началото, после с подаване до него */
    double z0 = in->pos[GC_Z], zc = z0 + job->approach;
    if (zc < c.tgt[GC_Z]) { c.tgt[GC_Z] = zc; gc_out(in, &c); }
    c.tgt[GC_Z] = z0;
    if (in->feed > 0) { c.kind = GC_FEED; c.feed = in->feed; }
    gc_out(in, &c);
    return 0;
}

int gc_run_text(gc_interp_t *in, const char *text, gc_err_t *e) { return gc_run_job(in, text, NULL, e); }

int gc_run_job(gc_interp_t *in, const char *text, const gc_job_t *job, gc_err_t *e)
{
    /* начала на редовете */
    int n = 1;
    for (const char *p = text; *p; p++)
        if (*p == '\n') n++;
    const char **ln = malloc(sizeof *ln * (size_t)n);
    if (!ln) return seterr(e, 0, "Няма памет");
    n = 0;
    ln[n++] = text;
    for (const char *p = text; *p; p++)
        if (*p == '\n' && p[1]) ln[n++] = p + 1;

    run_t R;
    memset(&R, 0, sizeof R);
    R.ln = ln;
    R.n = n;
    gc_interp_t *snap = NULL;
    run_t snapR;
    int rc = 0, r;

    int np = job && job->nparts > 0 && job->parts ? job->nparts : 0;
    for (int k = 0; k < np; k++)
        if (job->parts[k] < 1 || job->parts[k] >= GC_WCS_N) { rc = seterr(e, 0, "Невалидна нула на детайл %d", k + 1); goto out; }
    bool multi = np > 1;
    int start = job ? job->start_line : -1, spart = job && job->start_part >= 0 && job->start_part < np ? job->start_part : 0;
    if (start >= n) { rc = seterr(e, n - 1, "Редът %d е след края на файла", start + 1); goto out; }
    double real[GC_NAX];
    memcpy(real, in->pos, sizeof real);
    if (np >= 1) {   /* G54 на програмата → нулата на (първия) детайл */
        in->wcs_map = job->parts[0];
        if (in->wcs_prog == 1) in->wcs = in->wcs_map;
    }
    if (start >= 0) in->silent = true;
    in->wcs_multi = multi;

#define CHECK_START(k) \
    if (in->silent && R.pc == start && (!multi || (k) == spart)) { \
        if (approach(in, real, job, R.pc, line_has_m(in, &R, R.pc, 6), spart, multi, e)) { rc = -1; goto out; } \
    }
#define STEP() \
    do { r = step(in, &R, e); if (r < 0) { rc = -1; goto out; } } while (0)

    if (!multi) {
        while (!R.done) { CHECK_START(0); STEP(); }
    } else {
        bool has_m6 = false;
        for (int i = 0; i < n && !has_m6; i++) has_m6 = line_has_m(in, &R, i, 6);
        snap = malloc(sizeof *snap);
        if (!snap) { rc = seterr(e, 0, "Няма памет"); goto out; }
        /* без M6: цялата програма е един блок; с M6: редовете преди първата смяна са веднъж */
        if (has_m6)
            while (!R.done && !line_has_m(in, &R, R.pc, 6)) { CHECK_START(spart); STEP(); }
        while (!R.done) {
            if (has_m6) { CHECK_START(spart); STEP(); }   /* редът със смяната: веднъж за блока */
            if (R.done) break;
            *snap = *in;
            snapR = R;
            for (int k = 0; k < np; k++) {
                if (k > 0) {   /* същото състояние като в началото на блока; позицията остава реалната */
                    if (in->comp_side) gc_comp_finish(in);
                    double pos[GC_NAX], out[GC_NAX];
                    bool silent = in->silent;
                    /* какво е на машината сега (след предишния детайл) */
                    int sp = in->sp_dir, cool = in->cool, h = in->tlo_h, wcs = in->wcs;
                    double s = in->s, tlo = in->tlo;
                    bool tlo_on = in->tlo_on;
                    memcpy(pos, in->pos, sizeof pos);
                    memcpy(out, in->comp_out, sizeof out);
                    *in = *snap;
                    memcpy(in->pos, pos, sizeof pos);
                    memcpy(in->comp_out, out, sizeof out);
                    in->silent = silent;
                    R = snapR;
                    /* включеното в началото на блока се връща (напр. шпинделът след M5 в края на детайла);
                     * изключено не се спира: следващите редове на детайла сами го пускат */
                    if (in->sp_dir && (in->sp_dir != sp || in->s != s)) gc_emit_simple(in, GC_SPINDLE, in->sp_dir, in->s, R.pc);
                    if (in->cool & ~cool) gc_emit_simple(in, GC_COOLANT, in->cool | cool, 0, R.pc);
                    in->cool |= cool;
                    if (in->tlo_on && (!tlo_on || in->tlo != tlo || in->tlo_h != h)) gc_emit_simple(in, GC_TLO, in->tlo_h, in->tlo, R.pc);
                    if (!in->tlo_on && tlo_on) { in->tlo_on = true; in->tlo = tlo; in->tlo_h = h; }
                    if (!in->sp_dir && sp) { in->sp_dir = sp; in->s = s; }
                    in->wcs = wcs;   /* за сравнението отдолу: действащата нула на машината */
                }
                /* смяна на детайла: първо Z на безопасната височина, после към следващата нула */
                if ((k > 0 || in->wcs != job->parts[k]) && !in->silent && in->comp_out[GC_Z] < job->safe_z) {
                    gc_cmd_t c;
                    memset(&c, 0, sizeof c);
                    c.kind = GC_RAPID;
                    c.line = R.pc;
                    memcpy(c.tgt, in->comp_out, sizeof c.tgt);
                    c.tgt[GC_Z] = job->safe_z;
                    gc_out(in, &c);
                    memcpy(in->pos, c.tgt, sizeof in->pos);
                }
                in->wcs_map = job->parts[k];
                int eff = in->wcs_prog == 1 ? job->parts[k] : in->wcs_prog;
                if (eff != in->wcs) { in->wcs = eff; gc_emit_simple(in, GC_WCS_SELECT, eff, 0, R.pc); }
                gc_emit_simple(in, GC_PART, k, job->parts[k], R.pc);
                while (!R.done) {
                    if (line_has_m(in, &R, R.pc, 6)) break;   /* следващият блок */
                    if (k < np - 1 && (line_has_m(in, &R, R.pc, 2) || line_has_m(in, &R, R.pc, 30))) break;   /* краят: само след последния */
                    CHECK_START(k);
                    STEP();
                }
            }
            if (!has_m6) break;
        }
    }
    if (in->silent) { rc = seterr(e, start, "Редът %d не се изпълнява (след края на програмата или в подпрограма, която не се вика)", start + 1); goto out; }
    if (R.depth && !in->ended) { rc = seterr(e, n - 1, "Подпрограмата не завършва с M99"); goto out; }
    if (!in->ended) {
        in->ended = true;
        gc_emit_simple(in, GC_END, 0, 0, R.pc < n ? R.pc : n - 1);
    }
    if (in->comp_err[0]) rc = seterr(e, in->comp_err_line, "%s", in->comp_err);
#undef CHECK_START
#undef STEP
out:
    free(snap);
    free(ln);
    return rc;
}
