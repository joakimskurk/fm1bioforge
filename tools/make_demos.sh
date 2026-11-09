#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# BIOFORGE's demos for the page: the firmware itself on the host (tests/host.c), played by the scripts
# tests/scripts/demo_N_name.txt, its sound as it goes out over USB: build/audio/demos/N_name.wav (web/make_site.py
# encodes them and draws their spectra). Also used to listen: every one is the real app, its keys and knobs turned.
#   sh tools/make_demos.sh
set -e
cd "$(dirname "$0")/.."
mkdir -p build/host build/audio/demos
cc -O2 -Wall -Wno-unused-function -DDINGHY_HOST -DAPP_VER="\"${APP_VER:-demo}\"" -Ifirmware/hal -Ifirmware/src \
    -Ifirmware/gen -o build/host/bioforge-demo firmware/src/bioforge.c
for s in tests/scripts/demo_*.txt; do
    n=$(basename "$s" .txt); n=${n#demo_}
    build/host/bioforge-demo "build/audio/demos/$n" "$s" > /dev/null
    echo "demo   -> build/audio/demos/$n.wav"
done
