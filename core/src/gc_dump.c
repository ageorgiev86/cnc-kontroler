/* Текстов вид на командите: за тестовете (очакван изход) и за gcsim. */
#include "cnc/gcode.h"

#include <stdio.h>
#include <string.h>

/* число с до 4 знака след точката, без излишните нули; -0 става 0 */
static const char *num(double v, char *buf)
{
    snprintf(buf, 32, "%.4f", v);
    char *p = buf + strlen(buf) - 1;
    while (*p == '0') *p-- = 0;
    if (*p == '.') *p = 0;
    if (!strcmp(buf, "-0")) strcpy(buf, "0");
    return buf;
}

int gc_cmd_format(const gc_cmd_t *c, unsigned axes, char *out, size_t n)
{
    char t[32];
    size_t k = 0;
#define PUT(...) do { int r_ = snprintf(out + k, k < n ? n - k : 0, __VA_ARGS__); if (r_ > 0) k += (size_t)r_; } while (0)
    PUT("%d: ", c->line + 1);
    switch (c->kind) {
    case GC_RAPID:
    case GC_FEED:
    case GC_ARC:
        if (c->kind == GC_RAPID) PUT("RAPID");
        else if (c->kind == GC_FEED) PUT("FEED");
        else PUT("ARC %s G%d", c->cw ? "CW" : "CCW", c->plane);
        for (int a = 0; a < GC_NAX; a++)
            if (axes >> a & 1u) PUT(" %s%s", gc_axis_name(a), num(c->tgt[a], t));
        if (c->kind == GC_ARC) {
            PUT(" C");
            for (int a = 0; a < 3; a++) PUT("%s%s", a ? "," : "", num(c->ctr[a], t));
            if (c->turns) PUT(" +%d", c->turns);
        }
        if (c->kind != GC_RAPID) PUT(" F%s", num(c->feed, t));
        break;
    case GC_DWELL: PUT("DWELL %s s", num(c->val, t)); break;
    case GC_SPINDLE: PUT("SPINDLE %s S%s", c->ival > 0 ? "CW" : c->ival < 0 ? "CCW" : "OFF", num(c->val, t)); break;
    case GC_COOLANT:
        PUT("COOLANT");
        if (!c->ival) PUT(" OFF");
        if (c->ival & GC_COOL_MIST) PUT(" MIST");
        if (c->ival & GC_COOL_FLOOD) PUT(" FLOOD");
        if (c->ival & GC_COOL_TSC) PUT(" TSC");
        break;
    case GC_TOOL_PREP: PUT("TOOL_PREP T%d", c->ival); break;
    case GC_TOOL_CHANGE: PUT("TOOL_CHANGE T%d", c->ival); break;
    case GC_TLO: PUT("TLO H%d %s", c->ival, num(c->val, t)); break;
    case GC_ORIENT: PUT("ORIENT %s", num(c->val, t)); break;
    case GC_OVR_ENABLE: PUT("OVERRIDE %s", c->ival ? "ON" : "OFF"); break;
    case GC_PATH_MODE: PUT("PATH G%d.%d %s", c->ival / 10, c->ival % 10, num(c->val, t)); break;
    case GC_WCS_SELECT: PUT("WCS %d", c->ival); break;
    case GC_SET_WCS: PUT("SET_WCS %d", c->ival); break;
    case GC_SET_TOOL: PUT("SET_TOOL T%d", c->ival); break;
    case GC_HOME: PUT("HOME mask=%d", c->ival); break;
    case GC_PAUSE: PUT("PAUSE %s", c->ival ? "M1" : "M0"); break;
    case GC_END: PUT("END"); break;
    }
#undef PUT
    return (int)k;
}
