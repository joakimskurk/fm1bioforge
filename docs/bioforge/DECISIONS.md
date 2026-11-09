# BIOFORGE: decisions

What was decided, and why, newest last. Add to it, dated.

**D1 (2026-11-09) The idea document is the brief, not the specification.** The user: "the document is the idea; the
design and the content are to be fitted to the FM-1 and to what is possible". Its numbers (the organs' frequencies,
decays, the INIT values, the Turing's mapping) are kept as defaults wherever the chip allows them; its structure (five
pages, the gestures, the data model) is fitted to 27 keys, 14 buttons, 7 encoders and one core without an FPU.

**D2 (2026-11-09) A drum organ is one body that keeps ringing.** Each of HEART, BONE, TEETH and CLAW is one set of
resonators that every hit excites again, instead of a pool of voices. A struck object behaves like that (a fast roll
builds up, it does not restart), it costs the same however many hits, and nothing is ever stolen.

**D3 (2026-11-09) Modes are complex phasors.** `z = r·e^{iω}·z + x`, the output Im z: stable at 46 Hz and at 9 kHz in
32-bit integers, its frequency can bend mid-ring without a click (BONE's bounce, TEETH's spread), and its level is
normalised by (1 − r) so that DECAY does not change the loudness.

**D4 (2026-11-09) One modifier, and "hold a page button".** The document lists nine combinations and asks for fewer.
SEL (the SELECT encoder's push) is SHIFT. Holding SEQ, LFO or ARP turns the white keys into that page's buttons
(steps, Turing actions, mutations); letting go gives them back to playing.

**D5 (2026-11-09) ALGORITHM is the Turing's LOOP on every page.** The document wants one big knob for chaos-to-lock,
always at hand while playing. ALGORITHM is the FM-1's big encoder.

**D6 (2026-11-09) Four scenes, two slots each.** A scene (nine tracks, sounds, Turing, mix, FX) is ~13 KB; the store has
eight 16 KB slots. Each scene gets two (A/B, generation number, CRC), so a save cut off by a power cut leaves the last
good one, as the document asks. All four scenes stay in RAM, so a scene change while playing is instant and never
touches the flash.

**D7 (2026-11-09) Saving waits for silence.** An erase holds the interrupts off for ~50 ms a sector. SAVE while playing
is kept until STOP; the screen says UNSAVED in red until it is written.

**D8 (2026-11-09) Per-track length.** The document's VOICE phrase comes "once every fourth bar" and NOISE breathes over
four bars: a track has its own length (1–64), the playhead of each the transport's step modulo it.

**D9 (2026-11-09) Quantise at playback.** The document: raw timestamps first, then non-destructive quantise. A live note
keeps the tick it came on; the track's quantise (grid and strength) places it when it plays.

**D10 (2026-11-09) No chorus or gate in v0.1.** The document puts the CPU on TISSUE and the Turing first. Delay, reverb
and crush are in; PARASITE CHORUS and NECRO GATE wait for the measured budget on the chip.

**D11 (2026-11-09) Package identity FM-1_908XY.** 908 was free in the Firmware Hub's catalogue on 2026-11-09 (as
ORBITRIX and Star Distroyer use 967XY and 962XY); 90800 is a dev build.

**D12 (2026-11-09) The drums ring cheaply.** Measured on the host the first engine took 3 140 instructions a frame
for the awakening: too close to the chip's limit. Once a drum organ's excitation is over (the clicks played, the
friction and the vent under -66 dB), only its modes and its pitch body run; it sleeps at -72 dB, not -90. With the
sends summed per channel and the rib asleep when nothing strikes it: 2 400. Over 75 % of the interrupt, the drums out
of focus ring on two modes (the document's rule: the transients and the clock stay).

**D13 (2026-11-09) Four scenes from the document's situations.** RITUAL 00 THE AWAKENING is the document's start scene.
01 INFECTION is its SLOW INFECTION (LOOP 80 %, DEPTH 30 %, RATTLE on). 02 MARROW is slower and swung, the bass and
SPASM through the crusher, WAIL on SPASM. 03 EMPTY BODY is blank, for your own.

**D14 (2026-11-09) The look at 240 x 240.** The art is drawn by `tools/gen_art.py` from shapes: dark tissue with veins,
bone frames with vertebra notches, eye knobs (a slit pupil, a green arc for the value), membrane cells that light as a
green star, a spine of vertebrae for the register. Labels are a 5 x 7 pixel font of its own (40 characters a line);
values FONT_S and FONT_L. SEQ is the first page: the pattern is what tells most at a glance.

**D15 (2026-11-09, replaced by D17) No page in the browser yet.** The sister firmwares have one (the firmware compiled to WebAssembly,
an installer); BIOFORGE's comes after the sound has been heard on an FM-1.

**D16 (2026-11-09) The document's pictures are inspiration, not a design to match.** The user: the designs in the
pictures will not necessarily match what BIOFORGE ends up with; they are only the inspiration. What is kept from them
is the mood and the colour rules (§1 of the plan: almost black, bone, flesh, a little green; green is sound, red is
recording). The layout, the controls on each page and how much is on a screen follow the FM-1 and what reads at
240 x 240, and can change freely.

**D17 (2026-11-09) The page: demos and the firmware to try.** The user asked for a page with demos and a firmware to
test, like their other firmwares' pages. `web/` has BIOFORGE compiled to WebAssembly on a web FM-1 (after ORBITRIX's:
the buttons can be held, so SEQ, LFO, ARP and HOME work as on the panel; the scenes' slots are kept in the browser),
seven demos that are the firmware itself played by scripts (`tests/scripts/demo_*.txt`, rendered by
`tools/make_demos.sh`, never edited after), each with its spectrum, and the Web MIDI installer with the package CI
built and tested. CI's site job builds it on `main`, checks the WebAssembly in Node first, and publishes it once
GitHub Pages is switched on.
