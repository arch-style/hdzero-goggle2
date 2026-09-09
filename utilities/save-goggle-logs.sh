#!/bin/sh
#
# Copy goggle application logs into the local archive.
#
#   utilities/save-goggle-logs.sh [source] [destination]
#
# source        an SD card or any directory holding HDZGOGGLE*.log files
#               (default /Volumes/NO NAME), or a single log file -- somebody
#               else's, sent by mail. Its name becomes part of the archive
#               name, so rename it to something that says whose it is first.
# destination   default logs/ in the checkout, which is not tracked.
#
# The goggles keep this boot and the nine before it (HDZGOGGLE.log, then
# HDZGOGGLE.1.log to HDZGOGGLE.9.log, newest first). Ten boots is a lot of
# room to notice something and go and get the card, but it is not unlimited:
# what is on the card is a queue, and this is what takes things out of it.
#
# A log already in the archive is recognised by its checksum and skipped, so
# running this on the same card twice costs nothing, and running it every time
# the card is in the machine is the point -- the boot worth keeping is always
# identified after the fact.
#
# Names are <archive date>-[label-]boot<total>ms-<checksum>.log.gz. The date is
# this machine's: the goggles have no RTC battery and restore the same fake
# time every boot, so their own timestamps sort nothing. The boot total is read
# out of the log, so the boot that took ten seconds can be found by name.

set -e

SRC=${1:-/Volumes/NO NAME}
DEST=${2:-$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)/logs}

if [ -f "$SRC" ]; then
    files=$SRC
    label=$(basename "$SRC" | sed 's/\.log$//; s/[^A-Za-z0-9._-]/_/g')-
elif [ -d "$SRC" ]; then
    # Newest first, which is also the order they will be read in.
    files=$(ls "$SRC"/HDZGOGGLE.log "$SRC"/HDZGOGGLE.[0-9].log \
               "$SRC"/HDZGOGGLE.prev.log 2>/dev/null || true)
    label=
else
    echo "no card, directory or file at $SRC" >&2
    exit 1
fi

if [ -z "$files" ]; then
    echo "no HDZGOGGLE logs in $SRC" >&2
    exit 1
fi

mkdir -p "$DEST"

saved=0
skipped=0

for src in $files; do
    [ -s "$src" ] || continue

    sum=$(shasum -a 1 "$src" | cut -c1-8)

    if ls "$DEST"/*-"$sum".log.gz >/dev/null 2>&1; then
        skipped=$((skipped + 1))
        continue
    fi

    boot=$(grep -o 'boot total: app start to switch done [0-9]*ms' "$src" |
           tail -1 | grep -o '[0-9]*' || true)
    [ -n "$boot" ] || boot=unknown

    # Written by the app as its first line. Missing on logs from before that,
    # and on logs from stock firmware.
    build=$(grep -m1 -o 'build: .*' "$src" || echo "build: not stated")

    out="$DEST/$(date +%Y-%m-%d_%H%M)-${label}boot${boot}ms-$sum.log.gz"
    gzip -c "$src" > "$out"
    saved=$((saved + 1))
    echo "saved $(basename "$out")"
    echo "      $(wc -c < "$src" | tr -d ' ') bytes from $(basename "$src"), $build"
done

echo "$saved saved, $skipped already had"
