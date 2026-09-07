#include "core/favorites.h"

#include "driver/hardware.h"
#include "ui/page_common.h"
#include "ui/page_scannow.h"

favorites_source_t favorites_source(void) {
    if (g_source_info.source == SOURCE_ANALOG && GOGGLE_VER_2 &&
        (g_setting.source.analog_module == SETTING_SOURCES_ANALOG_MODULE_INTERNAL))
        return FAVORITES_SOURCE_ANALOG;

    return FAVORITES_SOURCE_HDZERO;
}

setting_favorites_list_t *favorites_list(void) {
    if (favorites_source() == FAVORITES_SOURCE_ANALOG)
        return &g_setting.favorites.analog;

    return &g_setting.favorites.hdzero;
}

uint8_t favorites_channel_max(void) {
    if (favorites_source() == FAVORITES_SOURCE_ANALOG)
        return ANALOG_CHANNEL_NUM;

    return HDZERO_CHANNEL_NUM;
}

const char *favorites_source_name(void) {
    return (favorites_source() == FAVORITES_SOURCE_ANALOG) ? "Analog" : "HDZero";
}

// Slots pointing past the end of the current band (F1 while on the low band,
// say) are skipped rather than clamped, so toggling the band back and forth
// does not silently rewrite what the user registered. Slots beyond the
// configured count keep their value but take no part in tuning.
static bool slot_usable(int slot) {
    const setting_favorites_list_t *list = favorites_list();
    uint8_t ch;

    if (slot >= list->count)
        return false;

    ch = list->channel[slot];
    return (ch >= 1) && (ch <= favorites_channel_max());
}

// Collects the usable slots, in slot order, into `out`, keeping only the
// first occurrence of each channel. Without this, the same channel in two
// slots makes favorites_step() hand back the channel it was already on, so
// the dial stalls there and never reaches the rest of the list.
static int favorites_collect(uint8_t *out) {
    const setting_favorites_list_t *list = favorites_list();
    int count = 0;

    for (int i = 0; i < FAVORITES_MAX; i++) {
        if (!slot_usable(i))
            continue;

        uint8_t ch = list->channel[i];
        bool seen = false;

        for (int j = 0; j < count; j++) {
            if (out[j] == ch) {
                seen = true;
                break;
            }
        }

        if (!seen)
            out[count++] = ch;
    }

    return count;
}

int favorites_valid_count(void) {
    uint8_t list[FAVORITES_MAX];
    return favorites_collect(list);
}

bool favorites_slot_duplicate(int slot) {
    const setting_favorites_list_t *list = favorites_list();

    if ((slot < 0) || (slot >= FAVORITES_MAX) || !slot_usable(slot))
        return false;

    uint8_t ch = list->channel[slot];

    for (int i = 0; i < slot; i++) {
        if (slot_usable(i) && (list->channel[i] == ch))
            return true;
    }

    return false;
}

bool favorites_active(void) {
    return favorites_list()->enable && (favorites_valid_count() >= 1);
}

uint8_t favorites_step(uint8_t channel, int dir) {
    uint8_t list[FAVORITES_MAX];
    int count = favorites_collect(list);

    if (count == 0)
        return channel;

    for (int i = 0; i < count; i++) {
        if (list[i] == channel) {
            int next = (dir > 0) ? i + 1 : i - 1;
            if (next >= count)
                next = 0;
            else if (next < 0)
                next = count - 1;
            return list[next];
        }
    }

    // Tuned to something outside the list (band change, VTX-driven change):
    // enter the list from whichever end the dial is heading towards.
    return (dir > 0) ? list[0] : list[count - 1];
}
