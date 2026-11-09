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
