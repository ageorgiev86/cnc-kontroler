/* Карта на височините: билинейна интерполация между четирите съседни точки. */
#include "cnc/hmap.h"

#include <math.h>

static double gx(const hm_map_t *m, int i) { return fmin(m->x1, m->x0 + i * m->res); }
static double gy(const hm_map_t *m, int j) { return fmin(m->y1, m->y0 + j * m->res); }

int hm_check(const hm_map_t *m)
{
    if (!m || !m->z || m->nx < 2 || m->ny < 2 || !(m->res > 0) || !(m->x1 > m->x0) || !(m->y1 > m->y0)) return -1;
    for (int k = 0; k < m->nx * m->ny; k++)
        if (!isfinite(m->z[k])) return -1;
    return 0;
}

double hm_corr(const hm_map_t *m, double x, double y)
{
    if (!m || x < m->x0 || x > m->x1 || y < m->y0 || y > m->y1) return 0;
    int i = (int)((x - m->x0) / m->res), j = (int)((y - m->y0) / m->res);
    if (i > m->nx - 2) i = m->nx - 2;
    if (j > m->ny - 2) j = m->ny - 2;
    double xa = gx(m, i), xb = gx(m, i + 1), ya = gy(m, j), yb = gy(m, j + 1);
    double u = xb > xa ? (x - xa) / (xb - xa) : 0, v = yb > ya ? (y - ya) / (yb - ya) : 0;
    if (u > 1) u = 1;
    if (v > 1) v = 1;
    const float *z = m->z;
    int ny = m->ny;
    double h = z[i * ny + j] * (1 - u) * (1 - v) + z[(i + 1) * ny + j] * u * (1 - v) +
               z[i * ny + j + 1] * (1 - u) * v + z[(i + 1) * ny + j + 1] * u * v;
    return h - m->zs;
}
