#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# The README's and the page's screens (docs/screens/*.png), from the app itself on the host: tests/scripts/readme.txt.
#   sh tools/readme_screens.sh
set -e
cd "$(dirname "$0")/.."
rm -f build/host/bioforge-*.png
sh tests/run_host.sh tests/scripts/readme.txt > /dev/null
for p in 2650:play 3080:steps 3600:sound 4100:fx 4700:mix 5200:hold 6200:help 6700:song 7500:deep; do
    cp "build/host/bioforge-${p%%:*}.png" "docs/screens/${p##*:}.png"
    echo "screen -> docs/screens/${p##*:}.png"
done
