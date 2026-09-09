#!/bin/sh
#
# Copy the goggles' application log off the SD card.
#
# The goggles keep two boots: HDZGOGGLE.log is the last one and
# HDZGOGGLE.prev.log the one before, and every power-on pushes the older of
# the two off the end. Anything worth reading twice -- a slow boot, a session
# that produced a bad recording -- is therefore one boot away from being gone.
# This branch is judged from those logs, so they are worth keeping.
#
#   utilities/save-goggle-logs.sh [card] [destination]
#
# Defaults to the card at /Volumes/NO NAME and to logs/ in the checkout, which
# is not tracked. A log already in the archive is recognised by its checksum
# and skipped, so running this twice on the same card costs nothing and
# plugging the card in between boots is always safe.
#
# Names are <archive date>-boot<total>ms-<checksum>.log.gz. The date is this
# machine's, because the goggles have no RTC battery and restore the same
# fake time every boot; the boot total is from the log itself, so the boot
# that took ten seconds can be found by name.

set -e

CARD=${1:-/Volumes/NO NAME}
DEST=${2:-$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)/logs}

if [ ! -d "$CARD" ]; then
    echo "no card at $CARD" >&2
    exit 1
fi

mkdir -p "$DEST"

saved=0
skipped=0

for name in HDZGOGGLE.log HDZGOGGLE.prev.log; do
    src="$CARD/$name"
    [ -s "$src" ] || continue

    sum=$(shasum -a 1 "$src" | cut -c1-8)

    if ls "$DEST"/*-"$sum".log.gz >/dev/null 2>&1; then
        skipped=$((skipped + 1))
        continue
    fi

    boot=$(grep -o 'boot total: app start to switch done [0-9]*ms' "$src" |
           tail -1 | grep -o '[0-9]*' || true)
    [ -n "$boot" ] || boot=unknown

    out="$DEST/$(date +%Y-%m-%d_%H%M)-boot${boot}ms-$sum.log.gz"
    gzip -c "$src" > "$out"
    saved=$((saved + 1))
    echo "saved $(basename "$out") ($(wc -c < "$src" | tr -d ' ') bytes from $name)"
done

echo "$saved saved, $skipped already had"
