# Licensing

BIOFORGE, like Dinghy under it, is free software under the GNU General Public License, version 3 only (`GPL-3.0-only`, full text in
`LICENSE`). You may use, study, change and share it. If you give someone a firmware built from it, give them its
complete source under the same licence too.

## Who made what

| | |
| --- | --- |
| The platform: `firmware/hal/`, `firmware/loader/`, `crt0.S`, `app.ld`, `firmware/src/` (`usb.c`, `ota.c`, `recovery.c`, `lcd.c`, `gfx.c`, `libc.c`, `bootguard.h`, `uac_fir.h`), `tools/` | Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments ([Felucca](https://github.com/hugelton/Felucca)), with the changes of [SLOOP](https://github.com/isod89/sloop-fm1) (isod89) |
| The SMK-37 board layer: `firmware/hal/smk_input.h`, `fm1_flash_map.h`, the SMK-37 parts of `fm1_cc.h`, `fm1_flash.h`, `fm1_lcd_hw.h`, `lcd.c`, `ota.c`, `bootguard.h`, `firmware/loader/`, `tools/fm1pkg_make.py`, `tools/fm1_install.py`, `tools/build.py`; `firmware/src/smk_wheel.h`; `tests/ota_test.c`, `ldr_test.c`, `install_test.py`, `smk_wheel_test.c`, `smk_pkg_test.py` | Copyright (C) 2026 M. Geisler (genkigenki/makkuro), from [genkigenki/sloop-fm1](https://github.com/genkigenki/sloop-fm1) branch `smk37` (the reference port of the [fm1-to-smk37](https://github.com/genkigenki/fm1-to-smk37) kit) |
| The TRS MIDI IN `firmware/src/midi_uart.c` | Felucca's, as sloopDX carries it, with the ring read by content from Felucca [Salt] 0.9-salt14 by ChanceTheMaker (found by keremimo) |
| The update loader's screen: `firmware/loader/ldr_screen.c`, `tests/ldr_screen_test.c` | Copyright (C) 2026 zvenson (from OberVave, the Firmware Hub's look); the idea, a loader that draws, its 5 x 7 letters and the order of things from [Skiff](https://github.com/illuminus5/skiff) by illuminus5, GPL-3.0 |
| From OberVave: the storage (blobs, slots), `LCD_IDLE`, the SMK-37 wiring of `platform.c`, `store.h`, the pad event queue in `smk_input.h`, `tests/smk_flash_test.c`, `tests/run_tests.sh`, the CI workflow | Copyright (C) 2026 zvenson |
| The wiring `platform.c`, the host mode `tests/host.c`, Dinghy's docs (`docs/`) | zvenson |
| The browser installer's protocol: `web/fm1pkg.js`, `web/fm1ota.js` (unchanged, from [SLOOP](https://github.com/isod89/sloop-fm1) `bac9c05`); the install flow of `web/index.html` and the shape of `web/make_site.py`, after SLOOP's `web/index_pkg.html` and `web/make_site.py` | Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments (Felucca), with the changes of SLOOP (isod89) |
| The import of Dinghy 0.5 as it is carried here (`tools/build.py`'s and the test runners' names, `tools/emu_ci.sh`, the CI workflow's shape) | Copyright (C) 2026 Joakim (joakimskurk), from FM-1 Clock and ORBITRIX |
| BIOFORGE: `firmware/src/bioforge.c` (in the place of Dinghy's `dinghy.c`), `bio_dsp.h`, `bio_organs.h`, `bio_engine.h`, `bio_turing.h`, `bio_seq.h`, `bio_ui.h`, `firmware/gen/bio_*.h`, `tools/gen_tables.py`, `gen_art.py`, `bio_render.c`, `bio_perf.c`, `bio_look.py`, `emu_check.sh`, `tests/bio_test.c`, `bio_emu.py`, `run_bio.sh`, `tests/web/app_test.mjs`, `tests/scripts/` (BIOFORGE's), `tools/make_demos.sh`, the page `web/index.html` and `web/make_site.py` (but for what is SLOOP's in them, above), BIOFORGE in the browser `web/app/` (after ORBITRIX's and FM-1 Clock's, its `web_host.c` after Dinghy's `tests/host.c`), `docs/bioforge/`, `docs/screens/`, `README.md`, `PLATFORM.md`, `CLAUDE.md` | Copyright (C) 2026 Joakim (joakimskurk) |
| The changes to Dinghy's own files, listed in PLATFORM.md: each of those files says so at its top ("Changed for BIOFORGE") | Copyright (C) 2026 Joakim (joakimskurk) |

## Third-party material

| What | Licence | Where |
| --- | --- | --- |
| Terminus font 8x16 (ter-u16n), rendered into `firmware/gen/felucca_font.h` | SIL OFL 1.1 | `assets/fonts/Terminus-LICENSE.txt` |
| JieLi AC79 SDK: `uboot.boot`, `cfg_tool.bin`, `eq_cfg_hw.bin` are read from your SDK checkout at build time and placed in the package; no SDK files are in this tree | Apache-2.0 | <https://gitee.com/Jieli-Tech/fw-AC79_AIoT_SDK> |

## The idea, the pictures and the Turing register

BIOFORGE comes from its author's idea document (*BIOFORGE idédokument v3.0*, not in this repository). Its numbers
(the organs' frequencies, decays and levels, the start scene) are the engine's defaults.

The pictures (`firmware/gen/bio_art.h`) are drawn by `tools/gen_art.py` from shapes and numbers; no image of anyone
else's is in them. The 5 x 7 font in it is written out in that tool.

The nervous system (`firmware/src/bio_turing.h`) follows the documented behaviour of the Music Thing Modular Turing
Machine by Tom Whitwell (https://www.musicthing.co.uk/Turing-Machine/): a shift register whose bit comes round,
flipped by chance or not, its LOOP, LENGTH and WRITE. It is an implementation of that idea of its own; no code or
hardware file of it is copied (the hardware is CC BY-SA, the Workshop Computer program GPL-3.0).

## The name

"BIOFORGE" is the name of this firmware, and it is its author's (Joakim, joakimskurk). The GPL covers the code, not
the name: under section 7(e) of the GPL, this licence gives no right to use "BIOFORGE", or a name that can be taken
for it, as the name of another work. A changed version, a fork that is passed on, or a firmware built on this one
takes a name of its own, and does not call itself BIOFORGE on its screens, in its package or on its page. Saying where
it comes from ("based on BIOFORGE by joakimskurk") is welcome, and is what the notices above ask for anyway. A copy
passed on unchanged keeps the name.

## Trademarks

"Felucca" and "Hügelton Instruments" are names of Hügelton Instruments; "M-VAVE", "FM-1", "SMK-37" and "MKE-P37"
belong to their owners. Dinghy and BIOFORGE are not made or endorsed by them.
