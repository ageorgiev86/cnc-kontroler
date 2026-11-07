/*
 * Докъде е стигнала програмата: backup SRAM.
 * Copyright (c) 2026 ageorgiev86. Всички права запазени.
 */
#include "resume.h"
#include "main.h"

#include <stddef.h>
#include <string.h>

#define RESUME_MAGIC   0x434E4352u   /* „RCNC“ */
#define RESUME_VERSION 1u

struct record {
  uint32_t magic;
  uint32_t version;
  uint32_t seq;                      /* пореден номер: по-новото копие е с по-голям */
  struct prog_pos pos;
  uint32_t crc;                      /* CRC-32 на всичко преди него */
};

/* backup SRAM: секция .backup_sram (линкер скриптът), без инициализация при старт */
static volatile struct record slot[2] __attribute__((section(".backup_sram")));

static struct prog_pos boot_pos;
static int boot_valid;
static uint32_t next_seq;
static unsigned next_slot;
static int battery_ok;

uint32_t crc32_calc(uint32_t crc, const void *data, uint32_t len)
{
  const uint8_t *p = data;

  crc = ~crc;
  while (len--) {
    crc ^= *p++;
    for (int k = 0; k < 8; k++) {
      crc = (crc >> 1) ^ (0xEDB88320u & (0u - (crc & 1u)));
    }
  }
  return ~crc;
}

static int slot_valid(unsigned i, struct record *out)
{
  struct record r;

  for (uint32_t k = 0; k < sizeof(r) / 4u; k++) {      /* по думи: паметта е некеширана */
    ((uint32_t *)&r)[k] = ((const volatile uint32_t *)&slot[i])[k];
  }
  if (r.magic != RESUME_MAGIC || r.version != RESUME_VERSION ||
      r.crc != crc32_calc(0, &r, (uint32_t)offsetof(struct record, crc))) {
    return 0;
  }
  *out = r;
  return 1;
}

void resume_init(void)
{
  struct record a;
  struct record b;
  int va;
  int vb;

  /* достъп до домейна на backup: защитата от запис се маха, тактът се включва */
  HAL_PWR_EnableBkUpAccess();
  __HAL_RCC_BKPRAM_CLK_ENABLE();
  /* backup регулаторът пази SRAM от VBAT, когато няма 3,3 V; без VBAT не тръгва */
  battery_ok = (HAL_PWREx_EnableBkUpReg() == HAL_OK);

  va = slot_valid(0, &a);
  vb = slot_valid(1, &b);
  if (va && vb) {
    va = ((int32_t)(a.seq - b.seq) > 0);                /* по-новото от двете */
    vb = !va;
  }
  if (va) {
    boot_pos   = a.pos;
    next_seq   = a.seq + 1u;
    next_slot  = 1;
    boot_valid = (a.pos.line != 0);
  } else if (vb) {
    boot_pos   = b.pos;
    next_seq   = b.seq + 1u;
    next_slot  = 0;
    boot_valid = (b.pos.line != 0);
  } else {
    memset(&boot_pos, 0, sizeof(boot_pos));
    next_seq   = 1;
    next_slot  = 0;
    boot_valid = 0;
  }
}

void resume_store(const struct prog_pos *p)
{
  struct record r;

  r.magic   = RESUME_MAGIC;
  r.version = RESUME_VERSION;
  r.seq     = next_seq++;
  r.pos     = *p;
  r.crc     = crc32_calc(0, &r, (uint32_t)offsetof(struct record, crc));

  /* пише се в по-старото копие; по-новото остава, ако токът спре сега */
  for (uint32_t k = 0; k < sizeof(r) / 4u; k++) {
    ((volatile uint32_t *)&slot[next_slot])[k] = ((const uint32_t *)&r)[k];
  }
  __DSB();
  next_slot ^= 1u;

  if (p->line == 0) {
    boot_valid = 0;                                      /* Linux потвърди или започна нова програма */
  }
}

int resume_boot_info(struct prog_pos *p)
{
  if (boot_valid) {
    *p = boot_pos;
  }
  return boot_valid;
}

int resume_battery_ok(void)
{
  return battery_ok;
}
