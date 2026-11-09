# BIOFORGE on the FM-1: the plan

**Status:** v0.1 plan, 2026-11-09. Platform: Dinghy 0.5 (`PLATFORM.md`, `docs/API.md`, `docs/RULES.md`).
**The brief:** the user's idea document *BIOFORGE, idédokument v3.0* (9 November 2026): a biomechanical rhythm machine
and synth that should sound like its own disturbing machinery. The user: *the document is the idea; the design and the
content are to be fitted to the FM-1 and to what is possible.* This plan is that fitting. Where it departs from the
document, `DECISIONS.md` says why.

## 1. What stays from the document

- **Three ground rules.** The dry sound looks the way it sounds; the keys go straight into the pattern memory; the
  Turing register is a playable nervous system that locks and evolves sound and rhythm, not a surprise button.
- **Six channels, nine organs.** DRUM (HEART, BONE, TEETH, CLAW), BASS, DRONE, SPASM, VOICE, NOISE.
- **TISSUE.** Every drum organ is an excitation (PRESSURE, FRACTURE, FRICTION, VENT) through damped modal resonators,
  an asymmetric flesh curve and a DC block. No sample, no oscillator kick with distortion.
- **Four macros per organ.** BODY, TEXTURE, MOTION, DECAY on the first page; the depth behind them.
- **Keyboard into memory.** LIVE REC while it plays, STEP REC while it stands, PARAM REC (p-locks), OVERDUB, REPLACE,
  ERASE, UNDO and REDO, non-destructive quantise.
- **Turing.** A 16-bit shift register clocked from the transport, LOOP from inverted lock through random to lock,
  LENGTH, WRITE 0/1, RESET, and three uses of the one register: RATTLE (ghost gates), ROT (CV to the texture), WAIL
  (notes from a pool). It never deletes or moves what the user played.
- **The awakening.** RITUAL 00 at power-on: 120 BPM, D Phrygian, the document's grid, the Turing in an eight-step
  locked texture loop.
- **The look.** Almost black, bone, oxidised metal, burgundy flesh, a little poison-green light. Green is active
  sound, red is recording or mutation, bone and grey are structure. Big fields, little text, one level at a time.

## 2. The FM-1 as BIOFORGE's body

27 keys (F3..G5, no velocity), 14 buttons, 7 encoders, the MASTER knob, LEDs under every key and button, a 240 x 240
screen. One AC791N core, no FPU: integers only; the audio interrupt gets 256 frames at 44.1 kHz every 5.8 ms.

### 2.1 The panel

| Control | BIOFORGE |
| --- | --- |
| **SELECT** | the focus: DRUM (the kit), HEART, BONE, TEETH, CLAW, BASS, DRONE, SPASM, VOICE, NOISE |
| **SEL** (SELECT pushed) | SHIFT, held; a tap keeps the knobs' second row |
| **ALGORITHM** | the Turing's LOOP, on every page: the one big knob from chaos to lock |
| **PRESETS** | the scene, RITUAL 00..03: THE AWAKENING, ABYSS, ASH, FROZEN (playing: from the next bar); SHIFT: a factory ritual into it (D19, D20, D22) |
| **KNOB 1..4** | the page's four parameters (SHIFT: the second row) |
| **HOME** | MIX: the six channels (again: the focused channel's sends) |
| **SEQ** | SEQ: the grid. **Held:** the white keys are the 16 steps of the focused track |
| **EDIT** | SYN: BODY TEXTURE MOTION DECAY (again: the organ's own depth; again: tune, level, pan) |
| **ENV** | the organ's envelope and level page |
| **LFO** | MUT: the Turing spine. **Held:** the white keys are WRITE 0, WRITE 1, RESET, FUSE |
| **FX** | FX: delay, reverb, crush (again: their times and colours) |
| **GLO** | GLOBAL: tempo, swing, quantise, record mode, count-in |
| **ARP** | **Held:** BURST, the performance mutations on the white keys (STUTTER, INVERT, FREEZE, SPASM) |
| **SAVE** | the scene to flash (playing: when it stops) |
| **PLAY** | start, stop. SHIFT + PLAY: PANIC (every voice off, the memory kept) |
| **REC** | playing: LIVE REC on the focused track, on and off. Stopped: STEP REC. SHIFT + REC: OVERDUB / REPLACE |
| **OCT− / OCT+** | the keys' octave (SEQ held: the page of 16 steps). SHIFT + OCT−: UNDO, SHIFT + OCT+: REDO |
| **MASTER** | the volume |

Fewer hidden combinations than the document's list: one modifier (SHIFT), and "hold a page button: the keys become
that page's buttons", the same rule on SEQ, LFO and ARP.

### 2.2 The keys

- **Focus DRUM:** the kit, in four zones of about seven keys: HEART F3–B3, BONE C4–F#4, TEETH G4–C#5, CLAW D5–G5. In a
  zone the middle key is the organ as it is set; the others tune its body up and down. TEETH: the lower keys closed
  (a short tooth run), the upper ones open (a long one). CLAW: the higher the key, the longer the drag. Each hit goes
  to its own organ's track. The zone centres glow.
- **Focus on one organ:** the keys play it chromatically (drums: their bodies tuned ±12 semitones about F4).
- **BASS, DRONE, SPASM, VOICE** are monophonic, last note wins, legato glides (BASS) and ties (DRONE): a note that
  goes on where the same note ends is not struck again.
- No velocity on the FM-1's keys: 100, or MIDI's, or the SMK-37's.

## 3. The sound

All in `firmware/src/bio_engine.h`, integers only, shared with the host tools.

### 3.1 TISSUE, the drums

Each organ is one body that keeps ringing: a new hit excites the same resonators (as a struck object does), so there
is no voice stealing and the cost is fixed.

```
trigger -> impact envelope -> PRESSURE  (asymmetric impulse + a pitch-swept body sine)
                           -> FRACTURE  (1-9 seeded micro-impulses, dispersed 2-65 ms)
                           -> FRICTION  (coloured noise -> moving band-pass, AM)
                           -> VENT      (high-passed noise, open/close envelope)
        excite = gP·pressure + gF·friction + gC·crackle + gV·vent
        -> 3-4 damped modal resonators (complex phasors, level normalised to their decay)
        -> body = Σ modes + sub;  y = DCblock(flesh(drive·(body + dry friction)))·amp
        -> DRUM bus (+ the shared rib: 8 % of each excitation into three resonators, ≤ -20 dB)
```

The document's numbers are the organs' defaults (`BIO_ORGAN[]`):

| Organ | PRESSURE | FRACTURE | FRICTION | VENT | Modes (Hz / ms) |
| --- | --- | --- | --- | --- | --- |
| **HEART** hydraulic pump | 142→46 Hz, τ 39 ms, tail 390 ms, 52 % | 1.2 ms slap + 2 clicks at 11–25 ms, 12 % | 85–470 Hz, 55 ms, 17 % | 3.0 kHz at 17 ms, 7 % | 93/170, 171/110, 286/64 |
| **BONE** fracture | small | 4–7 over 2–33 ms | pink 850–3600 Hz, cut by the clicks | little | 192/210, 293/124, 671/82, 1510/46, −2 % bend in 30 ms |
| **TEETH** ratchet | little | 3–8 tooth clicks over 15–75 ms, one rotation speed | ring-modulated by the body | 4.2–9 kHz | 760/34, 1078/55, 1624/24, 2670/90 |
| **CLAW** scraper | little | 2–6 micro-breaks, ±5 dB on the friction | 820→2900→400 Hz over the gesture | little | 215/180, 348/125, 580/300 |

CLAW's hit is a gesture: ATTACK 4–14 ms, DRAG 45–300 ms (the note's length and the key), RELEASE 18–150 ms.

**The macros** (0–100): **BODY** the modes' share, the sub, the material's mass (the modes a little lower and
longer); **TEXTURE** friction, fracture and vent against pressure, how many clicks; **MOTION** the modes' drift in a
hit, the vent's movement, how much the Turing's ROT moves this organ; **DECAY** all decays × 2^((D − 50) / 25).
INIT: HEART 76/48/18/46, BONE 48/74/26/42, TEETH 31/69/39/29, CLAW 61/82/52/58.

**Determinism.** Every hit's noise and clicks come from a seed made of the scene's seed, the organ, the bar and the
step. The same scene, the same start, the same knobs: the same sound, sample for sample (the tests check it).

### 3.2 The tonal organs

| Organ | Voice | INIT |
| --- | --- | --- |
| **BASS** muscle | OSC A sine↔triangle, OSC B at 2:1 phase-modulating A, an octave-under, 2-pole low-pass with its envelope, asymmetric drive, mono legato, glide | D1, morph 0.60, PM 0.55, 320 Hz, Q 0.8, A 4 ms D 210 ms S 62 % R 120 ms, glide 28 ms, drive 4 dB |
| **DRONE** womb | OSC A triangle/saw, OSC B sine at 1.012 (beating), light PM, a resonant band (the second waits for the budget), low-pass breathing over an 8-bar BREATH, ducked by HEART | D2, morph 1.25, PM 0.18, 610 Hz, Q 1.5, duck −1.5 dB / 140 ms, A 1.4 s R 2.8 s |
| **SPASM** nerve discharge | OSC A sine, OSC B at 1.414, a PM strike that falls fast, a band-pass "nerve node", ratchet 1–4 | G3, PM 2.4 falling in 75 ms, amp 160 ms, 1.8 kHz, Q 3.2 |
| **VOICE** artificial throat | a glottal pulse with a subharmonic, breath noise, three formants moving U O A E I (MOUTH) | A2, MOUTH 0.7, breath 18 %, shift −3 semitones, A 230 ms R 1.1 s |
| **NOISE** noise as percussion (D18) | the perc voice (`bio_perc.h`): a source (WHITE, PINK, DIGI and METAL shift-register noise, CRACKLE), grain (sample and hold), a resonant state-variable filter morphing low-pass → band-pass → high-pass that pings at high resonance, a sweep on its own envelope (a zap down, a rise up), BURSTS 1..4 (a clap), a click, an envelope that strikes or is held while the key is (GATE), drive, a crusher, a decorrelated side for width; its loudness kept wherever the filter is | a breath: pink, 1.5 kHz band-pass, A 250 ms, held, 4-bar BREATH (D8) |

Each has the same four macros, mapped to what they mean for it (BODY: mass, sub, drive; TEXTURE: PM, growl, breath,
roughness; MOTION: the filter's movement, the Turing's depth on it; DECAY: its envelope). On the perc voice BODY is the
cutoff (120 Hz .. 2.4 kHz at 50 .. 14 kHz), TEXTURE the grain (CRACKLE's density), MOTION the sweep, DECAY the strike's.

**The deep kick, RUMBLE, DUB (D22).** A drum organ's MODEL KICK (D8 from 66): a sine swept to its pitch, held, a long
decay, drive and two low-pass poles (`bio_kick.h`). RUMBLE: HEART's own dark low tail, ducked by PUMP, which ducks the
returns too. DUB: a synth's minor chord through a closing low-pass.

**Crackle (D21).** The noise core's sources are eight: WHITE PINK DIGI METAL and the crackles DUST, FIRE (clustered
pops over a smoulder), FRY (a dense sizzle), CREAK (gliding stick-slip); TEXTURE their density.

**Landscapes (D21).** A synth's MODEL (E1) makes it one of nine voices (`bio_land.h`): HUM STEEL PISTON (industry),
GLASS BELL CHOIR (cosmic), BLOOD BUBBLE SLIME (the body). The reverb's SPACE: HALL ROOM CAVE METAL VAST.

**Rhythm made of noise (D20).** The sequencer locks each noise step's seed (LOOP: LOCK keeps it, lower lets it slip,
DOUBLE LOCK inverts it each pass); SCATTER (E1) gives each step its own slice from it (band, resonance, length, shape,
level, place, grain); TONE (E2) puts the band on the note with a second a fifth up, BODY its octave; DRIFT (E3) moves
the band over four bars; a noise step's ratchet repeats its own noise (a stutter). On SYN with SHIFT.

**MODEL NOISE.** A drum organ's D8 (MODEL) over 50 makes it a perc voice with the same parameters; its SOUND knob
(ENV, SHIFT, KNOB 1) loads one of sixteen noise sounds (`bio_kits.h`: TICK, HISS, METAL, DIGI SNR, CLAP, ZAP, RISE,
CRUSH, CRACKLE, GRAIN, PING, THUMP, WASH, FLAM SNR, BIT HAT, LASER) or TISSUE, the drum's own body again.

**PUMP** (MIX, the sends, SHIFT, KNOB 2): HEART ducks the other five channels, up to −18 dB, in 2 ms, back with a
time constant of a sixth of a beat (by the next eighth).

**Life signals** (not channels): HEART PRESSURE (HEART's impact envelope: ducks the DRONE, moves its filter), BREATH (a
slow cycle in bars: the DRONE's filter, NOISE, VOICE), NERVE DRIFT (a slow random walk, a little detune and colour).

### 3.3 Effects, the bus

- **BLOOD DELAY**: stereo, in beat divisions (1/32 … 3/4, up to 0.74 s), feedback ≤ 85 %, a darker echo each repeat
  (low-pass in the loop), ping-pong, a send on into the reverb.
- **BILE REVERB**: four modulated delay lines (FDN4, Hadamard mix), two diffusers, damping, high-pass 150 Hz, decay
  0.25–12 s, at half rate. FREEZE (BURST): feedback 0.998, input closed.
- **MARROW CRUSH**: a parallel send: asymmetric drive, then sample-rate and bit reduction, a fold. Meant for SPASM,
  NOISE, BASS and the drums; never on all by itself.
- Sends A (delay), B (reverb), C (crush) per channel. The dry signal stays.
- **Master**: −6 dB at INIT, a DC block, a soft peak limiter near −1 dBFS.
- PARASITE CHORUS and NECRO GATE from the document: not in v0.1 (§7).

## 4. The sequencer: the RITUAL GRID

All in `firmware/src/bio_seq.h`.

- **Time.** A 16th step is 96 ticks. The audio interrupt runs the transport in eight sub-blocks of 32 frames (0.73 ms):
  events, the Turing clock, envelopes. Tempo 40–240 BPM, swing 0–100 (a late off-beat, up to a triplet's).
- **A scene** (RITUAL 00..03): nine tracks, each up to 64 steps long (its own length: polymeter; VOICE's phrase comes
  every fourth bar), the nine organs' sounds, the Turing, the mix, the FX, a seed. All four live in RAM; switching is
  instant (on the next bar while playing).
- **A track**: up to 96 note events `{start tick, length in ticks, note, velocity, flags: accent, ratchet 1-4,
  chance, anchor}`, sorted by start; per step up to four p-locks (BODY TEXTURE MOTION DECAY); its quantise (OFF,
  1/32, 1/16, 1/8, 1/4) and strength (0–100 %, INIT 1/16 at 75 %); mute.
- **Quantise is non-destructive.** Live notes keep their raw time; playback places them by the track's quantise, so
  turning it off brings the performance back.
- **Note-offs are safe.** Each event carries its length; a voice ends at its end, at a STOP, at PANIC, at a scene
  change. No hanging notes, no double note-on (one event per start, pitch and track).

### 4.1 Recording

| | How | What goes into the memory |
| --- | --- | --- |
| **LIVE REC** | PLAY, REC on, play | note-on at the tick it came, its length at the note-off, velocity |
| **STEP REC** | stopped, REC, a key | the note at the cursor (KNOB 1 moves it) with the step length; the cursor moves on |
| **PARAM REC** | REC on, turn a macro (SYN) | a p-lock on the step the playhead is on (stopped: the cursor's) |
| **OVERDUB** (INIT) | | new notes beside the old; the same note at the same quantised start replaces it |
| **REPLACE** | SHIFT + REC, red REPLACE | from the first new note on, each step the playhead enters is cleared on that track |
| **ERASE** | SHIFT held + a key while recording | that note's events are removed where the playhead passes |
| **UNDO / REDO** | SHIFT + OCT− / OCT+ | 16 levels; a recording pass is one |

The keys always sound. With REC off they are only a playing surface. MIDI notes (USB, the TRS jack) go the same way
and are recorded the same way. Playback never records itself.

**Hold SEQ** and the white keys are the focused track's steps on its current page: a tap toggles a step (drums: the
organ's note; the others: the last note played), a held step and KNOB 1–4 write its p-locks. The keys' LEDs show the
steps and the playhead.

### 4.2 Saving

Each scene has two slots in flash (A and B, a generation number, a CRC): a save writes the older one, so a save cut
off by a power cut leaves the last good one. A flash erase stops the sound for some 200 ms, so SAVE while playing waits
for STOP. The screen always says SAVED or a red UNSAVED. The global settings are a blob.

## 5. The nervous system: the NERVOUS TAPE

All in `firmware/src/bio_turing.h`.

- **R**: 16 bits. On each clock edge: `tap = (R >> (L − 1)) & 1`, `in = tap ^ flip`, `R = (R << 1 | in) & 0xFFFF`.
  L ∈ {2, 3, 4, 5, 6, 8, 12, 16}.
- **LOOP** (ALGORITHM, 0–100 %): the chance that the bit is flipped is 1 − LOOP. 0–3 %: always flipped (DOUBLE LOCK:
  period 2L, the pattern and its inverse); 97–100 %: never (LOCK: period L); 50 %: random. Nothing random happens in
  LOCK.
- **CLOCK**: 1/32, 1/16, 1/8 (INIT), 1/4 of the transport. **WRITE 0 / 1** force the next bit; **RESET** brings back the
  saved R and RNG state (and the clock's phase).
- **CV8** = R & 0xFF, held between clocks and smoothed 8–35 ms where it moves a filter or a level.
- **Routes** (depth each, × TURING DEPTH):

| Route | From | To | INIT |
| --- | --- | --- | --- |
| NERVE→SCRAPE | CV8 | CLAW's friction band, 900–3400 Hz | 22 % |
| NERVE→METAL | CV8 | TEETH's mode spread, ±9 % | 16 % |
| NERVE→AIR | CV8 | HEART's and BONE's vent, +0–18 % | 11 % |
| ROT | CV8 | every organ's TEXTURE | on |
| RATTLE | bit 3 AND NOT bit 1 | ghost hits on TEETH and CLAW, at most 2 a step | off |
| WAIL | CV8 into a note pool (D Phrygian, 1–2 octaves) | SPASM's and VOICE's pitch | off |

- **Merge, never erase.** The Turing adds ghosts, accents and texture; it never moves or deletes a played note, and an
  anchored step (HEART's 1 5 9 13 at INIT) is never ghosted over. **FUSE** (LFO held + key 4) writes the ghosts of the
  last loop into the focused track as real notes: explicit, and undoable.
- INIT: R = 0xB35D, L = 8, LOOP 100 % (locked), DEPTH 20 %, clock 1/8, ROT to TEXTURE, WAIL and RATTLE off.

## 6. The screens

240 x 240, drawn on a background made by `tools/gen_art.py` (bone ribs, tubes, membranes; generated, not copied from
anyone's art), only what changed redrawn.

- **Top (0–27):** BIOFORGE in bone, the page tabs MIX SEQ SYN MUT FX GLO (green the current one). SEQ is the first.
- **Status (29–46):** the transport (▶ ■ ●), LIVE / STEP / REPL, the focus, the scene, BPM, SAVED or EDIT; for a
  moment LOOP's value and word, or a message (UNDO, P-LOCK, SAVED).
- **Middle (28–183):** the page.
- **Bottom (184–239):** four eye knobs (a red iris, a green arc for the value), their names and values.

| Page | First screen | Under it |
| --- | --- | --- |
| **MIX** | six channel strips: organ, level, meter, mute | the focused channel's level and sends A B C |
| **SEQ** | nine tracks x 16 steps of membrane cells, the playhead, the cursor; REC state, mode, quantise, page | the cursor's length, velocity, chance, ratchet |
| **SYN** | the focused organ, a picture of its state (the modes ringing, the excitation), BODY TEXTURE MOTION DECAY | its own four or eight; tune, level, pan |
| **MUT** | the spine: 16 vertebrae (R's bits), the LOOP's value large, L, CLOCK, CV8 | the routes' depths, WAIL's scale |
| **FX** | BLOOD DELAY, BILE REVERB, MARROW CRUSH: their mixes | times, feedback, decay, damping, crush |
| **GLOBAL** | BPM, SWING, QUANTISE, STRENGTH | REC MODE, COUNT-IN, STEP LENGTH |

A red pulse runs along the top frame while notes are actually being written; green when the pattern plays.

## 7. Cut, simplified or later

| From the document | In v0.1 | Why |
| --- | --- | --- |
| 1–64 steps, pattern bank | 4 scenes of nine tracks, each 1–64 steps | flash: each scene has two slots (A/B) of the eight |
| PARASITE CHORUS, NECRO GATE | later | the CPU first for TISSUE (§5.10 of the document) |
| MIX scenes A/B, FLESH MIX crossfade, XFADE curves | later | one scene at a time; switching on the bar |
| BURST page (MORPH TISSUE CHAIN SPASM) | ARP held: STUTTER, INVERT, FREEZE, SPASM | the same idea in four keys |
| Microtiming ±20 ms cap, FRITID/RAW mode | the raw time is kept; quantise decides | simpler and non-destructive |
| Count-in | one bar, on or off | |
| Project export / import | later | the platform keeps SysEx for its updater |
| 32-bit float reference | the engine is integer; the host renders the same code | no FPU on the chip |

## 8. Budget

The audio interrupt: 256 frames in 5.8 ms; the target is under half.

**Measured on the host** (`make perf`: callgrind, the engine built `-Os` as the chip's build is): about **2 400
instructions a frame** for the awakening, **2 900** with every organ sounding and every effect on. The chip's pi32v2
does a 32 x 32 -> 64 multiply in one instruction, so the host's count is a fair proxy. Taking the whole interrupt as
~5 300 of the chip's instructions a frame (ORBITRIX's count of the same chip), that is about 45 % and 55 %.
**fm1-emu in CI measures it on the emulated chip** (tools/emu_check.sh: the interrupts' share, and the engine's own
`isr_load`).

What made it fit (from 3 140 a frame): a drum organ whose excitation is over runs a fast path, only its modes and its
pitch body; organs sleep at -72 dB; the sends are summed per channel, not per organ; the rib sleeps; the reverb's
modulation is computed once a sub-block. Over 75 % of the interrupt (its own measure, `isr_load`), the drums out of
focus ring on two modes instead of four (the document's CPU rule: the transients, the clock and the recording are
never cut).

Memory, measured by the build: the image 172 KB of 568; RAM 17 KB of 96; the pool 297 KB of 336 (the canvas
240 x 134, the delay 2 x 32768 int16, the reverb, four scenes of ~9.6 KB, the undo).

## 9. Order of work

1. The engine and its host renders (`tools/bio_render.c` → `build/audio/*.wav`), tests under ASan/UBSan
   (`tests/bio_test.c`): the organs dry, determinism, no clipping, no DC, the Turing's periods.
2. The sequencer and recording, tested by replaying scripted key presses.
3. The app: pages, panel, LEDs, saving; host scripts and screenshots.
4. The FM-1 build (sizes, the chip's checks), the instruction count, CI, the README. Then the FM-1 itself, with the
   user.
