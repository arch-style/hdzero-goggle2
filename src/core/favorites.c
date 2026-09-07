#include "core/favorites.h"

#include "ui/page_scannow.h"

// Slots pointing past the end of the current band (F1 while on the low band,
// say) are skipped rather than clamped, so toggling the band back and forth
// does not silently rewrite what the user registered. Slots beyond the
// configured count keep their value but take no part in tuning.
static bool slot_usable(int slot) {
    uint8_t ch;

    if (slot >= g_setting.favorites.count)
        return false;

    ch = g_setting.favorites.channel[slot];
    return (ch >= 1) && (ch <= HDZERO_CHANNEL_NUM);
}

// Collects the usable slots, in slot order, into `list`, keeping only the
// first occurrence of each channel. Without this, the same channel in two
// slots makes favorites_step() hand back the channel it was already on, so
// the dial stalls there and never reaches the rest of the list.
static int favorites_collect(uint8_t *list) {
    int count = 0;

    for (int i = 0; i < FAVORITES_MAX; i++) {
        if (!slot_usable(i))
            continue;

        uint8_t ch = g_setting.favorites.channel[i];
        bool seen = false;

        for (int j = 0; j < count; j++) {
            if (list[j] == ch) {
                seen = true;
                break;
            }
        }

        if (!seen)
            list[count++] = ch;
    }

    return count;
}

bool favorites_slot_duplicate(int slot) {
    if ((slot < 0) || (slot >= FAVORITES_MAX) || !slot_usable(slot))
        return false;

    uint8_t ch = g_setting.favorites.channel[slot];

    for (int i = 0; i < slot; i++) {
        if (slot_usable(i) && (g_setting.favorites.channel[i] == ch))
            return true;
    }

    return false;
}

int favorites_valid_count(void) {
    uint8_t list[FAVORITES_MAX];
    return favorites_collect(list);
}

bool favorites_active(void) {
    return g_setting.favorites.enable && (favorites_valid_count() >= 1);
}

uint8_t favorites_get(int index) {
    uint8_t list[FAVORITES_MAX];
    int count = favorites_collect(list);

    if ((index < 0) || (index >= count))
        return 0;

    return list[index];
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
