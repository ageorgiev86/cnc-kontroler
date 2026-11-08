/* Мрежата на системата от Настройки → Мрежа: вика tools/cnc-net.sh (инсталиран като
 * /usr/local/sbin/cnc-net, през sudo без парола). В симулатора не прави нищо. */
#ifndef CNC_NET_APPLY_H
#define CNC_NET_APPLY_H

#include <stdbool.h>
#include <stddef.h>

/* Прилага адреса (net.mode/ip/mask/gw/dns) и/или името (net.host) от записаните настройки.
 * msg получава отговора на скрипта (последния ред) или причината за грешка. true = успех. */
bool net_apply(bool addr, bool host, char *msg, size_t n);

#endif
