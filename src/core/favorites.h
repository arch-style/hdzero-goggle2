#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>

#include "core/settings.h"

// Favorite channels let the dial cycle through a short user-picked list
// instead of walking the whole band. Slots hold channel indexes of the
// currently selected HDZero band only; see setting_favorites_t.

// Number of slots holding a channel that is valid for the current band.
int favorites_valid_count(void);

// True when favorite tuning should take over the dial: the feature is enabled
// and at least two slots are usable (a single entry would make the dial a
// no-op, which reads as a broken goggle).
bool favorites_active(void);

// Channel of the `index`-th usable favorite (0-based), or 0 when out of range.
uint8_t favorites_get(int index);

// Neighbour of `channel` in the favorite list, wrapping around.
// dir > 0 walks up the list, dir < 0 walks down. When `channel` is not itself
// a favorite the walk starts from the end the dial came from.
uint8_t favorites_step(uint8_t channel, int dir);

#ifdef __cplusplus
}
#endif
