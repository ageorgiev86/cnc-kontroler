/* Временно: екраните, които още не са написани */
#include "ui.h"
#include "ui_style.h"
static void stub(lv_obj_t *scr, const char *t)
{
    lv_obj_t *c = ui_card(scr, t, NULL);
    lv_obj_set_size(c, LV_PCT(100), LV_PCT(100));
}
void tools_create(lv_obj_t *scr) { stub(scr, "ИНСТРУМЕНТИ"); }
void tools_update(uint32_t now) {}
void probe_create(lv_obj_t *scr) { stub(scr, "ОФСЕТИ"); }
void probe_update(uint32_t now) {}
void diag_create(lv_obj_t *scr) { stub(scr, "ДИАГНОСТИКА"); }
void diag_update(uint32_t now) {}
void set_create(lv_obj_t *scr) { stub(scr, "НАСТРОЙКИ"); }
void set_update(uint32_t now) {}
void set_show(void) {}
void files_create(lv_obj_t *scr) { stub(scr, "ПРОГРАМИ"); }
void files_update(uint32_t now) {}
void files_show(void) {}
void ui_test_screen(const char *act) {}
