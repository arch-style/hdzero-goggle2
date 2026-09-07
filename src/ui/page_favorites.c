#include "page_favorites.h"

#include <stdio.h>

#include <log/log.h>
#include <minIni.h>

#include "core/app_state.h"
#include "core/common.hh"
#include "core/favorites.h"
#include "core/osd.h"
#include "core/settings.h"
#include "lang/language.h"
#include "page_common.h"
#include "page_scannow.h"
#include "ui/ui_style.h"

enum {
    ROW_ENABLE = 0,
    ROW_COUNT_SEL,
    ROW_SLOT_FIRST,
    ROW_SLOT_LAST = ROW_SLOT_FIRST + FAVORITES_MAX - 1,
    ROW_BACK,
    ROW_HINT, // not selectable, just the last grid row we occupy
    ROW_COUNT = ROW_BACK + 1
};

static lv_coord_t col_dsc[] = {160, 200, 200, 160, 160, 160, LV_GRID_TEMPLATE_LAST};
// 12 rows of 54 rather than the usual 60: this page needs 11 selectable rows,
// one more than any other page, and the Back row has to stay on screen.
static lv_coord_t row_dsc[] = {54, 54, 54, 54, 54, 54, 54, 54, 54, 54, 54, 54, LV_GRID_TEMPLATE_LAST};

static btn_group_t btn_group_fav;
static lv_obj_t *count_label;
static lv_obj_t *slot_label[FAVORITES_MAX];
static lv_obj_t *hint_label;

// Row currently being edited with the dial, or -1 when nothing is focused.
static int editing_row = -1;

static void count_label_update(void) {
    char buf[64];

    if (editing_row == ROW_COUNT_SEL)
        snprintf(buf, sizeof(buf), "%s: #FFFF00 %d#", _lang("Number of channels"), g_setting.favorites.count);
    else
        snprintf(buf, sizeof(buf), "%s: %d", _lang("Number of channels"), g_setting.favorites.count);

    lv_label_set_text(count_label, buf);
}

static void slot_label_update(int slot) {
    char buf[64];
    uint8_t ch = g_setting.favorites.channel[slot];
    const char *value;

    if (ch == 0)
        value = _lang("Empty");
    else
        value = channel2str(1, g_setting.source.hdzero_band, ch);

    if (editing_row == ROW_SLOT_FIRST + slot)
        snprintf(buf, sizeof(buf), "%s %d: #FFFF00 %s#", _lang("Slot"), slot + 1, value);
    else if (favorites_slot_duplicate(slot))
        snprintf(buf, sizeof(buf), "%s %d: %s #FF8000 (%s)#", _lang("Slot"), slot + 1, value, _lang("duplicate"));
    else
        snprintf(buf, sizeof(buf), "%s %d: %s", _lang("Slot"), slot + 1, value);

    lv_label_set_text(slot_label[slot], buf);

    // Slots past the configured count stay on screen but greyed out, and the
    // dial skips over them.
    bool in_use = (slot < g_setting.favorites.count);
    lv_obj_t *panel = pp_favorites.p_arr.panel[ROW_SLOT_FIRST + slot];

    if (in_use) {
        lv_obj_add_flag(panel, FLAG_SELECTABLE);
        lv_obj_clear_state(slot_label[slot], STATE_DISABLED);
    } else {
        lv_obj_clear_flag(panel, FLAG_SELECTABLE);
        lv_obj_add_state(slot_label[slot], STATE_DISABLED);
    }
}

static void hint_label_update(void) {
    char buf[128];
    int valid = favorites_valid_count();

    if (editing_row >= 0)
        snprintf(buf, sizeof(buf), "%s", _lang("Turn the dial to change, click to confirm"));
    else if (g_setting.favorites.enable && (valid == 0))
        snprintf(buf, sizeof(buf), "#FF8000 %s#", _lang("Register at least 1 channel to use favorites"));
    else if (g_setting.favorites.enable && (valid == 1))
        snprintf(buf, sizeof(buf), "%s", _lang("The dial stays locked to this channel"));
    else
        snprintf(buf, sizeof(buf), " ");

    lv_label_set_text(hint_label, buf);
}

static void page_favorites_update(void) {
    count_label_update();
    for (int i = 0; i < FAVORITES_MAX; i++)
        slot_label_update(i);
    hint_label_update();
}

static lv_obj_t *page_favorites_create(lv_obj_t *parent, panel_arr_t *arr) {
    char buf[128];
    int row = 0;

    lv_obj_t *page = lv_menu_page_create(parent, NULL);
    lv_obj_clear_flag(page, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(page, 1053, 900);
    lv_obj_add_style(page, &style_subpage, LV_PART_MAIN);
    lv_obj_set_style_pad_top(page, 24, 0); // other pages use 94; we need the height

    lv_obj_t *section = lv_menu_section_create(page);
    lv_obj_add_style(section, &style_submenu, LV_PART_MAIN);
    lv_obj_set_size(section, 1053, 894);

    snprintf(buf, sizeof(buf), "%s:", _lang("Favorites"));
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

    create_btn_group_item(&btn_group_fav, cont, 2, _lang("Favorites"), _lang("Off"), _lang("On"), "", "", row++);
    btn_group_set_sel(&btn_group_fav, g_setting.favorites.enable ? 1 : 0);

    count_label = create_label_item(cont, "", 1, row++, 3);

    for (int i = 0; i < FAVORITES_MAX; i++)
        slot_label[i] = create_label_item(cont, "", 1, row++, 3);

    snprintf(buf, sizeof(buf), "< %s", _lang("Back"));
    create_label_item(cont, buf, 1, row++, 3);

    hint_label = lv_label_create(cont);
    lv_label_set_recolor(hint_label, true);
    lv_label_set_text(hint_label, " ");
    lv_obj_set_style_text_font(hint_label, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_align(hint_label, LV_TEXT_ALIGN_LEFT, 0);
    lv_obj_set_style_text_color(hint_label, lv_color_make(255, 255, 255), 0);
    lv_obj_set_style_pad_top(hint_label, 12, 0);
    lv_label_set_long_mode(hint_label, LV_LABEL_LONG_WRAP);
    lv_obj_set_grid_cell(hint_label, LV_GRID_ALIGN_START, 1, 4, LV_GRID_ALIGN_START, ROW_HINT, 1);

    pp_favorites.p_arr.max = row;

    page_favorites_update();
    return page;
}

static void on_enter(void) {
    // The band can have changed on the Source page since the last visit, which
    // changes both the channel names and which slots are usable.
    editing_row = -1;
    page_favorites_update();
}

static void on_exit(void) {
    editing_row = -1;
}

static void on_roller(uint8_t key) {
    if (g_app_state != APP_STATE_SUBMENU_ITEM_FOCUSED)
        return;

    if (editing_row == ROW_COUNT_SEL) {
        uint8_t count = g_setting.favorites.count;

        if (key == DIAL_KEY_UP)
            count = (count >= FAVORITES_MAX) ? 1 : count + 1;
        else if (key == DIAL_KEY_DOWN)
            count = (count <= 1) ? FAVORITES_MAX : count - 1;

        g_setting.favorites.count = count;
        page_favorites_update();
        return;
    }

    if ((editing_row < ROW_SLOT_FIRST) || (editing_row > ROW_SLOT_LAST))
        return;

    int slot = editing_row - ROW_SLOT_FIRST;

    // 0 is the "empty" entry, so the value wraps over [0, channel count].
    uint8_t ch = g_setting.favorites.channel[slot];
    uint8_t max = HDZERO_CHANNEL_NUM;

    if (key == DIAL_KEY_UP)
        ch = (ch >= max) ? 0 : ch + 1;
    else if (key == DIAL_KEY_DOWN)
        ch = (ch == 0) ? max : ch - 1;

    g_setting.favorites.channel[slot] = ch;
    slot_label_update(slot);
}

static void favorites_save_row(int row) {
    char key[8];

    if (row == ROW_COUNT_SEL) {
        ini_putl("favorites", "count", g_setting.favorites.count, SETTING_INI);
        LOGI("favorites: count = %d", g_setting.favorites.count);
        return;
    }

    int slot = row - ROW_SLOT_FIRST;
    snprintf(key, sizeof(key), "ch%d", slot + 1);
    ini_putl("favorites", key, g_setting.favorites.channel[slot], SETTING_INI);
    LOGI("favorites: slot %d = channel %d", slot + 1, g_setting.favorites.channel[slot]);
}

static void on_click(uint8_t key, int sel) {
    if (sel == ROW_ENABLE) {
        btn_group_toggle_sel(&btn_group_fav);
        g_setting.favorites.enable = btn_group_get_sel(&btn_group_fav) == 1;
        settings_put_bool("favorites", "enable", g_setting.favorites.enable);
        hint_label_update();
        return;
    }

    if ((sel != ROW_COUNT_SEL) && ((sel < ROW_SLOT_FIRST) || (sel > ROW_SLOT_LAST)))
        return;

    if (editing_row == sel) {
        favorites_save_row(sel);
        editing_row = -1;
        app_state_push(APP_STATE_SUBMENU);
    } else {
        editing_row = sel;
        app_state_push(APP_STATE_SUBMENU_ITEM_FOCUSED);
    }

    page_favorites_update();
}

page_pack_t pp_favorites = {
    .p_arr = {
        .cur = 0,
        .max = ROW_COUNT,
    },
    .name = "Favorites",
    .create = page_favorites_create,
    .enter = on_enter,
    .exit = on_exit,
    .on_created = NULL,
    .on_update = NULL,
    .on_roller = on_roller,
    .on_click = on_click,
    .on_right_button = NULL,
};
