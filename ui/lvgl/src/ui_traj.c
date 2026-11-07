/* Картата „Траектория“: 3D изглед на програмата в lv_canvas.
 * Основата (решетка, оси, цялата траектория бледо) се рисува в отделен буфер само при промяна на изгледа,
 * размера или програмата. При всяко обновяване основата се копира и отгоре се рисуват изминатият път
 * и инструментът. Така на Orange Pi се рисуват само няколкостотин линии на кадър. */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "program.h"
#include "ui.h"
#include "ui_style.h"

static const uint32_t PAL[] = {0x59aee6, 0x3dbb7e, 0xe47ab3, 0xff9447, 0xa78bfa, 0x4fd1c5, 0xd9e45a, 0xf07167};
static const struct { float yaw, pitch; } VIEWS[4] = {
    {-(float)M_PI / 6, (float)M_PI / 3}, {0, 0}, {0, (float)M_PI / 2}, {-(float)M_PI / 2, (float)M_PI / 2}};

static lv_color_t tool_color(int t)
{
    for (int i = 0; i < P.ntools; i++)
        if (P.tools[i] == t) return lv_color_hex(PAL[i % 8]);
    return C_ACC;
}

/* --- проекция (като makeProj в макета) --- */
typedef struct { float cx, cy, cz, cyw, syw, cp, sp, sc, w2, h2; } proj_t;

static void raw(const proj_t *p, float x, float y, float z, float *a, float *b)
{
    float dx = x - p->cx, dy = y - p->cy, dz = z - p->cz;
    float x1 = dx * p->cyw - dy * p->syw, y1 = dx * p->syw + dy * p->cyw;
    *a = x1;
    *b = -(y1 * p->cp + dz * p->sp);
}

static lv_point_precise_t P2(const proj_t *p, float x, float y, float z)
{
    float a, b;
    raw(p, x, y, z, &a, &b);
    lv_point_precise_t r = {(lv_value_precise_t)lroundf(p->w2 + a * p->sc), (lv_value_precise_t)lroundf(p->h2 + b * p->sc)};
    return r;
}

static proj_t make_proj(const traj_t *t)
{
    proj_t p;
    float x0 = fminf(P.x0, 0), x1 = fmaxf(P.x1, 0), y0 = fminf(P.y0, 0), y1 = fmaxf(P.y1, 0);
    p.cx = (x0 + x1) / 2; p.cy = (y0 + y1) / 2; p.cz = (P.z0 + P.z1) / 2;
    p.cyw = cosf(t->yaw); p.syw = sinf(t->yaw); p.cp = cosf(t->pitch); p.sp = sinf(t->pitch);
    p.sc = 1; p.w2 = t->w / 2.0f; p.h2 = t->h / 2.0f;
    float mx = 1e-6f, my = 1e-6f;
    const float xs[2] = {x0, x1}, ys[2] = {y0, y1}, zs[2] = {P.z0, P.z1};
    for (int i = 0; i < 2; i++)
        for (int j = 0; j < 2; j++)
            for (int k = 0; k < 2; k++) {
                float a, b;
                raw(&p, xs[i], ys[j], zs[k], &a, &b);
                mx = fmaxf(mx, fabsf(a)); my = fmaxf(my, fabsf(b));
            }
    p.sc = fminf(p.w2 / mx, p.h2 / my) * 0.84f * t->zoom;
    return p;
}

static void line(lv_layer_t *l, lv_point_precise_t a, lv_point_precise_t b, lv_color_t c, int32_t w, lv_opa_t opa, int32_t dash)
{
    lv_draw_line_dsc_t d;
    lv_draw_line_dsc_init(&d);
    d.p1 = a; d.p2 = b; d.color = c; d.width = w; d.opa = opa;
    d.dash_width = dash; d.dash_gap = dash;
    d.round_start = d.round_end = !dash;
    lv_draw_line(l, &d);
}

/* ход от точка i0 до i1 (включително) */
static void path(lv_layer_t *l, const proj_t *p, int i0, int i1, lv_color_t c, int32_t w, lv_opa_t opa, int32_t dash)
{
    lv_point_precise_t a = P2(p, P.pt[i0].x, P.pt[i0].y, P.pt[i0].z);
    for (int i = i0 + 1; i <= i1; i++) {
        lv_point_precise_t b = P2(p, P.pt[i].x, P.pt[i].y, P.pt[i].z);
        line(l, a, b, c, w, opa, dash);
        a = b;
    }
}

static int32_t px(float x) { return (int32_t)lroundf(x * g_em / 16.0f); }   /* ширина на линии като в макета */

static void draw_base(traj_t *t)
{
    proj_t p = make_proj(t);
    lv_canvas_fill_bg(t->base_cv, C_SURF, LV_OPA_COVER);
    lv_layer_t l;
    lv_canvas_init_layer(t->base_cv, &l);

    /* решетка в равнината Z0 */
    static const float steps[] = {1, 2, 5, 10, 20, 50, 100, 200, 500, 1000};
    float step = 1000;
    for (unsigned i = 0; i < sizeof steps / sizeof steps[0]; i++)
        if (steps[i] * p.sc >= 14 * g_em / 16.0f) { step = steps[i]; break; }
    float gx0 = floorf(fminf(P.x0, 0) / step) * step, gx1 = ceilf(fmaxf(P.x1, 0) / step) * step;
    float gy0 = floorf(fminf(P.y0, 0) / step) * step, gy1 = ceilf(fmaxf(P.y1, 0) / step) * step;
    if ((gx1 - gx0) / step < 160 && (gy1 - gy0) / step < 160) {
        for (float x = gx0; x <= gx1 + 1e-3f; x += step) line(&l, P2(&p, x, gy0, 0), P2(&p, x, gy1, 0), C_LINE, 1, LV_OPA_COVER, 0);
        for (float y = gy0; y <= gy1 + 1e-3f; y += step) line(&l, P2(&p, gx0, y, 0), P2(&p, gx1, y, 0), C_LINE, 1, LV_OPA_COVER, 0);
    }

    /* цялата траектория бледо */
    for (int i = 0; i < P.nop; i++) {
        const op_t *o = &P.op[i];
        if (o->kind != OP_MOVE) continue;
        if (o->rapid) path(&l, &p, o->p0, o->p0 + o->np - 1, lv_color_hex(0x5b6f8f), 1, LV_OPA_COVER, px(4));
        else path(&l, &p, o->p0, o->p0 + o->np - 1, tool_color(o->tool), px(1.7f), 82, 0);
    }

    /* оси X Y Z от нулата */
    float L = 28 * g_em / 16.0f / p.sc;
    lv_point_precise_t o0 = P2(&p, 0, 0, 0);
    const struct { const char *n; lv_color_t c; float v[3]; } ax[3] = {
        {"X", C_BAD, {L, 0, 0}}, {"Y", C_OK, {0, L, 0}}, {"Z", C_INFO, {0, 0, L}}};
    for (int i = 0; i < 3; i++) {
        lv_point_precise_t e = P2(&p, ax[i].v[0], ax[i].v[1], ax[i].v[2]);
        if (abs((int)(e.x - o0.x)) + abs((int)(e.y - o0.y)) < 2 * g_em / 16) continue;
        line(&l, o0, e, ax[i].c, px(1.6f), LV_OPA_COVER, 0);
        lv_draw_label_dsc_t ld;
        lv_draw_label_dsc_init(&ld);
        ld.color = ax[i].c; ld.font = F.ui_xs; ld.text = ax[i].n;
        int32_t lx = (int32_t)(e.x + (e.x - o0.x) * 0.12f), ly = (int32_t)(e.y + (e.y - o0.y) * 0.12f);
        lv_area_t a = {lx - 6, ly - EM(0.6f), lx + EM(1), ly + EM(0.6f)};
        lv_draw_label(&l, &ld, &a);
    }
    lv_canvas_finish_layer(t->base_cv, &l);
    t->base_dirty = false;
}

static void draw_top(traj_t *t)
{
    proj_t p = make_proj(t);
    memcpy(t->buf->data, t->base->data, t->buf->data_size);
    lv_layer_t l;
    lv_canvas_init_layer(t->canvas, &l);
    /* изминатият път */
    int cur = M.op < P.nop ? M.op : P.nop - 1;
    for (int i = 0; i < cur && i < P.nop; i++) {
        const op_t *o = &P.op[i];
        if (o->kind != OP_MOVE) continue;
        if (o->rapid) path(&l, &p, o->p0, o->p0 + o->np - 1, C_DIM, 1, LV_OPA_COVER, px(4));
        else path(&l, &p, o->p0, o->p0 + o->np - 1, tool_color(o->tool), px(2.4f), LV_OPA_COVER, 0);
    }
    double wx = machine_work(AX_X), wy = machine_work(AX_Y), wz = machine_work(AX_Z);
    if (cur >= 0 && P.op[cur].kind == OP_MOVE && M.op_s > 0 && (M.state == MS_RUN || M.state == MS_HOLD)) {
        const op_t *o = &P.op[cur];
        int k = o->p0;
        while (k < o->p0 + o->np - 1 && P.cum[k + 1] < M.op_s) k++;
        lv_color_t c = o->rapid ? C_DIM : tool_color(o->tool);
        int32_t w = o->rapid ? 1 : px(2.4f), dsh = o->rapid ? px(4) : 0;
        if (k > o->p0) path(&l, &p, o->p0, k, c, w, LV_OPA_COVER, dsh);
        line(&l, P2(&p, P.pt[k].x, P.pt[k].y, P.pt[k].z), P2(&p, (float)wx, (float)wy, (float)wz), c, w, LV_OPA_COVER, dsh);
    }
    /* инструментът: дръжка нагоре, кръг на върха и кръст */
    double dia;
    machine_tool_name(M.tool, &dia);
    lv_point_precise_t tp = P2(&p, (float)wx, (float)wy, (float)wz);
    lv_point_precise_t up = P2(&p, (float)wx, (float)wy, (float)wz + 20);
    float rr = fmaxf(3.5f * g_em / 16.0f, (float)dia / 2 * p.sc);
    line(&l, tp, up, C_ACC, (int32_t)fmaxf(2 * g_em / 16, fminf(rr * 2, 10 * g_em / 16)), 140, 0);
    lv_draw_rect_dsc_t rd;
    lv_draw_rect_dsc_init(&rd);
    rd.radius = LV_RADIUS_CIRCLE; rd.bg_color = C_ACC; rd.bg_opa = 41;
    rd.border_color = C_ACC; rd.border_width = px(1.5f); rd.border_opa = LV_OPA_COVER;
    lv_area_t a = {(int32_t)(tp.x - rr), (int32_t)(tp.y - rr), (int32_t)(tp.x + rr), (int32_t)(tp.y + rr)};
    lv_draw_rect(&l, &rd, &a);
    int32_t cr = (int32_t)rr + px(5);
    lv_point_precise_t h0 = {tp.x - cr, tp.y}, h1 = {tp.x + cr, tp.y}, v0 = {tp.x, tp.y - cr}, v1 = {tp.x, tp.y + cr};
    line(&l, h0, h1, C_ACC, px(1.5f), LV_OPA_COVER, 0);
    line(&l, v0, v1, C_ACC, px(1.5f), LV_OPA_COVER, 0);
    lv_canvas_finish_layer(t->canvas, &l);
    lv_obj_invalidate(t->canvas);
}

static void alloc_bufs(traj_t *t, int32_t w, int32_t h)
{
    if (t->buf) { lv_draw_buf_destroy(t->buf); lv_draw_buf_destroy(t->base); }
    t->w = w; t->h = h;
    t->buf = lv_draw_buf_create((uint32_t)w, (uint32_t)h, LV_COLOR_FORMAT_NATIVE, 0);
    t->base = lv_draw_buf_create((uint32_t)w, (uint32_t)h, LV_COLOR_FORMAT_NATIVE, 0);
    lv_canvas_set_draw_buf(t->canvas, t->buf);
    lv_canvas_set_draw_buf(t->base_cv, t->base);
    t->base_dirty = true;
    t->drawn_at = 0;
}

static void on_size(lv_event_t *e)
{
    traj_t *t = lv_event_get_user_data(e);
    int32_t w = lv_obj_get_content_width(t->body), h = lv_obj_get_content_height(t->body);
    if (w < 16 || h < 16 || (w == t->w && h == t->h)) return;
    alloc_bufs(t, w, h);
}

static void set_view(traj_t *t, int v)
{
    if (v >= 0) { t->yaw = VIEWS[v].yaw; t->pitch = VIEWS[v].pitch; }
    t->view = v;
    for (int i = 0; i < 4; i++) ui_btn_set_kind(t->seg[i], i == v ? BTN_ON : BTN_NORMAL);
    t->base_dirty = true;
}

static void on_view(lv_event_t *e)
{
    traj_t *t = lv_event_get_user_data(e);
    int v = (int)(intptr_t)lv_obj_get_user_data(lv_event_get_target_obj(e));
    if (v == 4) { t->zoom = 1; t->base_dirty = true; return; }   /* „Побери“ */
    t->zoom = 1;
    set_view(t, v);
}

/* плъзгане: завъртане (като в макета) */
static void on_drag(lv_event_t *e)
{
    traj_t *t = lv_event_get_user_data(e);
    lv_point_t v;
    lv_indev_get_vect(lv_indev_active(), &v);
    if (!v.x && !v.y) return;
    t->yaw += v.x * 0.01f;
    t->pitch = fmaxf(0, fminf((float)M_PI / 2, t->pitch + v.y * 0.01f));
    if (t->view >= 0) set_view(t, -1);
    t->base_dirty = true;
}

void traj_create(traj_t *t, lv_obj_t *parent)
{
    memset(t, 0, sizeof *t);
    t->zoom = 1;
    lv_obj_t *hr;
    t->card = ui_card(parent, "ТРАЕКТОРИЯ", &hr);
    static const char *const views[] = {"3D", "XY", "XZ", "YZ", "ПОБЕРИ"};
    lv_obj_t *s = ui_box(hr);
    ui_flex(s, LV_FLEX_FLOW_ROW, 0.25f);
    for (int i = 0; i < 5; i++) {
        t->seg[i] = ui_btn(s, views[i], i == 0 ? BTN_ON : BTN_NORMAL, true);
        lv_obj_set_user_data(t->seg[i], (void *)(intptr_t)i);
        lv_obj_add_event_cb(t->seg[i], on_view, LV_EVENT_CLICKED, t);
    }
    /* в портрет заглавието се скрива (както в макета) */
    if (g_portrait) lv_obj_set_hidden(lv_obj_get_child(lv_obj_get_child(t->card, 0), 0), true);

    lv_obj_t *b = t->body = ui_box(t->card);
    lv_obj_set_width(b, LV_PCT(100));
    lv_obj_set_flex_grow(b, 1);
    lv_obj_add_event_cb(b, on_size, LV_EVENT_SIZE_CHANGED, t);

    t->base_cv = lv_canvas_create(b);
    lv_obj_set_hidden(t->base_cv, true);
    t->canvas = lv_canvas_create(b);
    lv_obj_set_clickable(t->canvas, true);
    lv_obj_add_event_cb(t->canvas, on_drag, LV_EVENT_PRESSING, t);

    /* легенда, подсказка и ред с Z/F/T */
    t->leg = ui_box(b);
    lv_obj_set_ignore_layout(t->leg, true);
    lv_obj_align(t->leg, LV_ALIGN_TOP_LEFT, EM(0.5f), EM(0.45f));
    lv_obj_set_style_bg_color(t->leg, C_SURF, 0);
    lv_obj_set_style_bg_opa(t->leg, 209, 0);
    lv_obj_set_style_pad_all(t->leg, EM(0.35f), 0);
    lv_obj_set_style_radius(t->leg, EM(0.25f), 0);
    ui_flex(t->leg, LV_FLEX_FLOW_COLUMN, 0.15f);
    lv_obj_set_flex_align(t->leg, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    for (int i = 0; i <= P.ntools; i++) {
        lv_obj_t *r = ui_box(t->leg);
        ui_flex(r, LV_FLEX_FLOW_ROW, 0.45f);
        lv_obj_t *sw = ui_box(r);
        lv_obj_set_size(sw, EM(1.2f * 0.74f), EM(0.28f * 0.74f) + 1);
        lv_obj_set_style_bg_opa(sw, LV_OPA_COVER, 0);
        char buf[64];
        if (i < P.ntools) {
            lv_obj_set_style_bg_color(sw, lv_color_hex(PAL[i % 8]), 0);
            snprintf(buf, sizeof buf, "T%d · %s", P.tools[i], machine_tool_name(P.tools[i], NULL));
        } else {
            lv_obj_set_style_bg_color(sw, lv_color_hex(0x5b6f8f), 0);
            snprintf(buf, sizeof buf, "бърз ход");
        }
        ui_label(r, F.txt_sm, C_DIM, buf);
    }
    lv_obj_t *hint = ui_label(b, F.txt_sm, C_FAINT, "плъзни: завъртане");
    lv_obj_set_ignore_layout(hint, true);
    lv_obj_align(hint, LV_ALIGN_TOP_RIGHT, -EM(0.6f), EM(0.55f));
    t->info = ui_label(b, F.num_sm, C_DIM, "");
    lv_obj_set_ignore_layout(t->info, true);
    lv_obj_align(t->info, LV_ALIGN_BOTTOM_LEFT, EM(0.6f), -EM(0.5f));
    lv_obj_set_style_bg_color(t->info, C_SURF, 0);
    lv_obj_set_style_bg_opa(t->info, 217, 0);
    lv_obj_set_style_pad_hor(t->info, EM(0.45f), 0);
    lv_obj_set_style_pad_ver(t->info, EM(0.25f), 0);
    set_view(t, 0);
}

void traj_update(traj_t *t, uint32_t now)
{
    char buf[64];
    snprintf(buf, sizeof buf, "Z%9.3f   F %d   T%d", machine_work(AX_Z), (int)lroundf(M.feed_act), M.tool);
    ui_set_text(t->info, buf);
    if (!t->buf) return;
    bool base = t->base_dirty;
    if (base) draw_base(t);
    float x = (float)machine_work(AX_X), y = (float)machine_work(AX_Y), z = (float)machine_work(AX_Z);
    bool moved = x != t->drawn_x || y != t->drawn_y || z != t->drawn_z || M.op != t->drawn_op;
    /* до 10 пъти в секунда по време на работа */
    if (base || (moved && now - t->drawn_at >= 100)) {
        draw_top(t);
        t->drawn_x = x; t->drawn_y = y; t->drawn_z = z; t->drawn_op = M.op;
        t->drawn_at = now;
    }
}
