#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Joakim (joakimskurk)
# BIOFORGE's own host tests (no FM-1, no JieLi toolchain; Python with numpy and Pillow): the tables are what their tool
# makes, the engine and the memory pass tests/bio_test.c (plain, and again under ASan/UBSan), the app compiles without a
# warning (gcc and clang), the renders and their spectrograms land in build/audio/. Dinghy's own tests:
# tests/run_tests.sh (after ./build.sh).
set -e
cd "$(dirname "$0")/.."
mkdir -p build/host build/audio
fail=0
run() { echo "== $1"; shift; "$@" || fail=1; }
I="-Ifirmware/src -Ifirmware/gen"

run "firmware/gen/bio_tables.h is what tools/gen_tables.py makes" python3 tools/gen_tables.py --check
if [ -f tools/gen_art.py ]; then
    if python3 tools/gen_art.py --check >/dev/null 2>&1; then echo "== firmware/gen/bio_art.h is what tools/gen_art.py makes"; echo "bio_art.h: up to date"
    else echo "== (bio_art.h differs from what this Pillow makes: run tools/gen_art.py and look; not a failure)"; fi
fi
run "the engine and the memory compile without a warning (-Wextra -Werror)" \
    cc -O2 -Wall -Wextra -Werror -Wno-unused-function $I -o build/bio_test tests/bio_test.c
run "the tests" build/bio_test
run "the tests under ASan and UBSan (a signed overflow is a bug on the FM-1 too)" sh -c \
    "cc -O1 -g -fsanitize=address,undefined -fno-sanitize-recover=undefined -Wall -Wno-unused-function $I -o build/bio_test_san tests/bio_test.c && build/bio_test_san > build/bio_test_san.log"
run "the renderer" cc -O2 -Wall -Wextra -Werror -Wno-unused-function $I -o build/bio_render tools/bio_render.c
if [ -f firmware/src/bioforge.c ]; then
    CF="-O2 -Wall -Werror -Wno-unused-function -DDINGHY_HOST -DAPP_VER=\"test\" -Ifirmware/hal -Ifirmware/src -Ifirmware/gen"
    run "bioforge.c compiles for the host without a warning (-Werror)" cc $CF -o build/host/bioforge firmware/src/bioforge.c
    if command -v clang >/dev/null 2>&1; then
        run "bioforge.c with clang (-Werror; the chip's compiler is a clang)" clang $CF -o build/host/bioforge-clang firmware/src/bioforge.c
    fi
    run "the SMK-37 build of it, for the host" cc $CF -Wno-unused-variable -DBOARD_SMK37=1 -o build/host/bioforge-smk37 firmware/src/bioforge.c
fi
run "the awakening (8 bars), dry (4), each organ alone: build/audio/*.wav" sh -c '
    build/bio_render awaken build/audio/awaken.wav 8 && build/bio_render dry build/audio/dry.wav 4 &&
    for o in 0 1 2 3 4 5 6 7 8; do build/bio_render organ $o build/audio/organ$o.wav >/dev/null || exit 1; done'
if python3 -c 'import numpy, PIL' 2>/dev/null; then
    run "their spectrograms: build/audio/*.png" sh -c '
        python3 tools/bio_look.py build/audio/awaken.wav build/audio/awaken.png 8 &&
        python3 tools/bio_look.py build/audio/dry.wav build/audio/dry.png 4 &&
        for o in 0 1 2 3; do python3 tools/bio_look.py build/audio/organ$o.wav build/audio/organ$o.png 0.6 || exit 1; done &&
        for o in 4 5 6 7 8; do python3 tools/bio_look.py build/audio/organ$o.wav build/audio/organ$o.png 2 || exit 1; done'
fi
[ "$fail" = 0 ] && echo "ALL BIOFORGE TESTS PASSED" || echo "SOME BIOFORGE TESTS FAILED"
exit "$fail"
