/* Вътрешни функции между gc_interp.c и gc_comp.c (не са част от публичния API). */
#ifndef GC_INTERNAL_H
#define GC_INTERNAL_H

#include "cnc/gcode.h"

/* окончателният изход към планера; пази и последната изпратена позиция (in->comp_out) */
void gc_out(gc_interp_t *in, const gc_cmd_t *c);

/* компенсация на радиуса (gc_comp.c) */
void gc_comp_start(gc_interp_t *in, int side, double r);   /* side: +1 G41, -1 G42 */
void gc_comp_push(gc_interp_t *in, const gc_cmd_t *c);     /* команда в програмирани координати */
void gc_comp_finish(gc_interp_t *in);                      /* G40 / край: последният сегмент и чакащите */

#endif
