# Working on BIOFORGE

- Bare-metal FM-1 firmware on Dinghy 0.5. The brief is the user's idea document (BIOFORGE idédokument v3.0, not in the
  repository); the plan fitted to the FM-1 is `docs/bioforge/PLAN.md`, and what was decided, and why, is
  `docs/bioforge/DECISIONS.md` (add to it, dated). The user: the document is the idea; design and content are fitted
  to the FM-1 and to what is possible.
- The code: `firmware/src/bio_engine.h` (the organs, the rib, the FX: no platform, shared with the host tools),
  `bio_turing.h` (the register), `bio_seq.h` (scenes, tracks, events, recording, undo), `bio_ui.h` (the screens),
  `bioforge.c` (the app: the panel, MIDI, saving). `firmware/gen/bio_art.h` is made by `tools/gen_art.py`: never edit
  it.
- After an engine or sequencer change: `tests/run_bio.sh` (the tests, again under ASan/UBSan, the renders in
  `build/audio/`). After an app change: `tests/run_host.sh tests/scripts/<script>.txt` and look at the screens. Then
  `./build.sh` (stops at a `FAIL` line: never read only its `ok` lines) and `tests/run_tests.sh`.
- Audio code: no float, no 64-bit division, no flash access in the render, no loop whose length a parameter sets beyond
  its fixed maximum, fixed point in range (UBSan finds a signed overflow, and it is a bug on the FM-1 too). In a
  feedback loop round toward zero. A constant that looks like a hardware address (0x02000000, 0x1Exxxxx ...) fails the
  build's register check: write it as a shift.
- The budget (docs/RULES.md): the audio ISR under half of its 5.8 ms.
- Only edit Dinghy's own files (`platform.c`, `gfx.c`, `hal/`, `loader/`, `tools/build.py`) when the task needs it, and
  list the departure in PLATFORM.md.
- Dinghy, Felucca, SLOOP and the SMK-37 layer stay credited: that is the code's licence. The Turing register follows
  the Music Thing Modular Turing Machine's documented behaviour (credited in the README); no code of it is copied.
- Never install on an FM-1, push, or tag a release without asking.
