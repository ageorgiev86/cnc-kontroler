/* Виртуален STM32: същият протокол като firmware-а (docs/spi-protokol.md), за симулатора на
 * интерфейса и за тестовете. Опашка от 256 сегмента, изпълнявани по часовника, реферирането с
 * виртуален датчик, грешките UNDERRUN, LINK_TIMEOUT, ESTOP, LIMIT и STATUS. */
#ifndef CNC_VMCU_H
#define CNC_VMCU_H

#include "cnc/link.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct vmcu vmcu_t;

vmcu_t *vmcu_new(void);
void vmcu_free(vmcu_t *v);
/* времето: с часовника на системата (за симулатора) или ръчно (за тестовете) */
void vmcu_manual_time(vmcu_t *v, bool on);
void vmcu_advance(vmcu_t *v, uint32_t us);
/* един пакет: tx от Linux, отговорът в rx (отговорът е на същия пакет) */
int vmcu_xfer(vmcu_t *v, const uint8_t tx[LK_PKT], uint8_t rx[LK_PKT]);
/* пренос за mc.c */
lk_port_t *vmcu_port(vmcu_t *v);

/* виртуалната машина */
void vmcu_set_inputs(vmcu_t *v, uint32_t set, uint32_t clear);   /* local_in */
void vmcu_set_home_switch(vmcu_t *v, int axis, int32_t steps);  /* датчикът HOME сработва при тази позиция */
int64_t vmcu_pos_q(const vmcu_t *v, int axis);                  /* позиция, 1/65536 стъпка */
uint32_t vmcu_outputs(const vmcu_t *v);
int vmcu_fault(const vmcu_t *v);

#ifdef __cplusplus
}
#endif

#endif
