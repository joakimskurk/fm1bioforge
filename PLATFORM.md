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
| `.github/workflows/release.yml` | replaced by `build.yml` (zvenson/fm1-build, as FM-1 Clock, VaveHero, RoboSeq and SkurkyMello) |

Nothing in `firmware/hal/`, `firmware/loader/` or `firmware/app.ld` is changed. `platform.c` has one change, from FM-1
Clock: with `DINGHY_WEB` it includes `web/app/web_host.c` (a page's FM-1) instead of `tests/host.c`; BIOFORGE has
no page yet.

## Updating Dinghy

1. Diff the new upstream against the commit above for the imported paths.
2. Apply upstream changes; re-apply the table above.
3. `make test`, `tests/run_tests.sh`, CI's build, a boot in fm1-emu.
4. Update the commit here.
