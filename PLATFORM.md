# PLATFORM

BIOFORGE runs on **Dinghy 0.5**, imported as source, as FM-1 Clock carries it (joakimskurk/fm1clock `7a20f13`).

- upstream: https://github.com/zvenson/dinghy
- commit: `ea1b6bf` (2026-11, after "Dinghy 0.5" `1675934`; the release workflow's file pattern only)
- Dinghy's docs stay in `docs/` (`API.md`, `RULES.md`, `MAKE-IT-YOURS.md`, `SMK37.md`); BIOFORGE's own are in
  `docs/bioforge/`. Dinghy's README is `docs/DINGHY_README.md` (the toolchain, the SDK, the host mode).

## What differs from upstream

Kept small, so a later Dinghy can be merged by diffing these files.

| file | change |
| --- | --- |
| `firmware/src/dinghy.c` | replaced by `bioforge.c`: the app; `APP_NAME "BIOFORGE"`, `STORE_MAGIC` "BIOF", `CV_MAX` 240 x 134; with it `bio_*.h` and `firmware/gen/bio_*.h` |
| `firmware/src/store.h` | the default `STORE_MAGIC` "BIOF" |
| `firmware/src/gfx.c` | the canvas outside `.pool` on the macOS host (Mach-O has no such section), as ORBITRIX has it |
| `tools/build.py` | compiles `bioforge.c`; outputs `bioforge.*`, `bioforge.syms` (the symbol table: `tools/emu_check.sh` reads `isr_load` by its address) and `bioforge.notes`; identity `FM-1_908XY` (X.Y the release, 90800 a dev build; 908 was free in the Firmware Hub's catalogue on 2026-11-09); loader screen "BIOFORGE" in poison green `0x8FE5`; data family `BIOFRG` |
| `build.sh` | a comment |
| `tests/host.c` | `APP_NAME` and the output's name |
| `tests/run_host.sh`, `tests/run_tests.sh`, `tests/install_test.py` | `bioforge.*` names; the SMK-37 packages' identities `_908XY` |
| `README.md` | moved to `docs/DINGHY_README.md`; BIOFORGE's README in its place |
| `.github/workflows/release.yml` | replaced by `build.yml` (zvenson/fm1-build, as FM-1 Clock, VaveHero, RoboSeq and SkurkyMello), with a job for the site (GitHub Pages) |
| `web/` (Dinghy has none) | the site: `fm1pkg.js` and `fm1ota.js` from SLOOP (`bac9c05`, unchanged: the update protocol over Web MIDI; Dinghy's `tests/install_test.py` checks `fm1pkg.js` against `tools/fm1pkg_make.py`), BIOFORGE's page, `make_site.py`, and `web/app/` (the firmware in the browser) |

`platform.c` has one change from FM-1 Clock: with `DINGHY_WEB` it includes `web/app/web_host.c` (the page's FM-1)
instead of `tests/host.c`.

### The second core (D38, 2026-11-20)

The render runs on both of the FM-1's cores. The second core's code is SLOOP's (`bac9c05`, 2.8.5: X0X's HAL by Charles
Vestal, after Melodee by Kerem Kilic, with X0X's hardening for units whose core 1 misreads, brought into SLOOP by Andre
Rezemini and isod89), all GPL-3.0. This is the one departure in Dinghy's own files, because the task needs it:

| file | change |
| --- | --- |
| `firmware/hal/fm1_cpu1.h`, `fm1_cpu1.S` | new, SLOOP's, unchanged: the start (through the chip's ROM, the PC limits open), the mailbox on cache lines of its own, a check word on every job, the waiting loop in RAM, the 8 ms timeout, one core until power-off after a failure (`.noinit.c1bar`) |
| `firmware/hal/fm1_sys.h` | SLOOP's additions: the supplies raised to M-VAVE's own levels at boot (VDC14 1.60 V, SYSVDD 1.38 V; X0X 1.0.9: at the boot loader's, with both cores running, some units read the flash and the SRAM wrong), `fm1_cpu1_halt` (core 1 held with the bus traps off, Felucca 1.6.1), core 1 held before a reboot, a core reset and the update loader |
| `firmware/hal/fm1_guard.h` | `fm1_guard_pc_open` (the PC limits open while core 1 starts) |
| `firmware/hal/fm1_irq.h` | a fault on core 1 writes the record (EMU = `000000C1`) and stops there; core 0 gives it up |
| `firmware/app.ld` | `.c1_text` after `.ram_text` (inside the guard's RAM-code window: `_rt_end = _c1_end`), `.noinit.c1bar` after the other `.noinit` objects (none of them moves) |
| `tools/build.py` | assembles `fm1_cpu1.S`, places `.c1_text` in the image, reports it (an error if it is missing) |
| `firmware/src/platform.c` | in `fm1_cstart`: core 1 held first, the supplies raised, its loop copied, then started and proved by a self-test job before the audio and the timers (`core1_boot`, after SLOOP's `cpu1.c`); `core1_run`, `core1_wait`, `core1_state` for the app (the host has none); a crash beside core 1 bars it |

`firmware/loader/` is not changed (it includes `fm1_sys.h`, whose additions it does not call but for the hold in
`fm1_core_reset`, as in SLOOP).

## Updating Dinghy

1. Diff the new upstream against the commit above for the imported paths.
2. Apply upstream changes; re-apply the table above.
3. `make test`, `tests/run_tests.sh`, CI's build, a boot in fm1-emu.
4. Update the commit here.
