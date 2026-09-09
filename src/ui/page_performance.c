#include "page_performance.h"

#include <stdio.h>

#include <log/log.h>

#include "core/app_state.h"
#include "core/common.hh"
#include "core/settings.h"
#include "lang/language.h"
#include "page_common.h"
#include "ui/ui_style.h"
#include <minIni.h>

enum {
    ROW_HEAD_SWITCH = 0,
    ROW_FAST_MENU,
    ROW_KEEP_DISPLAY,
    ROW_SKIP_AUDIO,
    ROW_DVR_STOP_WAIT,
    ROW_DVR_START_WAIT,
    ROW_MENU_ASYNC_DISPLAY,
    ROW_HEAD_TUNER,
    ROW_SPI_BURST,
    ROW_FAST_EFUSE,
    ROW_ASYNC_TUNER,
    ROW_HEAD_MENU,
    ROW_ANTIALIAS_OFF,
    ROW_HEAD_BOOT,
    ROW_BOOT_DISPLAY,
    ROW_BOOT_FONTS,
    ROW_SKIP_BOOT_MENU,
    ROW_DEFER_MENU,
    ROW_ASYNC_IMU,
    ROW_ASYNC_DISPLAY,
    ROW_EARLY_TIMING,
    ROW_SKIP_WIFI_STOP,
    ROW_HEAD_INPUT,
    ROW_STATUSBAR_IDLE,
    ROW_TIMED_LONG_PRESS,
    ROW_LONG_PRESS_MS,
    ROW_SPLIT_LOCK,
    ROW_HEAD_SOUND,
    ROW_BUTTON_BEEP,
    ROW_DIAL_BEEP,
    ROW_BACK,
    ROW_COUNT
};

// Measured on the goggles with the switch path instrumented, so the cost of
// leaving one off is visible rather than implied.
// Where two of these hide work in the same window they cannot both claim it,
// so each figure is what that switch is worth ON ITS OWN and the comment
// under the list says where it overlaps another. Where one switch stops
// another from doing anything at all the figure is replaced at runtime; see
// row_inert() below.
#define SAVING_FAST_MENU      "-2300ms"
#define SAVING_KEEP_DISPLAY   "-1100ms x2"
#define SAVING_SKIP_AUDIO     "-460ms switch"
#define SAVING_DVR_STOP_WAIT  "2000ms > actual"
#define SAVING_DVR_START_WAIT "2000ms > actual"
#define SAVING_MENU_ASYNC_DSP "beside dispw"
#define SAVING_BOOT_DISPLAY   "-1140ms"
#define SAVING_BOOT_FONTS     "-550..-1000ms"
#define SAVING_SKIP_BOOT_MENU "no menu flash"
#define SAVING_ASYNC_IMU      "-686ms"
#define SAVING_ASYNC_DISPLAY  "-1092ms alone"
#define SAVING_STATUSBAR_IDLE "200Hz > 20Hz"
#define SAVING_LONG_PRESS     "500ms fixed"
#define SAVING_SPLIT_LOCK     "10 unlocks"
#define SAVING_ANTIALIAS      "faster redraw"
#define SAVING_SPI_BURST      "-715ms tuner"
#define SAVING_ASYNC_TUNER    "-844ms alone"
#define SAVING_SKIP_WIFI_STOP "-1080ms after"
#define SAVING_EARLY_TIMING   "est. -364ms"
#define SAVING_DEFER_MENU     "est. -70..-240"
#define SAVING_FAST_EFUSE     "est. -330ms"

// The beeps are the two rows on this page that buy nothing: they are input
// feedback that happens to be cheap. Their column says how long the beep is,
// not what is saved, so it is worded as a duration to keep it out of the
// column of figures above.
#define DURATION_BUTTON_BEEP "beep 50/200ms"
#define DURATION_DIAL_BEEP   "beep 15ms"

// Shown in place of the figure when the row's switch cannot do anything in
// the combination that is currently set. Short because the column is 180px;
// the reason goes in the comment under the list.
#define SAVING_INERT "(no effect)"

// panel_arr_t carries MAX_PANELS of them, so this page cannot outgrow it.
_Static_assert(ROW_COUNT <= MAX_PANELS, "Performance page has more rows than MAX_PANELS");

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

// MAX_PANELS entries, not ROW_COUNT: create_select_item() places a hidden
// selection panel on every row up to MAX_PANELS and a panel needs its track
// to exist. The tracks past ROW_BACK cost no height -- the grid layout skips
// hidden children and lv_obj_get_scroll_bottom() only measures visible ones --
// so the list still ends where "Back" does.
static lv_coord_t row_dsc[] = {PERF_ROW_H, PERF_ROW_H, PERF_ROW_H, PERF_ROW_H,
                               PERF_ROW_H, PERF_ROW_H, PERF_ROW_H, PERF_ROW_H,
                               PERF_ROW_H, PERF_ROW_H, PERF_ROW_H, PERF_ROW_H,
                               PERF_ROW_H, PERF_ROW_H, PERF_ROW_H, PERF_ROW_H,
                               PERF_ROW_H, PERF_ROW_H, PERF_ROW_H, PERF_ROW_H,
                               PERF_ROW_H, PERF_ROW_H, PERF_ROW_H, PERF_ROW_H,
                               PERF_ROW_H, PERF_ROW_H, PERF_ROW_H, PERF_ROW_H,
                               PERF_ROW_H, PERF_ROW_H, PERF_ROW_H, PERF_ROW_H,
                               LV_GRID_TEMPLATE_LAST};

static btn_group_t btn_group_tuner;
static btn_group_t btn_group_overlay;
static btn_group_t btn_group_audio;
static btn_group_t btn_group_dvr_stop_wait;
static btn_group_t btn_group_dvr_start_wait;
static btn_group_t btn_group_menu_async_display;
static btn_group_t btn_group_boot_display;
static btn_group_t btn_group_boot_fonts;
static btn_group_t btn_group_skip_boot_menu;
static btn_group_t btn_group_async_imu;
static btn_group_t btn_group_async_display;
static btn_group_t btn_group_statusbar_idle;
static btn_group_t btn_group_long_press;
static btn_group_t btn_group_split_lock;
static btn_group_t btn_group_button_beep;
static btn_group_t btn_group_dial_beep;
static btn_group_t btn_group_antialias;
static btn_group_t btn_group_spi_burst;
static btn_group_t btn_group_async_tuner;
static btn_group_t btn_group_skip_wifi_stop;
static btn_group_t btn_group_early_timing;
static btn_group_t btn_group_defer_menu;
static btn_group_t btn_group_fast_efuse;
static slider_group_t slider_long_press;
static lv_obj_t *perf_cont;
static lv_obj_t *perf_comment;

// Kept per row so a row can be greyed and its figure replaced after any
// switch that changes whether it does anything.
static lv_obj_t *row_name[ROW_COUNT];
static lv_obj_t *row_saving[ROW_COUNT];
static const char *row_saving_text[ROW_COUNT];

// The slider carries an index into long_press_choices, and shows the value.
static void long_press_slider_update(void) {
    char buf[16];
    int idx = long_press_choice_index(g_setting.input.long_press_ms);

    if (idx < 0)
        idx = 0;

    lv_slider_set_value(slider_long_press.slider, idx, LV_ANIM_OFF);
    snprintf(buf, sizeof(buf), "%dms", long_press_choices[idx]);
    lv_label_set_text(slider_long_press.label, buf);
}

// The saving goes in the columns to the right of the Off/On buttons, which
// create_btn_group_item() leaves free; in the row's own label the text would
// run under the buttons.
static void create_saving_label(lv_obj_t *cont, const char *saving, int row) {
    row_saving[row] = create_label_item_compact(cont, saving, 4, row, 2, 60,
                                                LV_TEXT_ALIGN_LEFT, LV_GRID_ALIGN_START,
                                                &lv_font_montserrat_20);
    row_saving_text[row] = saving;
}

// A group title, not a choice, so the dial passes over it.
static void create_heading(lv_obj_t *cont, panel_arr_t *arr, const char *name, int row) {
    lv_obj_t *label = create_label_item(cont, _lang(name), 1, row, 3);
    lv_obj_set_style_text_color(label, lv_color_make(0xC0, 0xC0, 0x40), 0);
    lv_obj_clear_flag(arr->panel[row], FLAG_SELECTABLE);
}

static void create_toggle(btn_group_t *group, lv_obj_t *cont, const char *name,
                          bool value, const char *saving, int row) {
    create_btn_group_item(group, cont, 2, _lang(name), _lang("Off"), _lang("On"), "", "", row);
    btn_group_set_sel(group, value ? 1 : 0);
    create_saving_label(cont, saving, row);
    row_name[row] = group->label;
}

// A switch can be on and still have nothing to do, because another switch has
// already taken the work away or because the code behind it refuses to run
// without its neighbours. Leaving a measured figure next to one of those
// reads as a saving the user is not getting, so the figure is replaced and
// the row greyed while that lasts.
static bool row_inert(int row) {
    switch (row) {
    case ROW_FAST_MENU:
        // app_enter_menu() leaves the tuner running when the menu is drawn
        // over the video, so it never reaches the standby this row asks for
        // and the -2300ms is already in Menu Over Video's own figure.
        return g_setting.speed.keep_display;

    case ROW_EARLY_TIMING:
        // boot_workers_start() checks all three and logs a warning instead of
        // starting the timing, so on its own this row does nothing at all.
        return !(g_setting.speed.async_display && g_setting.speed.boot_display &&
                 g_setting.speed.skip_boot_menu);

    case ROW_LONG_PRESS_MS:
        // input_device.c only reads the time on the timed path.
        return !g_setting.speed.timed_long_press;

    default:
        return false;
    }
}

// Only these three can go inert, so only these three are walked.
static const int inert_rows[] = {ROW_FAST_MENU, ROW_EARLY_TIMING, ROW_LONG_PRESS_MS};

static void set_row_state(lv_obj_t *obj, bool inert) {
    if (!obj)
        return;

    if (inert)
        lv_obj_add_state(obj, STATE_DISABLED);
    else
        lv_obj_clear_state(obj, STATE_DISABLED);
}

static void perf_rows_refresh(void) {
    for (size_t i = 0; i < sizeof(inert_rows) / sizeof(inert_rows[0]); ++i) {
        int row = inert_rows[i];
        bool inert = row_inert(row);

        set_row_state(row_name[row], inert);
        set_row_state(row_saving[row], inert);

        // The long press row has no figure to swap: its right hand column is
        // the value itself, which still has to say what it is set to, so it
        // is greyed rather than replaced.
        if (row == ROW_LONG_PRESS_MS)
            set_row_state(slider_long_press.label, inert);
        else if (row_saving[row] && row_saving_text[row])
            lv_label_set_text(row_saving[row], inert ? _lang(SAVING_INERT) : row_saving_text[row]);
    }
}

// Why the selected row is doing nothing, when it is doing nothing.
static const char *perf_inert_comment(int row) {
    switch (row) {
    case ROW_FAST_MENU:
        return _lang("Nothing to do while Menu Over Video is on: the tuner is never stopped.");

    case ROW_EARLY_TIMING:
        return _lang("Does nothing until Async Display Setup, Skip Display Setup and Skip Boot Menu are all on.");

    case ROW_LONG_PRESS_MS:
        return _lang("Only used while Timed Long Press is on.");

    default:
        break;
    }

    return NULL;
}

// What the selected section is worth knowing about, shown under the list.
static const char *perf_comment_text(int row) {
    const char *inert = row_inert(row) ? perf_inert_comment(row) : NULL;

    if (inert)
        return inert;

    switch (row) {
    case ROW_FAST_MENU:
        // The first switch after start-up has nothing to hold on to yet, so
        // it still pays for the tuner initialisation in full.
        return _lang("Takes effect from the second switch onward.");

    case ROW_KEEP_DISPLAY:
        // The video's resolution is what it is; this only stops the menu
        // overriding it, so whether the menu ends up scaled follows the source.
        return _lang("The menu then uses the video resolution, scaled down below 1080p. Keep Tuner Alive is then unnecessary.");

    case ROW_SKIP_AUDIO:
        return _lang("No effect on sound. From the second switch onward, so not at start-up.");

    case ROW_DVR_STOP_WAIT:
        return _lang("Only while recording, which auto record makes any time there is video.");

    case ROW_DVR_START_WAIT:
        return _lang("The auto start holds the same lock the switch wants, so this shortens both.");

    case ROW_MENU_ASYNC_DISPLAY:
        return _lang("The recorder and audio stop run beside dispw. The video goes at the press, not after.");

    case ROW_ANTIALIAS_OFF:
        return _lang("Only while the menu is scaled. Restored when it closes.");

    case ROW_SPI_BURST:
        return _lang("Tuner init 1663ms to 948ms. Hidden behind the display change at start-up, so it shows at the switch.");

    case ROW_FAST_EFUSE:
        return _lang("Reads the tuner calibration once for both chips. Check the fingerprint in the log.");

    case ROW_ASYNC_TUNER:
        return _lang("Next start-up, straight to HDZero video only. Near zero unless Early Video Timing is on.");

    case ROW_SKIP_WIFI_STOP:
        return _lang("Frees the main loop for a second after the video, not before it.");

    case ROW_BOOT_DISPLAY:
        return _lang("Applies at the next start-up. The boot screen keeps the kernel's mode.");

    case ROW_BOOT_FONTS:
        return _lang("Applies at the next start-up. Worth more when the fonts are on the card.");

    case ROW_SKIP_BOOT_MENU:
        return _lang("The menu is on screen during start-up until the video covers it.");

    case ROW_DEFER_MENU:
        return _lang("Moves 976ms off the path, but exposes the OSD font wait and the tuner under it.");

    case ROW_ASYNC_IMU:
        return _lang("Brought up alongside the rest of start-up, waited for before the threads run.");

    case ROW_ASYNC_DISPLAY:
        return _lang("Runs with the tuner init. The panel blanks a little earlier.");

    case ROW_EARLY_TIMING:
        // The three it depends on are on, or this row would be inert and
        // perf_inert_comment() would have answered instead.
        return _lang("Next start-up: the video timing starts before the rest of the boot path.");

    case ROW_STATUSBAR_IDLE:
        return _lang("Status bar only: measured 20 times a second instead of 200, and not redrawn when unchanged.");

    case ROW_TIMED_LONG_PRESS:
        return _lang("A long press is timed instead of counting key repeats.");

    case ROW_LONG_PRESS_MS:
        return _lang("How long the button is held before a long press is reported.");

    case ROW_SPLIT_LOCK:
        return _lang("The dial and buttons wait on the drawing loop less often.");

    case ROW_BUTTON_BEEP:
        return _lang("Dial button and right button. 50ms short, 200ms long. Not a speed setting.");

    case ROW_DIAL_BEEP:
        return _lang("One 15ms beep per detent. Not a speed setting.");

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

    create_heading(cont, arr, "Menu / Video Switch", ROW_HEAD_SWITCH);
    create_toggle(&btn_group_tuner, cont, "Keep Tuner Alive",
                  g_setting.speed.fast_menu, SAVING_FAST_MENU, ROW_FAST_MENU);
    create_toggle(&btn_group_overlay, cont, "Menu Over Video",
                  g_setting.speed.keep_display, SAVING_KEEP_DISPLAY, ROW_KEEP_DISPLAY);
    create_toggle(&btn_group_audio, cont, "Skip Audio Setup",
                  g_setting.speed.skip_audio, SAVING_SKIP_AUDIO, ROW_SKIP_AUDIO);
    create_toggle(&btn_group_dvr_stop_wait, cont, "Poll DVR Stop",
                  g_setting.speed.dvr_stop_wait, SAVING_DVR_STOP_WAIT, ROW_DVR_STOP_WAIT);
    create_toggle(&btn_group_dvr_start_wait, cont, "Poll DVR Start",
                  g_setting.speed.dvr_start_wait, SAVING_DVR_START_WAIT, ROW_DVR_START_WAIT);
    create_toggle(&btn_group_menu_async_display, cont, "Async Menu Display",
                  g_setting.speed.menu_async_display, SAVING_MENU_ASYNC_DSP, ROW_MENU_ASYNC_DISPLAY);

    // All three are DM6302_init(): two make it shorter, the third moves it off
    // the start-up path. Split between the switch and the boot sections they
    // read as unrelated, which is how one ended up being measured against the
    // other.
    create_heading(cont, arr, "Tuner Init", ROW_HEAD_TUNER);
    create_toggle(&btn_group_spi_burst, cont, "Burst Tuner Writes",
                  g_setting.speed.spi_burst, SAVING_SPI_BURST, ROW_SPI_BURST);
    create_toggle(&btn_group_fast_efuse, cont, "Dual-Chip EFUSE Read",
                  g_setting.speed.fast_efuse, SAVING_FAST_EFUSE, ROW_FAST_EFUSE);
    create_toggle(&btn_group_async_tuner, cont, "Async Tuner Init",
                  g_setting.speed.async_tuner, SAVING_ASYNC_TUNER, ROW_ASYNC_TUNER);

    create_heading(cont, arr, "Menu", ROW_HEAD_MENU);
    create_toggle(&btn_group_antialias, cont, "Menu Antialias OFF",
                  g_setting.speed.menu_antialias_off, SAVING_ANTIALIAS, ROW_ANTIALIAS_OFF);

    create_heading(cont, arr, "Boot", ROW_HEAD_BOOT);
    create_toggle(&btn_group_boot_display, cont, "Skip Display Setup",
                  g_setting.speed.boot_display, SAVING_BOOT_DISPLAY, ROW_BOOT_DISPLAY);
    create_toggle(&btn_group_boot_fonts, cont, "Preload OSD Fonts",
                  g_setting.speed.boot_fonts, SAVING_BOOT_FONTS, ROW_BOOT_FONTS);

    create_toggle(&btn_group_skip_boot_menu, cont, "Skip Boot Menu",
                  g_setting.speed.skip_boot_menu, SAVING_SKIP_BOOT_MENU, ROW_SKIP_BOOT_MENU);

    create_toggle(&btn_group_defer_menu, cont, "Defer Menu Build",
                  g_setting.speed.defer_menu, SAVING_DEFER_MENU, ROW_DEFER_MENU);

    create_toggle(&btn_group_async_imu, cont, "Async Motion Sensor",
                  g_setting.speed.async_imu, SAVING_ASYNC_IMU, ROW_ASYNC_IMU);

    create_toggle(&btn_group_async_display, cont, "Async Display Setup",
                  g_setting.speed.async_display, SAVING_ASYNC_DISPLAY, ROW_ASYNC_DISPLAY);

    // Directly under the last of the three it needs, so the reason it is
    // greyed is on the screen with it.
    create_toggle(&btn_group_early_timing, cont, "Early Video Timing",
                  g_setting.speed.boot_display_early, SAVING_EARLY_TIMING, ROW_EARLY_TIMING);

    create_toggle(&btn_group_skip_wifi_stop, cont, "Skip WiFi Stop",
                  g_setting.speed.skip_wifi_stop, SAVING_SKIP_WIFI_STOP, ROW_SKIP_WIFI_STOP);

    create_heading(cont, arr, "Input", ROW_HEAD_INPUT);
    // One row for what used to be Throttle UI Updates and Skip Idle Redraws.
    // Both act on the status bar and nothing else: the throttle cuts how often
    // statubar_update() is called, the diff drops the redraws that are left
    // when the text has not changed. Neither has a cost and neither is worth
    // having without the other, so two rows only offered combinations nobody
    // wanted.
    create_toggle(&btn_group_statusbar_idle, cont, "Skip Idle Redraws",
                  g_setting.speed.ui_throttle && g_setting.speed.label_diff,
                  SAVING_STATUSBAR_IDLE, ROW_STATUSBAR_IDLE);
    create_toggle(&btn_group_long_press, cont, "Timed Long Press",
                  g_setting.speed.timed_long_press, SAVING_LONG_PRESS, ROW_TIMED_LONG_PRESS);
    create_slider_item(&slider_long_press, cont, _lang("Long Press Time"),
                       LONG_PRESS_CHOICE_NUM - 1, 0, ROW_LONG_PRESS_MS);
    // create_slider_item() puts its value label in column 5, which this page
    // does not have room for, so give the slider one column and the value the
    // one the savings use on the other rows.
    lv_obj_set_grid_cell(slider_long_press.slider, LV_GRID_ALIGN_STRETCH, 3, 1,
                         LV_GRID_ALIGN_CENTER, ROW_LONG_PRESS_MS, 1);
    lv_obj_set_grid_cell(slider_long_press.label, LV_GRID_ALIGN_START, 4, 2,
                         LV_GRID_ALIGN_CENTER, ROW_LONG_PRESS_MS, 1);
    // create_slider_item() has already given both of these a disabled colour,
    // so perf_rows_refresh() only has to set the state.
    row_name[ROW_LONG_PRESS_MS] = slider_long_press.name;
    long_press_slider_update();

    create_toggle(&btn_group_split_lock, cont, "Split UI Lock",
                  g_setting.speed.split_lock, SAVING_SPLIT_LOCK, ROW_SPLIT_LOCK);

    // Not speed settings, and their column is a duration rather than a
    // saving, so they are their own group instead of sitting at the end of a
    // column of measured figures.
    create_heading(cont, arr, "Sound", ROW_HEAD_SOUND);
    create_toggle(&btn_group_button_beep, cont, "Button Beep",
                  g_setting.input.button_beep, DURATION_BUTTON_BEEP, ROW_BUTTON_BEEP);
    create_toggle(&btn_group_dial_beep, cont, "Dial Beep",
                  g_setting.input.dial_beep, DURATION_DIAL_BEEP, ROW_DIAL_BEEP);

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
    perf_rows_refresh();
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

// One row, the two settings behind it. Both keys are still written so an
// older build reading the same ini finds what it expects.
static void toggle_statusbar_idle(void) {
    btn_group_toggle_sel(&btn_group_statusbar_idle);

    bool on = btn_group_get_sel(&btn_group_statusbar_idle) == 1;

    g_setting.speed.ui_throttle = on;
    g_setting.speed.label_diff = on;
    settings_put_bool("speed", "ui_throttle", on);
    settings_put_bool("speed", "label_diff", on);
    LOGI("speed: ui_throttle=%s label_diff=%s", on ? "on" : "off", on ? "on" : "off");
}

// The framework has already moved the selection by the time this runs.
static void on_roller(uint8_t key) {
    int cur = pp_performance.p_arr.cur;

    if (g_app_state == APP_STATE_SUBMENU_ITEM_FOCUSED && cur == ROW_LONG_PRESS_MS) {
        int idx = long_press_choice_index(g_setting.input.long_press_ms);

        if (idx < 0)
            idx = 0;

        if (key == DIAL_KEY_UP && idx < LONG_PRESS_CHOICE_NUM - 1)
            idx++;
        else if (key == DIAL_KEY_DOWN && idx > 0)
            idx--;

        g_setting.input.long_press_ms = long_press_choices[idx];
        long_press_slider_update();
        return;
    }

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

    perf_rows_refresh();
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

    case ROW_DVR_STOP_WAIT:
        toggle_setting(&btn_group_dvr_stop_wait, &g_setting.speed.dvr_stop_wait, "dvr_stop_wait");
        break;

    case ROW_DVR_START_WAIT:
        toggle_setting(&btn_group_dvr_start_wait, &g_setting.speed.dvr_start_wait, "dvr_start_wait");
        break;

    case ROW_MENU_ASYNC_DISPLAY:
        toggle_setting(&btn_group_menu_async_display, &g_setting.speed.menu_async_display, "menu_async_display");
        break;

    case ROW_BOOT_DISPLAY:
        toggle_setting(&btn_group_boot_display, &g_setting.speed.boot_display, "boot_display");
        break;

    case ROW_BOOT_FONTS:
        toggle_setting(&btn_group_boot_fonts, &g_setting.speed.boot_fonts, "boot_fonts");
        break;

    case ROW_SKIP_BOOT_MENU:
        toggle_setting(&btn_group_skip_boot_menu, &g_setting.speed.skip_boot_menu, "skip_boot_menu");
        break;

    case ROW_DEFER_MENU:
        toggle_setting(&btn_group_defer_menu, &g_setting.speed.defer_menu, "defer_menu");
        break;

    case ROW_ASYNC_IMU:
        toggle_setting(&btn_group_async_imu, &g_setting.speed.async_imu, "async_imu");
        break;

    case ROW_ASYNC_DISPLAY:
        toggle_setting(&btn_group_async_display, &g_setting.speed.async_display, "async_display");
        break;

    case ROW_EARLY_TIMING:
        toggle_setting(&btn_group_early_timing, &g_setting.speed.boot_display_early, "boot_display_early");
        break;

    case ROW_ASYNC_TUNER:
        toggle_setting(&btn_group_async_tuner, &g_setting.speed.async_tuner, "async_tuner");
        break;

    case ROW_SKIP_WIFI_STOP:
        toggle_setting(&btn_group_skip_wifi_stop, &g_setting.speed.skip_wifi_stop, "skip_wifi_stop");
        break;

    case ROW_SPI_BURST:
        toggle_setting(&btn_group_spi_burst, &g_setting.speed.spi_burst, "spi_burst");
        break;

    case ROW_FAST_EFUSE:
        toggle_setting(&btn_group_fast_efuse, &g_setting.speed.fast_efuse, "fast_efuse");
        break;

    case ROW_STATUSBAR_IDLE:
        toggle_statusbar_idle();
        break;

    case ROW_TIMED_LONG_PRESS:
        toggle_setting(&btn_group_long_press, &g_setting.speed.timed_long_press, "timed_long_press");
        break;

    case ROW_LONG_PRESS_MS:
        // Same two-step edit the fans page uses: click to take the dial, turn
        // to change, click to put it back.
        if (g_app_state == APP_STATE_SUBMENU_ITEM_FOCUSED) {
            ini_putl("input", "long_press_ms", g_setting.input.long_press_ms, SETTING_INI);
            LOGI("input: long_press_ms=%d", g_setting.input.long_press_ms);
            lv_obj_add_style(slider_long_press.slider, &style_silder_main, LV_PART_MAIN);
            app_state_push(APP_STATE_SUBMENU);
        } else {
            app_state_push(APP_STATE_SUBMENU_ITEM_FOCUSED);
            lv_obj_add_style(slider_long_press.slider, &style_silder_select, LV_PART_MAIN);
        }
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

    // Half a dozen of these decide whether another row does anything, and the
    // comment under the list changes with them, so both are redone after any
    // click rather than listing which rows affect which.
    perf_rows_refresh();
    perf_comment_update();
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
