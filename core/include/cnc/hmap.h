/* Карта на височините: мрежа от измерени Z и билинейна корекция (docs/interpretator-planer.md, стъпка 4).
 * Координатите са машинни: интерфейсът мери в работни и прибавя отместването на нулата. */
#ifndef CNC_HMAP_H
#define CNC_HMAP_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    double x0, y0, x1, y1;   /* областта (машинни XY); последната точка е на ръба */
    double res;              /* стъпка на мрежата, мм */
    int nx, ny;
    const float *z;          /* nx × ny, z[i * ny + j] = Z в точка (x0 + i·res, y0 + j·res), машинна */
    double zs;               /* Z на повърхността, спрямо която е корекцията (машинна) */
} hm_map_t;

/* корекцията на Z в (x, y): карта − zs; извън областта 0 */
double hm_corr(const hm_map_t *m, double x, double y);
/* проверка: размери, стъпка, всички точки измерени; 0 = наред */
int hm_check(const hm_map_t *m);

#ifdef __cplusplus
}
#endif

#endif
