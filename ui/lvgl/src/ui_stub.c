/* Временно: екраните, които още не са написани */
#include "ui.h"
#include "ui_style.h"
static void stub(lv_obj_t *scr, const char *t)
{
    lv_obj_t *c = ui_card(scr, t, NULL);
    lv_obj_set_size(c, LV_PCT(100), LV_PCT(100));
}
void files_create(lv_obj_t *scr) { stub(scr, "ПРОГРАМИ"); }
void files_update(uint32_t now) {}
void files_show(void) {}
void ui_test_screen(const char *act) {}
