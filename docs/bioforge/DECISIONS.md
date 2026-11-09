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

**D13 (2026-11-09, replaced by D19) Four scenes from the document's situations.** RITUAL 00 THE AWAKENING is the document's start scene.
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
seven (now eight, D19) demos that are the firmware itself played by scripts (`tests/scripts/demo_*.txt`, rendered by
`tools/make_demos.sh`, never edited after), each with its spectrum, and the Web MIDI installer with the package CI
built and tested. CI's site job builds it on `main`, checks the WebAssembly in Node first, and publishes it once
GitHub Pages is switched on.

**D18 (2026-11-09) Noise as percussion, and a more modern sound.** The user heard the page: the demos and the sounds
were all the same, it sounded like a 1996 horror game; fine as a start, but they had hoped for something more modern
in the noise: noise as percussion, and a much more versatile expression, with sound examples along the way. So:
- NOISE is now a percussion voice (`bio_perc.h`) instead of a breath pad: a source of five kinds, grain, a morphing
  resonant filter that keeps its loudness wherever it is put (a low-pass gains half an octave for each octave its
  cutoff is under the source's top, a band-pass loses half of Q's), a sweep, bursts, a click, a struck or held
  envelope, drive (at 0 dB still a soft knee), a crusher, width. About 110 instructions a sample while it sounds.
- Any drum organ can become one (MODEL, D8), so the kit can be four noise sounds, or mixed with TISSUE. Sixteen
  sounds to start from (`bio_kits.h`), on the SOUND knob; TISSUE gives a drum its own body back.
- PUMP: the kick ducks the rest, the most recognisable modern mix move, one knob.
- The old NOISE (breath, scrape, steam) is gone: the perc voice held with BREATH does what it did.
- The scene's format is "BOF2" (the mix's PUMP): scenes saved by an earlier build are not read; the factory ones load.

**D19 (2026-11-09, the start scenes changed by D20) Four scenes, four worlds; factory rituals on SHIFT + PRESETS.** INFECTION and MARROW were the
awakening with other knobs, which is why everything sounded the same. The four scenes at the start are now RITUAL 00
THE AWAKENING (the document's), 01 STATIC CULT (132 BPM, noise as percussion, PUMP), 02 SPLICE (100 BPM, broken: 32
steps, ratchets, chances, a 12-step hat against it, crushed, WAIL on both), 03 VOID (70 BPM, slow and wide: held
washes, crackle, the drone under a huge HEART, a long reverb). `bio_rituals.h` builds them from step strings; SHIFT +
PRESETS loads any of six (those, INFECTION, EMPTY BODY) into the scene that plays, the one in flash kept until SAVE.
The demos follow: one each for the four, one turning TEETH's SOUND through the kit, the organ tour, SLOW INFECTION,
and FROM NOTHING TO A LOOP, now with BONE turned into a CLAP and TEETH a TICK on the way.

**D20 (2026-11-09) Rhythm made of noise: the noise core.** The user, on D18's sounds: they had imagined rhythmic noise,
not distorted drums. Four sketches were made to choose by ear (outside the firmware, in floating point, with nothing
an FM-1 could not do): A slices of noise locked into a groove, B one noise cut by a gate, C a wall of noise through a
band on the note, D clicks, bursts, digital noise and stutters. The user: much closer to what they had in mind, and it
should shape the synth; all four in the core, and everything BIOFORGE has kept, the noise added to it (TISSUE and the
tonal organs stay). So the perc voice became the noise core:
- **Locked noise.** The sequencer gives each step of a noise organ its own seed and keeps it: at LOCK the same noise
  comes back pass after pass and is the groove (A). LOOP lets a step's noise slip as often as a bit of the register
  would flip; DOUBLE LOCK swaps it for its inverse each pass. Its own draws, so the register runs as it did. The same
  start, the same noise (cleared at PLAY and at a new scene).
- **SCATTER** (E1): each step its own slice, from its locked seed: the band (+-3 octaves), resonance, length (down to a
  quarter), shape (a chop, a strike, a swell), level, place (to hard left or right), now and then grain.
- **TONE** (E2): the band on the note (BODY its octave), a second band a fifth over it (C).
- **DRIFT** (E3): the band over four bars, with the breath (B).
- A noise step's ratchet repeats its own noise: a stutter, with no buffer (D). Velocity opens the band (an accent half
  an octave).
- Parameters 20 -> 24 (E1..E4), the scene format "BOF3". On SYN, SHIFT shows SCATTER TONE DRIFT on a noise organ.
- The four scenes at the start are now RITUAL 00 THE AWAKENING (the document's), 01 FROZEN (A), 02 STREAM (B), 03 WALL
  (C); DATA (D), STATIC CULT, SPLICE, VOID, INFECTION and EMPTY BODY are on SHIFT + PRESETS. FROZEN, STREAM, WALL and
  DATA are the sketches rebuilt in the engine; each costs under 1 500 instructions a frame (the awakening 2 566).

**D21 (2026-11-09) Organic crackle, and landscapes for the synths and the rooms.** The user: pretty good, but more
versatility; some crackle, organic; and the synths and the whole sound need more sonic landscapes. Asked which: the
crackle of a fire, of frying, of wood creaking; the worlds of industry, the cosmic / ambient, the body and its fluids
(not nature).
- **Crackle** as noise-core sources, now eight (SOURCE in eighths): WHITE PINK DIGI METAL, then DUST (the old sparse
  crackle), FIRE (pops whose chance rises with a wandering heat and after each pop, so they come in clusters; most
  small, a few big, each a snap of noise; a smoulder under them), FRY (the same, dense and quick, over a sizzle), CREAK
  (stick-slip: a train of slips whose rate glides and is never a clock, through the voice's filter as a body; it creaks,
  then rests). TEXTURE is their density. Six sounds in the kit: EMBERS, SPARKS, SIZZLE, GREASE, CREAK, HINGE. As any
  noise step, crackle locks: the same sparks every bar.
- **Landscapes** (`bio_land.h`): a synth's MODEL (E1; SYN, SHIFT, KNOB 1) makes BASS, DRONE, SPASM or VOICE one of
  nine voices instead of its own: HUM (a pulse and its octave through a fluttering band, sparks), STEEL (a bowed plate,
  four modes), PISTON (a machine that strokes on the beat while the key is held: a thump, a clank, steam), GLASS
  (inharmonic partials beating), BELL (FM, its index falling), CHOIR (three throats through formants), BLOOD (a pulse
  on the beat, lub and dub, and the rush), BUBBLE (rising drops), SLIME (a wet squelching low-pass). The organ keeps its
  envelope and role; the four macros mean what each model says. Any synth any model: 36 voices from four organs.
- **SPACE** (FX, a third page): the reverb as HALL (as it was), ROOM, CAVE (bigger, darker, later), METAL (lines so
  short their echoes are a pitch: a tank) or VAST (the biggest, longest, open).
- Rituals EMBERS (crackle), FOUNDRY (industry), ORBIT (cosmic), INNARDS (the body), on SHIFT + PRESETS; demos of each.
  Scene format "BOF4". The awakening costs 2 647 instructions a frame (2 566 before), the landscapes 1 700 .. 2 000.

**D22 (2026-11-09) Dark ambient meets noise; a deep kick; clean, not lofi.** The user: not there yet. They want grim
dark ambient meeting a bit more noise, a deep techno kick with almost no top, noise and crackle in time but with a
musical, rhythmic meaning, atmosphere; and it sounded very lofi and choppy. What was found and done:
- **Lofi.** MARROW CRUSH was on in every scene by default (its return at 40 %, BASS, SPASM and NOISE sent to it): off
  now, sends 0; SPLICE keeps it on purpose. The landscapes' saws and pulses were naive (aliasing): polyBLEP now
  (`bio_saw`, `bio_pulse`). The half-rate reverb took the full band in (folding the noise's highs down) and gave images
  out: a low-pass before (7 kHz) and after (9 kHz). A landscape no longer resets its phases and filters when a note comes
  over a sounding one (a click each note).
- **MODEL KICK** (`bio_kick.h`) on a drum organ (D8: TISSUE under 33, NOISE to 65, KICK from 66): a sine swept down to
  its pitch, held, a long decay, drive, then two low-pass poles (TONE): almost nothing over 1 kHz (a test). Its phase
  runs on through a new hit and its attack is a ramp: no click. DEEP KICK, SUB KICK, DUB KICK in the kit.
- **RUMBLE** (MIX's sends, SHIFT, KNOB 3): HEART into a low tail of its own (two lines at an eighth of the rate, feeding
  each other through low-passes, about 2 s), ducked by PUMP. **PUMP** now ducks the delay's and the reverb's returns too,
  so the whole space breathes with the kick.
- **DUB** (a landscape model): a minor chord (minor, 7th, 9th, fifths) of four saws through a low-pass that the note
  opens and DECAY closes, for the delay to carry.
- **ABYSS** (122 BPM) and **ASH** (116 BPM): the direction, as rituals; the start scenes are THE AWAKENING, ABYSS, ASH,
  FROZEN, the rest on SHIFT + PRESETS. Scene format "BOF5".

**D23 (2026-11-09) A reference, measured; the drone as the body.** The user gave a 30-minute recording, "Grim Dark
Industrial Ambience", for its feel, to mix with melody and a little more rhythm. Nobody here can listen, so it was
measured (`tools/bio_feel.py`: third-octave spectrum, band energies, width, flatness, crest, a pulse's
clarity, persistent pitches): very dark (its centroid about 160 Hz; from 1 kHz to 13 kHz it falls by about 30 dB), a
full body from 100 Hz to 2 kHz, a drone on E flat 1 with its harmonics and a cluster of semitones (D, E flat, E) over
it, nearly all tone (flatness 0.001), wide (the side 8 dB under the middle), dense (RMS -12 dBFS), no pulse to speak of,
layers fading in and out over minutes. Against it ABYSS had a hole from 100 Hz to 1.6 kHz, a flat floor of hiss over
2 kHz and twice the width: the kick and the noise were doing everything, the body was missing. So:
- **MASS** and **CLUSTER** (landscape models): three band-limited saws a few cents apart (the note, its fifth and
  octave, a sine under; or the note and the two semitones over it), a two-pole low-pass breathing with the breath,
  grit (a slow noise roughening it), and a side of their own (the upper voices apart, the root in the middle).
- **DARK** (MIX's sends, SHIFT, KNOB 4): the master's two-pole low-pass, 18 kHz down to 1.2 kHz.
- The limiter now cuts for the whole sub-block at once (it was ramping down and the start of a loud one clipped).
- **RUST** (108 BPM): the drone body, a smouldering rumble and a wash of noise, a sub kick and its rumble, a far bone,
  dark noise slices locked in 16ths, and a bell the register plays from the scale (WAIL). Measured the same way it is
  within a few dB of the reference in every band to 5 kHz (centroid 150 Hz, side -10 dB), with the pulse and the melody
  the user asked for. The start scenes: THE AWAKENING, RUST, ABYSS, ASH. Scene format "BOF6".
