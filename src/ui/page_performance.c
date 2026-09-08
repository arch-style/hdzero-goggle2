#include "page_performance.h"

#include <stdio.h>

#include <log/log.h>

#include "core/settings.h"
#include "lang/language.h"
#include "page_common.h"
#include "ui/ui_style.h"

enum {
    ROW_HEAD_SWITCH = 0,
    ROW_FAST_MENU,
    ROW_KEEP_DISPLAY,
    ROW_SKIP_AUDIO,
    ROW_HEAD_MENU,
    ROW_ANTIALIAS_OFF,
    ROW_HEAD_BOOT,
    ROW_BOOT_DISPLAY,
    ROW_BOOT_FONTS,
    ROW_HEAD_INPUT,
    ROW_UI_THROTTLE,
    ROW_LABEL_DIFF,
    ROW_TIMED_LONG_PRESS,
    ROW_SPLIT_LOCK,
    ROW_BUTTON_BEEP,
    ROW_DIAL_BEEP,
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
#define SAVING_LABEL_DIFF   "no redraw"
#define SAVING_LONG_PRESS   "500ms fixed"
#define SAVING_SPLIT_LOCK   "10 unlocks"
#define SAVING_BUTTON_BEEP  "50 / 200ms"
#define SAVING_DIAL_BEEP    "15ms"
#define SAVING_ANTIALIAS    "faster redraw"

// create_btn_group_item() gives its label a 320px box at column 1 and puts the
// first button's arrow at the start of column 2, so column 1 has to be wider
// than that box or the arrow lands on the name. The last column holds the
// saving and is clipped at the container edge, so it gets the rest: 180px was
// still cutting the longer ones short.
// 90 + 340 + 175 + 175 + 180 fills the 960px container exactly.
static lv_coord_t col_dsc[] = {90, 340, 175, 175, 180, 0, LV_GRID_TEMPLATE_LAST};
// 51 rather than 60: thirteen rows plus a note is more than the stock page
// height allows at the usual spacing.
// This page has more rows than fit however they are sized, so rather than
// shrinking them until the estimate of the visible area happens to hold --
// an estimate that has been wrong more than once here -- the container
// scrolls and the dial brings the selection into view.
#define PERF_ROW_H 51

// The first fitting attempt left a third of the page unused, so take the
// space: the photographs show the menu background running well past where
// the rows stopped. Anything past this still scrolls.
#define PERF_VISIBLE_H (780 - PERF_ROW_H) // one row given back to the comment

static lv_coord_t row_dsc[] = {PERF_ROW_H, PERF_ROW_H, PERF_ROW_H, PERF_ROW_H,
                               PERF_ROW_H, PERF_ROW_H, PERF_ROW_H, PERF_ROW_H,
                               PERF_ROW_H, PERF_ROW_H, PERF_ROW_H, PERF_ROW_H,
                               PERF_ROW_H, PERF_ROW_H, PERF_ROW_H, PERF_ROW_H,
                               PERF_ROW_H, PERF_ROW_H, LV_GRID_TEMPLATE_LAST};

static btn_group_t btn_group_tuner;
static btn_group_t btn_group_overlay;
static btn_group_t btn_group_audio;
static btn_group_t btn_group_boot_display;
static btn_group_t btn_group_boot_fonts;
static btn_group_t btn_group_ui_throttle;
static btn_group_t btn_group_label_diff;
static btn_group_t btn_group_long_press;
static btn_group_t btn_group_split_lock;
static btn_group_t btn_group_button_beep;
static btn_group_t btn_group_dial_beep;
static btn_group_t btn_group_antialias;
static lv_obj_t *perf_cont;
static lv_obj_t *perf_comment;

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

// What the selected section is worth knowing about, shown under the list.
static const char *perf_comment_text(int row) {
    switch (row) {
    case ROW_FAST_MENU:
        // The first switch after start-up has nothing to hold on to yet, so
        // it still pays for the tuner initialisation in full.
        return _lang("Takes effect from the second switch onward.");

    case ROW_KEEP_DISPLAY:
        // The video's resolution is what it is; this only stops the menu
        // overriding it, so whether the menu ends up scaled follows the source.
        return _lang("The menu then uses the video resolution, scaled down below 1080p.");

    case ROW_SKIP_AUDIO:
        return _lang("No effect on sound. Takes effect from the second switch onward.");

    case ROW_ANTIALIAS_OFF:
        return _lang("Only while the menu is scaled. Restored when it closes.");

    case ROW_BOOT_DISPLAY:
        return _lang("Applies at the next start-up. The boot screen keeps the kernel's mode.");

    case ROW_BOOT_FONTS:
        return _lang("Applies at the next start-up.");

    case ROW_UI_THROTTLE:
        return _lang("Status bar only. Its values are measured twice a second anyway.");

    case ROW_LABEL_DIFF:
        return _lang("Status bar only. Nothing is drawn later than before.");

    case ROW_TIMED_LONG_PRESS:
        return _lang("A long press becomes 500ms regardless of the key repeat rate.");

    case ROW_SPLIT_LOCK:
        return _lang("The dial and buttons wait on the drawing loop less often.");

    case ROW_BUTTON_BEEP:
        return _lang("Dial button and right button. 50ms short, 200ms long.");

    case ROW_DIAL_BEEP:
        return _lang("One 15ms beep per detent.");

    default:
        break;
    }

    return _lang("All off is the original behaviour.");
}

static void perf_comment_update(void) {
    if (perf_comment)
        lv_label_set_text(perf_comment, perf_comment_text(pp_performance.p_arr.cur));
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
    lv_obj_set_size(cont, 960, PERF_VISIBLE_H);
    lv_obj_set_pos(cont, 0, 0);
    lv_obj_set_layout(cont, LV_LAYOUT_GRID);
    lv_obj_add_style(cont, &style_context, LV_PART_MAIN);

    // Taller than it is, so it scrolls; on_roller keeps the selection in view.
    lv_obj_set_scroll_dir(cont, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(cont, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_style_pad_top(cont, 0, 0);
    lv_obj_scroll_to(cont, 0, 0, LV_ANIM_OFF);
    perf_cont = cont;

    lv_obj_set_style_grid_column_dsc_array(cont, col_dsc, 0);
    lv_obj_set_style_grid_row_dsc_array(cont, row_dsc, 0);

    create_select_item(arr, cont);

    create_heading(cont, arr, _lang("Menu / Video Switch"), ROW_HEAD_SWITCH);
    create_toggle(&btn_group_tuner, cont, "Keep Tuner Alive",
                  g_setting.speed.fast_menu, SAVING_FAST_MENU, ROW_FAST_MENU);
    create_toggle(&btn_group_overlay, cont, "Menu Over Video",
                  g_setting.speed.keep_display, SAVING_KEEP_DISPLAY, ROW_KEEP_DISPLAY);
    create_toggle(&btn_group_audio, cont, "Skip Audio Setup",
                  g_setting.speed.skip_audio, SAVING_SKIP_AUDIO, ROW_SKIP_AUDIO);

    create_heading(cont, arr, _lang("Menu"), ROW_HEAD_MENU);
    create_toggle(&btn_group_antialias, cont, "Menu Antialias OFF",
                  g_setting.speed.menu_antialias_off, SAVING_ANTIALIAS, ROW_ANTIALIAS_OFF);

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
    create_toggle(&btn_group_button_beep, cont, "Button Beep",
                  g_setting.input.button_beep, SAVING_BUTTON_BEEP, ROW_BUTTON_BEEP);
    create_toggle(&btn_group_dial_beep, cont, "Dial Beep",
                  g_setting.input.dial_beep, SAVING_DIAL_BEEP, ROW_DIAL_BEEP);

    snprintf(buf, sizeof(buf), "< %s", _lang("Back"));
    create_label_item(cont, buf, 1, ROW_BACK, 3);

    pp_performance.p_arr.max = ROW_COUNT;

    // Outside the scrolling list, so it stays put while the list moves.
    perf_comment = lv_label_create(section);
    lv_obj_set_width(perf_comment, 960);
    lv_obj_set_style_text_font(perf_comment, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_align(perf_comment, LV_TEXT_ALIGN_LEFT, 0);
    lv_obj_set_style_text_color(perf_comment, lv_color_make(255, 255, 255), 0);
    lv_obj_set_style_pad_left(perf_comment, 90, 0);
    lv_obj_set_style_pad_top(perf_comment, PERF_ROW_H, 0); // a row clear of the list
    lv_label_set_long_mode(perf_comment, LV_LABEL_LONG_WRAP);
    perf_comment_update();

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

// The framework has already moved the selection by the time this runs.
static void on_roller(uint8_t key) {
    int cur = pp_performance.p_arr.cur;

    // Bring the row above into view first. The dial skips over the section
    // headings, so without this the heading scrolls off the moment the
    // selection reaches the first item under it, which is exactly when it is
    // most wanted.
    if (cur > 0 && pp_performance.p_arr.panel[cur - 1])
        lv_obj_scroll_to_view(pp_performance.p_arr.panel[cur - 1], LV_ANIM_OFF);

    if (pp_performance.p_arr.panel[cur])
        lv_obj_scroll_to_view(pp_performance.p_arr.panel[cur], LV_ANIM_OFF);

    perf_comment_update();
}

static void on_enter(void) {
    if (perf_cont)
        lv_obj_scroll_to(perf_cont, 0, 0, LV_ANIM_OFF);

    perf_comment_update();
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

    case ROW_ANTIALIAS_OFF:
        toggle_setting(&btn_group_antialias, &g_setting.speed.menu_antialias_off, "menu_antialias_off");
        main_menu_apply_antialiasing();
        break;

    case ROW_BUTTON_BEEP:
        toggle_setting_in("input", &btn_group_button_beep, &g_setting.input.button_beep, "button_beep");
        break;

    case ROW_DIAL_BEEP:
        toggle_setting_in("input", &btn_group_dial_beep, &g_setting.input.dial_beep, "dial_beep");
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
    .enter = on_enter,
    .exit = NULL,
    .on_created = NULL,
    .on_update = NULL,
    .on_roller = on_roller,
    .on_click = on_click,
    .on_right_button = NULL,
};
