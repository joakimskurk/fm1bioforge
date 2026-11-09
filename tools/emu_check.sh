#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# BIOFORGE's package in fm1-emu (tools/emu_ci.sh builds it): 6 s of the real firmware on the emulated chip. PLAY at
# 0.5 s (the awakening), the screen at 2.5 and 4.5 s. From its report, its sound and its screens
# (build/emu/): how much of the chip the interrupts take (the engine and the platform), the engine's own measure of
# its share (isr_load, read from RAM by its address in build/bioforge.syms), sound after PLAY, a screen drawn.
#   tools/emu_check.sh build/bioforge.fwsc        FM1_EMU: the fm1-emu binary
set -e
PKG="${1:-build/bioforge.fwsc}"
EMU="${FM1_EMU:-$HOME/fm1-emu/target/release/fm1-emu}"
OUT=build/emu
mkdir -p "$OUT/work"
cp "$PKG" "$OUT/pkg.fwsc"
ADDR=$(awk '$NF == "isr_load" { print $1 }' build/bioforge.syms 2>/dev/null | head -1)
s0=$(date +%s)
(cd "$OUT" && timeout "${EMU_TIMEOUT:-1500}" "$EMU" --fwsc pkg.fwsc --ms 6000 --keys '500:PLAY' \
    --wav out.wav --snap 2500,4500 ${ADDR:+--dump $ADDR 1} >report.txt 2>&1) || true
echo "emu_check: 6 s of BIOFORGE took $(( $(date +%s) - s0 )) s to emulate"
grep -E "in an ISR|^time |^dump| 0x" "$OUT/report.txt" | sed 's/^/  emu: /' || true
fail=0
run() { echo "== $1"; shift; "$@" || fail=1; }
run "the chip: the interrupts take under 90 % of it (the awakening playing)" python3 tests/bio_emu.py isr "$OUT/report.txt" 90
run "the engine's own measure of its share (isr_load)" python3 tests/bio_emu.py load "$OUT/report.txt"
run "sound after PLAY (the awakening)" python3 tests/bio_emu.py sound "$OUT/out.wav" 1.0 5.5
run "a screen drawn (the grid at 2.5 and 4.5 s)" python3 tests/bio_emu.py screen "$OUT/work/lcd_2500.bmp" "$OUT/work/lcd_4500.bmp"
[ $fail = 0 ] && echo "emu_check: all passed" || { echo "emu_check: FAILED"; exit 1; }
