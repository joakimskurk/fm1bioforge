/* SPDX-License-Identifier: GPL-3.0-only */
/* BIOFORGE: a biomechanical rhythm machine and synth for the M-VAVE FM-1. Rhythm out of pressure, fracture, friction
 * and vents in resonant bodies; tonal organs for mass, breath, nerve discharges and an artificial throat; the keys
 * straight into the pattern memory; a Turing register as a nervous system that locks and slowly changes it all.
 * The plan and its reasons: docs/bioforge/PLAN.md, DECISIONS.md.
 *
 * Two ways to play it (D45). SIMPLE, as it starts: five pages, one a button, each four knobs, a second page on the same
 * button again, never deeper; plain names (KICK SNARE HAT PERC BASS PAD PLUCK LEAD NOISE), the organs' own as
 * decoration; holding a button says what it does. DEEP (SEL + EDIT): every page of before, SHIFT and all.
 *   SELECT       the track: DRUMS (the kit), KICK .. NOISE              PRESETS  the scene (playing: on the one);
 *   ALGORITHM    MUTATE: how much the music changes by itself (DEEP: LOOP)      SEL + PRESETS: a factory scene into it
 *   HOME  PLAY: the track's four macros, named as its sound has them; again: MIX. Held: white keys 1-9 mute the tracks,
 *         10 the drums
 *   SEQ   STEPS; again: the track's length, a step's repeats, nudge, note. Held: the white keys are the steps
 *   EDIT  SOUND: the sound (a list), tune, attack, release; again: its ARP (a drum: inside it). ENV: SOUND too
 *   FX    the track into the delay, the reverb, the crusher, the tape; again: the delay's time, the room
 *   GLO   SONG: tempo, swing, key, scale; again: the chain (each scene's bars, then which)
 *   ARP   tapped: the track's ARP; held: BREAK IT on the white keys (STUTTER, INVERT, FREEZE, SPASM; SCAR: REPEAT 1/16,
 *         REPEAT 1/8, REVERSE, SMEAR, SCREW, DROP)           LFO  held: WRITE 0, WRITE 1, RESET, FUSE, LOCK
 *   SAVE  the scene to flash (SEL: every edited scene)
 *   PLAY  start / stop (SEL: PANIC)  REC  playing: LIVE REC; stopped: STEP REC (SEL: OVERDUB / REPLACE)
 *   OCT- OCT+  the keys' octave (SEQ held: the page of steps; SEL: UNDO, REDO)
 *   MIDI  notes on channel 1 to the track, 2..10 to KICK..NOISE (recorded as the keys are); start and stop */
#define APP_NAME "BIOFORGE"
#define STORE_MAGIC 0x464F4942u                 /* "BIOF": this firmware's own (store.h) */
#define CV_MAX (240u * 134u)                    /* the main area in one canvas */
static void app_idle(void);
#define LCD_IDLE app_idle
#include "platform.c"
#define SEQ_LOCK() fm1_irq_off()
#define SEQ_UNLOCK() fm1_irq_on()
#include "bio_seq.h"
#include "bio_rituals.h"
#include "bio_ui.h"

#if defined(DINGHY_HOST) && defined(__APPLE__)
#define BIO_POOL
#else
#define BIO_POOL __attribute__((section(".pool")))
#endif

#ifdef DINGHY_HOST
static uint32_t app_ticks(void) { return fm1_ms * 24000u; }
#else
static uint32_t app_ticks(void) { return fm1_ticks(); }
#endif

/* ---- the state */
static bio_t eng BIO_POOL;
static seq_scene_t scenes[SEQ_NSCENE];          /* (in RAM: the pool keeps the engine, SCAR's 64 KB in it, D29) */
static seq_undo_t undo_buf[SEQ_NUNDO], redo_buf[SEQ_NREDO];
static seq_t sq;
static volatile uint32_t eng_off, isr_load, t_audio;
static volatile uint32_t c1_load;               /* the second core's share of the 5.8 ms in its units (D38), % */
static uint32_t load_q8[6];                     /* the figures below smoothed (x 256: an integer mean settles where it
                                                   is, not 15 points under it, D38) */
static volatile uint32_t emu_fig[7];            /* isr_load, c1_load, core1_state, then core 0's parts (the tick, its
                                                   units, the wait, the mix to the end), %: fm1-emu reads them by their
                                                   address (tools/emu_check.sh, build/bioforge.syms) */

enum { PG_MIX, PG_SEQ, PG_SYN, PG_MUT, PG_FX, PG_GLO, PG_N };   /* DEEP's pages */
static const char *const PG_NAME[PG_N] = {"MIX", "SEQ", "SYN", "MUT", "FX", "GLO"};
enum { SP_PLAY, SP_STEPS, SP_SOUND, SP_FX, SP_SONG, SP_N };      /* SIMPLE's (D45) */
static const char *const SP_NAME[SP_N] = {"PLAY", "STEPS", "SOUND", "FX", "SONG"};
/* the tracks by what they are for (D45); the organs' own names are the decoration */
static const char *const TRK_NAME[BIO_NORG] = {"KICK", "SNARE", "HAT", "PERC", "BASS", "PAD", "PLUCK", "LEAD", "NOISE"};
static const char *const ORG_NAME[BIO_NORG] = {"HEART", "BONE", "TEETH", "CLAW", "BASS", "DRONE", "SPASM", "VOICE", "NOISE"};
static const char *const CH_NAME[BIO_NCH] = {"DRUMS", "BASS", "PAD", "PLUCK", "LEAD", "NOISE"};
static const char *const NOTE_NAME[12] = {"C", "C#", "D", "EB", "E", "F", "F#", "G", "AB", "A", "BB", "B"};
#define F_KIT 0u                                /* focus 0: the kit; 1..9: organ 0..8 */

typedef struct {                                /* kept as settings (blob 0) */
    uint8_t ver, scene, focus, keyled;
    int8_t oct[10];
    uint8_t replace, midich, deep, pad;         /* deep: DEEP's pages (D45; 0 in what an older one kept: SIMPLE) */
} bio_set_t;
static bio_set_t set = {1, 0, F_KIT, 0, {0, 0, 0, 0, 0, -3, -2, 0, -1, 0}, 0, 0, 0, 0};

static struct {
    uint32_t page, sub, shift, latched, last_drum, cursor, steppage, steprec;
    uint32_t sp, ssub;                          /* SIMPLE's page and its second (D45) */
    uint32_t t_down[20], used;                  /* when each button went down (the SMK-37 has 18); a bit: it did
                                                   something while held */
    uint32_t help, t_help;                      /* the button whose card shows (99: none) */
    uint32_t ro, t_ro;                          /* the readout: 1..4 a knob, 5 MUTATE, 6 the scene; when */
    uint32_t played;                            /* PLAY pressed once: the first hint is done */
    uint32_t held_steps, step_used, keys_prev, note_on[27], step_keys_down, last_note[BIO_NORG];
    uint32_t dirty_draw, t_msg, t_loop, t_input;
    uint32_t dirty[SEQ_NSCENE], save_req[SEQ_NSCENE];
    const char *msg;
    uint32_t burst, stutter_at, sel_alone, sel_down;
    uint32_t tur_seen, writing_seen, t_writing;
    uint8_t kit[BIO_NORG];                      /* the SOUND each organ was last given (0: TISSUE) */
    uint32_t factory;                           /* the factory ritual SHIFT + PRESETS last loaded */
} ui;
#define D_TOP 1u                                /* the logo, the track, the scene, the tempo */
#define D_STATUS 2u                             /* the pages' tabs */
#define D_MAIN 4u
#define D_KNOBS 8u
#define D_HINT 16u                              /* SIMPLE's line under the picture: a hint, or what just happened */
#define D_ALL 31u
#define NO_HELP 99u

static seq_scene_t *SC(void) { return &scenes[sq.cur]; }
static uint32_t cur_org(void) { return set.focus == F_KIT ? ui.last_drum : set.focus - 1u; }
static void say(const char *m) { ui.msg = m; ui.t_msg = fm1_ms; ui.dirty_draw |= D_TOP | D_HINT; }
static void touched(void) { ui.dirty[sq.cur] = 1; ui.dirty_draw |= D_TOP; ui.t_input = fm1_ms; }
static const char *track_name(void) { return set.focus == F_KIT ? "DRUMS" : TRK_NAME[set.focus - 1u]; }

/* ---- the knobs: what each of the four turns on this page */
enum { KT_NONE, KT_P, KT_LVL, KT_SA, KT_SB, KT_SC, KT_MASTER, KT_RIB, KT_TDEPTH, KT_TLEN, KT_TCLK, KT_TRATTLE,
       KT_TROUTE, KT_TWAIL, KT_TSCALE, KT_FX, KT_BPM, KT_SWING, KT_QUANT, KT_QSTR, KT_RECMODE, KT_STEPLEN, KT_KEYLED,
       KT_MIDICH, KT_CURSOR, KT_EVLEN, KT_EVVEL, KT_EVCH, KT_TRLEN, KT_EVRAT, KT_EVNUDGE, KT_EVNOTE, KT_PUMP, KT_KIT,
       KT_RUMBLE, KT_DARK, KT_KEY, KT_SCALEM, KT_ARP, KT_ARATE, KT_ARANGE, KT_ACHORD, KT_AGATE, KT_WARP, KT_RUPTURE, KT_SBARS,
       KT_SNEXT, KT_SOUND };
typedef struct { uint8_t type, idx, org; const char *name; } knob_t;   /* org: an organ of its own (BIO_NORG: the track's) */
static uint32_t korg(knob_t n) { return n.org < BIO_NORG ? n.org : cur_org(); }

static const char *const DRUM_DEEP[8] = {"PRESS", "FRACT", "FRIC", "VENT", "RESO", "MATTER", "DRIVE", "MODEL"};
static const char *const PERC_DEEP[8] = {"SOURCE", "RESO", "MODE", "BURSTS", "CLICK", "DRIVE", "CRUSH", "MODEL"};   /* MODEL NOISE */
static const char *const KICK_DEEP[8] = {"TONE", "DRIVE", "KNOCK", "HOLD", "", "", "", "MODEL"};   /* MODEL KICK (D22) */
static const char *const DEEP[5][8] = {
    {"CUTOFF", "RESO", "ENV", "GLIDE", "MORPH", "PM", "SUB", "DRIVE"},          /* BASS */
    {"CUTOFF", "RESO", "BEAT", "COUPLE", "BREATH", "MORPH", "PM", "BANDS"},     /* DRONE */
    {"RATIO", "PM", "PM DEC", "BAND", "RATCH", "Q", "", ""},                    /* SPASM */
    {"MOUTH", "BREATH", "SHIFT", "SUB", "", "", "", ""},                        /* VOICE */
    {"SOURCE", "RESO", "MODE", "BURSTS", "CLICK", "DRIVE", "CRUSH", "BREATH"},  /* NOISE: the perc voice */
};
static const char *const MACRO[4] = {"BODY", "TEXTURE", "MOTION", "DECAY"};
static const char *const CORE[4] = {"SCATTER", "TONE", "DRIFT", ""};   /* the noise core's (D20), SYN SHIFT */

static uint32_t org_perc(uint32_t o) { return bio_is_perc(o, SC()->p[o]); }
static const char *deep_name(uint32_t o, uint32_t i)
{
    return o < 4u ? (org_perc(o) ? PERC_DEEP[i] : bio_is_kick(o, SC()->p[o]) ? KICK_DEEP[i] : DRUM_DEEP[i]) : DEEP[o - 4u][i];
}

/* ---- SIMPLE (D45): each sound's four macros by what they do in it (bio_land.h, bio_organs.h, bio_perc.h, bio_kick.h
   say what BODY, TEXTURE, MOTION and DECAY are there) */
static const char *const MAC_OWN[4][4] = {
    {"WEIGHT", "GROWL", "SWEEP", "DECAY"},      /* BASS as it is: the sub and drive, the PM, the filter's sweep */
    {"BANDS", "EDGE", "BREATHE", "DECAY"},      /* DRONE: its two bands, triangle to saw, the breath on its filter */
    {"RING", "STRIKE", "WANDER", "DECAY"},      /* SPASM: the band's Q, the PM strike, the nerve moving the band */
    {"SIZE", "BREATH", "DRIFT", "DECAY"},       /* VOICE: the throat's size, its breath, the mouth moving by itself */
};
static const char *const MAC_LAND[BIO_NLAND][4] = {
    {"", "", "", ""},
    {"BAND", "SPARKS", "FLUTTER", "DECAY"},     /* HUM */
    {"OCTAVE", "RING", "BOW", "DECAY"},         /* STEEL */
    {"CLANK", "SPEED", "STEAM", "DECAY"},       /* PISTON */
    {"SUB", "SHINE", "BEAT", "DECAY"},          /* GLASS */
    {"BRIGHT", "RATIO", "WOBBLE", "DECAY"},     /* BELL */
    {"VOWEL", "BREATH", "SPREAD", "DECAY"},     /* CHOIR */
    {"BAND", "RUSH", "PULSE", "DECAY"},         /* BLOOD */
    {"SIZE", "DROPS", "SCATTER", "DECAY"},      /* BUBBLE */
    {"SQUARE", "GURGLE", "SQUELCH", "DECAY"},   /* SLIME */
    {"BRIGHT", "CHORD", "OPEN", "DECAY"},       /* DUB */
    {"BRIGHT", "GRIT", "BREATHE", "DECAY"},     /* MASS */
    {"BRIGHT", "GRIT", "BREATHE", "DECAY"},     /* CLUSTER */
    {"REED", "STOPS", "TREMOLO", "DECAY"},      /* ORGAN */
    {"BRIGHT", "OCTAVE", "SWAY", "DECAY"},      /* STRINGS */
    {"BRIGHT", "SWELL", "VIBRATO", "DECAY"},    /* BRASS */
    {"BRIGHT", "PLUCK", "DETUNE", "DECAY"},     /* HARPSI */
};
static const char *const MAC_TISSUE[4] = {"WEIGHT", "GRIT", "DRIFT", "DECAY"};
static const char *const MAC_KICK[4] = {"PITCH", "SWEEP", "FALL", "DECAY"};
static const char *const MAC_PERC[4] = {"BRIGHT", "GRAIN", "SWEEP", "DECAY"};
static const char *const IN_TISSUE[4] = {"THUMP", "CRACK", "SCRAPE", "AIR"};   /* inside a drum: D1..D4 */
static const char *const IN_KICK[4] = {"TONE", "DRIVE", "KNOCK", "HOLD"};
static const char *const IN_PERC[4] = {"SOURCE", "RING", "FILTER", "BURSTS"};
/* the sounds by how they sound: a synth's own voice, then the landscapes; the drums' and NOISE's from the kit */
static const char *const OWN_PLAIN[4] = {"SYNTH BASS", "DRONE", "FM PLUCK", "THROAT"};
static const char *const OWN_DESC[4] = {"MUSCLE: A SINE, ITS SUB, A SNAPPING FILTER", "WOMB: TWO WAVES BEATING, BREATHING",
                                        "NERVE: A STRUCK FM NOTE THAT BITES", "A THROAT SINGING ITS VOWELS"};
static const char *const LAND_DESC[BIO_NLAND] = {
    "", "THE MAINS HUMMING, SPARKS FLYING", "BOWED METAL: A PLATE RINGING", "A MACHINE RUNNING WHILE HELD",
    "GLASS: PARTIALS BEATING", "A DIRTY FM BELL", "THREE THROATS SINGING", "A PULSE: LUB-DUB ON THE BEAT",
    "DROPS RISING IN WATER", "A WET SQUELCH", "DUB CHORDS INTO THE DELAY", "A WALL: OCTAVES AND A FIFTH",
    "A DARK CLUSTER OF SEMITONES", "A PIPE ORGAN", "A STRING MACHINE: SAWS THAT SWAY", "SEVENTIES SYNTH BRASS",
    "A HARPSICHORD, PLUCKED"};
static const char *const KIT_DESC[] = {
    "A CLOSED HAT", "AN OPEN HAT, WIDE", "A HAT OF A SHIFT REGISTER", "A DIGITAL SNARE", "A CLAP: FOUR STRIKES",
    "A ZAP: PINGING, SWEPT DOWN", "NOISE OPENING WHILE HELD", "A CRUSHED SNARE", "CRACKLE", "DIGITAL GRAIN",
    "A NOISE TOM: THE FILTER RINGS", "A KICK OF NOISE", "A WASH OF NOISE, HELD", "A SNARE WITH A FLAM",
    "A SNARE: A CRACK AND ITS SIZZLE", "A CRUSHED HAT", "A LASER: METAL SWEPT DOWN", "A FIRE, HELD",
    "SPARKS", "A PAN OF FAT, HELD", "GREASE POPPING", "WOOD CREAKING, HELD", "A HINGE",
    "FM FOLDING PAST NYQUIST", "SIX SQUARES: A METAL HAT", "SIX SQUARES RINGING, HARSH", "A FORMULA'S BYTES, HELD",
    "A DEEP KICK", "A SUB KICK: THE FLOOR ITSELF", "A DUB KICK: TIGHT, WARM", "A PUNCH KICK: HEARD ON ANY SPEAKER",
    "A WAR DRUM: PLAY IT OVER C4"};
_Static_assert(sizeof KIT_DESC / sizeof KIT_DESC[0] == BIO_NPERC_KIT, "a line for each of the kit's sounds");

static uint32_t is_kick(uint32_t o) { return o < 4u && bio_is_kick(o, SC()->p[o]); }
static const char *macro_name(uint32_t o, uint32_t k)
{
    uint32_t m;
    if (org_perc(o)) return MAC_PERC[k];
    if (is_kick(o)) return MAC_KICK[k];
    if (o < 4u) return MAC_TISSUE[k];
    m = bio_land_of(SC()->p[o][P_E1]);
    return m ? MAC_LAND[m][k] : MAC_OWN[o - 4u][k];
}
static const char *inside_name(uint32_t o, uint32_t k)
{
    return org_perc(o) ? IN_PERC[k] : is_kick(o) ? IN_KICK[k] : IN_TISSUE[k];
}
static uint32_t has_arp(uint32_t o) { return o >= 4u; }   /* the synths and NOISE (D33) */
static uint32_t pg2_open(void) { return ui.ssub && ui.sp != SP_STEPS; }   /* (STEPS' second page is the same grid) */

static knob_t knob_simple(uint32_t k)
{
    knob_t n = {KT_NONE, 0, BIO_NORG, ""};
    uint32_t o = cur_org(), kit = set.focus == F_KIT;
    switch (ui.sp) {
    case SP_PLAY:
        if (!ui.ssub) {                         /* the instrument: the four macros; the kit: the four drums' levels */
            n.type = KT_P;
            if (kit) { n.idx = P_LEVEL; n.org = (uint8_t)k; n.name = TRK_NAME[k]; }
            else { n.idx = (uint8_t)(P_BODY + k); n.name = macro_name(o, k); }
        } else {                                /* MIX: the track's level and pan; the pump, the master */
            if (k == 0u) {
                n.name = "LEVEL";
                if (kit || o >= 4u) { n.type = KT_LVL; n.idx = BIO_ORG_CH[o]; }
                else { n.type = KT_P; n.idx = P_LEVEL; }
            }
            if (k == 1u && !kit) { n.type = KT_P; n.idx = P_PAN; n.name = "PAN"; }
            if (k == 2u) { n.type = KT_PUMP; n.name = "PUMP"; }
            if (k == 3u) { n.type = KT_MASTER; n.name = "MASTER"; }
        }
        break;
    case SP_STEPS: {
        static const char *const A[4] = {"STEP", "LENGTH", "VELOCITY", "CHANCE"};
        static const char *const B[4] = {"STEPS", "REPEAT", "NUDGE", "NOTE"};
        static const uint8_t TA[4] = {KT_CURSOR, KT_EVLEN, KT_EVVEL, KT_EVCH}, TB[4] = {KT_TRLEN, KT_EVRAT, KT_EVNUDGE, KT_EVNOTE};
        n.type = ui.ssub ? TB[k] : TA[k];
        n.name = ui.ssub ? B[k] : A[k];
        break;
    }
    case SP_SOUND:
        if (!ui.ssub) {                         /* the sound; its tune and envelope (a drum: its level last) */
            static const char *const N[4] = {"SOUND", "TUNE", "ATTACK", "RELEASE"};
            static const uint8_t I[4] = {0, P_TUNE, P_A, P_A + 3};
            n.type = k ? KT_P : KT_SOUND;
            n.idx = I[k];
            n.name = N[k];
            if (k == 3u && o < 4u) { n.idx = P_LEVEL; n.name = "LEVEL"; }
        } else if (has_arp(o)) {                /* its ARP */
            static const char *const N[4] = {"ARP", "RATE", "RANGE", "CHORD"};
            static const uint8_t T[4] = {KT_ARP, KT_ARATE, KT_ARANGE, KT_ACHORD};
            n.type = T[k];
            n.name = N[k];
        } else {                                /* inside the drum */
            n.type = KT_P;
            n.idx = (uint8_t)(P_D1 + k);
            n.name = inside_name(o, k);
        }
        break;
    case SP_FX:
        if (!ui.ssub) {                         /* the track (the drums: their channel) into the effects; the tape */
            static const char *const N[4] = {"DELAY", "REVERB", "CRUSH", "TAPE"};
            static const uint8_t T[4] = {KT_SA, KT_SB, KT_SC, KT_WARP};
            n.type = T[k];
            n.idx = BIO_ORG_CH[o];
            n.name = N[k];
        } else {                                /* the effects themselves */
            static const char *const N[4] = {"TIME", "FEEDBACK", "ROOM", "TAIL"};
            static const uint8_t I[4] = {0, 1, 16, 7};
            n.type = KT_FX;
            n.idx = I[k];
            n.name = N[k];
        }
        break;
    default:                                    /* SONG; again: the chain */
        if (!ui.ssub) {
            static const char *const N[4] = {"TEMPO", "SWING", "KEY", "SCALE"};
            static const uint8_t T[4] = {KT_BPM, KT_SWING, KT_KEY, KT_SCALEM};
            n.type = T[k];
            n.name = N[k];
        } else if (k < 2u) {
            n.type = k ? KT_SNEXT : KT_SBARS;
            n.name = k ? "THEN" : "BARS";
        }
    }
    return n;
}

static knob_t knob_deep(uint32_t k)
{
    knob_t n = {KT_NONE, 0, BIO_NORG, ""};
    uint32_t o = cur_org(), sh = ui.shift || ui.latched;
    switch (ui.page) {
    case PG_MIX:
        if (ui.sub == 0u) {
            static const char *const A[4] = {"DRUM", "BASS", "DRONE", "SPASM"};
            static const char *const B[4] = {"VOICE", "NOISE", "DEPTH", "MASTER"};
            if (!sh) { n.type = KT_LVL; n.idx = (uint8_t)k; n.name = A[k]; }
            else if (k < 2u) { n.type = KT_LVL; n.idx = (uint8_t)(k + 4u); n.name = B[k]; }
            else if (k == 2u) { n.type = KT_TDEPTH; n.name = B[2]; }
            else { n.type = KT_MASTER; n.name = B[3]; }
        } else {
            static const char *const A[4] = {"LEVEL", "DELAY", "REVERB", "CRUSH"};
            n.idx = BIO_ORG_CH[o];
            if (sh) {
                if (k == 0u) { n.type = KT_RIB; n.name = "RIB"; }
                if (k == 1u) { n.type = KT_PUMP; n.name = "PUMP"; }
                if (k == 2u) { n.type = KT_RUMBLE; n.name = "RUMBLE"; }
                if (k == 3u) { n.type = KT_DARK; n.name = "DARK"; }
            }
            else { n.type = (uint8_t)(k == 0u ? KT_LVL : k == 1u ? KT_SA : k == 2u ? KT_SB : KT_SC); n.name = A[k]; }
        }
        break;
    case PG_SEQ: {
        static const char *const A[4] = {"STEP", "LENGTH", "VELO", "CHANCE"};
        static const char *const B[4] = {"TRK LEN", "RATCHET", "NUDGE", "NOTE"};
        static const uint8_t TA[4] = {KT_CURSOR, KT_EVLEN, KT_EVVEL, KT_EVCH}, TB[4] = {KT_TRLEN, KT_EVRAT, KT_EVNUDGE, KT_EVNOTE};
        n.type = sh ? TB[k] : TA[k];
        n.name = sh ? B[k] : A[k];
        break;
    }
    case PG_SYN:
        if (ui.sub == 0u) {
            n.type = KT_P;
            n.idx = (uint8_t)(sh ? (org_perc(o) ? P_E1 + k : o >= 4u && k == 0u ? P_E1 : P_D1 + k) : P_BODY + k);
            n.name = sh ? (org_perc(o) ? CORE[k] : o >= 4u && k == 0u ? "MODEL" :
                           o >= 4u && bio_land_of(SC()->p[o][P_E1]) ? "" : deep_name(o, k)) : MACRO[k];   /* (a synth: its MODEL, D21) */
        } else if (ui.sub == 1u) {
            n.type = KT_P;
            n.idx = (uint8_t)(sh ? P_D5 + k : P_D1 + k);
            n.name = deep_name(o, sh ? 4u + k : k);
        } else {                                /* ENV: the envelope; drums: the gesture, tune, level, pan */
            static const char *const T[4] = {"ATTACK", "DECAY", "SUSTAIN", "RELEASE"};
            static const char *const D[4] = {"ATTACK", "TUNE", "LEVEL", "PAN"};
            static const char *const S2[4] = {"TUNE", "LEVEL", "PAN", ""};
            static const char *const PE[4] = {"ATTACK", "GATE", "WIDTH", "RELEASE"};   /* the perc voice's */
            static const char *const DS[4] = {"SOUND", "GATE", "WIDTH", "RELEASE"};
            static const uint8_t DI[4] = {P_A, P_TUNE, P_LEVEL, P_PAN}, SI[4] = {P_TUNE, P_LEVEL, P_PAN, P_SPARE};
            n.type = KT_P;
            if (o < 4u) {                       /* SHIFT: the SOUND (TISSUE or a noise one), and a noise one's envelope */
                n.idx = sh ? (uint8_t)(P_A + k) : DI[k];
                n.name = sh ? (k == 0u || org_perc(o) ? DS[k] : "") : D[k];
                if (sh && k == 0u) n.type = KT_KIT;
            }
            else if (sh) { n.idx = SI[k]; n.name = o == O_NOISE && k == 3u ? "SOUND" : S2[k]; if (o == O_NOISE && k == 3u) n.type = KT_KIT; }
            else { n.idx = (uint8_t)(P_A + k); n.name = o == O_NOISE ? PE[k] : T[k]; }
        }
        if (!n.name[0]) n.type = KT_NONE;
        break;
    case PG_MUT:
        if (ui.sub == 0u) {
            static const char *const A[4] = {"LENGTH", "CLOCK", "DEPTH", "RATTLE"};
            static const char *const B[4] = {"SCRAPE", "METAL", "AIR", "ROT"};
            static const uint8_t TA[4] = {KT_TLEN, KT_TCLK, KT_TDEPTH, KT_TRATTLE};
            if (sh) { n.type = KT_TROUTE; n.idx = (uint8_t)k; n.name = B[k]; }
            else { n.type = TA[k]; n.name = A[k]; }
        } else {
            if (k == 0u) { n.type = KT_TWAIL; n.name = "WAIL"; }
            if (k == 1u) { n.type = KT_TSCALE; n.name = "SCALE"; }
            if (k == 2u) { n.type = KT_RUPTURE; n.name = "RUPTURE"; }   /* (D35: the register cuts in) */
        }
        break;
    case PG_FX: {
        static const char *const A0[4] = {"DELAY", "REVERB", "CRUSH", "D>REV"}, *const B0[4] = {"D TIME", "D FDBK", "R DECAY", "R SIZE"};
        static const char *const A1[4] = {"D TONE", "PING", "R DAMP", "R PRE"}, *const B1[4] = {"C DRIVE", "C RATE", "C BITS", "C FOLD"};
        static const uint8_t IA0[4] = {4, 10, 15, 5}, IB0[4] = {0, 1, 7, 6}, IA1[4] = {2, 3, 8, 9}, IB1[4] = {11, 12, 13, 14};
        n.type = KT_FX;
        if (ui.sub == 0u) { n.idx = sh ? IB0[k] : IA0[k]; n.name = sh ? B0[k] : A0[k]; }
        else if (ui.sub == 1u) { n.idx = sh ? IB1[k] : IA1[k]; n.name = sh ? B1[k] : A1[k]; }
        else {                                  /* FX again: the room, and the kick ducking the returns (D25) */
            n.idx = (uint8_t)(16u + k);
            n.name = k == 0u ? "SPACE" : k == 1u ? "DUCK" : k == 2u ? "WARP" : "";
            if (k == 2u) n.type = KT_WARP;      /* (D35: the tape wavers) */
            if (!n.name[0]) n.type = KT_NONE;
        }
        break;
    }
    default: {                                  /* GLO; again: the notes (D31); again: the SONG (D40) */
        static const char *const A[4] = {"BPM", "SWING", "QUANT", "Q-STR"}, *const B[4] = {"REC", "STEPLEN", "KEYS", "MIDI CH"};
        static const uint8_t TA[4] = {KT_BPM, KT_SWING, KT_QUANT, KT_QSTR}, TB[4] = {KT_RECMODE, KT_STEPLEN, KT_KEYLED, KT_MIDICH};
        if (ui.sub == 2u) {                     /* SONG (D40): this scene's BARS and THEN */
            n.type = k == 0u ? KT_SBARS : k == 1u ? KT_SNEXT : KT_NONE;
            n.name = k == 0u ? "BARS" : k == 1u ? "THEN" : "";
            break;
        }
        if (ui.sub == 1u) {                     /* KEY SCALE, the focus's ARP (D33); SHIFT: its RANGE CHORD GATE */
            static const char *const N[4] = {"KEY", "SCALE", "ARP", "RATE"}, *const NS[4] = {"RANGE", "CHORD", "GATE", ""};
            static const uint8_t TN[4] = {KT_KEY, KT_SCALEM, KT_ARP, KT_ARATE}, TS[4] = {KT_ARANGE, KT_ACHORD, KT_AGATE, KT_NONE};
            n.type = sh ? TS[k] : TN[k];
            n.name = sh ? NS[k] : N[k];
            if ((n.type == KT_ARP || n.type == KT_ARATE || n.type == KT_ARANGE || n.type == KT_ACHORD || n.type == KT_AGATE) && o < 4u) {
                n.type = KT_NONE;                   /* (the drums have no ARP) */
                n.name = "";
            }
            break;
        }
        n.type = sh ? TB[k] : TA[k];
        n.name = sh ? B[k] : A[k];
    }
    }
    return n;
}
static knob_t knob_for(uint32_t k) { return set.deep ? knob_deep(k) : knob_simple(k); }

static uint32_t cursor_events(void) { return seq_step_events(&sq, cur_org(), ui.cursor); }   /* at the cursor */

static int32_t knob_get(knob_t n, int32_t *lo, int32_t *hi)
{
    seq_scene_t *sc = SC();
    uint32_t o = korg(n), m, i;
    seq_track_t *t = &sc->tr[o];
    uint8_t *fx = (uint8_t *)&sc->fx;
    *lo = 0;
    *hi = 100;
    switch (n.type) {
    case KT_P: if (n.idx == P_TUNE) *hi = 48; return sc->p[o][n.idx];
    case KT_SOUND:                              /* a synth: its own voice or a landscape; else the kit's (D45) */
        if (o >= 4u && o < O_NOISE) { *hi = BIO_NLAND - 1; return (int32_t)bio_land_of(sc->p[o][P_E1]); }
        *lo = o == O_NOISE ? 1 : 0;
        *hi = (int32_t)(o == O_NOISE ? BIO_NPERC_KIT - BIO_NKICK_KIT : BIO_NPERC_KIT);
        return ui.kit[o];
    case KT_LVL: return sc->mix.lvl[n.idx];
    case KT_SA: return sc->mix.sa[n.idx];
    case KT_SB: return sc->mix.sb[n.idx];
    case KT_SC: return sc->mix.sc[n.idx];
    case KT_MASTER: return sc->mix.master;
    case KT_RIB: return sc->mix.rib;
    case KT_PUMP: return sc->mix.pump;
    case KT_RUMBLE: return sc->mix.rumble;
    case KT_DARK: return sc->mix.dark;
    case KT_WARP: return sc->mix.warp;
    case KT_RUPTURE: return sc->tur.rupture;
    case KT_SBARS: *hi = 7; return sc->song_bars & 7u;
    case KT_SNEXT: *hi = SEQ_NSCENE - 1u; return sc->song_next % SEQ_NSCENE;
    case KT_KIT:                                /* (the kicks a drum organ's only) */
        *lo = o == O_NOISE ? 1 : 0;
        *hi = (int32_t)(o == O_NOISE ? BIO_NPERC_KIT - BIO_NKICK_KIT : BIO_NPERC_KIT);
        return ui.kit[o];
    case KT_TDEPTH: return sc->tur.depth;
    case KT_TLEN: *hi = 7; return sc->tur.len_i;
    case KT_TCLK: *hi = 3; return sc->tur.clock;
    case KT_TRATTLE: return sc->tur.rattle;
    case KT_TROUTE: return n.idx == 0u ? sc->tur.r_scrape : n.idx == 1u ? sc->tur.r_metal : n.idx == 2u ? sc->tur.r_air : sc->tur.r_rot;
    case KT_TWAIL: *hi = 3; return sc->tur.wail;
    case KT_TSCALE: return sc->tur.scale;
    case KT_FX:
        if (n.idx == 0u) *hi = 9;
        if (n.idx == 3u) *hi = 1;
        if (n.idx == 1u) *hi = 85;
        if (n.idx == 5u) *hi = 100;
        if (n.idx == 16u) *hi = (int32_t)BIO_NSPACE - 1;
        return fx[n.idx];
    case KT_BPM: *lo = 400; *hi = 2400; return sc->bpm10;
    case KT_KEY: *hi = 11; return (int32_t)bio_key_root(&sc->tur);
    case KT_ARP: *hi = SEQ_NARP - 1; return t->arp < SEQ_NARP ? t->arp : 0;
    case KT_ARATE: *hi = 5; return (int32_t)seq_arp_rate(t);
    case KT_ARANGE: *lo = 1; *hi = 3; return (int32_t)seq_arp_range(t);
    case KT_ACHORD: *hi = (int32_t)SEQ_NCHORD - 1; return (int32_t)seq_arp_chord(t);
    case KT_AGATE: *lo = 1; *hi = 10; return (int32_t)seq_arp_gate(t) / 10;
    case KT_SCALEM: *hi = (int32_t)BIO_NSCALE - 1; return (int32_t)bio_key_scale(&sc->tur);
    case KT_SWING: return sc->swing;
    case KT_QUANT: *hi = 4; return t->quant;
    case KT_QSTR: return t->qstr;
    case KT_RECMODE: *hi = 1; return set.replace;
    case KT_STEPLEN: *lo = 1; *hi = 16; return sc->steplen;
    case KT_KEYLED: *hi = 1; return set.keyled;
    case KT_MIDICH: *hi = 16; return set.midich;
    case KT_CURSOR: *hi = t->len - 1; return (int32_t)ui.cursor;
    case KT_TRLEN: *lo = 1; *hi = 64; return t->len;
    default: break;
    }
    m = cursor_events();                        /* the events at the cursor: the first one shows */
    for (i = 0; i < 32u; i++) if (m >> i & 1u) break;
    if (i == 32u) { *hi = 0; return -1; }
    switch (n.type) {
    case KT_EVLEN: *lo = 6; *hi = 64 * 96; return t->ev[i].len;
    case KT_EVVEL: *lo = 1; *hi = 127; return t->ev[i].vel;
    case KT_EVCH: *hi = 7; return (t->ev[i].fl >> EV_CH_SH) & 7u;
    case KT_EVRAT: *hi = 3; return (t->ev[i].fl >> EV_RAT_SH) & 3u;
    case KT_EVNUDGE: *lo = -48; *hi = 47; return (int32_t)t->ev[i].start - (int32_t)ui.cursor * 96;
    case KT_EVNOTE: *hi = 127; return t->ev[i].note;
    default: return 0;
    }
}

static void knob_turn(knob_t n, int32_t d)
{
    seq_scene_t *sc = SC();
    uint32_t o = korg(n);
    seq_track_t *t = &sc->tr[o];
    uint8_t *fx = (uint8_t *)&sc->fx;
    int32_t lo, hi, v = knob_get(n, &lo, &hi), nv;
    if (n.type == KT_NONE) return;
    if (n.type == KT_SOUND && o >= 4u && o < O_NOISE) {   /* a synth's sound: its MODEL */
        nv = bio_clamp(v + d, lo, hi);
        if (nv == v) return;
        sc->p[o][P_E1] = BIO_LAND(nv);
        touched();
        ui.dirty_draw |= D_ALL;
        return;
    }
    if (n.type == KT_SOUND) n.type = KT_KIT;    /* a drum's, NOISE's: the kit */
    if (n.type == KT_BPM) d *= ui.shift ? 1 : 10;
    if (n.type == KT_EVLEN) d *= v < 96 ? 6 : 24;
    nv = bio_clamp(v + d, lo, hi);
    /* p-locks: a macro turned with a step key held (SEQ), or while recording (PARAM REC), or in STEP REC */
    if (n.type == KT_P && n.idx <= P_DECAY && (ui.held_steps || (sq.rec && sq.playing) || (ui.steprec && !sq.playing))) {
        uint32_t st = ui.held_steps ? (uint32_t)__builtin_ctz(ui.held_steps) + ui.steppage * 16u
                    : sq.playing ? seq_now(&sq, o, 0) / SEQ_TPS : ui.cursor;
        uint32_t cur = t->lock[st % SEQ_MAXLEN][n.idx];
        if (ui.held_steps) ui.step_used = 1;
        seq_begin(&sq);
        seq_lock_set(&sq, o, st % SEQ_MAXLEN, n.idx, (uint32_t)bio_clamp((int32_t)(cur == BIO_NOLOCK ? (uint32_t)v : cur) + d, 0, 100));
        say("P-LOCK");
        touched();
        ui.dirty_draw |= D_MAIN | D_KNOBS;
        return;
    }
    if (nv == v) return;
    switch (n.type) {
    case KT_P: sc->p[o][n.idx] = (uint8_t)nv; break;
    case KT_LVL: sc->mix.lvl[n.idx] = (uint8_t)nv; break;
    case KT_SA: case KT_SB: case KT_SC: {       /* a send; in SIMPLE an effect that is off wakes with it (D45) */
        uint8_t *ret = n.type == KT_SA ? &sc->fx.d_mix : n.type == KT_SB ? &sc->fx.r_mix : &sc->fx.c_mix;
        (n.type == KT_SA ? sc->mix.sa : n.type == KT_SB ? sc->mix.sb : sc->mix.sc)[n.idx] = (uint8_t)nv;
        if (!set.deep && nv > 0 && !*ret) {
            *ret = n.type == KT_SC ? 50u : 24u;
            say(n.type == KT_SA ? "THE DELAY WAKES" : n.type == KT_SB ? "THE REVERB WAKES" : "THE CRUSHER WAKES");
        }
        break;
    }
    case KT_MASTER: sc->mix.master = (uint8_t)nv; break;
    case KT_RIB: sc->mix.rib = (uint8_t)nv; break;
    case KT_PUMP: sc->mix.pump = (uint8_t)nv; break;
    case KT_RUMBLE: sc->mix.rumble = (uint8_t)nv; break;
    case KT_DARK: sc->mix.dark = (uint8_t)nv; break;
    case KT_WARP: sc->mix.warp = (uint8_t)nv; break;
    case KT_RUPTURE: sc->tur.rupture = (uint8_t)nv; break;
    case KT_SBARS: sc->song_bars = (uint8_t)nv; break;
    case KT_SNEXT: sc->song_next = (uint8_t)nv; break;
    case KT_KIT: {                              /* a sound into the organ (its PAN kept); 0: the drum's own body again */
        uint8_t pan = sc->p[o][P_PAN];
        uint32_t i;
        ui.kit[o] = (uint8_t)nv;
        if (nv) bio_preset_set(sc->p[o], &BIO_PERC_KIT[nv - 1], o);
        else for (i = 0; i < BIO_NP; i++) sc->p[o][i] = BIO_P_INIT[o][i];
        sc->p[o][P_PAN] = pan;
        ui.dirty_draw |= D_ALL;
        break;
    }
    case KT_TDEPTH: sc->tur.depth = (uint8_t)nv; break;
    case KT_TLEN: sc->tur.len_i = (uint8_t)nv; break;
    case KT_TCLK: sc->tur.clock = (uint8_t)nv; break;
    case KT_TRATTLE: sc->tur.rattle = (uint8_t)nv; break;
    case KT_TROUTE:
        if (n.idx == 0u) sc->tur.r_scrape = (uint8_t)nv; else if (n.idx == 1u) sc->tur.r_metal = (uint8_t)nv;
        else if (n.idx == 2u) sc->tur.r_air = (uint8_t)nv; else sc->tur.r_rot = (uint8_t)nv;
        break;
    case KT_TWAIL: sc->tur.wail = (uint8_t)nv; break;
    case KT_TSCALE: sc->tur.scale = (uint8_t)nv; break;
    case KT_FX: fx[n.idx] = (uint8_t)nv; break;
    case KT_BPM: sc->bpm10 = (uint16_t)nv; break;
    case KT_KEY: sc->tur.key = (uint8_t)((sc->tur.key & 0xF0u) | (uint32_t)nv); break;
    case KT_ARP: t->arp = (uint8_t)nv; break;
    case KT_ARATE: t->arp_rr = (uint8_t)((t->arp_rr & 0xF0u) | ((uint32_t)nv + 4u) % 6u); break;
    case KT_ARANGE: t->arp_rr = (uint8_t)((t->arp_rr & 0x0Fu) | (((uint32_t)nv + 1u) % 3u) << 4); break;
    case KT_ACHORD: t->arp_cg = (uint8_t)((t->arp_cg & 0xF0u) | (uint32_t)nv); break;
    case KT_AGATE: t->arp_cg = (uint8_t)((t->arp_cg & 0x0Fu) | (((uint32_t)nv + 4u) % 10u) << 4); break;
    case KT_SCALEM: sc->tur.key = (uint8_t)((sc->tur.key & 0x0Fu) | (uint32_t)nv << 4); break;
    case KT_SWING: SEQ_LOCK(); sc->swing = (uint8_t)nv; { uint32_t k; for (k = 0; k < BIO_NORG; k++) sq.ver[k]++; } SEQ_UNLOCK(); break;
    case KT_QUANT: SEQ_LOCK(); t->quant = (uint8_t)nv; sq.ver[o]++; SEQ_UNLOCK(); break;
    case KT_QSTR: SEQ_LOCK(); t->qstr = (uint8_t)nv; sq.ver[o]++; SEQ_UNLOCK(); break;
    case KT_RECMODE: set.replace = (uint8_t)nv; sq.rec_replace = set.replace; break;
    case KT_STEPLEN: sc->steplen = (uint8_t)nv; break;
    case KT_KEYLED: set.keyled = (uint8_t)nv; break;
    case KT_MIDICH: set.midich = (uint8_t)nv; break;
    case KT_CURSOR: ui.cursor = (uint32_t)nv; ui.steppage = ui.cursor / 16u; break;
    case KT_TRLEN:
        SEQ_LOCK();
        t->len = (uint8_t)nv;
        sq.ver[o]++;
        SEQ_UNLOCK();
        if (ui.cursor >= t->len) { ui.cursor = t->len - 1u; ui.steppage = ui.cursor / 16u; }
        break;
    default: {                                  /* the events at the cursor, all of them */
        uint32_t m = cursor_events(), i;
        seq_begin(&sq);
        SEQ_LOCK();
        seq_touch(&sq, o);
        for (i = 0; i < 32u; i++)
            if (m >> i & 1u) {
                seq_ev_t *ev = &t->ev[i];
                if (n.type == KT_EVLEN) ev->len = (uint16_t)nv;
                if (n.type == KT_EVVEL) ev->vel = (uint8_t)nv;
                if (n.type == KT_EVCH) ev->fl = (uint8_t)((ev->fl & ~(7u << EV_CH_SH)) | (uint32_t)nv << EV_CH_SH);
                if (n.type == KT_EVRAT) ev->fl = (uint8_t)((ev->fl & ~(3u << EV_RAT_SH)) | (uint32_t)nv << EV_RAT_SH);
                if (n.type == KT_EVNUDGE) ev->start = (uint16_t)((int32_t)ui.cursor * 96 + nv);
                if (n.type == KT_EVNOTE) ev->note = (uint8_t)nv;
            }
        sq.ver[o]++;
        SEQ_UNLOCK();
    }
    }
    touched();
    ui.dirty_draw |= D_KNOBS | D_MAIN;
}

static uint32_t kit_same(uint32_t o, uint32_t v)   /* the organ is the kit's sound v (1..) as it is, but its PAN */
{
    uint32_t i;
    if (!v || v > BIO_NPERC_KIT) return 0;
    for (i = 0; i < BIO_NP; i++)
        if (i != P_PAN && (i != P_D8 || o != O_NOISE) && SC()->p[o][i] != BIO_PERC_KIT[v - 1u].p[i]) return 0;
    return 1;
}
static void knob_text(knob_t n, char *s)
{
    int32_t lo, hi, v = knob_get(n, &lo, &hi);
    char *p = s;
    *s = 0;
    switch (n.type) {
    case KT_NONE: return;
    case KT_P:
        if (n.idx == P_TUNE) { if (v > 24) *p++ = '+'; i2s(p, v - 24); return; }
        if (n.idx == P_PAN) { if (v == 50) str_cat(p, "C"); else { *p++ = v < 50 ? 'L' : 'R'; u2s(p, (uint32_t)bio_abs(v - 50) * 2u); } return; }
        if (org_perc(korg(n))) {              /* the perc voice's switches */
            if (n.idx == P_D1) { str_cat(p, BIO_PS_NAME[bio_ps_of((uint32_t)v)]); return; }
            if (n.idx == P_D3) { str_cat(p, v <= 8 ? "LOW" : v >= 92 ? "HIGH" : v >= 42 && v <= 58 ? "BAND" : v < 50 ? "LO>BD" : "BD>HI"); return; }
            if (n.idx == P_D4) { u2s(p, v < 25 ? 1u : v < 50 ? 2u : v < 75 ? 3u : 4u); return; }
            if (n.idx == P_D && !v) { str_cat(p, "HIT"); return; }
            if ((n.idx == P_E1 || n.idx == P_E2 || n.idx == P_E3) && !v) { str_cat(p, "OFF"); return; }
        }
        if (n.idx == P_D8 && korg(n) < 4u) { str_cat(p, v >= 66 ? "KICK" : v >= 33 ? "NOISE" : "TISSUE"); return; }
        if (n.idx == P_E1 && korg(n) >= 4u && korg(n) < O_NOISE) {   /* a synth's MODEL: its own, or a landscape */
            uint32_t m = bio_land_of((uint32_t)v);
            str_cat(p, m ? BIO_LAND_NAME[m] : BIO_OWN_NAME[korg(n) - 4u]);
            return;
        }
        break;
    case KT_SOUND:
        if (korg(n) >= 4u && korg(n) < O_NOISE) {
            str_cat(p, v ? BIO_LAND_NAME[v] : set.deep ? BIO_OWN_NAME[korg(n) - 4u] : OWN_PLAIN[korg(n) - 4u]);
            return;
        }
        /* fall through */
    case KT_KIT:
        if (!v) { str_cat(p, "TISSUE"); return; }
        p = str_cat(p, BIO_PERC_KIT[v - 1].name);
        if (!kit_same(korg(n), (uint32_t)v)) str_cat(p, "*");   /* turned since (or never it): marked */
        return;
    case KT_TLEN: u2s(s, BIO_TUR_LEN[v & 7]); return;
    case KT_TCLK: str_cat(s, BIO_TUR_CLK_NAME[v & 3]); return;
    case KT_TWAIL: { static const char *const W[4] = {"OFF", "SPASM", "VOICE", "BOTH"}; str_cat(s, W[v & 3]); return; }
    case KT_TRATTLE: if (!v) { str_cat(s, "OFF"); return; } break;
    case KT_DARK: case KT_RUMBLE: case KT_PUMP: case KT_WARP: case KT_RUPTURE: if (!v) { str_cat(s, "OFF"); return; } break;
    case KT_SBARS: if (!v) { str_cat(s, "OFF"); return; } u2s(s, SEQ_SONG_BARS[v & 7]); return;
    case KT_SNEXT: p = str_cat(s, "R0"); u2s(p, (uint32_t)v); return;
    case KT_FX:
        if (n.idx == 0u) { str_cat(s, BIO_DIV_NAME[v < 10 ? v : 4]); return; }
        if (n.idx == 3u) { str_cat(s, v ? "ON" : "OFF"); return; }
        if (n.idx == 16u) { str_cat(s, BIO_SPACE_NAME[v < (int32_t)BIO_NSPACE ? v : 0]); return; }
        break;
    case KT_BPM: p = u2s(p, (uint32_t)v / 10u); *p++ = '.'; u2s(p, (uint32_t)v % 10u); return;
    case KT_KEY: str_cat(s, NOTE_NAME[(2u + (uint32_t)v) % 12u]); return;
    case KT_ARP: str_cat(s, SEQ_ARP_NAME[(uint32_t)v < SEQ_NARP ? (uint32_t)v : 0u]); return;
    case KT_ARATE: str_cat(s, SEQ_ARP_RATE_NAME[(uint32_t)v % 6u]); return;
    case KT_ARANGE: p = u2s(s, (uint32_t)v); str_cat(p, " OCT"); return;
    case KT_ACHORD: str_cat(s, SEQ_CHORD_NAME[(uint32_t)v % SEQ_NCHORD]); return;
    case KT_AGATE: p = u2s(s, (uint32_t)v * 10u); str_cat(p, "%"); return;
    case KT_SCALEM: str_cat(s, BIO_SCALE_NAME[(uint32_t)v < BIO_NSCALE ? (uint32_t)v : 0u]); return;
    case KT_QUANT: str_cat(s, SEQ_QNAME[v < 5 ? v : 0]); return;
    case KT_RECMODE: str_cat(s, v ? "REPLACE" : "OVERDUB"); return;
    case KT_KEYLED: str_cat(s, v ? "OFF" : "ON"); return;
    case KT_MIDICH: if (!v) { str_cat(s, "OMNI"); return; } break;
    case KT_CURSOR: u2s(s, (uint32_t)v + 1u); return;
    case KT_EVLEN:
        if (hi == 0) { str_cat(s, "-"); return; }
        p = u2s(p, (uint32_t)v / 96u);
        if (v % 96) { *p++ = '+'; u2s(p, (uint32_t)(v % 96)); }
        return;
    case KT_EVCH: if (hi == 0) { str_cat(s, "-"); return; } p = u2s(s, (8u - (uint32_t)v) * 100u / 8u); str_cat(p, "%"); return;
    case KT_EVRAT: if (hi == 0) { str_cat(s, "-"); return; } u2s(s, (uint32_t)v + 1u); return;
    case KT_EVNOTE: if (hi == 0) { str_cat(s, "-"); return; } p = str_cat(p, NOTE_NAME[v % 12]); i2s(p, v / 12 - 1); return;
    case KT_EVVEL: case KT_EVNUDGE: if (hi == 0) { str_cat(s, "-"); return; } break;
    default: break;
    }
    i2s(s, v);
}

/* ---- the keys: what a key plays in the focus. The kit: four zones, the middle key of each the organ as it is set */
static const uint8_t KIT_ZONE[27] = {0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 2, 2, 3, 3, 3, 3, 3, 3};
static const uint8_t KIT_MID[4] = {3, 10, 17, 23};
static void key_map(uint32_t k, uint32_t *org, uint32_t *note)
{
    if (set.focus == F_KIT) {
        *org = KIT_ZONE[k];
        *note = (uint32_t)(60 + (int32_t)k - (int32_t)KIT_MID[*org]);
    } else {
        int32_t o = (int32_t)set.focus - 1, n = o < 4 ? 60 + (int32_t)k - 12 : 53 + (int32_t)k;
        n += set.oct[set.focus] * 12;
        *org = (uint32_t)o;
        *note = (uint32_t)bio_clamp(n, 0, 127);
    }
}
static int is_white(uint32_t k) { return !((0x54Au >> ((5u + k) % 12u)) & 1u); }   /* key 0 is F3 */
static uint32_t white_index(uint32_t k) { uint32_t i, n = 0; for (i = 0; i < k; i++) if (is_white(i)) n++; return n; }

/* where the transport is for a note played now: the frames since the audio interrupt last ran */
static uint32_t now_tick(uint32_t trk)
{
    uint32_t el = (app_ticks() - t_audio) * 147u / 80000u;   /* (24 MHz to 44.1 kHz) */
    return seq_now(&sq, trk, el > 512u ? 512u : el);
}

static void note_down(uint32_t org, uint32_t note, uint32_t vel)
{
    seq_live(&sq, org, note, vel, 1);
    if (org < 4u) ui.last_drum = org;
    ui.last_note[org] = note;
    ui.t_input = fm1_ms;
    if (sq.playing && sq.rec) {
        if (ui.shift) {                         /* ERASE as the playhead passes */
            sq.rec_trk = org;
            sq.erase_note = note;
        } else if (seq_rec_on(&sq, org, note, vel, now_tick(org)) < 0) {
            say("TRACK FULL");
        } else {
            touched();
        }
    } else if (ui.steprec && !sq.playing) {     /* STEP REC: the note at the cursor */
        seq_track_t *t = &SC()->tr[org];
        if (!ui.step_keys_down) seq_begin(&sq);
        ui.step_keys_down++;
        if (ui.shift) seq_step_erase(&sq, org, ui.cursor % t->len, note);
        else if (seq_step_write(&sq, org, ui.cursor % t->len, note, vel, SC()->steplen) < 0) say("TRACK FULL");
        touched();
        ui.dirty_draw |= D_MAIN | D_KNOBS;
    }
}
static void note_up(uint32_t org, uint32_t note)
{
    seq_live(&sq, org, note, 0, 0);
    if (sq.erase_note == note) sq.erase_note = 0xFFu;
    if (sq.playing && sq.rec) seq_rec_off(&sq, org, note, now_tick(org));
    if (ui.steprec && !sq.playing && ui.step_keys_down && !--ui.step_keys_down) {   /* all up: the cursor moves on */
        ui.cursor = (ui.cursor + SC()->steplen) % SC()->tr[cur_org()].len;
        ui.steppage = ui.cursor / 16u;
        ui.dirty_draw |= D_MAIN | D_KNOBS;
    }
}

#define K_MODE 0xFFFFu                          /* a key that is a page's button while held, not a note */
static void keys_in(void)
{
    uint32_t keys = fm1_in.notes, taps = fm1_input_note_edges() & ~keys & ~ui.keys_prev, k;
    uint32_t down = (keys & ~ui.keys_prev) | taps, up = (ui.keys_prev & ~keys) | taps;   /* (a tap shorter than a pass:
                                                   down and up in this one) */
    uint32_t seqh = fm1_in.buttons >> BTN_SEQ & 1u, lfoh = fm1_in.buttons >> BTN_LFO & 1u;
    uint32_t arph = fm1_in.buttons >> BTN_ARP & 1u, homeh = fm1_in.buttons >> BTN_HOME & 1u;
    ui.keys_prev = keys;
    if (down) ui.used |= fm1_in.buttons;
    for (k = 0; k < 27u; k++) {
        uint32_t b = 1u << k, w = is_white(k) ? white_index(k) : 99u;
        if (down & b) {
            if (seqh && w < 16u) {              /* the steps */
                ui.used |= 1u << BTN_SEQ;
                ui.held_steps |= 1u << w;
                ui.step_used = 0;
                continue;
            }
            if (lfoh && w < 5u) {               /* the nerve's buttons */
                ui.used |= 1u << BTN_LFO;
                if (w == 0u) sq.tur.write = 1;
                if (w == 1u) sq.tur.write = 2;
                if (w == 2u) { sq.tur_reset = 1; say("RESET"); }
                if (w == 3u) { uint32_t f = seq_fuse(&sq); say(f ? "FUSED" : "NOTHING TO FUSE"); if (f) touched(); }
                if (w == 4u) { SC()->tur.loop = SC()->tur.loop >= 97u ? 50u : 100u; say(SC()->tur.loop >= 97u ? "LOCK" : "UNLOCK"); touched(); }
                ui.note_on[k] = K_MODE;
                continue;
            }
            if (arph && w < 10u) {              /* BURST; 5 .. 10: SCAR (the last pressed plays; 9, 10: SCREW, DROP) */
                ui.used |= 1u << BTN_ARP;
                ui.burst |= 1u << w;
                if (w == 0u) ui.stutter_at = sq.ticks / SEQ_TPS * SEQ_TPS;
                if (w == 1u) sq.tur.invert = 1;
                if (w == 2u) eng.freeze = 1;
                if (w >= 4u) { eng.scar.want = SCAR_REP16 + (w - 4u); say(BIO_SCAR_NAME[SCAR_REP16 + (w - 4u)]); }
                ui.note_on[k] = K_MODE;
                continue;
            }
            if (homeh && w < 10u) {             /* the mutes (D45): 1-4 the drums' tracks, 5-9 the synths' and NOISE's
                                                   channels (silent at once), 10 the drums together */
                seq_scene_t *sc = SC();
                if (w < 4u) sc->tr[w].mute ^= 1u;
                else if (w < 9u) sc->mix.mute[BIO_ORG_CH[w]] ^= 1u;
                else sc->mix.mute[CH_DRUM] ^= 1u;
                touched();
                ui.used |= 1u << BTN_HOME;
                ui.dirty_draw |= D_MAIN;
                ui.note_on[k] = K_MODE;
                continue;
            }
            {
                uint32_t org, note;
                key_map(k, &org, &note);
                ui.note_on[k] = org << 8 | note | 1u << 16;   /* (bit 16: a note, so that HEART's 0 is not "none") */
                note_down(org, note, 100u);
            }
        }
        if (up & b) {
            if (w < 16u && (ui.held_steps >> w & 1u)) {   /* a step key let go: toggled, unless it set a lock */
                uint32_t o = cur_org(), st = ui.steppage * 16u + w;
                ui.held_steps &= ~(1u << w);
                if (!ui.step_used && st < SC()->tr[o].len) {
                    uint32_t note = o < 4u ? 60u : ui.last_note[o] ? ui.last_note[o] : (uint32_t)bio_clamp(65 + set.oct[o + 1u] * 12, 0, 127);
                    seq_begin(&sq);
                    seq_step_toggle(&sq, o, st, note, 100u);
                    touched();
                }
                ui.dirty_draw |= D_MAIN;
                continue;
            }
            if (ui.note_on[k] == K_MODE) {
                if (w == 0u || w == 1u) sq.tur.write = 0;
                if (w < 10u && (ui.burst >> w & 1u)) {
                    ui.burst &= ~(1u << w);
                    if (w == 1u) sq.tur.invert = 0;
                    if (w == 2u) eng.freeze = 0;
                    if (w >= 4u && eng.scar.want == SCAR_REP16 + (w - 4u)) {   /* back to another still held, or off */
                        uint32_t h = ui.burst >> 4 & 63u, j = 6;
                        while (j && !(h >> (j - 1u) & 1u)) j--;
                        eng.scar.want = j ? SCAR_REP16 + j - 1u : SCAR_OFF;
                    }
                }
                ui.note_on[k] = 0;
                continue;
            }
            if (ui.note_on[k]) {
                note_up(ui.note_on[k] >> 8 & 0xFFu, ui.note_on[k] & 0xFFu);
                ui.note_on[k] = 0;
            }
        }
    }
    if (!seqh) ui.held_steps = 0;
}

static void midi_in(void)
{
    uint8_t st, d1, d2;
    while (midi_get(&st, &d1, &d2)) {
        uint32_t ty = st & 0xF0u, ch = st & 15u, org;
        if (st == 0xFAu || st == 0xFBu) { if (!sq.playing) seq_play(&sq); ui.dirty_draw |= D_TOP; continue; }
        if (st == 0xFCu) { if (sq.playing) seq_stop(&sq); ui.dirty_draw |= D_TOP; continue; }
        if (ty != 0x90u && ty != 0x80u) continue;
        if (set.midich && ch > 9u && ch != (uint32_t)set.midich - 1u) continue;
        org = ch >= 1u && ch <= 9u ? ch - 1u : cur_org();
        if (ty == 0x90u && d2) note_down(org, d1, d2);
        else note_up(org, d1);
    }
}

static uint32_t idle_ok;
static void app_idle(void)                      /* while the screen's DMA runs: the keys and MIDI do not wait */
{
    if (!idle_ok) return;
    idle_ok = 0;
    keys_in();
    midi_in();
    idle_ok = 1;
}

/* ---- saving: each scene in two slots (A, B) with a generation number; the newer good one is read back */
typedef struct { uint32_t gen; seq_scene_t sc; } slot_t;
static uint32_t gen_of[SEQ_NSCENE], slot_of[SEQ_NSCENE];
static void scenes_load(void)
{
    slot_t *tmp = (slot_t *)(void *)cv_px;      /* (the canvas, before anything is drawn) */
    uint32_t i, k;
    _Static_assert(sizeof(slot_t) <= SLOT_MAX && sizeof(slot_t) <= CV_MAX * 2u, "a scene must fit a slot and the canvas");
    for (i = 0; i < SEQ_NSCENE; i++) {
        uint32_t best = ~0u, bg = 0;
        for (k = 0; k < 2u; k++)
            if (!slot_load(i * 2u + k, tmp, sizeof *tmp) && tmp->sc.magic == SEQ_MAGIC && (best == ~0u || tmp->gen > bg)) {
                best = k;
                bg = tmp->gen;
            }
        if (best != ~0u && !slot_load(i * 2u + best, tmp, sizeof *tmp)) {
            scenes[i] = tmp->sc;
            gen_of[i] = tmp->gen;
            slot_of[i] = best;
        } else {
            gen_of[i] = 0;
            slot_of[i] = 1;                     /* (the first save goes to A) */
        }
    }
}
static int scene_save(uint32_t i)
{
    slot_t *tmp = (slot_t *)(void *)cv_px;
    uint32_t k = slot_of[i] ^ 1u;
    int r;
    lcd_sync();
    tmp->gen = gen_of[i] + 1u;
    tmp->sc = scenes[i];
    r = slot_save(i * 2u + k, tmp, sizeof *tmp);
    if (!r) { gen_of[i]++; slot_of[i] = k; }
    ui.dirty_draw = D_ALL;                      /* (the canvas held the slot) */
    return r;
}
/* the four scenes before anything is saved: four of the factory rituals (bio_rituals.h), ABYSS first (D45) */
static const uint8_t DEFAULT_RITUAL[SEQ_NSCENE] = {2, 1, 0, 3};   /* ABYSS, RUST, THE AWAKENING, ASH */
static void defaults(void)
{
    uint32_t i;
    for (i = 0; i < SEQ_NSCENE; i++) BIO_RITUAL[DEFAULT_RITUAL[i]](&scenes[i]);
}
/* which SOUND an organ has (the kit's list, 1..; 0: its own body), for the SOUND knob */
static void kits_find(void)
{
    uint32_t o, i, j;
    for (o = 0; o < BIO_NORG; o++) {
        const uint8_t *p = SC()->p[o];
        if (o != O_NOISE && (o >= 4u || (!bio_is_perc(o, p) && !bio_is_kick(o, p)))) { ui.kit[o] = 0; continue; }
        if (!ui.kit[o]) ui.kit[o] = 1;
        for (i = 0; i < BIO_NPERC_KIT; i++) {
            for (j = 0; j < BIO_NP; j++)
                if (j != P_PAN && (j != P_D8 || o != O_NOISE) && p[j] != BIO_PERC_KIT[i].p[j]) break;
            if (j == BIO_NP) { ui.kit[o] = (uint8_t)(i + 1u); break; }
        }
    }
}
/* SHIFT + PRESETS: a factory ritual into the scene that plays (the one in flash stays until SAVE) */
static void factory_load(int32_t e)
{
    seq_scene_t *sc = SC();
    ui.factory = (uint32_t)(((int32_t)ui.factory + e % (int32_t)BIO_NRITUAL + (int32_t)BIO_NRITUAL) % (int32_t)BIO_NRITUAL);
    SEQ_LOCK();
    BIO_RITUAL[ui.factory](sc);
    {
        uint32_t k;
        for (k = 0; k < BIO_NORG; k++) sq.ver[k]++;
    }
    sq.nundo = sq.nredo = 0;                    /* (its tracks are not the ones undo kept) */
    sq.tur_reset = 1;
    SEQ_UNLOCK();
    kits_find();
    say(BIO_RITUAL_NAME[ui.factory]);
    touched();
    ui.dirty_draw = D_ALL;
}

/* ---- drawing */
static const char *sound_name(uint32_t o)       /* what a track sounds as, by name */
{
    if (o >= 4u && o < O_NOISE) {
        uint32_t m = bio_land_of(SC()->p[o][P_E1]);
        return m ? BIO_LAND_NAME[m] : set.deep ? BIO_OWN_NAME[o - 4u] : OWN_PLAIN[o - 4u];
    }
    if ((org_perc(o) || is_kick(o)) && ui.kit[o] && ui.kit[o] <= BIO_NPERC_KIT) return BIO_PERC_KIT[ui.kit[o] - 1u].name;
    return is_kick(o) ? "KICK" : org_perc(o) ? "NOISE" : "TISSUE";
}
static void clip_cpy(char *d, const char *s, uint32_t n) { while (n-- && *s) *d++ = *s++; *d = 0; }
static const char *loop_word(uint32_t l)
{
    return l >= 97u ? "LOCK" : l >= 60u ? "SLIP" : l > 40u ? "RANDOM" : l > 3u ? "INV SLIP" : "DBL LOCK";
}
/* MUTATE (D45): how far the register is from a locked loop, either way (LOOP 100 or 0: none; 50: all) */
static uint32_t mutate_of(uint32_t loop) { return 100u - (uint32_t)bio_abs((int32_t)loop - 50) * 2u; }
static const char *mutate_word(uint32_t m) { return !m ? "LOCKED" : m < 25u ? "SLIPS" : m < 70u ? "CHANGES" : "RANDOM"; }

/* the top: the logo; the track and its sound (DEEP: what just happened); the transport, the scene, the tempo */
static void draw_top(void)
{
    char s[40], *p;
    seq_scene_t *sc = SC();
    uint32_t recording = (sq.rec && sq.playing) || ui.steprec, o = cur_org(), n;
    int32_t x, j, xb;
    cv_at(0, 0, 240, 28);
    SPR(7, 5, LOGO, 16);                        /* (midway between the panel's lines: D30) */
    if (set.deep && ui.msg && fm1_ms - ui.t_msg < 1500u) {
        cv_t5(112, 5, ui.msg, COL_BONE, 1);
    } else if (set.deep && ui.t_loop && fm1_ms - ui.t_loop < 1500u) {
        p = str_cat(s, "LOOP ");
        p = u2s(p, sc->tur.loop);
        p = str_cat(p, "% ");
        str_cat(p, loop_word(sc->tur.loop));
        cv_t5(112, 5, s, COL_GREEN, 1);
    } else {
        x = cv_t5(112, 5, track_name(), COL_GREEN, 1) + 6;
        if (set.focus == F_KIT) x = cv_t5(x, 5, TRK_NAME[o], COL_BONE, 1) + 6;
        n = (uint32_t)(238 - x) / 6u;
        clip_cpy(s, sound_name(o), n < 39u ? n : 39u);
        cv_t5(x, 5, s, COL_FLESH, 1);
    }
    if (recording && fm1_ms - ui.t_writing < 300u)   /* the red pulse: notes are going in now */
        cv_fillb(108, 14, 130, 12, COL_RED, 70);
    if (sq.playing) for (j = 0; j < 4; j++) cv_rect(112 + j, 16 + j, 1, 8 - 2 * j, COL_GREEN);   /* play */
    else cv_rect(111, 17, 6, 6, COL_BONE_D);                                                 /* stop */
    for (j = 0; j < 6; j++) {                   /* rec */
        int32_t w = j == 0 || j == 5 ? 2 : 6;
        cv_rect(124 - w / 2, 17 + j, w, 1, recording ? COL_RED : COL_DIM);
    }
    x = 131;
    if (recording) x = cv_t5(x, 17, sq.rec && sq.playing ? (set.replace ? "REPL" : "LIVE") : "STEP", COL_RED, 1) + 5;
    p = u2s(s, sc->bpm10 / 10u);
    p = str_cat(p, ".");
    u2s(p, sc->bpm10 % 10u);
    xb = 236 - t5w(s, 1);
    cv_t5(xb, 17, s, COL_GREEN, 1);
    p = str_cat(s, "R0");
    p = u2s(p, sq.cur);
    if (sq.next != sq.cur) { p = str_cat(p, ">"); u2s(p, sq.next); }
    x = cv_t5(x, 17, s, ui.dirty[sq.cur] ? COL_RED : COL_BONE, 1) + 5;   /* (red: edited since it was kept) */
    if (sq.next == sq.cur && xb - 5 > x) {
        clip_cpy(s, sc->name, (uint32_t)(xb - 5 - x) / 6u);
        cv_t5(x, 17, s, COL_FLESH, 1);
    }
    cv_out();
}

/* the pages' tabs: SIMPLE's five (the one shown lit, two dots: its first page or the second), or DEEP's six */
static void draw_status(void)
{
    uint32_t i, n = set.deep ? PG_N : SP_N, cur = set.deep ? ui.page : ui.sp;
    int32_t w = set.deep ? 33 : 46, x0 = 3;
    cv_at(0, 29, 240, 18);
    for (i = 0; i < n; i++) {
        int32_t x = x0 + (int32_t)i * (w + 1);
        const char *nm = set.deep ? PG_NAME[i] : SP_NAME[i];
        if (i == cur) {
            cv_fillb(x, 2, w, 14, COL_GREEN_D, 150);
            cv_frame(x, 2, w, 14, COL_GREEN);
            cv_t5c(x + w / 2 - (set.deep ? 0 : 4), 6, nm, COL_GREEN, 1);
            if (!set.deep) {                    /* (which of its two pages) */
                cv_rect(x + w - 10, 8, 3, 3, ui.ssub ? COL_DIM : COL_GREEN);
                cv_rect(x + w - 6, 8, 3, 3, ui.ssub ? COL_GREEN : COL_DIM);
            }
        } else {
            cv_t5c(x + w / 2, 6, nm, COL_BONE_D, 1);
        }
    }
    if (set.deep) cv_t5(236 - t5w("DEEP", 1), 6, "DEEP", COL_FLESH, 1);
    cv_out();
}

static uint32_t is_layer(uint32_t b) { return b == BTN_HOME || b == BTN_ARP || b == BTN_LFO; }
/* SIMPLE's line under the picture: what the page is, what to do next; or what just happened */
static const char *hint(char *s)
{
    uint32_t o = cur_org();
    char *p;
    if (ui.help != NO_HELP)
        return ui.help == BTN_HOME ? "HOLD HOME, A WHITE KEY: MUTE IT" : ui.help == BTN_ARP ? "HOLD ARP, A WHITE KEY: BREAK IT"
             : ui.help == BTN_LFO ? "HOLD LFO: THE REGISTER'S KEYS" : "LET GO: BACK TO THE PAGE";
    if (fm1_in.buttons >> BTN_SEQ & 1u) {
        p = str_cat(s, "THE WHITE KEYS: STEPS OF ");
        str_cat(p, TRK_NAME[o]);
        return s;
    }
    if (ui.steprec && !sq.playing) return "STEP REC: A KEY WRITES AT THE STEP";
    switch (ui.sp) {
    case SP_PLAY:
        if (!ui.ssub) return ui.played ? "SELECT: A TRACK. K1-K4: ITS SOUND" : "PRESS PLAY. HOLD A BUTTON: WHAT IT DOES";
        p = str_cat(s, "DRUMS ");
        p = u2s(p, SC()->mix.lvl[CH_DRUM]);
        p = str_cat(p, "   CPU ");
        p = u2s(p, isr_load);
        if (core1_state == CORE1_OK) { p = str_cat(p, "+"); p = u2s(p, c1_load); }
        str_cat(p, "%");
        return s;
    case SP_STEPS: return ui.ssub ? "STEPS: HOW LONG THE TRACK IS" : "HOLD SEQ: THE KEYS ARE THE STEPS";
    case SP_SOUND:
        if (!ui.ssub) return has_arp(o) ? "K1: THE SOUND. EDIT AGAIN: ITS ARP" : "K1: THE SOUND. EDIT AGAIN: INSIDE IT";
        return has_arp(o) ? "ARP: HELD KEYS PLAY IN TURN" : "INSIDE THE DRUM. EDIT: BACK";
    case SP_FX:
        if (ui.ssub) return "THE DELAY'S TIME, THE REVERB'S ROOM";
        return o < 4u ? "THE DRUMS INTO THE EFFECTS" : "THIS TRACK INTO THE EFFECTS";
    default: return ui.ssub ? "EACH SCENE ITS BARS, THEN THE NEXT" : "GLO AGAIN: THE CHAIN OF SCENES";
    }
}
static void draw_hint(void)
{
    char s[48];
    if (set.deep) return;
    cv_at(0, 171, 240, 11);
    if (ui.msg && fm1_ms - ui.t_msg < 1500u) cv_t5c(120, 2, ui.msg, COL_BONE, 1);
    else cv_t5c(120, 2, hint(s), COL_FLESH, 1);
    cv_out();
}

static void draw_knobs(void)
{
    uint32_t k;
    char s[16];
    cv_at(0, 185, 240, 55);
    for (k = 0; k < 4u; k++) {
        knob_t n = knob_for(k);
        int32_t cx = 30 + (int32_t)k * 60, lo, hi, v;
        if (n.type == KT_NONE) { SPR(cx - 13, 12, EYE, 4); continue; }
        v = knob_get(n, &lo, &hi);
        cv_t5c(cx, 3, n.name, set.deep && (ui.shift || ui.latched) ? COL_FLESH : COL_BONE, 1);
        SPR(cx - 13, 12, EYE, 16);
        cv_arc(cx, 25, 16, hi > lo && v >= lo ? (uint32_t)((v - lo) * 100 / (hi - lo)) : 0u, COL_GREEN, COL_GROOVE);
        knob_text(n, s);
        cv_t5c(cx, 43, s, COL_GREEN, 1);
    }
    cv_out();
}

static uint32_t play_step(uint32_t trk)         /* the step that plays on a track (999: stopped) */
{
    uint32_t L = (uint32_t)SC()->tr[trk].len * SEQ_TPS;
    return sq.playing && L ? (sq.ticks % L) / SEQ_TPS : 999u;
}

static uint32_t trk_muted(uint32_t r) { return SC()->tr[r].mute || SC()->mix.mute[BIO_ORG_CH[r]]; }
static void draw_seq(uint32_t y0, uint32_t dy)  /* the grid: its rows from y0, dy apart (SIMPLE's a little closer) */
{
    seq_scene_t *sc = SC();
    uint32_t r, c, o = cur_org();
    char s[8];
    for (c = 0; c < 16u; c++) {                 /* the step numbers: the beats, and a dot for the playhead */
        uint32_t st = ui.steppage * 16u + c;
        if (!(c & 3u)) { u2s(s, st + 1u); cv_t5(40 + (int32_t)c * 12, (int32_t)y0 - 9, s, COL_BONE_D, 1); }
        if (st == play_step(o)) cv_rect(42 + (int32_t)c * 12, (int32_t)y0 - 2, 4, 1, COL_GREEN);
    }
    for (r = 0; r < BIO_NORG; r++) {
        seq_track_t *t = &sc->tr[r];
        uint32_t L = t->len, ph = play_step(r);
        uint32_t page = r == o ? ui.steppage : (sq.playing ? ph / 16u : 0u), y = y0 + r * dy;
        uint16_t lc = r == o || (set.focus == F_KIT && r < 4u) ? COL_GREEN : trk_muted(r) ? COL_DIM : COL_BONE_D;
        cv_t5(5, (int32_t)y + 3, TRK_NAME[r], lc, 1);
        for (c = 0; c < 16u; c++) {
            uint32_t st = page * 16u + c;
            int32_t x = 38 + (int32_t)c * 12;
            if (st >= L) { cv_fillb(x + 4, (int32_t)y + 5, 3, 3, COL_DIM, 120); continue; }
            if (seq_step_events(&sq, r, st)) SPR(x, (int32_t)y, CELL_ON, trk_muted(r) ? 6u : 16u);
            else SPR(x, (int32_t)y, CELL_OFF, trk_muted(r) ? 8u : 16u);
            if (t->lock[st][0] != BIO_NOLOCK || t->lock[st][1] != BIO_NOLOCK || t->lock[st][2] != BIO_NOLOCK ||
                t->lock[st][3] != BIO_NOLOCK)
                cv_rect(x + 9, (int32_t)y, 3, 3, COL_RED);
            if (st == ph) cv_frame(x - 1, (int32_t)y - 1, 14, 14, COL_GREEN);
            if (r == o && st == ui.cursor && (!sq.playing || ui.steprec)) cv_frame(x - 1, (int32_t)y - 1, 14, 14, ui.steprec ? COL_RED : COL_BONE);
        }
        if (L > 16u) { u2s(s, page + 1u); cv_t5(233, (int32_t)y + 3, s, COL_DIM, 1); }   /* which page of it */
    }
}

static void draw_mix(void)
{
    seq_scene_t *sc = SC();
    uint32_t ch;
    char s[24], *p;
    for (ch = 0; ch < BIO_NCH; ch++) {
        int32_t x = 6 + (int32_t)ch * 38, lv = ui.sub ? (int32_t)sc->mix.sb[ch] : (int32_t)sc->mix.lvl[ch], m = eng.meter[ch], seg, j;
        uint16_t nc = BIO_ORG_CH[cur_org()] == ch ? COL_GREEN : COL_BONE;
        cv_t5c(x + 17, 4, CH_NAME[ch], sc->mix.mute[ch] ? COL_DIM : nc, 1);
        seg = m > 64 ? (bio_log2((uint32_t)m) - (6 << 16)) / 37000 : 0;   /* 16 segments, -54 .. 0 dB */
        for (j = 0; j < 16; j++) cv_rect(x + 4, 103 - j * 5, 8, 4, j < seg ? (j > 13 ? COL_RED : COL_GREEN) : COL_GROOVE);
        cv_rect(x + 22, 24, 2, 82, COL_BONE_D);   /* the fader: a bone track, a flesh handle */
        for (j = 0; j < 82; j += 8) cv_rect(x + 19, 24 + j, 8, 1, COL_DIM);
        {
            int32_t hy = 24 + 82 - lv * 82 / 100 - 4;
            cv_rect(x + 17, hy, 12, 8, COL_RED_D);
            cv_frame(x + 17, hy, 12, 8, COL_FLESH);
            cv_rect(x + 18, hy + 3, 10, 2, COL_RED);
        }
        cv_frame(x + 8, 110, 18, 11, sc->mix.mute[ch] ? COL_RED : COL_DIM);
        cv_t5c(x + 17, 112, "M", sc->mix.mute[ch] ? COL_RED : COL_DIM, 1);
    }
    p = str_cat(s, ui.sub ? "REVERB SENDS  " : "");
    p = str_cat(p, "CPU ");
    p = u2s(p, isr_load);
    if (core1_state == CORE1_OK) {              /* (D38) the second core's share after it */
        p = str_cat(p, "+");
        p = u2s(p, c1_load);
    }
    str_cat(p, "%");
    cv_t5(232 - t5w(s, 1), 121, s, COL_DIM, 1);
}

static void draw_syn(void)
{
    uint32_t o = cur_org(), i;
    char s[20], *p;
    cv_t5(8, 5, TRK_NAME[o], COL_GREEN, 2);
    if (o < 4u || o == O_NOISE) {               /* what it is: a drum (and its SOUND, if noise), or NOISE's SOUND */
        int32_t x = 8 + t5w(TRK_NAME[o], 2) + 8;
        uint32_t kk = o < 4u && bio_is_kick(o, SC()->p[o]);
        if (o < 4u) { cv_t5(x, 11, "DRUM", COL_BONE_D, 1); x += t5w("DRUM", 1) + 6; }
        if ((org_perc(o) || kk) && kit_same(o, ui.kit[o])) cv_t5(x, 11, BIO_PERC_KIT[ui.kit[o] - 1u].name, COL_FLESH, 1);
        else if (o < 4u && (org_perc(o) || kk)) cv_t5(x, 11, kk ? "KICK" : "NOISE", COL_FLESH, 1);
    }
    if (ui.sub) cv_t5(200, 11, ui.sub == 1u ? "DEEP" : "ENV", COL_FLESH, 1);
    {                                           /* the scope: the organ's own output, now */
        uint32_t w = eng.scope_w;
        int32_t px = -1, py = 0, x;
        cv_fillb(6, 24, 228, 54, RGB(0, 0, 0), 90);
        cv_rect(6, 51, 228, 1, COL_GROOVE);
        for (x = 0; x < 228; x++) {
            int32_t v = eng.scope[(w - 228u + (uint32_t)x) & 255u], y = 51 - bio_clamp(v * 26 / 10000, -26, 26);
            if (px >= 0) cv_line(px, py, 6 + x, y, COL_GREEN);
            px = 6 + x;
            py = y;
        }
    }
    if (org_perc(o)) {                          /* noise as percussion: its source, the filter's shape and cutoff, its level */
        const uint8_t *pp = SC()->p[o];
        const bio_perc_t *v = o == O_NOISE ? &eng.noise : &eng.drum[o].np;
        int32_t a = bio_lf_hz(120), m = bio_lf_hz(2400), b = bio_lf_hz(14000), lf, x, px = -1, py = 0, q10;
        int32_t mode = pp[P_D3], lo = bio_lf_hz(40), hi = bio_lf_hz(18000);
        static const char *const L[4] = {"SOURCE", "MODE", "CUTOFF", "DECAY"};
        lf = pp[P_BODY] <= 50u ? a + (m - a) * (int32_t)pp[P_BODY] / 50 : m + (b - m) * ((int32_t)pp[P_BODY] - 50) / 50;
        if (pp[P_E2]) lf = bio_lf_hz(262) + (((int32_t)pp[P_BODY] * 5 + 50) / 100 - 2) * 65536;   /* TONE: on the note (C4 shown) */
        lf += ((int32_t)pp[P_TUNE] - 24) * BIO_SEMI;
        q10 = 5 + (int32_t)pp[P_D2] * (int32_t)pp[P_D2] / 50;   /* (as bio_perc_hit) */
        for (i = 0; i < 4u; i++) {
            uint32_t hz;
            cv_t5(10 + (int32_t)i * 57, 82, L[i], COL_BONE_D, 1);
            p = s;
            if (i == 0u) str_cat(p, BIO_PS_NAME[bio_ps_of(pp[P_D1])]);
            if (i == 1u) str_cat(p, mode <= 8 ? "LOW" : mode >= 92 ? "HIGH" : mode >= 42 && mode <= 58 ? "BAND" : mode < 50 ? "LO>BD" : "BD>HI");
            if (i == 2u && pp[P_E2]) {        /* TONE: the note, an octave or two over it */
                int32_t oc = ((int32_t)pp[P_BODY] * 5 + 50) / 100 - 2;
                p = str_cat(p, "NOTE");
                if (oc) { *p++ = oc > 0 ? '+' : '-'; u2s(p, (uint32_t)bio_abs(oc)); }
            } else if (i == 2u) {
                hz = bio_exp2(lf - (14 << 16)) >> 2;
                if (hz >= 1000u) { p = u2s(p, hz / 1000u); *p++ = '.'; p = u2s(p, hz % 1000u / 100u); str_cat(p, "K"); }
                else u2s(p, hz);
            }
            if (i == 3u) { p = u2s(p, bio_perc_ms(pp[P_DECAY], 4, 120, 3000)); str_cat(p, "MS"); }
            cv_t5(10 + (int32_t)i * 57, 92, s, COL_FLESH, 1);
        }
        /* the filter's shape over 40 Hz .. 18 kHz: 12 dB an octave, its peak by Q; the cutoff now (the sweep) */
        for (x = 0; x < 216; x++) {
            int32_t f = lo + (hi - lo) / 216 * x, d = (f - lf) / 4096, g, pk;   /* d: sixteenths of an octave */
            int32_t glp = d > 0 ? -12 * d : 0, ghp = d < 0 ? 12 * d : 0, gbp = -6 * bio_abs(d);
            g = mode <= 50 ? glp + (gbp - glp) * mode / 50 : gbp + (ghp - gbp) * (mode - 50) / 50;
            pk = bio_abs(d) < 8 ? (8 - bio_abs(d)) * (q10 > 7 ? q10 - 7 : 0) / 4 : 0;   /* (dB x 16) */
            g = bio_clamp((g + pk) / 16, -30, 24);
            {
                int32_t y = 112 - g * 14 / 30;
                if (px >= 0) cv_line(px, py, 12 + x, y, COL_FLESH);
                px = 12 + x;
                py = y;
            }
        }
        if (v->awake) {
            int32_t c = (v->lf0 + bio_mul30(v->sweep, v->fenv) - lo) * 216 / (hi - lo);
            cv_rect(12 + bio_clamp(c, 0, 215), 100, 1, 26, COL_GREEN);
            cv_rect(184, 128, 2 + (v->env >> 24) * 44 / 64, 3, v->gate ? COL_GREEN : COL_FLESH);
        }
    } else if (o < 4u && bio_is_kick(o, SC()->p[o])) {   /* the kick: its pitch, sweep, tail, tone; its level now */
        const uint8_t *pp = SC()->p[o];
        const bio_kick_t *v = &eng.drum[o].kk;
        static const char *const L[4] = {"PITCH", "SWEEP", "DECAY", "TONE"};
        int32_t a = bio_lf_hz(30), m = bio_lf_hz(48), b = bio_lf_hz(80);
        int32_t lf = pp[P_BODY] <= 50u ? a + (m - a) * (int32_t)pp[P_BODY] / 50 : m + (b - m) * ((int32_t)pp[P_BODY] - 50) / 50;
        int32_t lt = pp[P_D1] <= 50u ? bio_lf_hz(60) + (bio_lf_hz(220) - bio_lf_hz(60)) * (int32_t)pp[P_D1] / 50
                                     : bio_lf_hz(220) + (bio_lf_hz(2000) - bio_lf_hz(220)) * ((int32_t)pp[P_D1] - 50) / 50;
        for (i = 0; i < 4u; i++) {
            cv_t5(10 + (int32_t)i * 57, 82, L[i], COL_BONE_D, 1);
            p = s;
            if (i == 0u) { p = u2s(p, bio_exp2(lf + ((int32_t)pp[P_TUNE] - 24) * BIO_SEMI - (14 << 16)) >> 2); str_cat(p, "HZ"); }
            if (i == 1u) { p = u2s(p, (5u + (uint32_t)pp[P_TEXTURE] * 35u / 100u) / 10u); *p++ = '.';
                           p = u2s(p, (5u + (uint32_t)pp[P_TEXTURE] * 35u / 100u) % 10u); str_cat(p, " OCT"); }
            if (i == 2u) { p = u2s(p, bio_perc_ms(pp[P_DECAY], 80, 450, 3000)); str_cat(p, "MS"); }
            if (i == 3u) { p = u2s(p, bio_exp2(lt - (14 << 16)) >> 2); str_cat(p, "HZ"); }
            cv_t5(10 + (int32_t)i * 57, 92, s, COL_FLESH, 1);
        }
        if (v->awake) cv_rect(12, 112, 2 + (v->env >> 24) * 216 / 64, 4, COL_GREEN);
    } else if (o < 4u) {                        /* the body: its modes, ringing where they are; the excitation */
        bio_drum_t *d = &eng.drum[o];
        static const char *const C[4] = {"PRESS", "FRACT", "FRIC", "VENT"};
        for (i = 0; i < BIO_DRUM_DEF[o].nm; i++) {
            int32_t f = (bio_lf_hz(BIO_DRUM_DEF[o].m_f[i]) - bio_lf_hz(60)) * 216 / (bio_lf_hz(4000) - bio_lf_hz(60));
            int32_t a = d->awake ? (bio_abs(d->m[i].x1) + bio_abs(d->m[i].x2)) >> 15 : 0, h = bio_clamp(a, 1, 26);
            int32_t x = 12 + bio_clamp(f, 0, 216);
            cv_rect(x, 116 - h, 2, h, d->awake ? COL_GREEN : COL_DIM);
            u2s(s, BIO_DRUM_DEF[o].m_f[i]);
            cv_t5c(x, 120, s, COL_DIM, 1);
        }
        for (i = 0; i < 4u; i++) {
            uint32_t v = SC()->p[o][P_D1 + i];
            cv_t5(10 + (int32_t)i * 57, 82, C[i], COL_BONE_D, 1);
            cv_rect(10 + (int32_t)i * 57, 91, (int32_t)v * 44 / 100, 2, COL_FLESH);
        }
    } else if (o < O_NOISE) {                   /* a tonal organ: its envelope now, its note; its MODEL */
        bio_tone_t *v = &eng.tone[o - 4u];
        uint32_t md = bio_land_of(SC()->p[o][P_E1]);
        cv_t5(8 + t5w(TRK_NAME[o], 2) + 8, 11, md ? BIO_LAND_NAME[md] : BIO_OWN_NAME[o - 4u], md ? COL_FLESH : COL_BONE_D, 1);
        int32_t e = v->awake ? (v->env.v >> 24) : 0;
        cv_t5(10, 86, "ENV", COL_BONE_D, 1);
        cv_rect(34, 87, 2 + e * 120 / 64, 5, v->gate ? COL_GREEN : COL_FLESH);
        if (v->awake) {
            p = str_cat(s, NOTE_NAME[v->note % 12u]);
            i2s(p, (int32_t)v->note / 12 - 1);
            cv_t5(176, 84, s, COL_GREEN, 2);
        }
        if ((o == O_VOICE && !md) || md == L_CHOIR) cv_t5(10, 104, "U  O  A  E  I", COL_DIM, 1);   /* (the vowels: a throat's) */
    }
}

static void draw_mut(void)
{
    seq_scene_t *sc = SC();
    uint32_t i, r = sq.tur.r, L = BIO_TUR_LEN[sc->tur.len_i & 7u];
    char s[24], *p;
    for (i = 0; i < 16u; i++) {                 /* the spine: R's 16 bits, the newest on the right */
        uint32_t bit = 15u - i, on = (sq.tur.invert ? ~r : r) >> bit & 1u;
        int32_t x = 16 + (int32_t)i * 13;
        if (on) SPR(x, 6, VERT_ON, 16); else SPR(x, 6, VERT_OFF, bit < L ? 16u : 8u);
    }
    cv_rect(16 + (16 - (int32_t)L) * 13, 28, (int32_t)L * 13 - 1, 2, COL_FLESH);   /* LENGTH */
    p = u2s(s, sc->tur.loop);
    str_cat(p, "%");
    cv_ts(12, 38, &FONT_L, s, sc->tur.loop >= 97u ? COL_GREEN : COL_BONE);
    cv_t5(14, 74, loop_word(sc->tur.loop), sc->tur.loop >= 97u ? COL_GREEN : COL_FLESH, 2);
    p = str_cat(s, "CLOCK "); str_cat(p, BIO_TUR_CLK_NAME[sc->tur.clock & 3u]);
    cv_t5(140, 40, s, COL_BONE, 1);
    p = str_cat(s, "LENGTH "); u2s(p, L);
    cv_t5(140, 52, s, COL_BONE, 1);
    p = str_cat(s, "DEPTH "); p = u2s(p, sc->tur.depth); str_cat(p, "%");
    cv_t5(140, 64, s, COL_BONE, 1);
    cv_t5(140, 78, "CV8", COL_BONE_D, 1);
    cv_rect(162, 78, 64, 6, COL_GROOVE);
    cv_rect(162, 78, (int32_t)bio_tur_cv8(&sq.tur) / 4, 6, COL_GREEN);
    {
        static const char *const RN[4] = {"SCRAPE", "METAL", "AIR", "ROT"};
        uint8_t rv[4];
        rv[0] = sc->tur.r_scrape; rv[1] = sc->tur.r_metal; rv[2] = sc->tur.r_air; rv[3] = sc->tur.r_rot;
        for (i = 0; i < 4u; i++) {
            int32_t x = 10 + (int32_t)i * 57;
            cv_t5(x, 96, RN[i], rv[i] ? COL_BONE : COL_DIM, 1);
            cv_rect(x, 105, 44, 2, COL_GROOVE);
            cv_rect(x, 105, (int32_t)rv[i] * 44 / 100, 2, COL_GREEN_D);
        }
        p = str_cat(s, "RATTLE ");
        if (sc->tur.rattle) u2s(p, sc->tur.rattle); else str_cat(p, "OFF");
        cv_t5(10, 116, s, sc->tur.rattle ? COL_RED : COL_DIM, 1);
        p = str_cat(s, "WAIL ");
        str_cat(p, sc->tur.wail == 0u ? "OFF" : sc->tur.wail == 1u ? "SPASM" : sc->tur.wail == 2u ? "VOICE" : "BOTH");
        cv_t5(88, 116, s, sc->tur.wail ? COL_GREEN : COL_DIM, 1);
        p = str_cat(s, "RUPTURE ");             /* (D35) */
        if (sc->tur.rupture) u2s(p, sc->tur.rupture); else str_cat(p, "OFF");
        cv_t5(160, 116, s, sc->tur.rupture ? COL_RED : COL_DIM, 1);
    }
}

static void draw_fx(void)
{
    seq_scene_t *sc = SC();
    char s[48], *p;
    uint32_t t60 = sc->fx.r_decay <= 23u ? 250u + (uint32_t)sc->fx.r_decay * 2550u / 23u : 2800u + ((uint32_t)sc->fx.r_decay - 23u) * 9200u / 77u;
    cv_t5(10, 6, "BLOOD DELAY", sc->fx.d_mix ? COL_RED : COL_DIM, 2);
    p = str_cat(s, BIO_DIV_NAME[sc->fx.d_time < 10u ? sc->fx.d_time : 4u]);
    p = str_cat(p, "  FDBK "); p = u2s(p, sc->fx.d_fb);
    p = str_cat(p, sc->fx.d_ping ? "  PING" : "");
    p = str_cat(p, "  >REV "); u2s(p, sc->fx.d_rev);
    cv_t5(10, 24, s, COL_BONE, 1);
    cv_rect(10, 34, sc->fx.d_mix * 2, 3, COL_RED_D);
    cv_t5(10, 46, "BILE REVERB", sc->fx.r_mix ? COL_GREEN : COL_DIM, 2);
    cv_t5(150, 52, BIO_SPACE_NAME[sc->fx.r_space < BIO_NSPACE ? sc->fx.r_space : 0u], COL_FLESH, 1);
    p = str_cat(s, "DECAY "); p = u2s(p, t60 / 1000u); p = str_cat(p, "."); p = u2s(p, t60 / 100u % 10u);
    p = str_cat(p, "S  SIZE "); p = u2s(p, 50u + sc->fx.r_size * 130u / 100u);
    p = str_cat(p, "%  DAMP "); p = u2s(p, sc->fx.r_damp);
    if (eng.freeze) str_cat(p, "  FREEZE");
    cv_t5(10, 64, s, COL_BONE, 1);
    cv_rect(10, 74, sc->fx.r_mix * 2, 3, COL_GREEN_D);
    cv_t5(10, 86, "MARROW CRUSH", sc->fx.c_mix ? COL_FLESH : COL_DIM, 2);
    p = str_cat(s, "DRIVE "); p = u2s(p, sc->fx.c_drive);
    p = str_cat(p, "  RATE "); p = u2s(p, sc->fx.c_rate);
    p = str_cat(p, "  BITS "); p = u2s(p, 16u - sc->fx.c_bits * 11u / 100u);
    p = str_cat(p, "  FOLD "); u2s(p, sc->fx.c_fold);
    cv_t5(10, 104, s, COL_BONE, 1);
    cv_rect(10, 114, sc->fx.c_mix * 2, 3, COL_FLESH);
    if (sc->mix.warp) {                         /* (D35) */
        p = str_cat(s, "WARP "); u2s(p, sc->mix.warp);
        cv_t5(10, 122, s, COL_FLESH, 1);
        cv_t5(80, 122, "THE TAPE WAVERS", COL_DIM, 1);
    } else {
        cv_t5(10, 122, "SENDS A B C: HOME, HOME AGAIN", COL_DIM, 1);
    }
}

static void draw_glo(void)
{
    seq_scene_t *sc = SC();
    seq_track_t *t = &sc->tr[cur_org()];
    char s[48], *p;
    if (ui.sub == 2u) {                         /* SONG (D40): the four scenes, how long each plays and what follows */
        uint32_t i;
        cv_t5(12, 8, "SONG", COL_GREEN, 3);
        cv_t5(110, 10, "EACH SCENE ITS BARS,", COL_BONE_D, 1);
        cv_t5(110, 20, "THEN THE ONE AFTER IT", COL_BONE_D, 1);
        for (i = 0; i < SEQ_NSCENE; i++) {
            const seq_scene_t *c = &scenes[i];
            uint32_t on = i == sq.cur, y = 40u + i * 18u;
            p = str_cat(s, "R0"); p = u2s(p, i); p = str_cat(p, " ");
            p = str_cat(p, c->name[0] ? c->name : "-");
            cv_t5(14, (int32_t)y, s, on ? COL_GREEN : i == sq.next ? COL_FLESH : COL_BONE, 1);
            if (c->song_bars & 7u) {
                p = u2s(s, SEQ_SONG_BARS[c->song_bars & 7u]);
                p = str_cat(p, c->song_bars == 1u ? " BAR > R0" : " BARS > R0");
                u2s(p, c->song_next % SEQ_NSCENE);
            } else str_cat(s, "FOR EVER");
            cv_t5(120, (int32_t)y, s, c->song_bars ? (on ? COL_GREEN : COL_BONE) : COL_DIM, 1);
            if (on && (c->song_bars & 7u)) {   /* how far it is */
                uint32_t nb = SEQ_SONG_BARS[c->song_bars & 7u], w = (sq.sbars % nb) * 100u / nb;
                cv_rect(120, (int32_t)y + 9, (int32_t)w, 2, COL_GREEN_D);
            }
        }
        return;
    }
    if (ui.sub == 1u) {                         /* the notes (D31): the KEY, its SCALE, the notes of it */
        uint32_t r = (2u + bio_key_root(&sc->tur)) % 12u, m = bio_key_scale(&sc->tur), c = bio_scale_count(m), j;
        static const char *const W[4] = {"OFF", "SPASM", "VOICE", "SPASM VOICE"};
        cv_ts(12, 8, &FONT_L, NOTE_NAME[r], COL_GREEN);
        cv_t5(14, 44, "KEY", COL_BONE_D, 2);
        cv_t5(96, 12, BIO_SCALE_NAME[m], COL_BONE, 2);
        p = str_cat(s, c >= 12u ? "ALL TWELVE" : "");
        for (j = 0; j < c && c < 12u; j++) {
            p = str_cat(p, NOTE_NAME[(r + (uint32_t)bio_scale_note(m, j)) % 12u]);
            if (j + 1u < c) p = str_cat(p, " ");
        }
        cv_t5(96, 34, s, COL_FLESH, 1);
        p = str_cat(s, "THE SYNTHS AND NOISE IN ");
        p = str_cat(p, NOTE_NAME[r]);
        p = str_cat(p, bio_key_shift(&sc->tur) ? " (" : "");
        if (bio_key_shift(&sc->tur)) {
            p = str_cat(p, bio_key_shift(&sc->tur) > 0 ? "+" : "");
            p = i2s(p, bio_key_shift(&sc->tur));
            p = str_cat(p, ")");
        }
        cv_t5(14, 74, s, COL_BONE, 1);
        cv_t5(14, 88, "THE DRUMS AS THEY ARE", COL_DIM, 1);
        p = str_cat(s, "WAIL: "); str_cat(p, W[sc->tur.wail & 3u]);
        cv_t5(14, 102, s, sc->tur.wail ? COL_BONE : COL_DIM, 1);
        {                                       /* the focus's ARP (D33) */
            uint32_t o = cur_org();
            if (o >= 4u) {
                p = str_cat(s, "ARP "); p = str_cat(p, TRK_NAME[o]); p = str_cat(p, ": ");
                p = str_cat(p, SEQ_ARP_NAME[t->arp < SEQ_NARP ? t->arp : 0u]);
                if (t->arp) {
                    p = str_cat(p, " "); p = str_cat(p, SEQ_ARP_RATE_NAME[seq_arp_rate(t)]);
                    p = str_cat(p, " "); p = u2s(p, seq_arp_range(t)); p = str_cat(p, " OCT ");
                    p = str_cat(p, SEQ_CHORD_NAME[seq_arp_chord(t)]);
                }
            } else {
                str_cat(s, "ARP: THE SYNTHS AND NOISE");
            }
            cv_t5(14, 60, s, o >= 4u && t->arp ? COL_GREEN : COL_DIM, 1);
        }
        p = str_cat(s, "RITUAL 0"); p = u2s(p, sq.cur); p = str_cat(p, " "); str_cat(p, sc->name);
        cv_t5(14, 116, s, COL_FLESH, 1);
        return;
    }
    p = u2s(s, sc->bpm10 / 10u);
    p = str_cat(p, ".");
    u2s(p, sc->bpm10 % 10u);
    cv_ts(12, 8, &FONT_L, s, COL_GREEN);
    cv_t5(14, 44, "BPM", COL_BONE_D, 2);
    p = str_cat(s, "SWING "); p = u2s(p, sc->swing); str_cat(p, "%");
    cv_t5(130, 12, s, COL_BONE, 1);
    p = str_cat(s, "QUANT "); p = str_cat(p, SEQ_QNAME[t->quant < 5u ? t->quant : 0u]);
    p = str_cat(p, " "); p = u2s(p, t->qstr); str_cat(p, "%");
    cv_t5(130, 26, s, COL_BONE, 1);
    cv_t5(130, 36, TRK_NAME[cur_org()], COL_DIM, 1);
    cv_t5(14, 74, set.replace ? "REC: REPLACE" : "REC: OVERDUB", set.replace ? COL_RED : COL_BONE, 1);
    p = str_cat(s, "STEP REC LENGTH "); u2s(p, sc->steplen);
    cv_t5(14, 88, s, COL_BONE, 1);
    p = str_cat(s, "UNDO "); p = u2s(p, sq.nundo); p = str_cat(p, "  REDO "); u2s(p, sq.nredo);
    cv_t5(14, 102, s, COL_DIM, 1);
    p = str_cat(s, "RITUAL 0"); p = u2s(p, sq.cur); p = str_cat(p, " "); str_cat(p, sc->name);
    cv_t5(14, 116, s, COL_FLESH, 1);
    cv_t5(232 - t5w(APP_VER, 1), 116, APP_VER, COL_DIM, 1);
}

/* ---- SIMPLE's pictures (D45): into the main canvas, 240 x 122 */
/* PLAY: the nine tracks, each its sound and the sixteen steps it plays now; the track the knobs turn lit */
static void draw_s_play(void)
{
    seq_scene_t *sc = SC();
    uint32_t r, c, o = cur_org();
    char s[12];
    for (r = 0; r < BIO_NORG; r++) {
        seq_track_t *t = &sc->tr[r];
        int32_t y = 3 + (int32_t)r * 13;
        uint32_t on = set.focus == F_KIT ? r < 4u : r == o, mu = trk_muted(r), L = t->len, ph = play_step(r);
        uint32_t page = ph != 999u ? ph / 16u : 0u;
        if (on) cv_fillb(3, y - 2, 234, 12, COL_GREEN_D, 130);
        cv_t5(7, y, TRK_NAME[r], mu ? COL_RED : on ? COL_GREEN : COL_BONE, 1);   /* (red: muted) */
        clip_cpy(s, sound_name(r), 10u);
        cv_t5(42, y, s, mu ? COL_DIM : on ? COL_BONE : COL_FLESH, 1);
        for (c = 0; c < 16u; c++) {
            uint32_t st = page * 16u + c;
            int32_t x = 107 + (int32_t)c * 8;
            if (st >= L) { cv_rect(x + 2, y + 3, 2, 2, COL_DIM); continue; }
            if (seq_step_events(&sq, r, st)) cv_rect(x, y, 6, 7, st == ph ? COL_BONE : mu ? COL_GREEN_D : COL_GREEN);
            else {
                cv_rect(x, y, 6, 7, (c & 3u) ? COL_GROOVE : mix565(COL_GROOVE, COL_DIM, 110));
                if (st == ph) cv_frame(x, y, 6, 7, COL_BONE_D);
            }
        }
    }
}

/* MIX: each track's level (and its channel's meter under it), its pan; muted ones marked */
static void draw_s_mix(void)
{
    seq_scene_t *sc = SC();
    uint32_t r, o = cur_org();
    for (r = 0; r < BIO_NORG; r++) {
        int32_t y = 3 + (int32_t)r * 13, ch = BIO_ORG_CH[r], m = eng.meter[ch], seg;
        int32_t lv = r < 4u ? sc->p[r][P_LEVEL] : sc->mix.lvl[ch], pan = sc->p[r][P_PAN];
        uint32_t on = set.focus == F_KIT ? r < 4u : r == o, mu = trk_muted(r);
        if (on) cv_fillb(3, y - 2, 234, 12, COL_GREEN_D, 130);
        cv_t5(7, y, TRK_NAME[r], on ? COL_GREEN : mu ? COL_DIM : COL_BONE, 1);
        cv_rect(44, y + 1, 100, 4, COL_GROOVE);
        cv_rect(44, y + 1, lv, 4, mu ? COL_DIM : on ? COL_GREEN : COL_BONE_D);
        seg = m > 64 ? (bio_log2((uint32_t)m) - (6 << 16)) / 37000 : 0;   /* (as DEEP's MIX: 16 steps, -54 .. 0 dB) */
        seg = bio_clamp(seg, 0, 16);
        cv_rect(44, y + 6, seg * 100 / 16, 1, seg > 13 ? COL_RED : COL_GREEN);
        cv_rect(156, y + 3, 50, 1, COL_GROOVE);
        cv_rect(155 + pan / 2, y + 1, 3, 5, mu ? COL_DIM : COL_FLESH);
        if (mu) cv_t5(214, y, "M", COL_RED, 1);
    }
}

/* SOUND: the list of sounds about the one it has, a line on that one; the organ's own output now */
static const char *sound_of(uint32_t o, int32_t j, int desc)
{
    if (o >= 4u && o < O_NOISE) return j ? (desc ? LAND_DESC[j] : BIO_LAND_NAME[j]) : desc ? OWN_DESC[o - 4u] : OWN_PLAIN[o - 4u];
    if (!j) return desc ? "A BODY OF FLESH: STRUCK, IT RINGS" : "TISSUE";
    return desc ? KIT_DESC[j - 1] : BIO_PERC_KIT[j - 1].name;
}
static void draw_scope(int32_t y0, int32_t h)
{
    uint32_t w = eng.scope_w;
    int32_t px = -1, py = 0, x, mid = y0 + h / 2;
    cv_fillb(6, y0, 228, h, RGB(0, 0, 0), 90);
    cv_rect(6, mid, 228, 1, COL_GROOVE);
    for (x = 0; x < 228; x++) {
        int32_t v = eng.scope[(w - 228u + (uint32_t)x) & 255u], y = mid - bio_clamp(v * (h / 2) / 10000, -(h / 2), h / 2);
        if (px >= 0) cv_line(px, py, 6 + x, y, COL_GREEN);
        px = 6 + x;
        py = y;
    }
}
static void draw_s_sound(void)
{
    uint32_t o = cur_org();
    knob_t n = {KT_SOUND, 0, BIO_NORG, ""};
    int32_t lo, hi, v = knob_get(n, &lo, &hi), i;
    char s[16], *p;
    cv_t5(234 - t5w(ORG_NAME[o], 1), 2, ORG_NAME[o], COL_DIM, 1);   /* the organ it is (the decoration) */
    for (i = -2; i <= 2; i++) {
        int32_t j = v + i;
        if (j < lo || j > hi) continue;
        if (!i) {
            cv_fillb(24, 24, 192, 20, COL_GREEN_D, 150);
            cv_frame(24, 24, 192, 20, COL_GREEN);
            p = str_cat(s, sound_of(o, j, 0));
            if ((o < 4u || o == O_NOISE) && j && !kit_same(o, (uint32_t)j)) str_cat(p, "*");   /* (turned since) */
            cv_t5c(120, 27, s, COL_GREEN, 2);
        } else {
            cv_t5c(120, i < 0 ? 24 + i * 11 : 37 + i * 11, sound_of(o, j, 0), i == -1 || i == 1 ? COL_BONE_D : COL_DIM, 1);
        }
    }
    cv_t5c(120, 72, sound_of(o, v, 1), COL_BONE, 1);
    draw_scope(86, 34);
}

/* SOUND again: a synth's ARP (the notes it would play, drawn), a drum's inside */
static void draw_s_sound2(void)
{
    uint32_t o = cur_org(), k;
    char s[40], *p;
    if (has_arp(o)) {
        seq_track_t *t = &SC()->tr[o];
        uint32_t a = t->arp < SEQ_NARP ? t->arp : 0u, rg = seq_arp_range(t), nn = 3u + (seq_arp_chord(t) == 1u || seq_arp_chord(t) == 2u);
        cv_t5(8, 4, "ARP", a ? COL_GREEN : COL_BONE_D, 3);
        cv_t5(70, 4, SEQ_ARP_NAME[a], a ? COL_GREEN : COL_DIM, 2);
        p = str_cat(s, SEQ_ARP_RATE_NAME[seq_arp_rate(t)]);
        p = str_cat(p, "  ");
        p = u2s(p, rg);
        p = str_cat(p, rg == 1u ? " OCTAVE  " : " OCTAVES  ");
        str_cat(p, SEQ_CHORD_NAME[seq_arp_chord(t)]);
        cv_t5(70, 22, s, a ? COL_BONE : COL_DIM, 1);
        if (!a) { cv_t5c(120, 64, "OFF: THE KEYS PLAY AS THEY ARE", COL_FLESH, 1); return; }
        for (k = 0; k < 16u; k++) {             /* sixteen of its notes, low to high */
            uint32_t m = rg * nn, i = k % (m ? m : 1u), h;
            if (a == 2u) i = m - 1u - i;
            if (a == 3u) { uint32_t q = k % (2u * m - 2u > 0u ? 2u * m - 2u : 1u); i = q < m ? q : 2u * m - 2u - q; }
            if (a == 4u) i = (sq.tur.r >> k ^ sq.tur.r >> (k + 5u)) % (m ? m : 1u);
            h = 6u + i * 60u / (m > 1u ? m - 1u : 1u);
            if (a == 5u) {                      /* CHORD: the notes together */
                uint32_t c;
                for (c = 0; c < nn; c++) cv_rect(10 + (int32_t)k * 14, 104 - (int32_t)c * 16, 10, 12, COL_GREEN);
                continue;
            }
            cv_rect(14 + (int32_t)k * 14, 118 - (int32_t)h, 2, (int32_t)h, COL_GROOVE);
            cv_rect(10 + (int32_t)k * 14, 112 - (int32_t)h, 10, 6, COL_GREEN);
        }
        return;
    }
    cv_t5(8, 2, "INSIDE", COL_BONE, 2);         /* a drum: what it is made of */
    cv_t5(90, 7, sound_name(o), COL_FLESH, 1);
    for (k = 0; k < 4u; k++) {
        int32_t y = 24 + (int32_t)k * 15, v = SC()->p[o][P_D1 + k];
        cv_t5(10, y, inside_name(o, k), COL_BONE_D, 1);
        cv_rect(70, y + 1, 150, 5, COL_GROOVE);
        cv_rect(70, y + 1, v * 150 / 100, 5, COL_FLESH);
    }
    draw_scope(86, 34);
}

/* FX: the track into the three effects (their own names under them, the decoration), and the tape */
static void draw_s_fx(void)
{
    seq_scene_t *sc = SC();
    uint32_t ch = BIO_ORG_CH[cur_org()], k;
    uint32_t t60 = sc->fx.r_decay <= 23u ? 250u + (uint32_t)sc->fx.r_decay * 2550u / 23u : 2800u + ((uint32_t)sc->fx.r_decay - 23u) * 9200u / 77u;
    static const char *const N[4] = {"DELAY", "REVERB", "CRUSH", "TAPE"}, *const OLD[4] = {"BLOOD", "BILE", "MARROW", "WARP"};
    char s[40], *p;
    for (k = 0; k < 4u; k++) {
        int32_t y = 3 + (int32_t)k * 30;
        uint32_t send = k == 0u ? sc->mix.sa[ch] : k == 1u ? sc->mix.sb[ch] : k == 2u ? sc->mix.sc[ch] : sc->mix.warp;
        uint32_t ret = k == 0u ? sc->fx.d_mix : k == 1u ? sc->fx.r_mix : k == 2u ? sc->fx.c_mix : 1u;
        uint16_t c = k == 0u ? COL_RED : k == 1u ? COL_GREEN : COL_FLESH;
        cv_t5(8, y, N[k], ret ? COL_BONE : COL_DIM, 2);
        cv_t5(8, y + 16, OLD[k], COL_DIM, 1);
        cv_rect(100, y + 3, 130, 6, COL_GROOVE);
        cv_rect(100, y + 3, (int32_t)send * 130 / 100, 6, ret ? c : COL_DIM);
        p = s;
        if (!ret) str_cat(p, "OFF: TURN UP TO WAKE");
        else if (k == 0u) { p = str_cat(p, BIO_DIV_NAME[sc->fx.d_time < 10u ? sc->fx.d_time : 4u]); p = str_cat(p, "  FEEDBACK "); u2s(p, sc->fx.d_fb); }
        else if (k == 1u) {
            p = str_cat(p, BIO_SPACE_NAME[sc->fx.r_space < BIO_NSPACE ? sc->fx.r_space : 0u]);
            p = str_cat(p, "  "); p = u2s(p, t60 / 1000u); p = str_cat(p, "."); p = u2s(p, t60 / 100u % 10u); str_cat(p, " S");
        }
        else if (k == 2u) { p = str_cat(p, "DRIVE "); p = u2s(p, sc->fx.c_drive); p = str_cat(p, "  BITS "); u2s(p, 16u - sc->fx.c_bits * 11u / 100u); }
        else str_cat(p, send ? "THE WHOLE OUTPUT WAVERS" : "STILL");
        cv_t5(100, y + 14, s, ret ? COL_FLESH : COL_DIM, 1);
    }
}

/* FX again: the delay's echoes as they fall (its time, its feedback), the room and how long it rings */
static void draw_s_fx2(void)
{
    seq_scene_t *sc = SC();
    uint32_t t60 = sc->fx.r_decay <= 23u ? 250u + (uint32_t)sc->fx.r_decay * 2550u / 23u : 2800u + ((uint32_t)sc->fx.r_decay - 23u) * 9200u / 77u;
    int32_t sp = BIO_DIV[sc->fx.d_time < 10u ? sc->fx.d_time : 4u] * 110 / 384, x, h = 36, j;
    char s[40], *p;
    cv_t5(8, 2, "DELAY", COL_BONE, 2);
    p = str_cat(s, BIO_DIV_NAME[sc->fx.d_time < 10u ? sc->fx.d_time : 4u]);
    p = str_cat(p, "  FEEDBACK ");
    u2s(p, sc->fx.d_fb);
    cv_t5(76, 2, s, COL_FLESH, 1);
    if (sc->fx.d_ping) cv_t5(76, 12, "PING-PONG: LEFT, RIGHT", COL_BONE_D, 1);
    if (sp < 3) sp = 3;
    cv_rect(8, 60, 224, 1, COL_GROOVE);
    for (x = 8, j = 0; x < 232 && h > 1 && j < 80; x += sp, j++) {   /* the hit, then each echo a little lower */
        cv_rect(x, 60 - h, 3, h, j ? COL_RED : COL_BONE);
        h = h * (int32_t)(sc->fx.d_fb > 85u ? 85u : sc->fx.d_fb) / 100;
    }
    cv_t5(8, 66, "REVERB", COL_BONE, 2);
    p = str_cat(s, BIO_SPACE_NAME[sc->fx.r_space < BIO_NSPACE ? sc->fx.r_space : 0u]);
    p = str_cat(p, "  ");
    p = u2s(p, t60 / 1000u); p = str_cat(p, "."); p = u2s(p, t60 / 100u % 10u);
    str_cat(p, " S");
    cv_t5(88, 71, s, COL_FLESH, 1);
    {                                           /* its tail: 0 .. 12 s across */
        int32_t w = (int32_t)(t60 * 224u / 12000u), i;
        for (i = 0; i < w && i < 224; i++) {
            int32_t hh = 30 - i * 30 / (w ? w : 1);
            cv_rect(8 + i, 118 - hh, 1, hh, (i & 1) ? COL_GREEN_D : COL_GREEN);
        }
    }
}

/* SONG: the tempo, the swing; the key, its scale and its notes */
static void draw_s_song(void)
{
    seq_scene_t *sc = SC();
    uint32_t r = (2u + bio_key_root(&sc->tur)) % 12u, m = bio_key_scale(&sc->tur), c = bio_scale_count(m), j;
    char s[48], *p;
    p = u2s(s, sc->bpm10 / 10u);
    p = str_cat(p, ".");
    u2s(p, sc->bpm10 % 10u);
    cv_ts(10, 2, &FONT_L, s, COL_GREEN);
    cv_t5(12, 38, "BPM", COL_BONE_D, 1);
    p = str_cat(s, "SWING "); p = u2s(p, sc->swing); str_cat(p, "%");
    cv_t5(140, 8, s, COL_BONE, 1);
    p = str_cat(s, "MUTATE "); p = u2s(p, mutate_of(sc->tur.loop)); str_cat(p, "%");
    cv_t5(140, 20, s, COL_BONE_D, 1);
    cv_t5(140, 32, mutate_word(mutate_of(sc->tur.loop)), COL_DIM, 1);
    cv_ts(10, 52, &FONT_L, NOTE_NAME[r], COL_GREEN);
    cv_t5(12, 88, "KEY", COL_BONE_D, 1);
    cv_t5(60, 56, BIO_SCALE_NAME[m], COL_BONE, 2);
    p = str_cat(s, c >= 12u ? "ALL TWELVE" : "");
    for (j = 0; j < c && c < 12u; j++) {
        p = str_cat(p, NOTE_NAME[(r + (uint32_t)bio_scale_note(m, j)) % 12u]);
        if (j + 1u < c) p = str_cat(p, " ");
    }
    cv_t5(60, 76, s, COL_FLESH, 1);
    cv_t5(60, 88, "THE SYNTHS FOLLOW IT", COL_DIM, 1);
    p = str_cat(s, "SCENE 0"); p = u2s(p, sq.cur); p = str_cat(p, " "); str_cat(p, sc->name);
    cv_t5(12, 108, s, COL_FLESH, 1);
}

/* SONG again: the chain, the four scenes: how long each plays and which follows */
static void draw_s_chain(void)
{
    uint32_t i;
    char s[48], *p;
    cv_t5(8, 2, "CHAIN", COL_GREEN, 2);
    cv_t5(84, 7, "ITS BARS, THEN WHICH", COL_BONE_D, 1);
    for (i = 0; i < SEQ_NSCENE; i++) {
        const seq_scene_t *c = &scenes[i];
        uint32_t on = i == sq.cur, y = 28u + i * 23u;
        p = str_cat(s, "R0"); p = u2s(p, i); p = str_cat(p, " ");
        p = str_cat(p, c->name[0] ? c->name : "-");
        if (on) cv_fillb(4, (int32_t)y - 3, 232, 19, COL_GREEN_D, 110);
        cv_t5(10, (int32_t)y, s, on ? COL_GREEN : i == sq.next ? COL_FLESH : COL_BONE, 1);
        if (c->song_bars & 7u) {
            p = u2s(s, SEQ_SONG_BARS[c->song_bars & 7u]);
            p = str_cat(p, c->song_bars == 1u ? " BAR > R0" : " BARS > R0");
            u2s(p, c->song_next % SEQ_NSCENE);
        } else str_cat(s, "FOR EVER");
        cv_t5(124, (int32_t)y, s, c->song_bars ? (on ? COL_GREEN : COL_BONE) : COL_DIM, 1);
        if (on && (c->song_bars & 7u)) {        /* how far it is */
            uint32_t nb = SEQ_SONG_BARS[c->song_bars & 7u], w = (sq.sbars % nb) * 100u / nb;
            cv_rect(124, (int32_t)y + 10, 100, 2, COL_GROOVE);
            cv_rect(124, (int32_t)y + 10, (int32_t)w, 2, COL_GREEN);
        }
    }
}

/* ---- holding a button (D45): its keys as tiles (HOME's mutes, ARP's breaks, LFO's register), or what it does */
static void draw_tiles(uint32_t b)
{
    static const char *const ARP_T[10] = {"STUTTER", "INVERT", "FREEZE", "SPASM", "REPEAT 1/16", "REPEAT 1/8", "REVERSE",
                                          "SMEAR", "SCREW", "DROP"};
    static const char *const LFO_T[5] = {"WRITE 0", "WRITE 1", "RESET", "FUSE", "LOCK"};
    seq_scene_t *sc = SC();
    uint32_t n = b == BTN_LFO ? 5u : 10u, i;
    char s[4];
    cv_fillb(0, 0, 240, 134, RGB(0, 0, 0), 120);
    for (i = 0; i < n; i++) {
        int32_t x = 4 + (int32_t)(i % 4u) * 58, y = 2 + (int32_t)(i / 4u) * 30;
        const char *nm = b == BTN_HOME ? (i < 9u ? TRK_NAME[i] : "DRUMS") : b == BTN_ARP ? ARP_T[i] : LFO_T[i];
        uint32_t lit = b == BTN_HOME ? (i < 4u ? sc->tr[i].mute : i < 9u ? sc->mix.mute[BIO_ORG_CH[i]] : sc->mix.mute[CH_DRUM])
                     : b == BTN_ARP ? ui.burst >> i & 1u
                     : i < 2u ? sq.tur.write == i + 1u : i == 4u ? sc->tur.loop >= 97u : 0u;
        uint16_t c = b == BTN_HOME ? COL_RED : COL_GREEN;
        const char *sp = nm;
        while (*sp && *sp != ' ') sp++;
        cv_fillb(x, y, 56, 28, lit ? (b == BTN_HOME ? COL_RED_D : COL_GREEN_D) : RGB(18, 18, 22), 225);
        cv_frame(x, y, 56, 28, lit ? c : COL_DIM);
        u2s(s, i + 1u);
        cv_t5(x + 3, y + 3, s, COL_DIM, 1);
        if (*sp) {                              /* two words: two lines */
            char w1[12];
            clip_cpy(w1, nm, (uint32_t)(sp - nm));
            cv_t5c(x + 28, y + 9, w1, lit ? c : COL_BONE, 1);
            cv_t5c(x + 28, y + 18, sp + 1, lit ? c : COL_BONE, 1);
        } else {
            cv_t5c(x + 28, y + 13, nm, lit ? c : COL_BONE, 1);
        }
        if (b == BTN_HOME && lit) cv_t5(x + 46, y + 3, "M", COL_RED, 1);
    }
}

static const char *help_text(uint32_t b)
{
    switch (b) {
    case BTN_OCT_DOWN: return "OCT-\nTHE KEYS AN OCTAVE DOWN.\n\nSEL + OCT-: UNDO.\nSEQ HELD: THE STEPS' PAGE BEFORE.";
    case BTN_OCT_UP: return "OCT+\nTHE KEYS AN OCTAVE UP.\n\nSEL + OCT+: REDO.\nSEQ HELD: THE STEPS' NEXT PAGE.";
    case BTN_FX: return "FX\nTHE TRACK INTO THE DELAY, THE\nREVERB AND THE CRUSHER; TAPE\nWAVERS IT ALL.\nFX AGAIN: THE DELAY'S TIME AND\nFEEDBACK, THE ROOM, ITS TAIL.";
    case BTN_SEL: return "SEL\nHOLD IT, THEN:\nOCT- UNDO       OCT+ REDO\nPRESETS  A FACTORY SCENE\nREC  OVERDUB OR REPLACE\nSAVE EVERY SCENE  PLAY SILENCE\nEDIT  DEEP: EVERY PAGE";
    case BTN_ENV: return "ENV\nTHE SOUND PAGE: THE SOUND,\nITS TUNE, ATTACK, RELEASE.";
    case BTN_EDIT: return "EDIT: SOUND\nK1 PICKS THE TRACK'S SOUND;\nITS TUNE, ATTACK, RELEASE.\nEDIT AGAIN: ITS ARP\n(A DRUM: WHAT IT IS MADE OF).\nSEL + EDIT: DEEP, EVERY PAGE.";
    case BTN_GLO: return "GLO: SONG\nTEMPO, SWING, KEY, SCALE.\nGLO AGAIN: THE CHAIN: HOW MANY\nBARS THIS SCENE PLAYS, AND\nWHICH SCENE FOLLOWS IT.";
    case BTN_SAVE: return "SAVE\nTHIS SCENE INTO THE FM-1,\nWHEN IT IS STOPPED AND QUIET.\nSEL + SAVE: EVERY EDITED SCENE.\nLEFT ALONE, STOPPED, IT SAVES.";
    case BTN_SEQ: return "SEQ: STEPS\nHOLD IT: THE WHITE KEYS ARE THE\nTRACK'S STEPS. HOLD A STEP, TURN\nK1-K4 ON PLAY: A LOCK THERE.\nSEQ AGAIN: THE TRACK'S LENGTH,\nA STEP'S REPEATS, NUDGE, NOTE.";
    case BTN_PLAY: return "PLAY\nSTART, STOP.\n\nREC + PLAY, STOPPED: RECORD\nFROM THE START.\nSEL + PLAY: SILENCE (PANIC).";
    case BTN_REC: return "REC\nPLAYING: WHAT YOU PLAY GOES\nINTO THE TRACK.\nSTOPPED: STEP REC: A KEY WRITES\nAT THE STEP (K1), THEN THE NEXT.\nSEL + REC: OVERDUB OR REPLACE.";
    default: return "";
    }
}
static void draw_help(uint32_t b)
{
    const char *t = help_text(b);
    char line[40];
    int32_t y = 8, first = 1;
    cv_fillb(4, 2, 232, 118, RGB(6, 6, 8), 235);
    cv_frame(4, 2, 232, 118, COL_GREEN);
    while (*t) {
        uint32_t n = 0;
        while (t[n] && t[n] != '\n' && n < 38u) n++;
        clip_cpy(line, t, n);
        if (first) { cv_t5(12, y, line, COL_GREEN, 2); y += 22; first = 0; }
        else { cv_t5(12, y, line, COL_BONE, 1); y += 11; }
        t += n;
        if (*t == '\n') t++;
    }
}

/* ---- a knob turned: its name and value large for a moment; MUTATE with the register; the scene */
#define RO_MS 1100u
static void draw_readout(void)
{
    char s[24], *p;
    const char *name = "", *sub = "";
    int32_t frac = -1;
    if (!ui.ro || fm1_ms - ui.t_ro >= RO_MS) return;
    cv_fillb(22, 20, 196, 80, RGB(6, 6, 8), 230);
    cv_frame(22, 20, 196, 80, COL_GREEN);
    if (ui.ro <= 4u) {
        knob_t n = knob_for(ui.ro - 1u);
        int32_t lo, hi, v;
        if (n.type == KT_NONE) return;
        v = knob_get(n, &lo, &hi);
        knob_text(n, s);
        name = n.name;
        frac = hi > lo && v >= lo ? (v - lo) * 1000 / (hi - lo) : 0;
        if (n.type == KT_SOUND) sub = sound_of(korg(n), v, 1);
    } else if (ui.ro == 5u) {                   /* MUTATE, and the register it moves */
        uint32_t m = mutate_of(SC()->tur.loop), L = BIO_TUR_LEN[SC()->tur.len_i & 7u], i;
        name = "MUTATE";
        p = u2s(s, m);
        str_cat(p, "%");
        sub = mutate_word(m);
        for (i = 0; i < 16u; i++) {
            uint32_t bit = 15u - i, on = (sq.tur.invert ? ~sq.tur.r : sq.tur.r) >> bit & 1u;
            cv_rect(42 + (int32_t)i * 10, 86, 8, 8, on ? (bit < L ? COL_GREEN : COL_GREEN_D) : bit < L ? COL_GROOVE : COL_DIM);
        }
    } else {                                    /* the scene, now or on the one */
        name = "SCENE";
        p = str_cat(s, "R0");
        u2s(p, sq.next);
        sub = sq.playing && sq.next != sq.cur ? "ON THE ONE" : scenes[sq.next].name;
    }
    cv_t5c(120, 26, name, COL_BONE, 2);
    cv_t5c(120, 46, s, COL_GREEN, t5w(s, 3) <= 188 ? 3 : 2);
    if (sub[0]) cv_t5c(120, ui.ro == 5u ? 74 : 72, sub, COL_FLESH, 1);
    if (frac >= 0) {
        cv_rect(40, 88, 160, 4, COL_GROOVE);
        cv_rect(40, 88, frac * 160 / 1000, 4, COL_GREEN);
    }
}

static void draw_main(void)
{
    cv_at(0, 49, 240, set.deep ? 134u : 122u);
    if (ui.help != NO_HELP) {
        if (is_layer(ui.help)) draw_tiles(ui.help); else draw_help(ui.help);
    } else if (set.deep) {
        switch (ui.page) {
        case PG_MIX: draw_mix(); break;
        case PG_SEQ: draw_seq(13u, 13u); break;
        case PG_SYN: draw_syn(); break;
        case PG_MUT: draw_mut(); break;
        case PG_FX: draw_fx(); break;
        default: draw_glo();
        }
    } else {
        switch (ui.sp) {
        case SP_PLAY: if (ui.ssub) draw_s_mix(); else draw_s_play(); break;
        case SP_STEPS: draw_seq(12u, 12u); break;
        case SP_SOUND: if (ui.ssub) draw_s_sound2(); else draw_s_sound(); break;
        case SP_FX: if (ui.ssub) draw_s_fx2(); else draw_s_fx(); break;
        default: if (ui.ssub) draw_s_chain(); else draw_s_song();
        }
    }
    if (ui.help == NO_HELP) draw_readout();
    cv_out();
}

/* ---- the LEDs */
static void leds(void)
{
    uint32_t k, seqh = fm1_in.buttons >> BTN_SEQ & 1u, lfoh = fm1_in.buttons >> BTN_LFO & 1u, arph = fm1_in.buttons >> BTN_ARP & 1u;
    uint32_t homeh = fm1_in.buttons >> BTN_HOME & 1u;
    uint32_t o = cur_org(), L = SC()->tr[o].len, ph = play_step(o);
    static const uint8_t PAGE_BTN[PG_N] = {BTN_HOME, BTN_SEQ, BTN_EDIT, BTN_LFO, BTN_FX, BTN_GLO};
    static const uint8_t SP_BTN[SP_N] = {BTN_HOME, BTN_SEQ, BTN_EDIT, BTN_FX, BTN_GLO};
#if !BOARD_SMK37                                /* (the SMK-37's keys have no LEDs) */
    for (k = 0; k < 27u; k++) {
        uint32_t w = is_white(k) ? white_index(k) : 99u, on = 0, glow = 0;
        if (seqh && w < 16u) {                  /* the steps of the focused track */
            uint32_t st = ui.steppage * 16u + w;
            on = st == ph || (ui.held_steps >> w & 1u);
            glow = st < L && seq_step_events(&sq, o, st) != 0u;
        } else if (lfoh || arph) {              /* the page's keys */
            glow = w < (lfoh ? 5u : 10u);
            on = arph && w < 10u && (ui.burst >> w & 1u);
        } else if (homeh) {                     /* the mutes: lit, muted */
            seq_scene_t *sc = SC();
            glow = w < 10u;
            on = w < 4u ? sc->tr[w].mute : w < 9u ? sc->mix.mute[BIO_ORG_CH[w]] : w == 9u ? sc->mix.mute[CH_DRUM] : 0u;
        } else if (!set.keyled) {               /* the keys held; the playhead's white key; the kit's middles */
            on = fm1_in.notes >> k & 1u;
            glow = ph != 999u && w == ph % 16u;
            if (set.focus == F_KIT && (k == KIT_MID[0] || k == KIT_MID[1] || k == KIT_MID[2] || k == KIT_MID[3])) glow = 1;
        }
        fm1_led_key(KEY_LED(k), (int)on);
        fm1_led_key_glow(KEY_LED(k), (int)(glow && !on));
    }
#else
    (void)seqh; (void)lfoh; (void)arph; (void)homeh; (void)L; (void)ph;
#endif
    if (set.deep) {
        for (k = 0; k < PG_N; k++) fm1_led_key(PAGE_BTN[k], k == ui.page);
        fm1_led_key(BTN_ENV, ui.page == PG_SYN && ui.sub == 2u);
        fm1_led_key(BTN_ARP, 0);
    } else {                                    /* SIMPLE: the page's button; ARP lit while the track arpeggiates, LFO
                                                   with the register's clock while it mutates */
        for (k = 0; k < SP_N; k++) fm1_led_key(SP_BTN[k], k == ui.sp);
        fm1_led_key(BTN_ENV, 0);
        fm1_led_key(BTN_LFO, (int)(mutate_of(SC()->tur.loop) && (sq.tur.clocks & 1u)));
        fm1_led_key(BTN_ARP, (int)(has_arp(o) && SC()->tr[o].arp));
    }
    fm1_led_key(BTN_PLAY, (int)sq.playing);
    fm1_led_key(BTN_REC, (int)((sq.rec && sq.playing) || (ui.steprec && (fm1_ms & 512u))));
    fm1_led_key(BTN_SEL, (int)(ui.shift || (set.deep && ui.latched)));
    fm1_led_key(BTN_SAVE, (int)ui.dirty[sq.cur]);
}

/* ---- the panel */
static void page_to(uint32_t pg)                /* DEEP */
{
    if (ui.page == pg) {                        /* again: the page under it */
        uint32_t nsub = pg == PG_SEQ ? 1u : pg == PG_FX || pg == PG_GLO ? 3u : 2u;   /* (GLO again: the notes, D31; again:
                                                   the SONG, D40) */
        ui.sub = (ui.sub + 1u) % nsub;
    } else {
        ui.page = pg;
        ui.sub = 0;
    }
    ui.dirty_draw = D_ALL;
}
/* SIMPLE (D45): a page's button opens it; pressed again (let go soon, nothing else done while it was held): its second
   page, and back */
static uint32_t again_btn = NO_HELP;
static void sp_press(uint32_t sp, uint32_t btn)
{
    if (ui.sp != sp) { ui.sp = sp; ui.ssub = 0; again_btn = NO_HELP; ui.dirty_draw = D_ALL; }
    else again_btn = btn;
}
static uint32_t tapped(uint32_t rel, uint32_t btn)   /* let go after a short press that did nothing else */
{
    return (rel >> btn & 1u) && !(ui.used >> btn & 1u) && fm1_ms - ui.t_down[btn] < 450u;
}
static void deep_set(uint32_t on)
{
    set.deep = (uint8_t)on;
    ui.latched = 0;
    ui.help = NO_HELP;
    if (on) { ui.page = ui.sp == SP_STEPS ? PG_SEQ : ui.sp == SP_SOUND ? PG_SYN : ui.sp == SP_FX ? PG_FX : ui.sp == SP_SONG ? PG_GLO : PG_MIX; ui.sub = 0; }
    say(on ? "DEEP (SEL+EDIT: BACK)" : "SIMPLE");
    ui.dirty_draw = D_ALL;
}

static void rec_arm(void)
{
    seq_begin(&sq);
    sq.rec_mask = 0;
    sq.rec_started = 0;
    sq.rec_replace = set.replace;
    sq.rec_trk = cur_org();
    sq.rec = 1;
}

static void taps_in(uint32_t rel)               /* SIMPLE's taps, on letting go (after the presses: a tap that went
                                                   down and up between two looks is one too) */
{
    if (set.deep) return;
    if (again_btn != NO_HELP && tapped(rel, again_btn)) { ui.ssub ^= 1u; ui.dirty_draw = D_ALL; }
    if (again_btn != NO_HELP && (rel >> again_btn & 1u)) again_btn = NO_HELP;
    if (tapped(rel, BTN_ARP)) {                 /* ARP tapped: the track's ARP */
        if (has_arp(cur_org())) { ui.sp = SP_SOUND; ui.ssub = 1; ui.dirty_draw = D_ALL; }
        else say("THE DRUMS HAVE NO ARP");
    }
    if (tapped(rel, BTN_LFO)) { ui.ro = 5; ui.t_ro = fm1_ms; ui.dirty_draw |= D_MAIN; }   /* LFO tapped: MUTATE shown */
}

static void buttons_in(void)
{
    uint32_t rel, b = fm1_input_edges(&rel), held = fm1_in.buttons, i;
    ui.shift = held >> BTN_SEL & 1u;
    for (i = 0; i < 20u; i++) if (b >> i & 1u) { ui.t_down[i] = fm1_ms; ui.used &= ~(1u << i); }
    if (b) ui.used |= held & ~b;                /* (pressed with another held: that one did something) */
    if (b & (1u << BTN_SEL)) { ui.sel_down = fm1_ms; ui.sel_alone = 1; ui.dirty_draw |= D_KNOBS; }
    if (b & ~(1u << BTN_SEL)) ui.sel_alone = 0;
    if (rel & (1u << BTN_SEL)) {
        if (set.deep && ui.sel_alone && fm1_ms - ui.sel_down < 400u) ui.latched ^= 1u;   /* SEL tapped: the second row stays */
        ui.dirty_draw |= D_KNOBS;
    }
    if (b | rel) ui.dirty_draw |= D_HINT;
    if (!b) { taps_in(rel); return; }
    ui.t_input = fm1_ms;
    if ((b & (1u << BTN_EDIT)) && ui.shift) {   /* SEL + EDIT: DEEP or SIMPLE */
        ui.used |= 1u << BTN_EDIT;
        deep_set(!set.deep);
        b &= ~(1u << BTN_EDIT);
    }
    if (set.deep) {
        if (b & (1u << BTN_HOME)) page_to(PG_MIX);
        if ((b & (1u << BTN_SEQ)) && ui.page != PG_SEQ) page_to(PG_SEQ);
        if (b & (1u << BTN_EDIT)) { if (ui.page == PG_SYN && ui.sub == 2u) ui.sub = 1u; page_to(PG_SYN); }
        if (b & (1u << BTN_ENV)) { ui.page = PG_SYN; ui.sub = 2u; ui.dirty_draw = D_ALL; }
        if (b & (1u << BTN_LFO)) page_to(PG_MUT);
        if (b & (1u << BTN_FX)) page_to(PG_FX);
        if (b & (1u << BTN_GLO)) page_to(PG_GLO);
    } else {
        if (b & (1u << BTN_HOME)) sp_press(SP_PLAY, BTN_HOME);
        if (b & (1u << BTN_SEQ)) sp_press(SP_STEPS, BTN_SEQ);
        if (b & (1u << BTN_EDIT)) sp_press(SP_SOUND, BTN_EDIT);
        if (b & (1u << BTN_FX)) sp_press(SP_FX, BTN_FX);
        if (b & (1u << BTN_GLO)) sp_press(SP_SONG, BTN_GLO);
        if (b & (1u << BTN_ENV)) { ui.sp = SP_SOUND; ui.ssub = 0; ui.dirty_draw = D_ALL; }
    }
    if (b & (1u << BTN_OCT_DOWN)) {
        if (ui.shift) { say(seq_undo(&sq) ? "UNDO" : "NOTHING TO UNDO"); touched(); ui.dirty_draw |= D_MAIN | D_KNOBS; }
        else if (held >> BTN_SEQ & 1u) { if (ui.steppage) ui.steppage--; ui.used |= 1u << BTN_SEQ; ui.dirty_draw |= D_MAIN; }
        else if (set.focus != F_KIT && set.oct[set.focus] > -4) { set.oct[set.focus]--; say("OCTAVE DOWN"); }
    }
    if (b & (1u << BTN_OCT_UP)) {
        if (ui.shift) { say(seq_redo(&sq) ? "REDO" : "NOTHING TO REDO"); touched(); ui.dirty_draw |= D_MAIN | D_KNOBS; }
        else if (held >> BTN_SEQ & 1u) { if ((ui.steppage + 1u) * 16u < SC()->tr[cur_org()].len) ui.steppage++; ui.used |= 1u << BTN_SEQ; ui.dirty_draw |= D_MAIN; }
        else if (set.focus != F_KIT && set.oct[set.focus] < 3) { set.oct[set.focus]++; say("OCTAVE UP"); }
    }
    if (b & (1u << BTN_PLAY)) {
        ui.played = 1;
        if (ui.shift) {                         /* PANIC: everything silent, the memory kept */
            sq.rec = 0;
            if (sq.playing) seq_stop(&sq);
            bio_panic(&eng);
            say("PANIC");
        } else if (sq.playing) {
            sq.rec = 0;
            seq_stop(&sq);
        } else {
            if (ui.steprec || (held >> BTN_REC & 1u)) {   /* REC + PLAY from STOP: recording from the start */
                ui.steprec = 0;
                rec_arm();
            }
            seq_play(&sq);
        }
        ui.dirty_draw |= D_TOP | D_MAIN | D_HINT;
    }
    if (b & (1u << BTN_REC)) {
        if (ui.shift) {
            set.replace ^= 1u;
            sq.rec_replace = set.replace;
            say(set.replace ? "REPLACE" : "OVERDUB");
        } else if (sq.playing) {
            if (sq.rec) {
                uint32_t k;
                sq.rec = 0;
                SEQ_LOCK();
                for (k = 0; k < BIO_NORG; k++) seq_close_open(&sq, k, seq_now(&sq, k, 0));
                SEQ_UNLOCK();
            } else {
                rec_arm();
            }
        } else {
            ui.steprec ^= 1u;
            if (ui.steprec && set.deep && ui.page != PG_SEQ) { ui.page = PG_SEQ; ui.sub = 0; ui.dirty_draw = D_ALL; }
            if (ui.steprec && !set.deep && ui.sp != SP_STEPS) { ui.sp = SP_STEPS; ui.ssub = 0; ui.dirty_draw = D_ALL; }
        }
        ui.dirty_draw |= D_TOP | D_MAIN | D_HINT;
    }
    if (b & (1u << BTN_SAVE)) {
        if (ui.shift) { for (i = 0; i < SEQ_NSCENE; i++) if (ui.dirty[i]) ui.save_req[i] = 1; }
        else ui.save_req[sq.cur] = 1;
        say(sq.playing ? "SAVES ON STOP" : "SAVING");
    }
    taps_in(rel);
}

/* the button held: its tiles (HOME, ARP, LFO: a quarter second in), or SIMPLE's card of what it does (held alone for
   0.6 s); let go: the page again */
static void held_in(void)
{
    uint32_t h = NO_HELP, held = fm1_in.buttons, i;
    for (i = 0; i < 20u; i++) {
        uint32_t t;
        if (!(held >> i & 1u)) continue;
        t = fm1_ms - ui.t_down[i];
        if (is_layer(i) && t >= 250u) { h = i; break; }
        if (!set.deep && !is_layer(i) && !(ui.used >> i & 1u) && t >= 600u && help_text(i)[0]) h = i;
    }
    if (h != ui.help) { ui.help = h; ui.dirty_draw |= D_MAIN | D_HINT; }
}

static int32_t accel(int32_t e) { return e > 2 || e < -2 ? e * 3 : e; }   /* a fast turn goes further */

static void encoders_in(void)
{
    int32_t e;
    uint32_t k, any = 0;
    if ((e = fm1_enc_take(ENC_SELECT)) != 0) {  /* the track */
        int32_t f = ((int32_t)set.focus + e % 10 + 10) % 10;
        set.focus = (uint8_t)f;
        if (set.focus >= 1u && set.focus <= 4u) ui.last_drum = set.focus - 1u;
        sq.rec_trk = cur_org();
        if (ui.cursor >= SC()->tr[cur_org()].len) ui.cursor = 0;
        ui.steppage = ui.cursor / 16u;
        ui.dirty_draw = D_ALL;
        ui.t_input = fm1_ms;
        any = 1;
    }
    if ((e = fm1_enc_take(ENC_ALGORITHM)) != 0) {   /* MUTATE (DEEP: LOOP) */
        seq_scene_t *sc = SC();
        int32_t l = sc->tur.loop;
        if (set.deep) l = bio_clamp(l + accel(e), 0, 100);
        else if (l >= 50) l = bio_clamp(l - accel(e), 50, 100);   /* (more: toward 50, from the side it is on) */
        else l = bio_clamp(l + accel(e), 0, 50);
        sc->tur.loop = (uint8_t)l;
        ui.t_loop = fm1_ms;
        if (!set.deep) { ui.ro = 5; ui.t_ro = fm1_ms; }
        touched();
        ui.dirty_draw |= D_TOP | D_MAIN;
        any = 1;
    }
    if ((e = fm1_enc_take(ENC_PRESETS)) != 0 && ui.shift) { factory_load(e); ui.sel_alone = 0; ui.ro = 6; ui.t_ro = fm1_ms; any = 1; }   /* (held: a latch must not load one) */
    else if (e) {                               /* the scene */
        int32_t s = ((int32_t)sq.next + e % (int32_t)SEQ_NSCENE + (int32_t)SEQ_NSCENE) % (int32_t)SEQ_NSCENE;
        sq.next = (uint32_t)s;
        if (!sq.playing) { sq.cur = sq.next; sq.tur_reset = 1; ui.cursor = 0; ui.steppage = 0; }
        set.scene = (uint8_t)sq.next;
        ui.ro = 6;
        ui.t_ro = fm1_ms;
        ui.dirty_draw = D_ALL;
        ui.t_input = fm1_ms;
        any = 1;
    }
    for (k = 0; k < 4u; k++)
        if ((e = fm1_enc_take(ENC_KNOB1 + k)) != 0) {
            knob_t n = knob_for(k);
            knob_turn(n, accel(e));
            ui.sel_alone = 0;                   /* (SEL held for it: not a tap) */
            if (n.type != KT_NONE) { ui.ro = k + 1u; ui.t_ro = fm1_ms; ui.dirty_draw |= D_MAIN; }
            any = 1;
        }
    if (any) ui.used |= fm1_in.buttons;
}

static uint32_t load_smooth(uint32_t i, uint32_t ticks)   /* % of the 5.8 ms (139 320 ticks), over 16 calls */
{
    load_q8[i] = load_q8[i] - (load_q8[i] >> 4) + ((ticks + 696u) / 1393u << 4);
    return (load_q8[i] + 128u) >> 8;
}
static void app_audio(int32_t *out, uint32_t n)
{
    uint32_t t0 = app_ticks(), d;
    t_audio = t0;
    if (!eng_off) bio_render(&eng, out, n);
    d = app_ticks() - t0;
    isr_load = load_smooth(0, d);
    d = eng.c1_busy;                            /* (its jobs are all back: the render waited for each) */
    eng.c1_busy = 0;
    c1_load = load_smooth(1, d);
    emu_fig[0] = isr_load;
    emu_fig[1] = c1_load;
    emu_fig[2] = core1_state;
    {
        uint32_t k, w = eng.c0_wait;
        eng.c0_wait = 0;
        eng.c0_part[2] = w;
        for (k = 0; k < 4u; k++) {
            emu_fig[3u + k] = load_smooth(2u + k, eng.c0_part[k]);
            eng.c0_part[k] = 0;
        }
    }
}

#ifdef DINGHY_WEB
#define APP_STATE static                        /* in a browser: a pass a call, its state kept between */
#else
#define APP_STATE
#endif
static void app_main(void)
{
    APP_STATE uint32_t t_draw = 0, t_save_try = 0, ph_seen = ~0u, sc_seen = ~0u;
    uint32_t k;
#ifdef DINGHY_WEB
    static int started;
    if (!started) {
        started = 1;
#endif
    {
        bio_set_t s = set;
        if (!blob_load(0, &s, sizeof s) && s.ver == 1u && s.scene < SEQ_NSCENE && s.focus <= 9u) set = s;
    }
    bio_init(&eng);
#ifndef BIO_CORES
#define BIO_CORES 2                             /* (1: the render on one core, to measure against: DINGHY_DEFS=-DBIO_CORES=1) */
#endif
    if (core1_state == CORE1_OK && BIO_CORES > 1) {   /* the second core renders a share of each sub-block (D38) */
        eng.par.run = core1_run;
        eng.par.wait = core1_wait;
        eng.par.ticks = app_ticks;
    }
    defaults();
    scenes_load();
    sq.cur = sq.next = set.scene;
    seq_attach(&sq, &eng, scenes, undo_buf, redo_buf);
    sq.rec_replace = set.replace;
    ui.last_drum = O_HEART;
    ui.page = PG_SEQ;                           /* (DEEP: the grid first) */
    ui.sp = SP_PLAY;                            /* (SIMPLE: the instrument first, D45) */
    ui.help = NO_HELP;
    ui.dirty_draw = D_ALL;
    idle_ok = 1;
#ifdef DINGHY_WEB
    }
    do {
#else
    for (;;) {
#endif
#ifndef DINGHY_HOST
        if (usb.ota_req) eng_off = 1;           /* the updater gets the whole chip */
#endif
        platform_poll();
        idle_ok = 0;
        keys_in();
        midi_in();
        buttons_in();
        encoders_in();
        held_in();
        idle_ok = 1;
        eng.scope_org = cur_org();
        eng.focus = cur_org();
        eng.shed = isr_load > 75u ? 1u : isr_load < 60u ? 0u : eng.shed;   /* (with a gap, so it does not flicker) */
        if (sq.cur != sc_seen) { sc_seen = sq.cur; kits_find(); ui.dirty_draw = D_ALL; }   /* a scene changed on the bar */
        /* BURST: STUTTER holds the playhead on its step; SPASM fires a nerve burst */
        if ((ui.burst & 1u) && sq.playing && sq.ticks >= ui.stutter_at + SEQ_TPS / 2u) {
            SEQ_LOCK();
            sq.ticks = ui.stutter_at;
            for (k = 0; k < BIO_NORG; k++) sq.ver[k]++;
            SEQ_UNLOCK();
        }
        if (ui.burst & 8u) {
            static uint32_t t_sp;
            if (fm1_ms - t_sp >= 36u) { t_sp = fm1_ms; seq_live(&sq, O_SPASM, 55u + (fm1_ms >> 4 & 7u) * 2u, 96u, 1); }
        }
        if (sq.writing != ui.writing_seen) { ui.writing_seen = sq.writing; ui.t_writing = fm1_ms; ui.dirty_draw |= D_TOP; }
        /* saving: when nothing plays and nothing sounds */
        for (k = 0; k < SEQ_NSCENE; k++)
            if (ui.save_req[k] && !sq.playing && bio_quiet(&eng) && fm1_ms - t_save_try > 300u) {
                t_save_try = fm1_ms;
                ui.save_req[k] = 0;
                if (!scene_save(k)) { ui.dirty[k] = 0; say("SAVED"); }
                else say("SAVE FAILED");
                blob_save(0, &set, sizeof set);
            }
        if (!sq.playing && ui.dirty[sq.cur] && fm1_ms - ui.t_input > 6000u && bio_quiet(&eng) && fm1_ms - t_save_try > 6000u) {
            ui.save_req[sq.cur] = 1;            /* left alone, stopped, quiet: kept (the document's inactivity save) */
            t_save_try = fm1_ms;
        }
        /* drawing: what changed; the moving pages ~25 times a second */
        {
            uint32_t ph = sq.playing ? sq.ticks / SEQ_TPS : ~0u;
            uint32_t pg = set.deep ? ui.page : ui.sp, moving = set.deep ? pg == PG_MIX || pg == PG_SYN
                        : (pg == SP_PLAY && ui.ssub) || pg == SP_SOUND, help = ui.help != NO_HELP;
            if (ph != ph_seen) {
                ph_seen = ph;
                if (set.deep ? pg == PG_SEQ || (pg == PG_GLO && ui.sub == 2u) : pg != SP_FX) ui.dirty_draw |= D_MAIN;
                if (help && is_layer(ui.help)) ui.dirty_draw |= D_MAIN;
            }
            if (sq.tur.clocks != ui.tur_seen) {
                ui.tur_seen = sq.tur.clocks;
                if ((set.deep && pg == PG_MUT) || ui.ro == 5u) ui.dirty_draw |= D_MAIN;
            }
            if (moving && !help && fm1_ms - t_draw > 40u) ui.dirty_draw |= D_MAIN | (pg == SP_PLAY && !set.deep ? D_HINT : 0u);
            if (ui.msg && fm1_ms - ui.t_msg >= 1500u) { ui.msg = 0; ui.dirty_draw |= D_TOP | D_HINT; }
            if (ui.t_loop && fm1_ms - ui.t_loop >= 1500u) { ui.t_loop = 0; ui.dirty_draw |= D_TOP; }
            if (ui.ro && fm1_ms - ui.t_ro >= RO_MS) { ui.ro = 0; ui.dirty_draw |= D_MAIN; }
            if (ui.t_writing && fm1_ms - ui.t_writing >= 300u) { ui.t_writing = 0; ui.dirty_draw |= D_TOP; }
        }
        if (ui.dirty_draw && fm1_ms - t_draw >= 20u) {
            uint32_t d = ui.dirty_draw;
            ui.dirty_draw = 0;
            t_draw = fm1_ms;
            if (d & D_TOP) draw_top();
            if (d & D_STATUS) draw_status();
            if (d & D_MAIN) draw_main();
            if (d & D_KNOBS) draw_knobs();
            if (d & D_HINT) draw_hint();
            lcd_sync();
        }
        leds();
#ifdef DINGHY_WEB
    } while (0);
#else
    }
#endif
}

#ifdef DINGHY_WEB
/* the page (SIMPLE's 0..4; DEEP's 8 + its own), playing, recording, SIMPLE's second page, the track */
WEB_EXPORT(web_state) uint32_t web_state(void)
{
    return (set.deep ? 8u + ui.page : ui.sp) | (uint32_t)sq.playing << 4 | (uint32_t)sq.rec << 5 | ui.ssub << 6 |
           (uint32_t)set.focus << 8;
}
#endif
