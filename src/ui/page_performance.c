#include "page_performance.h"

#include <stdio.h>

#include <log/log.h>

#include "core/settings.h"
#include "lang/language.h"
#include "page_common.h"
#include "ui/ui_style.h"

enum {
    ROW_HEAD_MENU = 0,
    ROW_FAST_MENU,
    ROW_KEEP_DISPLAY,
    ROW_SKIP_AUDIO,
    ROW_HEAD_BOOT,
    ROW_BOOT_DISPLAY,
    ROW_BOOT_FONTS,
    ROW_HEAD_INPUT,
    ROW_UI_THROTTLE,
    ROW_LABEL_DIFF,
    ROW_TIMED_LONG_PRESS,
    ROW_SPLIT_LOCK,
    ROW_CLICK_BEEP,
    ROW_LONG_PRESS_BEEP,
    ROW_BACK,
    ROW_COUNT
};

// Measured on the goggles with the switch path instrumented, so the cost of
// leaving one off is visible rather than implied.
#define SAVING_FAST_MENU    "-2300ms"
#define SAVING_KEEP_DISPLAY "-1100ms x2"
#define SAVING_SKIP_AUDIO   "-460ms"
#define SAVING_BOOT_DISPLAY "-1140ms"
#define SAVING_BOOT_FONTS   "-1000ms"
#define SAVING_UI_THROTTLE  "200Hz > 20Hz"
#define SAVING_LABEL_DIFF   "no idle redraw"
#define SAVING_LONG_PRESS   "500ms, steady"
#define SAVING_SPLIT_LOCK   "10 unlocks/pass"
#define SAVING_CLICK_BEEP   "50ms beep"
#define SAVING_LONG_BEEP    "200ms beep"

static lv_coord_t col_dsc[] = {160, 200, 200, 160, 160, 160, LV_GRID_TEMPLATE_LAST};
// 51 rather than 60: thirteen rows plus a note is more than the stock page
// height allows at the usual spacing.
static lv_coord_t row_dsc[] = {51, 51, 51, 51, 51, 51, 51, 51, 51, 51, 51, 51, 51, 51, 51, 51, LV_GRID_TEMPLATE_LAST};

static btn_group_t btn_group_tuner;
static btn_group_t btn_group_overlay;
static btn_group_t btn_group_audio;
static btn_group_t btn_group_boot_display;
static btn_group_t btn_group_boot_fonts;
static btn_group_t btn_group_ui_throttle;
static btn_group_t btn_group_label_diff;
static btn_group_t btn_group_long_press;
static btn_group_t btn_group_split_lock;
static btn_group_t btn_group_click_beep;
static btn_group_t btn_group_long_beep;

// The saving goes in the columns to the right of the Off/On buttons, which
// create_btn_group_item() leaves free; in the row's own label the text would
// run under the buttons.
static void create_saving_label(lv_obj_t *cont, const char *saving, int row) {
    create_label_item_compact(cont, saving, 4, row, 2, 60,
                              LV_TEXT_ALIGN_LEFT, LV_GRID_ALIGN_START,
                              &lv_font_montserrat_20);
}

// A group title, not a choice, so the dial passes over it.
static void create_heading(lv_obj_t *cont, panel_arr_t *arr, const char *name, int row) {
    char buf[64];

    snprintf(buf, sizeof(buf), "%s", name);
    lv_obj_t *label = create_label_item(cont, buf, 1, row, 3);
    lv_obj_set_style_text_color(label, lv_color_make(0xC0, 0xC0, 0x40), 0);
    lv_obj_clear_flag(arr->panel[row], FLAG_SELECTABLE);
}

static void create_toggle(btn_group_t *group, lv_obj_t *cont, const char *name,
                          bool value, const char *saving, int row) {
    create_btn_group_item(group, cont, 2, _lang(name), _lang("Off"), _lang("On"), "", "", row);
    btn_group_set_sel(group, value ? 1 : 0);
    create_saving_label(cont, saving, row);
}

static lv_obj_t *page_performance_create(lv_obj_t *parent, panel_arr_t *arr) {
    char buf[512];

    lv_obj_t *page = lv_menu_page_create(parent, NULL);
    lv_obj_clear_flag(page, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(page, 1053, 900);
    lv_obj_add_style(page, &style_subpage, LV_PART_MAIN);
    lv_obj_set_style_pad_top(page, 24, 0); // this page is taller than most

    lv_obj_t *section = lv_menu_section_create(page);
    lv_obj_add_style(section, &style_submenu, LV_PART_MAIN);
    lv_obj_set_size(section, 1053, 894);
    lv_obj_set_style_pad_top(section, 36, 0);

    snprintf(buf, sizeof(buf), "%s:", _lang("Performance"));
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

    create_heading(cont, arr, _lang("Menu"), ROW_HEAD_MENU);
    create_toggle(&btn_group_tuner, cont, "Keep Tuner Alive",
                  g_setting.speed.fast_menu, SAVING_FAST_MENU, ROW_FAST_MENU);
    create_toggle(&btn_group_overlay, cont, "Menu Over Video",
                  g_setting.speed.keep_display, SAVING_KEEP_DISPLAY, ROW_KEEP_DISPLAY);
    create_toggle(&btn_group_audio, cont, "Skip Audio Setup",
                  g_setting.speed.skip_audio, SAVING_SKIP_AUDIO, ROW_SKIP_AUDIO);

    create_heading(cont, arr, _lang("Boot"), ROW_HEAD_BOOT);
    create_toggle(&btn_group_boot_display, cont, "Skip Display Setup",
                  g_setting.speed.boot_display, SAVING_BOOT_DISPLAY, ROW_BOOT_DISPLAY);
    create_toggle(&btn_group_boot_fonts, cont, "Preload OSD Fonts",
                  g_setting.speed.boot_fonts, SAVING_BOOT_FONTS, ROW_BOOT_FONTS);

    create_heading(cont, arr, _lang("Input"), ROW_HEAD_INPUT);
    create_toggle(&btn_group_ui_throttle, cont, "Throttle UI Updates",
                  g_setting.speed.ui_throttle, SAVING_UI_THROTTLE, ROW_UI_THROTTLE);
    create_toggle(&btn_group_label_diff, cont, "Skip Idle Redraws",
                  g_setting.speed.label_diff, SAVING_LABEL_DIFF, ROW_LABEL_DIFF);
    create_toggle(&btn_group_long_press, cont, "Timed Long Press",
                  g_setting.speed.timed_long_press, SAVING_LONG_PRESS, ROW_TIMED_LONG_PRESS);
    create_toggle(&btn_group_split_lock, cont, "Split UI Lock",
                  g_setting.speed.split_lock, SAVING_SPLIT_LOCK, ROW_SPLIT_LOCK);
    create_toggle(&btn_group_click_beep, cont, "Click Beep",
                  g_setting.input.click_beep, SAVING_CLICK_BEEP, ROW_CLICK_BEEP);
    create_toggle(&btn_group_long_beep, cont, "Long Press Beep",
                  g_setting.input.long_press_beep, SAVING_LONG_BEEP, ROW_LONG_PRESS_BEEP);

    snprintf(buf, sizeof(buf), "< %s", _lang("Back"));
    create_label_item(cont, buf, 1, ROW_BACK, 3);

    pp_performance.p_arr.max = ROW_COUNT;

    lv_obj_t *note = lv_label_create(cont);
    snprintf(buf, sizeof(buf), "%s\n%s",
             _lang("All off is the original behaviour."),
             _lang("Menu times are per switch, boot times are once at start-up."));
    lv_label_set_text(note, buf);

    lv_obj_set_style_text_font(note, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_align(note, LV_TEXT_ALIGN_LEFT, 0);
    lv_obj_set_style_text_color(note, lv_color_make(255, 255, 255), 0);
    lv_obj_set_style_pad_top(note, 12, 0);
    lv_label_set_long_mode(note, LV_LABEL_LONG_WRAP);
    lv_obj_set_grid_cell(note, LV_GRID_ALIGN_START, 1, 4, LV_GRID_ALIGN_START, ROW_COUNT, 1);

    return page;
}

// Flips one toggle and stores it, so each row is a line rather than a block.
static void toggle_setting_in(const char *section, btn_group_t *group, bool *value, const char *key) {
    btn_group_toggle_sel(group);
    *value = btn_group_get_sel(group) == 1;
    settings_put_bool((char *)section, (char *)key, *value);
    LOGI("%s: %s=%s", section, key, *value ? "on" : "off");
}

static void toggle_setting(btn_group_t *group, bool *value, const char *key) {
    toggle_setting_in("speed", group, value, key);
}

static void on_click(uint8_t key, int sel) {
    switch (sel) {
    case ROW_FAST_MENU:
        toggle_setting(&btn_group_tuner, &g_setting.speed.fast_menu, "fast_menu");
        break;

    case ROW_KEEP_DISPLAY:
        toggle_setting(&btn_group_overlay, &g_setting.speed.keep_display, "keep_display");
        break;

    case ROW_SKIP_AUDIO:
        toggle_setting(&btn_group_audio, &g_setting.speed.skip_audio, "skip_audio");
        break;

    case ROW_BOOT_DISPLAY:
        toggle_setting(&btn_group_boot_display, &g_setting.speed.boot_display, "boot_display");
        break;

    case ROW_BOOT_FONTS:
        toggle_setting(&btn_group_boot_fonts, &g_setting.speed.boot_fonts, "boot_fonts");
        break;

    case ROW_UI_THROTTLE:
        toggle_setting(&btn_group_ui_throttle, &g_setting.speed.ui_throttle, "ui_throttle");
        break;

    case ROW_LABEL_DIFF:
        toggle_setting(&btn_group_label_diff, &g_setting.speed.label_diff, "label_diff");
        break;

    case ROW_TIMED_LONG_PRESS:
        toggle_setting(&btn_group_long_press, &g_setting.speed.timed_long_press, "timed_long_press");
        break;

    case ROW_SPLIT_LOCK:
        toggle_setting(&btn_group_split_lock, &g_setting.speed.split_lock, "split_lock");
        break;

    case ROW_CLICK_BEEP:
        toggle_setting_in("input", &btn_group_click_beep, &g_setting.input.click_beep, "click_beep");
        break;

    case ROW_LONG_PRESS_BEEP:
        toggle_setting_in("input", &btn_group_long_beep, &g_setting.input.long_press_beep, "long_press_beep");
        break;

    default:
        break;
    }
}

page_pack_t pp_performance = {
    .p_arr = {
        .cur = 0,
        .max = ROW_COUNT,
    },
    .name = "Performance",
    .create = page_performance_create,
    .enter = NULL,
    .exit = NULL,
    .on_created = NULL,
    .on_update = NULL,
    .on_roller = NULL,
    .on_click = on_click,
    .on_right_button = NULL,
};
