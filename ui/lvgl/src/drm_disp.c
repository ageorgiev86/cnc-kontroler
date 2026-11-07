/* Собствен DRM/KMS дисплей за LVGL (вместо lv_linux_drm, който винаги взима първия режим на конектора).
 *  - показва конекторите и режимите им;
 *  - избира режим: зададения (--mode или Настройки → Дисплей), иначе предпочитания от монитора (EDID),
 *    иначе най-големия; ако мониторът не обяви търсения режим (напр. през HDMI → DVI без EDID),
 *    ползва вградени стандартни тайминги (CVT-RB / DMT / CEA);
 *  - един „dumb“ буфер XRGB8888, сканиран от CRTC; LVGL рисува на части и flush копира в него;
 *  - завъртане 0/90/180/270 (софтуерно, с lv_draw_rotate).
 * Ползва само стандартния legacy KMS API (drmModeSetCrtc), който работи на sun4i (H3) и vc4 (RPi). */
#include "drm_disp.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <xf86drm.h>
#include <xf86drmMode.h>

static struct {
    int fd;
    uint32_t conn_id, crtc_id, fb_id, handle, pitch;
    drmModeModeInfo mode;
    drmModeCrtc *saved;
    uint8_t *map;
    size_t size;
    int w, h, rot;
} D = {.fd = -1};

/* стандартни тайминги: часовник kHz, h: активни, начало, край, общо; v: същото; флагове */
typedef struct { int w, h, clk, hs, he, ht, vs, ve, vt; uint32_t fl; const char *n; } std_mode_t;
static const std_mode_t STD[] = {
    {1920, 1200, 154000, 1968, 2000, 2080, 1203, 1209, 1235, DRM_MODE_FLAG_PHSYNC | DRM_MODE_FLAG_NVSYNC, "CVT-RB"},
    {1920, 1080, 148500, 2008, 2052, 2200, 1084, 1089, 1125, DRM_MODE_FLAG_PHSYNC | DRM_MODE_FLAG_PVSYNC, "CEA"},
    {1680, 1050, 119000, 1728, 1760, 1840, 1053, 1059, 1080, DRM_MODE_FLAG_PHSYNC | DRM_MODE_FLAG_NVSYNC, "CVT-RB"},
    {1600, 900, 108000, 1624, 1704, 1800, 901, 904, 1000, DRM_MODE_FLAG_PHSYNC | DRM_MODE_FLAG_PVSYNC, "DMT"},
    {1280, 1024, 108000, 1328, 1440, 1688, 1025, 1028, 1066, DRM_MODE_FLAG_PHSYNC | DRM_MODE_FLAG_PVSYNC, "DMT"},
    {1280, 800, 71000, 1328, 1360, 1440, 803, 809, 823, DRM_MODE_FLAG_PHSYNC | DRM_MODE_FLAG_NVSYNC, "CVT-RB"},
    {1280, 720, 74250, 1390, 1430, 1650, 725, 730, 750, DRM_MODE_FLAG_PHSYNC | DRM_MODE_FLAG_PVSYNC, "CEA"},
    {1024, 768, 65000, 1048, 1184, 1344, 771, 777, 806, DRM_MODE_FLAG_NHSYNC | DRM_MODE_FLAG_NVSYNC, "DMT"},
    {800, 600, 40000, 840, 968, 1056, 601, 605, 628, DRM_MODE_FLAG_PHSYNC | DRM_MODE_FLAG_PVSYNC, "DMT"},
};

static int refresh(const drmModeModeInfo *m) { return m->vrefresh ? (int)m->vrefresh : m->htotal && m->vtotal ? (int)(m->clock * 1000.0 / (m->htotal * m->vtotal) + 0.5) : 0; }

static const char *conn_name(uint32_t t)
{
    switch (t) {
    case DRM_MODE_CONNECTOR_HDMIA: return "HDMI-A";
    case DRM_MODE_CONNECTOR_HDMIB: return "HDMI-B";
    case DRM_MODE_CONNECTOR_DVID: return "DVI-D";
    case DRM_MODE_CONNECTOR_DVII: return "DVI-I";
    case DRM_MODE_CONNECTOR_VGA: return "VGA";
    case DRM_MODE_CONNECTOR_Composite: return "Composite";
    case DRM_MODE_CONNECTOR_TV: return "TV";
    case DRM_MODE_CONNECTOR_DSI: return "DSI";
    case DRM_MODE_CONNECTOR_DPI: return "DPI";
    case DRM_MODE_CONNECTOR_LVDS: return "LVDS";
    case DRM_MODE_CONNECTOR_eDP: return "eDP";
    case DRM_MODE_CONNECTOR_DisplayPort: return "DP";
    case DRM_MODE_CONNECTOR_VIRTUAL: return "Virtual";
    default: return "Unknown";
    }
}

static void std_to_mode(const std_mode_t *s, drmModeModeInfo *m)
{
    memset(m, 0, sizeof *m);
    m->clock = (uint32_t)s->clk;
    m->hdisplay = (uint16_t)s->w; m->hsync_start = (uint16_t)s->hs; m->hsync_end = (uint16_t)s->he; m->htotal = (uint16_t)s->ht;
    m->vdisplay = (uint16_t)s->h; m->vsync_start = (uint16_t)s->vs; m->vsync_end = (uint16_t)s->ve; m->vtotal = (uint16_t)s->vt;
    m->flags = s->fl;
    m->type = DRM_MODE_TYPE_USERDEF;
    m->vrefresh = (uint32_t)refresh(m);
    snprintf(m->name, sizeof m->name, "%dx%d", s->w, s->h);
}

/* Режимът: want = "1920x1200", "auto" или NULL */
static bool pick_mode(drmModeConnector *c, const char *want, drmModeModeInfo *out)
{
    int ww = 0, wh = 0;
    if (want && strcmp(want, "auto") && sscanf(want, "%dx%d", &ww, &wh) == 2) {
        int best = -1;
        for (int i = 0; i < c->count_modes; i++) {
            drmModeModeInfo *m = &c->modes[i];
            if (m->hdisplay != ww || m->vdisplay != wh || (m->flags & DRM_MODE_FLAG_INTERLACE)) continue;
            if (best < 0 || abs(refresh(m) - 60) < abs(refresh(&c->modes[best]) - 60)) best = i;
        }
        if (best >= 0) { *out = c->modes[best]; return true; }
        for (unsigned i = 0; i < sizeof STD / sizeof STD[0]; i++)
            if (STD[i].w == ww && STD[i].h == wh) {
                std_to_mode(&STD[i], out);
                printf("Мониторът не обявява %dx%d: вграден режим %s %d Hz\n", ww, wh, STD[i].n, (int)out->vrefresh);
                return true;
            }
        printf("Режим %dx%d не е намерен; ползва се автоматичният\n", ww, wh);
    }
    for (int i = 0; i < c->count_modes; i++)
        if (c->modes[i].type & DRM_MODE_TYPE_PREFERRED) { *out = c->modes[i]; return true; }
    int best = -1;
    for (int i = 0; i < c->count_modes; i++) {
        drmModeModeInfo *m = &c->modes[i];
        if (m->flags & DRM_MODE_FLAG_INTERLACE) continue;
        if (best < 0 || m->hdisplay * m->vdisplay > c->modes[best].hdisplay * c->modes[best].vdisplay) best = i;
    }
    if (best >= 0) { *out = c->modes[best]; return true; }
    return false;
}

static void flush_cb(lv_display_t *disp, const lv_area_t *area, uint8_t *px)
{
    lv_area_t a = *area;
    int32_t w = lv_area_get_width(area), h = lv_area_get_height(area);
    uint32_t sstride = lv_draw_buf_width_to_stride((uint32_t)w, LV_COLOR_FORMAT_XRGB8888);
    if (D.rot) {
        lv_display_rotate_area(disp, &a);
        uint8_t *dst = D.map + (size_t)a.y1 * D.pitch + (size_t)a.x1 * 4;
        lv_draw_rotate(px, dst, w, h, (int32_t)sstride, (int32_t)D.pitch, (lv_display_rotation_t)D.rot, LV_COLOR_FORMAT_XRGB8888);
    } else {
        for (int32_t y = 0; y < h; y++)
            memcpy(D.map + (size_t)(a.y1 + y) * D.pitch + (size_t)a.x1 * 4, px + (size_t)y * sstride, (size_t)w * 4);
    }
    lv_display_flush_ready(disp);
}

void drm_disp_list(const char *dev)
{
    int fd = open(dev, O_RDWR | O_CLOEXEC);
    if (fd < 0) { fprintf(stderr, "%s: %s\n", dev, strerror(errno)); return; }
    drmModeRes *r = drmModeGetResources(fd);
    if (!r) { fprintf(stderr, "%s: не е KMS устройство\n", dev); close(fd); return; }
    for (int i = 0; i < r->count_connectors; i++) {
        drmModeConnector *c = drmModeGetConnector(fd, r->connectors[i]);
        if (!c) continue;
        printf("%s-%u: %s, %d режима\n", conn_name(c->connector_type), c->connector_type_id,
               c->connection == DRM_MODE_CONNECTED ? "свързан" : "не е свързан", c->count_modes);
        for (int k = 0; k < c->count_modes; k++)
            printf("   %4dx%-4d %3d Hz%s\n", c->modes[k].hdisplay, c->modes[k].vdisplay, refresh(&c->modes[k]),
                   c->modes[k].type & DRM_MODE_TYPE_PREFERRED ? "  (предпочитан)" : "");
        drmModeFreeConnector(c);
    }
    drmModeFreeResources(r);
    close(fd);
}

lv_display_t *drm_disp_create(const char *dev, const char *want, int rot_deg)
{
    D.fd = open(dev, O_RDWR | O_CLOEXEC);
    if (D.fd < 0) { fprintf(stderr, "%s: %s (потребителят в група video ли е?)\n", dev, strerror(errno)); return NULL; }
    drmModeRes *r = drmModeGetResources(D.fd);
    if (!r) { fprintf(stderr, "%s: не е KMS устройство\n", dev); return NULL; }

    /* свързан конектор с режими; HDMI/DVI е с предимство */
    drmModeConnector *c = NULL;
    for (int pass = 0; pass < 2 && !c; pass++)
        for (int i = 0; i < r->count_connectors && !c; i++) {
            drmModeConnector *k = drmModeGetConnector(D.fd, r->connectors[i]);
            if (!k) continue;
            bool hd = k->connector_type == DRM_MODE_CONNECTOR_HDMIA || k->connector_type == DRM_MODE_CONNECTOR_HDMIB ||
                      k->connector_type == DRM_MODE_CONNECTOR_DVID || k->connector_type == DRM_MODE_CONNECTOR_DVII;
            if (k->connection == DRM_MODE_CONNECTED && k->count_modes > 0 && (hd || pass == 1)) c = k;
            else drmModeFreeConnector(k);
        }
    if (!c) { fprintf(stderr, "Няма свързан екран (провери кабела и дали мониторът е включен)\n"); return NULL; }
    printf("Изход: %s-%u, режими от монитора:", conn_name(c->connector_type), c->connector_type_id);
    for (int k = 0; k < c->count_modes && k < 12; k++) printf(" %dx%d@%d", c->modes[k].hdisplay, c->modes[k].vdisplay, refresh(&c->modes[k]));
    printf("\n");
    if (!pick_mode(c, want, &D.mode)) { fprintf(stderr, "Няма подходящ режим\n"); return NULL; }
    D.conn_id = c->connector_id;

    /* CRTC: на текущия енкодер или първия възможен */
    D.crtc_id = 0;
    drmModeEncoder *e = c->encoder_id ? drmModeGetEncoder(D.fd, c->encoder_id) : NULL;
    if (e && e->crtc_id) D.crtc_id = e->crtc_id;
    for (int i = 0; i < c->count_encoders && !D.crtc_id; i++) {
        drmModeEncoder *x = drmModeGetEncoder(D.fd, c->encoders[i]);
        if (!x) continue;
        for (int k = 0; k < r->count_crtcs; k++)
            if (x->possible_crtcs & (1u << k)) { D.crtc_id = r->crtcs[k]; break; }
        drmModeFreeEncoder(x);
    }
    if (e) drmModeFreeEncoder(e);
    drmModeFreeConnector(c);
    drmModeFreeResources(r);
    if (!D.crtc_id) { fprintf(stderr, "Няма свободен CRTC\n"); return NULL; }

    /* буферът на екрана */
    struct drm_mode_create_dumb cr = {.width = D.mode.hdisplay, .height = D.mode.vdisplay, .bpp = 32};
    if (drmIoctl(D.fd, DRM_IOCTL_MODE_CREATE_DUMB, &cr)) { fprintf(stderr, "CREATE_DUMB: %s\n", strerror(errno)); return NULL; }
    D.handle = cr.handle; D.pitch = cr.pitch; D.size = cr.size;
    if (drmModeAddFB(D.fd, cr.width, cr.height, 24, 32, cr.pitch, cr.handle, &D.fb_id)) { fprintf(stderr, "AddFB: %s\n", strerror(errno)); return NULL; }
    struct drm_mode_map_dumb mp = {.handle = D.handle};
    if (drmIoctl(D.fd, DRM_IOCTL_MODE_MAP_DUMB, &mp)) { fprintf(stderr, "MAP_DUMB: %s\n", strerror(errno)); return NULL; }
    D.map = mmap(NULL, D.size, PROT_READ | PROT_WRITE, MAP_SHARED, D.fd, (off_t)mp.offset);
    if (D.map == MAP_FAILED) { fprintf(stderr, "mmap: %s\n", strerror(errno)); return NULL; }
    memset(D.map, 0, D.size);

    D.saved = drmModeGetCrtc(D.fd, D.crtc_id);
    if (drmModeSetCrtc(D.fd, D.crtc_id, D.fb_id, 0, 0, &D.conn_id, 1, &D.mode)) {
        fprintf(stderr, "Режимът %dx%d не може да се включи: %s\n", D.mode.hdisplay, D.mode.vdisplay, strerror(errno));
        if (errno == EACCES || errno == EPERM) fprintf(stderr, "Екранът е зает (графичен вход или друга програма?)\n");
        return NULL;
    }
    printf("Режим: %dx%d @ %d Hz (%s)\n", D.mode.hdisplay, D.mode.vdisplay, refresh(&D.mode), D.mode.name);

    D.rot = rot_deg == 90 ? LV_DISPLAY_ROTATION_90 : rot_deg == 180 ? LV_DISPLAY_ROTATION_180 : rot_deg == 270 ? LV_DISPLAY_ROTATION_270 : 0;
    D.w = D.mode.hdisplay;
    D.h = D.mode.vdisplay;
    lv_display_t *disp = lv_display_create(D.w, D.h);
    lv_display_set_color_format(disp, LV_COLOR_FORMAT_XRGB8888);
    lv_display_set_rotation(disp, (lv_display_rotation_t)D.rot);
    /* два буфера по 1/8 от екрана: LVGL рисува в единия, докато другият се копира */
    uint32_t bw = (uint32_t)(D.w > D.h ? D.w : D.h), bh = (uint32_t)((D.w > D.h ? D.h : D.w) / 8);
    uint32_t sz = lv_draw_buf_width_to_stride(bw, LV_COLOR_FORMAT_XRGB8888) * bh;
    void *b1 = malloc(sz), *b2 = malloc(sz);
    if (!b1 || !b2) { fprintf(stderr, "Няма памет за буферите\n"); return NULL; }
    lv_display_set_buffers(disp, b1, b2, sz, LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_flush_cb(disp, flush_cb);
    return disp;
}

void drm_disp_close(void)
{
    if (D.fd < 0) return;
    if (D.saved) {   /* връща конзолата, както е била */
        drmModeSetCrtc(D.fd, D.saved->crtc_id, D.saved->buffer_id, D.saved->x, D.saved->y, &D.conn_id, 1, &D.saved->mode);
        drmModeFreeCrtc(D.saved);
    }
    if (D.map && D.map != MAP_FAILED) munmap(D.map, D.size);
    if (D.fb_id) drmModeRmFB(D.fd, D.fb_id);
    if (D.handle) { struct drm_mode_destroy_dumb d = {.handle = D.handle}; drmIoctl(D.fd, DRM_IOCTL_MODE_DESTROY_DUMB, &d); }
    close(D.fd);
    D.fd = -1;
}
