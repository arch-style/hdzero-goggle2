#include "page_switch_speed.h"

#include <stdio.h>

#include <log/log.h>

#include "core/settings.h"
#include "lang/language.h"
#include "page_speed_common.h"
#include "ui/ui_style.h"

// Everything here takes effect the next time the button is pressed, so it is
// tested by pressing it rather than by writing the card and rebooting, which
// is what separates it from Boot Speed.

enum {
    ROW_HEAD_SWITCH = 0,
    ROW_FAST_MENU,
    ROW_KEEP_DISPLAY,
    ROW_MENU_ASYNC_DISPLAY,
    ROW_SKIP_AUDIO,
    ROW_HEAD_RECORDER,
    ROW_DVR_STOP_WAIT,
    ROW_DVR_START_WAIT,
    ROW_HEAD_TUNER,
    ROW_SPI_BURST,
    ROW_FAST_EFUSE,
    ROW_HEAD_DRAW,
    ROW_ANTIALIAS_OFF,
    ROW_STATUSBAR_IDLE,
    ROW_SPLIT_LOCK,
    ROW_BACK,
    ROW_COUNT
};

// Measured on the goggles with the switch path instrumented. Each figure is
// what that switch is worth ON ITS OWN; where one switch stops another from
// doing anything at all the figure is replaced at runtime, see row_inert().
#define SAVING_FAST_MENU      "-2300ms"
#define SAVING_KEEP_DISPLAY   "-1100ms x2"
#define SAVING_MENU_ASYNC_DSP "beside dispw"
#define SAVING_SKIP_AUDIO     "-460ms switch"
#define SAVING_DVR_STOP_WAIT  "2000ms > actual"
#define SAVING_DVR_START_WAIT "2000ms > actual"
#define SAVING_SPI_BURST      "-715ms tuner"
#define SAVING_FAST_EFUSE     "est. -330ms"
#define SAVING_ANTIALIAS      "faster redraw"
#define SAVING_STATUSBAR_IDLE "200Hz > 20Hz"
#define SAVING_SPLIT_LOCK     "10 unlocks"

_Static_assert(ROW_COUNT <= MAX_PANELS, "Switch Speed has more rows than MAX_PANELS");

// 51 rather than 60: sixteen rows plus a note is more than the stock page
// height allows at the usual spacing. Still taller than the container, so the
// list scrolls and the dial brings the selection into view.
#define SW_ROW_H     51
#define SW_VISIBLE_H (780 - SW_ROW_H) // one row given back to the comment

static lv_coord_t col_dsc[] = SPEED_COL_DSC;
static lv_coord_t row_dsc[] = {SW_ROW_H, SW_ROW_H, SW_ROW_H, SW_ROW_H,
                               SW_ROW_H, SW_ROW_H, SW_ROW_H, SW_ROW_H,
                               SW_ROW_H, SW_ROW_H, SW_ROW_H, SW_ROW_H,
                               SW_ROW_H, SW_ROW_H, SW_ROW_H, SW_ROW_H,
                               LV_GRID_TEMPLATE_LAST};

static speed_page_t pg;

static btn_group_t btn_group_tuner;
static btn_group_t btn_group_overlay;
static btn_group_t btn_group_menu_async_display;
static btn_group_t btn_group_audio;
static btn_group_t btn_group_dvr_stop_wait;
static btn_group_t btn_group_dvr_start_wait;
static btn_group_t btn_group_spi_burst;
static btn_group_t btn_group_fast_efuse;
static btn_group_t btn_group_antialias;
static btn_group_t btn_group_statusbar_idle;
static btn_group_t btn_group_split_lock;

static bool row_inert(int row) {
    if (row != ROW_FAST_MENU)
        return false;

    // app_switch_to_menu() leaves the tuner running when the menu is drawn over
    // the video, so it never reaches the standby this row asks for and the
    // -2300ms is already inside Menu Over Video's own figure.
    return g_setting.speed.keep_display;
}

static void rows_refresh(void) {
    speed_row_inert(&pg, ROW_FAST_MENU, row_inert(ROW_FAST_MENU));
}

static const char *comment_text(int row) {
    if (row == ROW_FAST_MENU && row_inert(row))
        return _lang("Nothing to do while Menu Over Video is on: the tuner is never stopped.");

    switch (row) {
    case ROW_FAST_MENU:
        // The first switch after start-up has nothing to hold on to yet, so it
        // still pays for the tuner initialisation in full.
        return _lang("Takes effect from the second switch onward.");

    case ROW_KEEP_DISPLAY:
        return _lang("The menu then uses the video resolution, scaled down below 1080p. Keep Tuner Alive is then unnecessary.");

    case ROW_MENU_ASYNC_DISPLAY:
        return _lang("The recorder and audio stop run beside dispw. The video goes at the press, not after.");

    case ROW_SKIP_AUDIO:
        return _lang("No effect on sound. From the second switch onward, so not at start-up.");

    case ROW_DVR_STOP_WAIT:
        return _lang("Only while recording, which auto record makes any time there is video.");

    case ROW_DVR_START_WAIT:
        return _lang("The auto start holds the same lock the switch wants, so this shortens both.");

    case ROW_SPI_BURST:
        return _lang("Tuner init 1663ms to 948ms. Hidden behind the display change at start-up, so it shows at the switch.");

    case ROW_FAST_EFUSE:
        return _lang("Reads the tuner calibration once for both chips. Check the fingerprint in the log.");

    case ROW_ANTIALIAS_OFF:
        return _lang("Only while the menu is scaled. Restored when it closes.");

    case ROW_STATUSBAR_IDLE:
        return _lang("Status bar only: measured 20 times a second instead of 200, and not redrawn when unchanged.");

    case ROW_SPLIT_LOCK:
        return _lang("The dial and buttons wait on the drawing loop less often.");

    default:
        break;
    }

    return _lang("All off is the original behaviour.");
}

static void comment_update(void) {
    speed_comment_set(&pg, comment_text(pp_switch_speed.p_arr.cur));
}

// One row, the two settings behind it. Both act on the status bar and nothing
// else: the throttle cuts how often statubar_update() is called, the diff
// drops the redraws that are left when the text has not changed. Both keys are
// still written so an older build reading the same ini finds what it expects.
static void toggle_statusbar_idle(void) {
    btn_group_toggle_sel(&btn_group_statusbar_idle);

    bool on = btn_group_get_sel(&btn_group_statusbar_idle) == 1;

    g_setting.speed.ui_throttle = on;
    g_setting.speed.label_diff = on;
    settings_put_bool("speed", "ui_throttle", on);
    settings_put_bool("speed", "label_diff", on);
    LOGI("speed: ui_throttle=%s label_diff=%s", on ? "on" : "off", on ? "on" : "off");
}

static lv_obj_t *page_switch_speed_create(lv_obj_t *parent, panel_arr_t *arr) {
    char buf[64];

    lv_obj_t *page = lv_menu_page_create(parent, NULL);
    lv_obj_clear_flag(page, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(page, 1053, 900);
    lv_obj_add_style(page, &style_subpage, LV_PART_MAIN);
    lv_obj_set_style_pad_top(page, 24, 0); // this page is taller than most

    lv_obj_t *section = lv_menu_section_create(page);
    lv_obj_add_style(section, &style_submenu, LV_PART_MAIN);
    lv_obj_set_size(section, 1053, 894);
    lv_obj_set_style_pad_top(section, 36, 0);

    snprintf(buf, sizeof(buf), "%s:", _lang("Switch Speed"));
    create_text(NULL, section, false, buf, LV_MENU_ITEM_BUILDER_VARIANT_2);

    lv_obj_t *cont = lv_obj_create(section);
    lv_obj_set_size(cont, 960, SW_VISIBLE_H);
    lv_obj_set_pos(cont, 0, 0);
    lv_obj_set_layout(cont, LV_LAYOUT_GRID);
    lv_obj_add_style(cont, &style_context, LV_PART_MAIN);

    // Taller than it is, so it scrolls; on_roller keeps the selection in view.
    lv_obj_set_scroll_dir(cont, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(cont, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_style_pad_top(cont, 0, 0);
    lv_obj_scroll_to(cont, 0, 0, LV_ANIM_OFF);

    lv_obj_set_style_grid_column_dsc_array(cont, col_dsc, 0);
    lv_obj_set_style_grid_row_dsc_array(cont, row_dsc, 0);

    create_select_item(arr, cont, GRID_ROWS(row_dsc));
    speed_page_begin(&pg, cont);

    speed_heading(&pg, arr, "Menu / Video Switch", ROW_HEAD_SWITCH);
    speed_toggle(&pg, &btn_group_tuner, "Keep Tuner Alive",
                 g_setting.speed.fast_menu, SAVING_FAST_MENU, ROW_FAST_MENU);
    speed_toggle(&pg, &btn_group_overlay, "Menu Over Video",
                 g_setting.speed.keep_display, SAVING_KEEP_DISPLAY, ROW_KEEP_DISPLAY);
    speed_toggle(&pg, &btn_group_menu_async_display, "Async Menu Display",
                 g_setting.speed.menu_async_display, SAVING_MENU_ASYNC_DSP, ROW_MENU_ASYNC_DISPLAY);
    speed_toggle(&pg, &btn_group_audio, "Skip Audio Setup",
                 g_setting.speed.skip_audio, SAVING_SKIP_AUDIO, ROW_SKIP_AUDIO);

    speed_heading(&pg, arr, "Recorder", ROW_HEAD_RECORDER);
    speed_toggle(&pg, &btn_group_dvr_stop_wait, "Poll DVR Stop",
                 g_setting.speed.dvr_stop_wait, SAVING_DVR_STOP_WAIT, ROW_DVR_STOP_WAIT);
    speed_toggle(&pg, &btn_group_dvr_start_wait, "Poll DVR Start",
                 g_setting.speed.dvr_start_wait, SAVING_DVR_START_WAIT, ROW_DVR_START_WAIT);

    // Async Tuner Init is the third of these and lives on Boot Speed: it only
    // moves the start-up init, where these two shorten every one.
    speed_heading(&pg, arr, "Tuner", ROW_HEAD_TUNER);
    speed_toggle(&pg, &btn_group_spi_burst, "Burst Tuner Writes",
                 g_setting.speed.spi_burst, SAVING_SPI_BURST, ROW_SPI_BURST);
    speed_toggle(&pg, &btn_group_fast_efuse, "Dual-Chip EFUSE Read",
                 g_setting.speed.fast_efuse, SAVING_FAST_EFUSE, ROW_FAST_EFUSE);

    speed_heading(&pg, arr, "Menu Drawing", ROW_HEAD_DRAW);
    speed_toggle(&pg, &btn_group_antialias, "Menu Antialias OFF",
                 g_setting.speed.menu_antialias_off, SAVING_ANTIALIAS, ROW_ANTIALIAS_OFF);
    speed_toggle(&pg, &btn_group_statusbar_idle, "Skip Idle Redraws",
                 g_setting.speed.ui_throttle && g_setting.speed.label_diff,
                 SAVING_STATUSBAR_IDLE, ROW_STATUSBAR_IDLE);
    speed_toggle(&pg, &btn_group_split_lock, "Split UI Lock",
                 g_setting.speed.split_lock, SAVING_SPLIT_LOCK, ROW_SPLIT_LOCK);

    snprintf(buf, sizeof(buf), "< %s", _lang("Back"));
    create_label_item(cont, buf, 1, ROW_BACK, 3);

    pp_switch_speed.p_arr.max = ROW_COUNT;

    speed_comment(&pg, section, SW_ROW_H); // a row clear of the list
    rows_refresh();
    comment_update();

    return page;
}

static void on_enter(void) {
    if (pg.cont)
        lv_obj_scroll_to(pg.cont, 0, 0, LV_ANIM_OFF);

    rows_refresh();
    comment_update();
}

// The framework has already moved the selection by the time this runs.
static void on_roller(uint8_t key) {
    (void)key;
    speed_scroll_to(&pg, &pp_switch_speed.p_arr);
    comment_update();
}

static void on_click(uint8_t key, int sel) {
    (void)key;

    switch (sel) {
    case ROW_FAST_MENU:
        speed_store(&btn_group_tuner, &g_setting.speed.fast_menu, "speed", "fast_menu");
        break;

    case ROW_KEEP_DISPLAY:
        speed_store(&btn_group_overlay, &g_setting.speed.keep_display, "speed", "keep_display");
        break;

    case ROW_MENU_ASYNC_DISPLAY:
        speed_store(&btn_group_menu_async_display, &g_setting.speed.menu_async_display, "speed", "menu_async_display");
        break;

    case ROW_SKIP_AUDIO:
        speed_store(&btn_group_audio, &g_setting.speed.skip_audio, "speed", "skip_audio");
        break;

    case ROW_DVR_STOP_WAIT:
        speed_store(&btn_group_dvr_stop_wait, &g_setting.speed.dvr_stop_wait, "speed", "dvr_stop_wait");
        break;

    case ROW_DVR_START_WAIT:
        speed_store(&btn_group_dvr_start_wait, &g_setting.speed.dvr_start_wait, "speed", "dvr_start_wait");
        break;

    case ROW_SPI_BURST:
        speed_store(&btn_group_spi_burst, &g_setting.speed.spi_burst, "speed", "spi_burst");
        break;

    case ROW_FAST_EFUSE:
        speed_store(&btn_group_fast_efuse, &g_setting.speed.fast_efuse, "speed", "fast_efuse");
        break;

    case ROW_ANTIALIAS_OFF:
        speed_store(&btn_group_antialias, &g_setting.speed.menu_antialias_off, "speed", "menu_antialias_off");
        main_menu_apply_antialiasing();
        break;

    case ROW_STATUSBAR_IDLE:
        toggle_statusbar_idle();
        break;

    case ROW_SPLIT_LOCK:
        speed_store(&btn_group_split_lock, &g_setting.speed.split_lock, "speed", "split_lock");
        break;

    default:
        return;
    }

    // Menu Over Video decides whether Keep Tuner Alive does anything, and the
    // comment changes with it, so both are redone after any click.
    rows_refresh();
    comment_update();
}

page_pack_t pp_switch_speed = {
    .p_arr = {
        .cur = 0,
        .max = ROW_COUNT,
    },
    .name = "Switch Speed",
    .create = page_switch_speed_create,
    .enter = on_enter,
    .exit = NULL,
    .on_created = NULL,
    .on_update = NULL,
    .on_roller = on_roller,
    .on_click = on_click,
    .on_right_button = NULL,
};
