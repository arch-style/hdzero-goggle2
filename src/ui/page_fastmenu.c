#include "page_fastmenu.h"

#include <stdio.h>

#include <log/log.h>

#include "core/settings.h"
#include "lang/language.h"
#include "page_common.h"
#include "ui/ui_style.h"

enum {
    ROW_FAST_MENU = 0,
    ROW_KEEP_DISPLAY,
    ROW_SKIP_AUDIO,
    ROW_BACK,
    ROW_COUNT
};

// Measured on the goggles with the switch path instrumented. The saving is
// shown next to each toggle so the cost of leaving one off is visible.
#define SAVING_FAST_MENU    "-2300ms"
#define SAVING_KEEP_DISPLAY "-1100ms x2"
#define SAVING_SKIP_AUDIO   "-460ms"

static lv_coord_t col_dsc[] = {160, 200, 200, 160, 160, 160, LV_GRID_TEMPLATE_LAST};
static lv_coord_t row_dsc[] = {60, 60, 60, 60, 60, 60, 60, 60, 60, 60, LV_GRID_TEMPLATE_LAST};

static btn_group_t btn_group_tuner;
static btn_group_t btn_group_overlay;
static btn_group_t btn_group_audio;

// The saving sits in the columns to the right of the Off/On buttons, which
// create_btn_group_item() leaves free; putting it in the row's own label
// would run the text under the buttons.
static void create_saving_label(lv_obj_t *cont, const char *saving, int row) {
    create_label_item_compact(cont, saving, 4, row, 2, 60,
                              LV_TEXT_ALIGN_LEFT, LV_GRID_ALIGN_START,
                              &lv_font_montserrat_20);
}

static lv_obj_t *page_fastmenu_create(lv_obj_t *parent, panel_arr_t *arr) {
    char buf[512];
    int row = 0;

    lv_obj_t *page = lv_menu_page_create(parent, NULL);
    lv_obj_clear_flag(page, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(page, 1053, 900);
    lv_obj_add_style(page, &style_subpage, LV_PART_MAIN);
    lv_obj_set_style_pad_top(page, 94, 0);

    lv_obj_t *section = lv_menu_section_create(page);
    lv_obj_add_style(section, &style_submenu, LV_PART_MAIN);
    lv_obj_set_size(section, 1053, 894);

    snprintf(buf, sizeof(buf), "%s:", _lang("Fast Menu"));
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

    create_btn_group_item(&btn_group_tuner, cont, 2, _lang("Keep Tuner Alive"), _lang("Off"), _lang("On"), "", "", ROW_FAST_MENU);
    btn_group_set_sel(&btn_group_tuner, g_setting.speed.fast_menu ? 1 : 0);
    create_saving_label(cont, SAVING_FAST_MENU, ROW_FAST_MENU);
    row++;

    create_btn_group_item(&btn_group_overlay, cont, 2, _lang("Menu Over Video"), _lang("Off"), _lang("On"), "", "", ROW_KEEP_DISPLAY);
    btn_group_set_sel(&btn_group_overlay, g_setting.speed.keep_display ? 1 : 0);
    create_saving_label(cont, SAVING_KEEP_DISPLAY, ROW_KEEP_DISPLAY);
    row++;

    create_btn_group_item(&btn_group_audio, cont, 2, _lang("Skip Audio Setup"), _lang("Off"), _lang("On"), "", "", ROW_SKIP_AUDIO);
    btn_group_set_sel(&btn_group_audio, g_setting.speed.skip_audio ? 1 : 0);
    create_saving_label(cont, SAVING_SKIP_AUDIO, ROW_SKIP_AUDIO);
    row++;

    snprintf(buf, sizeof(buf), "< %s", _lang("Back"));
    create_label_item(cont, buf, 1, row++, 3);

    pp_fastmenu.p_arr.max = row;

    lv_obj_t *note = lv_label_create(cont);
    snprintf(buf, sizeof(buf),
             "%s\n"
             "%s: %s. %s\n"
             "%s: %s. %s\n"
             "%s: %s. %s",
             _lang("All off is the original behaviour. Times are per switch."),
             _lang("Keep Tuner Alive"),
             _lang("Returning to video no longer re-initialises the receiver"),
             _lang("The receiver stays powered while the menu is open"),
             _lang("Menu Over Video"),
             _lang("The display is not reconfigured, saving the cost both ways"),
             _lang("The menu is cropped unless the video is 1080p"),
             _lang("Skip Audio Setup"),
             _lang("The mixer is left alone when it already holds the state"),
             _lang("No effect on sound"));
    lv_label_set_text(note, buf);

    lv_obj_set_style_text_font(note, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_align(note, LV_TEXT_ALIGN_LEFT, 0);
    lv_obj_set_style_text_color(note, lv_color_make(255, 255, 255), 0);
    lv_obj_set_style_pad_top(note, 12, 0);
    lv_label_set_long_mode(note, LV_LABEL_LONG_WRAP);
    lv_obj_set_grid_cell(note, LV_GRID_ALIGN_START, 1, 4, LV_GRID_ALIGN_START, row + 1, 4);

    return page;
}

static void on_click(uint8_t key, int sel) {
    switch (sel) {
    case ROW_FAST_MENU:
        btn_group_toggle_sel(&btn_group_tuner);
        g_setting.speed.fast_menu = btn_group_get_sel(&btn_group_tuner) == 1;
        settings_put_bool("speed", "fast_menu", g_setting.speed.fast_menu);
        LOGI("speed: fast_menu=%s", g_setting.speed.fast_menu ? "on" : "off");
        break;

    case ROW_KEEP_DISPLAY:
        btn_group_toggle_sel(&btn_group_overlay);
        g_setting.speed.keep_display = btn_group_get_sel(&btn_group_overlay) == 1;
        settings_put_bool("speed", "keep_display", g_setting.speed.keep_display);
        LOGI("speed: keep_display=%s", g_setting.speed.keep_display ? "on" : "off");
        break;

    case ROW_SKIP_AUDIO:
        btn_group_toggle_sel(&btn_group_audio);
        g_setting.speed.skip_audio = btn_group_get_sel(&btn_group_audio) == 1;
        settings_put_bool("speed", "skip_audio", g_setting.speed.skip_audio);
        LOGI("speed: skip_audio=%s", g_setting.speed.skip_audio ? "on" : "off");
        break;

    default:
        break;
    }
}

page_pack_t pp_fastmenu = {
    .p_arr = {
        .cur = 0,
        .max = ROW_COUNT,
    },
    .name = "Fast Menu",
    .create = page_fastmenu_create,
    .enter = NULL,
    .exit = NULL,
    .on_created = NULL,
    .on_update = NULL,
    .on_roller = NULL,
    .on_click = on_click,
    .on_right_button = NULL,
};
