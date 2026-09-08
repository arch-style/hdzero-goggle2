#include "ui/ui_main_menu.h"

#include <stdio.h>
#include <stdlib.h>

#include <log/log.h>
#include <lvgl/lvgl.h>

#include "common.hh"
#include "core/app_state.h"
#include "driver/hardware.h"
#include "driver/mcp3021.h"
#include "driver/oled.h"
#include "lang/language.h"
#include "ui/page_analog_rssi.h"
#include "ui/page_autoscan.h"
#include "ui/page_clock.h"
#include "ui/page_common.h"
#include "ui/page_elrs.h"
#include "ui/page_fans.h"
#include "ui/page_fastmenu.h"
#include "ui/page_favorites.h"
#include "ui/page_focus_chart.h"
#include "ui/page_headtracker.h"
#include "ui/page_imagesettings.h"
#include "ui/page_input.h"
#include "ui/page_osd.h"
#include "ui/page_playback.h"
#include "ui/page_power.h"
#include "ui/page_record.h"
#include "ui/page_scannow.h"
#include "ui/page_sleep.h"
#include "ui/page_source.h"
#include "ui/page_storage.h"
#include "ui/page_version.h"
#include "ui/page_wifi.h"
#include "ui/ui_image_setting.h"
#include "ui/ui_keyboard.h"
#include "ui/ui_porting.h"
#include "ui/ui_statusbar.h"
#include "ui/ui_style.h"

LV_IMG_DECLARE(img_arrow);

progress_bar_t progress_bar;

static lv_obj_t *menu;
static lv_obj_t *root_page;

/**
 * Page order is enforced by definition. The first MENU_STOCK_COUNT entries are
 * the stock menu, untouched and reachable exactly as before; everything this
 * fork adds sits behind them on a second page, reached by rolling past either
 * end of the list.
 */
static page_pack_t *page_packs[] = {
    &pp_scannow,
    &pp_source,
    &pp_imagesettings,
    &pp_osd,
    &pp_power,
    &pp_fans,
    &pp_record,
    &pp_autoscan,
    &pp_elrs,
    &pp_wifi,
    &pp_headtracker,
    &pp_playback,
    &pp_storage,
    &pp_version,
    &pp_focus_chart,
    &pp_clock,
    &pp_input,
    &pp_analog_rssi,
    &pp_sleep,

    // --- second page ---
    &pp_favorites,
    &pp_fastmenu,
};

#define PAGE_COUNT (ARRAY_SIZE(page_packs))

// Entries belonging to the stock first page.
#define MENU_STOCK_COUNT (PAGE_COUNT - 2)

// The sidebar fills the menu, which starts below the status bar.
#define MENU_POS_Y          96
#define MENU_SIDEBAR_HEIGHT (DRAW_VER_RES_FHD - MENU_POS_Y)

// The sidebar is a fixed height, so the entries have to shrink as pages are
// added rather than the last one dropping off the bottom. Never looser than
// the theme's own padding, so a short list keeps the stock look.
static lv_coord_t menu_entry_pad_ver(void) {
    const lv_coord_t theme_pad = 11;
    const lv_coord_t line_h = lv_font_montserrat_24.line_height;
    // Worst case for the gap the flex layout puts between entries. The exact
    // value is not readable from here, but the sidebar overflowing at 20
    // entries of 49px and fitting at 19 bounds it to 0..2.
    const lv_coord_t gap = 2;
    // Only the larger page has to fit, and it is the stock one, so this
    // reproduces the stock spacing rather than shrinking it for the additions.
    const lv_coord_t entries = (lv_coord_t)MENU_STOCK_COUNT;

    lv_coord_t entry_h = (MENU_SIDEBAR_HEIGHT - gap * (entries - 1)) / entries;
    lv_coord_t pad = (entry_h - line_h) / 2;

    if (pad < 2)
        pad = 2;
    if (pad > theme_pad)
        pad = theme_pad;

    return pad;
}

static page_pack_t *post_bootup_actions[PAGE_COUNT];
static size_t post_bootup_actions_count = 0;
static bool bootup_actions_fired = false;

static page_pack_t *find_pp(lv_obj_t *page) {
    for (uint32_t i = 0; i < PAGE_COUNT; i++) {
        if (page_packs[i]->page == page) {
            return page_packs[i];
        }
    }
    return NULL;
}

static void select_menu_tab(page_pack_t *pp) {
    lv_obj_clear_flag(pp->icon, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_style_bg_opa(((lv_menu_t *)menu)->selected_tab, LV_OPA_50, LV_STATE_CHECKED);
}

static void deselect_menu_tab(page_pack_t *pp) {
    // LV_OPA_20 is the default for pressed menu
    // see lv_theme_default.c styles->menu_pressed
    lv_obj_set_style_bg_opa(((lv_menu_t *)menu)->selected_tab, LV_OPA_20, LV_STATE_CHECKED);
    lv_obj_add_flag(pp->icon, LV_OBJ_FLAG_HIDDEN);
}

void submenu_enter(void) {
    page_pack_t *pp = find_pp(lv_menu_get_cur_main_page(menu));
    if (!pp) {
        return;
    }

    select_menu_tab(pp);

    if (pp->p_arr.max) {
        // if we have selectable entries, select the first selectable one
        for (pp->p_arr.cur = 0; !lv_obj_has_flag(pp->p_arr.panel[pp->p_arr.cur], FLAG_SELECTABLE); ++pp->p_arr.cur)
            ;
        set_select_item(&pp->p_arr, pp->p_arr.cur);
    }

    if (pp->enter) {
        // if your page as a enter event handler, call it
        pp->enter();
    }
}

void submenu_right_button(bool is_short) {
    page_pack_t *pp = find_pp(lv_menu_get_cur_main_page(menu));
    if (!pp) {
        return;
    }

    if (pp->on_right_button) {
        // if your page has a right_button event handler, call it
        pp->on_right_button(is_short);
    }
}

void submenu_roller(uint8_t key) {
    page_pack_t *pp = find_pp(lv_menu_get_cur_main_page(menu));
    if (!pp) {
        return;
    }

    if (pp->p_arr.max) {
        // if we have selectable entries, move selection
        if (key == DIAL_KEY_UP) {
            do {
                if (pp->p_arr.cur < pp->p_arr.max - 1)
                    pp->p_arr.cur++;
                else
                    pp->p_arr.cur = 0;
            } while (!lv_obj_has_flag(pp->p_arr.panel[pp->p_arr.cur], FLAG_SELECTABLE));
        } else if (key == DIAL_KEY_DOWN) {
            do {
                if (pp->p_arr.cur > 0)
                    pp->p_arr.cur--;
                else
                    pp->p_arr.cur = pp->p_arr.max - 1;
            } while (!lv_obj_has_flag(pp->p_arr.panel[pp->p_arr.cur], FLAG_SELECTABLE));
        }
        set_select_item(&pp->p_arr, pp->p_arr.cur);
    }

    // Allow roller to have latest item selected
    if (pp->on_roller) {
        // if your page as a roller event handler, call it
        pp->on_roller(key);
    }
}

// the submenu pages called on_roller event handler has to update
// the selection by setting pp->p_arr.cur if a selection change is needed
void submenu_roller_no_selection_change(uint8_t key) {
    page_pack_t *pp = find_pp(lv_menu_get_cur_main_page(menu));
    if (!pp) {
        return;
    }

    if (pp->on_roller) {
        // if your page as a roller event handler, call it
        pp->on_roller(key);
    }

    set_select_item(&pp->p_arr, pp->p_arr.cur);
}

void submenu_exit() {
    LOGI("submenu_exit");
    app_state_push(APP_STATE_MAINMENU);

    page_pack_t *pp = find_pp(lv_menu_get_cur_main_page(menu));
    if (!pp) {
        return;
    }

    deselect_menu_tab(pp);

    if (pp->exit) {
        // if your page as a exit event handler, call it
        pp->exit();
    }

    if (pp->p_arr.max) {
        // if we have selectable icons, reset the selector
        pp->p_arr.cur = 0;
        set_select_item(&pp->p_arr, -1);
    }
}

void submenu_click(void) {
    page_pack_t *pp = find_pp(lv_menu_get_cur_main_page(menu));
    if (!pp) {
        return;
    }

    if (pp->on_click) {
        // if your page as a click event handler, call it
        pp->on_click(DIAL_KEY_CLICK, pp->p_arr.cur);
    }

    if (pp->p_arr.max && g_app_state != APP_STATE_WIFI) {
        // if we have selectable icons, check if we hit the back button
        if (pp->p_arr.cur == pp->p_arr.max - 1) {
            submenu_exit();
        }
    }
}

// Which entries the current menu page covers, as an inclusive index range
// into page_packs.
static int menu_page = 0; // 0 = stock entries, 1 = this fork's additions

static int menu_page_first(int page) {
    return page ? MENU_STOCK_COUNT : 0;
}

static int menu_page_last(int page) {
    return page ? (int)PAGE_COUNT - 1 : MENU_STOCK_COUNT - 1;
}

// Entries outside the current page are hidden, which also takes them out of
// the sidebar's flex layout, so the page in view starts at the top.
static void menu_page_apply(void) {
    for (uint32_t i = 0; i < PAGE_COUNT; i++) {
        lv_obj_t *cont = lv_obj_get_parent(page_packs[i]->label);
        bool on_page = ((int)i >= menu_page_first(menu_page)) && ((int)i <= menu_page_last(menu_page));

        if (on_page)
            lv_obj_clear_flag(cont, LV_OBJ_FLAG_HIDDEN);
        else
            lv_obj_add_flag(cont, LV_OBJ_FLAG_HIDDEN);
    }
}

void menu_nav(uint8_t key) {
    static int8_t selected = 0;
    LOGI("menu_nav: key = %d,sel = %d,page = %d", key, selected, menu_page);

    // Rolling past either end of a page moves to the other page rather than
    // wrapping within it.
    if (key == DIAL_KEY_DOWN) {
        if (selected > menu_page_first(menu_page)) {
            selected--;
        } else {
            menu_page = !menu_page;
            menu_page_apply();
            selected = menu_page_last(menu_page);
        }
    } else if (key == DIAL_KEY_UP) {
        if (selected < menu_page_last(menu_page)) {
            selected++;
        } else {
            menu_page = !menu_page;
            menu_page_apply();
            selected = menu_page_first(menu_page);
        }
    }

    lv_obj_t *entry = lv_obj_get_child(lv_obj_get_child(lv_menu_get_cur_sidebar_page(menu), 0), selected);
    lv_event_send(entry, LV_EVENT_CLICKED, NULL);
    lv_obj_scroll_to_view(entry, LV_ANIM_OFF);
}

static void menu_reinit(void) {
    LOGI("menu_reinit");

    page_pack_t *pp = find_pp(lv_menu_get_cur_main_page(menu));
    if (!pp) {
        return;
    }

    if ((pp == &pp_scannow)) {
        scan_reinit();
    }

    deselect_menu_tab(pp);

    if (pp->p_arr.max) {
        // if we have selectable icons, reset the selector
        pp->p_arr.cur = 0;
        set_select_item(&pp->p_arr, -1);
    }
}

// The menu is laid out for the resolution it was built at, 1080p. Drawn over
// 720p video there is not room for it, so scale the whole tree rather than
// re-laying out all twenty-odd pages: an object with transform_zoom is
// rendered into a layer together with its children, so one call covers every
// page. Scaling is about the object's top-left, so its own offset is scaled
// by hand to keep the whole thing on screen.
#define MENU_POS_X 250

static lv_coord_t menu_design_ver_res = 0;

static void main_menu_fit_display(void) {
    lv_coord_t ver_res = lv_disp_get_ver_res(NULL);
    lv_coord_t zoom = LV_IMG_ZOOM_NONE;

    // Menu and status bar are scaled by the same factor, which maps the whole
    // 1080p layout onto the smaller screen: the bar sits at the origin so it
    // needs no repositioning, and the menu's offset scales with it. Derived
    // from the visible height, the canvas less the overscan margin.
    // Rounded up: overshooting clips a couple of rows of plain background off
    // the bottom, where rounding down would leave a visible gap instead.
    if (menu_design_ver_res > 0 && ver_res < menu_design_ver_res)
        zoom = ((ver_res - DISP_OVERSCAN) * LV_IMG_ZOOM_NONE + menu_design_ver_res - 1) / menu_design_ver_res;

    lv_obj_set_style_transform_zoom(menu, zoom, 0);
    lv_obj_set_pos(menu,
                   (MENU_POS_X * zoom) / LV_IMG_ZOOM_NONE,
                   (MENU_POS_Y * zoom) / LV_IMG_ZOOM_NONE);

    statusbar_set_zoom(zoom);

    LOGI("menu: zoom %d/%d for %dpx display", zoom, LV_IMG_ZOOM_NONE, ver_res);
}

bool main_menu_is_shown(void) {
    return !lv_obj_has_flag(menu, LV_OBJ_FLAG_HIDDEN);
}

void main_menu_show(bool is_show) {
    if (is_show) {
        // The display can have changed resolution since the last time.
        main_menu_fit_display();
        menu_reinit();
        lv_obj_clear_flag(menu, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(menu, LV_OBJ_FLAG_HIDDEN);
    }
}

static void main_menu_create_entry(lv_obj_t *menu, lv_obj_t *section, page_pack_t *pp) {
    LOGD("creating main menu entry %s", pp->name);

    pp->page = pp->create(menu, &pp->p_arr);

    lv_obj_t *cont = lv_menu_cont_create(section);
    lv_coord_t pad = menu_entry_pad_ver();
    lv_obj_set_style_pad_top(cont, pad, 0);
    lv_obj_set_style_pad_bottom(cont, pad, 0);

    pp->label = lv_label_create(cont);
    lv_label_set_text(pp->label, _lang(pp->name));
    lv_obj_set_style_text_font(pp->label, &lv_font_montserrat_24, 0);
    lv_label_set_long_mode(pp->label, LV_LABEL_LONG_SCROLL_CIRCULAR);

    pp->icon = lv_img_create(cont);
    lv_img_set_src(pp->icon, &img_arrow);
    lv_obj_add_flag(pp->icon, LV_OBJ_FLAG_HIDDEN);

    lv_obj_set_style_text_font(cont, &lv_font_montserrat_24, 0);
    lv_menu_set_load_page_event(menu, cont, pp->page);

    if (pp->on_created) {
        pp->on_created();
    }
}

static int post_bootup_actions_cmp(const void *lhs, const void *rhs) {
    const int32_t leftPriority = ((page_pack_t *)lhs)->post_bootup_run_priority;
    const int32_t rightPriority = ((page_pack_t *)rhs)->post_bootup_run_priority;

    if (leftPriority < rightPriority) {
        return -1;
    } else if (leftPriority > rightPriority) {
        return 1;
    }

    return 0;
}

void main_menu_init(void) {
    menu = lv_menu_create(lv_scr_act());
    // lv_obj_add_flag(menu, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(menu, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_set_style_bg_color(menu, lv_color_make(32, 32, 32), 0);
    lv_obj_set_style_border_width(menu, 2, 0);
    lv_obj_set_style_border_color(menu, lv_color_make(255, 0, 0), 0);
    lv_obj_set_style_border_side(menu, LV_BORDER_SIDE_LEFT | LV_BORDER_SIDE_RIGHT, 0);
    lv_obj_set_size(menu, lv_disp_get_hor_res(NULL) - 500, lv_disp_get_ver_res(NULL) - MENU_POS_Y);
    lv_obj_set_pos(menu, MENU_POS_X, MENU_POS_Y);
    menu_design_ver_res = lv_disp_get_ver_res(NULL);

    root_page = lv_menu_page_create(menu, "aaa");

    lv_obj_t *section = lv_menu_section_create(root_page);
    // Scrollable as a backstop: if the entries ever outgrow the sidebar even
    // at minimum padding, menu_nav() scrolls the selection into view instead
    // of leaving it unreachable. No scrollbar, and no movement while they fit.
    lv_obj_set_scroll_dir(section, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(section, LV_SCROLLBAR_MODE_OFF);

    for (uint32_t i = 0; i < PAGE_COUNT; i++) {
        main_menu_create_entry(menu, section, page_packs[i]);
        if (page_packs[i]->post_bootup_run_priority > 0 && page_packs[i]->post_bootup_run_function != NULL) {
            post_bootup_actions[post_bootup_actions_count++] = page_packs[i];
        }
    }

    // Resort based on priority
    qsort(post_bootup_actions, post_bootup_actions_count, sizeof(page_pack_t *), post_bootup_actions_cmp);

    menu_page_apply();

    lv_obj_add_style(section, &style_rootmenu, LV_PART_MAIN);
    lv_obj_set_size(section, 250, MENU_SIDEBAR_HEIGHT);
    lv_obj_set_pos(section, 0, 0);

    lv_obj_set_size(root_page, 250, MENU_SIDEBAR_HEIGHT);
    lv_obj_set_pos(root_page, 0, 0);
    lv_obj_set_style_border_width(root_page, 0, 0);
    lv_obj_set_style_radius(root_page, 0, 0);

    lv_menu_set_sidebar_page(menu, root_page);
    lv_event_send(lv_obj_get_child(lv_obj_get_child(lv_menu_get_cur_sidebar_page(menu), 0), 0), LV_EVENT_CLICKED, NULL);
    lv_obj_add_flag(lv_menu_get_sidebar_header(menu), LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(lv_menu_get_cur_sidebar_page(menu), LV_OBJ_FLAG_SCROLLABLE);

    progress_bar.bar = lv_bar_create(lv_scr_act());
    lv_obj_set_size(progress_bar.bar, 320, 20);
    lv_obj_align(progress_bar.bar, LV_ALIGN_CENTER, 0, 0);
    lv_obj_add_flag(progress_bar.bar, LV_OBJ_FLAG_HIDDEN);
    progress_bar.start = 0;
    progress_bar.val = 0;

    // Create Keyboard Object
    keyboard_init();
}

static void bootup_action_completed() {
    bootup_actions_fired = false;
}

static void handle_bootup_action() {
    static page_pack_t **next_bootup_action = &post_bootup_actions[0];
    if (next_bootup_action - &post_bootup_actions[0] >= post_bootup_actions_count) {
        return;
    }

    (*next_bootup_action++)->post_bootup_run_function(bootup_action_completed);
}

void main_menu_update() {
    static uint32_t delta_ms = 0;
    uint32_t now_ms = time_ms();
    delta_ms = now_ms - delta_ms;

    for (uint32_t i = 0; i < PAGE_COUNT; i++) {
        if (page_packs[i]->on_update) {
            page_packs[i]->on_update(delta_ms);
        }
    }

    if (!bootup_actions_fired) {
        bootup_actions_fired = true;
        handle_bootup_action();
    }
    delta_ms = now_ms;
}

void progress_bar_update() {
    static uint8_t state = 0; // 0=idle, 1= in process

    switch (state) {
    case 0:
        if (progress_bar.start) { // to start the progress bar
            state = 1;
            lv_obj_add_flag(menu, LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(progress_bar.bar, LV_OBJ_FLAG_HIDDEN);
            progress_bar.val = 0;
            // LOGI("Progress bar start");
        }
        break;

    case 1:
        if (progress_bar.start == 0) { // to end end progress bar
            state = 0;
            lv_obj_clear_flag(menu, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(progress_bar.bar, LV_OBJ_FLAG_HIDDEN);
            progress_bar.val = 0;
            // LOGI("Progress bar end");
        }
        break;
    }

    if (state == 1) {
        if (progress_bar.val < 100)
            progress_bar.val += 4;
        lv_bar_set_value(progress_bar.bar, progress_bar.val, LV_ANIM_OFF);
        lv_timer_handler();
    }
}
