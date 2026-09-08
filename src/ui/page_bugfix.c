#include "page_bugfix.h"

#include <stdio.h>

#include <log/log.h>

#include "core/settings.h"
#include "lang/language.h"
#include "page_common.h"
#include "ui/ui_style.h"

// Fixes for faults in the stock firmware, each one off by default so that all
// off is the behaviour the goggles shipped with. Separate from Performance:
// nothing here trades one thing for another, it is only about being correct.

enum {
    ROW_RETRY_TUNER = 0,
    ROW_BACK,
    ROW_COUNT
};

static lv_coord_t col_dsc[] = {90, 340, 175, 175, 180, 0, LV_GRID_TEMPLATE_LAST};
static lv_coord_t row_dsc[] = {60, 60, 60, 60, 60, 60, 60, 60, 60, 60, LV_GRID_TEMPLATE_LAST};

static btn_group_t btn_group_retry_tuner;

static lv_obj_t *page_bugfix_create(lv_obj_t *parent, panel_arr_t *arr) {
    char buf[512];

    lv_obj_t *page = lv_menu_page_create(parent, NULL);
    lv_obj_clear_flag(page, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(page, 1053, 900);
    lv_obj_add_style(page, &style_subpage, LV_PART_MAIN);
    lv_obj_set_style_pad_top(page, 94, 0);

    lv_obj_t *section = lv_menu_section_create(page);
    lv_obj_add_style(section, &style_submenu, LV_PART_MAIN);
    lv_obj_set_size(section, 1053, 894);

    snprintf(buf, sizeof(buf), "%s:", _lang("Bug Fix"));
    create_text(NULL, section, false, buf, LV_MENU_ITEM_BUILDER_VARIANT_2);

    lv_obj_t *cont = lv_obj_create(section);
    lv_obj_set_size(cont, 960, 894);
    lv_obj_set_pos(cont, 0, 0);
    lv_obj_set_layout(cont, LV_LAYOUT_GRID);
    lv_obj_clear_flag(cont, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_style(cont, &style_context, LV_PART_MAIN);

    lv_obj_set_style_grid_column_dsc_array(cont, col_dsc, 0);
    lv_obj_set_style_grid_row_dsc_array(cont, row_dsc, 0);

    create_select_item(arr, cont);

    create_btn_group_item(&btn_group_retry_tuner, cont, 2, _lang("Retry Tuner Init"),
                          _lang("Off"), _lang("On"), "", "", ROW_RETRY_TUNER);
    btn_group_set_sel(&btn_group_retry_tuner, g_setting.bugfix.retry_tuner_init ? 1 : 0);

    snprintf(buf, sizeof(buf), "< %s", _lang("Back"));
    create_label_item(cont, buf, 1, ROW_BACK, 3);

    pp_bugfix.p_arr.max = ROW_COUNT;

    lv_obj_t *note = lv_label_create(cont);
    snprintf(buf, sizeof(buf), "%s:\n    - %s\n    - %s\n    - %s",
             _lang("Retry Tuner Init"),
             _lang("The receivers sometimes fail to start: no picture, noise, or two dead antennas"),
             _lang("Stock calls the tuner open even when its init failed, so nothing ever retried"),
             _lang("On retries immediately, then again on the next switch to video"));
    lv_label_set_text(note, buf);

    lv_obj_set_style_text_font(note, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_align(note, LV_TEXT_ALIGN_LEFT, 0);
    lv_obj_set_style_text_color(note, lv_color_make(255, 255, 255), 0);
    lv_obj_set_style_pad_top(note, 12, 0);
    lv_label_set_long_mode(note, LV_LABEL_LONG_WRAP);
    lv_obj_set_grid_cell(note, LV_GRID_ALIGN_START, 1, 4, LV_GRID_ALIGN_START, ROW_COUNT + 1, 4);

    return page;
}

static void on_click(uint8_t key, int sel) {
    if (sel != ROW_RETRY_TUNER)
        return;

    btn_group_toggle_sel(&btn_group_retry_tuner);
    g_setting.bugfix.retry_tuner_init = btn_group_get_sel(&btn_group_retry_tuner) == 1;
    settings_put_bool("bugfix", "retry_tuner_init", g_setting.bugfix.retry_tuner_init);
    LOGI("bugfix: retry_tuner_init=%s", g_setting.bugfix.retry_tuner_init ? "on" : "off");
}

page_pack_t pp_bugfix = {
    .p_arr = {
        .cur = 0,
        .max = ROW_COUNT,
    },
    .name = "Bug Fix",
    .create = page_bugfix_create,
    .enter = NULL,
    .exit = NULL,
    .on_created = NULL,
    .on_update = NULL,
    .on_roller = NULL,
    .on_click = on_click,
    .on_right_button = NULL,
};
