/* SPDX-License-Identifier: GPL-3.0-only */
/* BIOFORGE: a biomechanical rhythm machine and synth for the M-VAVE FM-1. Rhythm out of pressure, fracture, friction
 * and vents in resonant bodies; tonal organs for mass, breath, nerve discharges and an artificial throat; the keys
 * straight into the pattern memory; a Turing register as a nervous system that locks and slowly changes it all.
 * The plan and its reasons: docs/bioforge/PLAN.md, DECISIONS.md.
 *
 *   SELECT       the focus: DRUM (the kit), HEART BONE TEETH CLAW BASS DRONE SPASM VOICE NOISE; SEL (pushed): SHIFT
 *   ALGORITHM    the Turing's LOOP, on every page (chaos to lock)
 *   PRESETS      the scene, RITUAL 00..03 (playing: on the next bar); SHIFT: a factory ritual into this one
 *   KNOB 1..4    the page's four (SHIFT, or SEL tapped: the second four)
 *   HOME  MIX (again: the sends); held: white keys 1-6 mute     SEQ  SEQ; held: the white keys are the steps
 *   EDIT  SYN (again: deeper)      ENV   the organ's envelope    LFO  MUT; held: WRITE 0, WRITE 1, RESET, FUSE, LOCK
 *   FX    FX (again: deeper)       GLO   tempo, swing, quantise  ARP  held: BURST (STUTTER, INVERT, FREEZE, SPASM;
 *                                                                       SCAR: REPEAT 1/16, REPEAT 1/8, REVERSE, SMEAR)
 *   SAVE  the scene to flash (SHIFT: every edited scene)
 *   PLAY  start / stop (SHIFT: PANIC)  REC  playing: LIVE REC; stopped: STEP REC (SHIFT: OVERDUB / REPLACE)
 *   OCT- OCT+  the keys' octave (SEQ held: the page of steps; SHIFT: UNDO, REDO)
 *   MIDI  notes on channel 1 to the focus, 2..10 to HEART..NOISE (recorded as the keys are); start and stop */
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

enum { PG_MIX, PG_SEQ, PG_SYN, PG_MUT, PG_FX, PG_GLO, PG_N };
static const char *const PG_NAME[PG_N] = {"MIX", "SEQ", "SYN", "MUT", "FX", "GLO"};
static const char *const ORG_NAME[BIO_NORG] = {"HEART", "BONE", "TEETH", "CLAW", "BASS", "DRONE", "SPASM", "VOICE", "NOISE"};
static const char *const CH_NAME[BIO_NCH] = {"DRUM", "BASS", "DRONE", "SPASM", "VOICE", "NOISE"};
static const char *const NOTE_NAME[12] = {"C", "C#", "D", "EB", "E", "F", "F#", "G", "AB", "A", "BB", "B"};
#define F_KIT 0u                                /* focus 0: the kit; 1..9: organ 0..8 */

typedef struct {                                /* kept as settings (blob 0) */
    uint8_t ver, scene, focus, keyled;
    int8_t oct[10];
    uint8_t replace, midich, pad[2];
} bio_set_t;
static bio_set_t set = {1, 0, F_KIT, 0, {0, 0, 0, 0, 0, -3, -2, 0, -1, 0}, 0, 0, {0, 0}};

static struct {
    uint32_t page, sub, shift, latched, last_drum, cursor, steppage, steprec;
    uint32_t held_steps, step_used, keys_prev, note_on[27], step_keys_down, last_note[BIO_NORG];
    uint32_t dirty_draw, t_msg, t_loop, t_input;
    uint32_t dirty[SEQ_NSCENE], save_req[SEQ_NSCENE];
    const char *msg;
    uint32_t burst, stutter_at, sel_alone, sel_down;
    uint32_t tur_seen, writing_seen, t_writing;
    uint8_t kit[BIO_NORG];                      /* the SOUND each organ was last given (0: TISSUE) */
    uint32_t factory;                           /* the factory ritual SHIFT + PRESETS last loaded */
} ui;
#define D_TOP 1u
#define D_STATUS 2u
#define D_MAIN 4u
#define D_KNOBS 8u
#define D_ALL 15u

static seq_scene_t *SC(void) { return &scenes[sq.cur]; }
static uint32_t cur_org(void) { return set.focus == F_KIT ? ui.last_drum : set.focus - 1u; }
static void say(const char *m) { ui.msg = m; ui.t_msg = fm1_ms; ui.dirty_draw |= D_STATUS; }
static void touched(void) { ui.dirty[sq.cur] = 1; ui.dirty_draw |= D_STATUS; ui.t_input = fm1_ms; }

/* ---- the knobs: what each of the four turns on this page */
enum { KT_NONE, KT_P, KT_LVL, KT_SA, KT_SB, KT_SC, KT_MASTER, KT_RIB, KT_TDEPTH, KT_TLEN, KT_TCLK, KT_TRATTLE,
       KT_TROUTE, KT_TWAIL, KT_TSCALE, KT_FX, KT_BPM, KT_SWING, KT_QUANT, KT_QSTR, KT_RECMODE, KT_STEPLEN, KT_KEYLED,
       KT_MIDICH, KT_CURSOR, KT_EVLEN, KT_EVVEL, KT_EVCH, KT_TRLEN, KT_EVRAT, KT_EVNUDGE, KT_EVNOTE, KT_PUMP, KT_KIT,
       KT_RUMBLE, KT_DARK };
typedef struct { uint8_t type, idx; const char *name; } knob_t;

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

static knob_t knob_for(uint32_t k)
{
    knob_t n = {KT_NONE, 0, ""};
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
            n.name = k == 0u ? "SPACE" : k == 1u ? "DUCK" : "";
            if (!n.name[0]) n.type = KT_NONE;
        }
        break;
    }
    default: {                                  /* GLO */
        static const char *const A[4] = {"BPM", "SWING", "QUANT", "Q-STR"}, *const B[4] = {"REC", "STEPLEN", "KEYS", "MIDI CH"};
        static const uint8_t TA[4] = {KT_BPM, KT_SWING, KT_QUANT, KT_QSTR}, TB[4] = {KT_RECMODE, KT_STEPLEN, KT_KEYLED, KT_MIDICH};
        n.type = sh ? TB[k] : TA[k];
        n.name = sh ? B[k] : A[k];
    }
    }
    return n;
}

static uint32_t cursor_events(void) { return seq_step_events(&sq, cur_org(), ui.cursor); }   /* at the cursor */

static int32_t knob_get(knob_t n, int32_t *lo, int32_t *hi)
{
    seq_scene_t *sc = SC();
    uint32_t o = cur_org(), m, i;
    seq_track_t *t = &sc->tr[o];
    uint8_t *fx = (uint8_t *)&sc->fx;
    *lo = 0;
    *hi = 100;
    switch (n.type) {
    case KT_P: if (n.idx == P_TUNE) *hi = 48; return sc->p[o][n.idx];
    case KT_LVL: return sc->mix.lvl[n.idx];
    case KT_SA: return sc->mix.sa[n.idx];
    case KT_SB: return sc->mix.sb[n.idx];
    case KT_SC: return sc->mix.sc[n.idx];
    case KT_MASTER: return sc->mix.master;
    case KT_RIB: return sc->mix.rib;
    case KT_PUMP: return sc->mix.pump;
    case KT_RUMBLE: return sc->mix.rumble;
    case KT_DARK: return sc->mix.dark;
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
    uint32_t o = cur_org();
    seq_track_t *t = &sc->tr[o];
    uint8_t *fx = (uint8_t *)&sc->fx;
    int32_t lo, hi, v = knob_get(n, &lo, &hi), nv;
    if (n.type == KT_NONE) return;
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
    case KT_SA: sc->mix.sa[n.idx] = (uint8_t)nv; break;
    case KT_SB: sc->mix.sb[n.idx] = (uint8_t)nv; break;
    case KT_SC: sc->mix.sc[n.idx] = (uint8_t)nv; break;
    case KT_MASTER: sc->mix.master = (uint8_t)nv; break;
    case KT_RIB: sc->mix.rib = (uint8_t)nv; break;
    case KT_PUMP: sc->mix.pump = (uint8_t)nv; break;
    case KT_RUMBLE: sc->mix.rumble = (uint8_t)nv; break;
    case KT_DARK: sc->mix.dark = (uint8_t)nv; break;
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
        if (org_perc(cur_org())) {              /* the perc voice's switches */
            if (n.idx == P_D1) { str_cat(p, BIO_PS_NAME[bio_ps_of((uint32_t)v)]); return; }
            if (n.idx == P_D3) { str_cat(p, v <= 8 ? "LOW" : v >= 92 ? "HIGH" : v >= 42 && v <= 58 ? "BAND" : v < 50 ? "LO>BD" : "BD>HI"); return; }
            if (n.idx == P_D4) { u2s(p, v < 25 ? 1u : v < 50 ? 2u : v < 75 ? 3u : 4u); return; }
            if (n.idx == P_D && !v) { str_cat(p, "HIT"); return; }
            if ((n.idx == P_E1 || n.idx == P_E2 || n.idx == P_E3) && !v) { str_cat(p, "OFF"); return; }
        }
        if (n.idx == P_D8 && cur_org() < 4u) { str_cat(p, v >= 66 ? "KICK" : v >= 33 ? "NOISE" : "TISSUE"); return; }
        if (n.idx == P_E1 && cur_org() >= 4u && cur_org() < O_NOISE) {   /* a synth's MODEL: its own, or a landscape */
            uint32_t m = bio_land_of((uint32_t)v);
            str_cat(p, m ? BIO_LAND_NAME[m] : BIO_OWN_NAME[cur_org() - 4u]);
            return;
        }
        break;
    case KT_KIT:
        if (!v) { str_cat(p, "TISSUE"); return; }
        p = str_cat(p, BIO_PERC_KIT[v - 1].name);
        if (!kit_same(cur_org(), (uint32_t)v)) str_cat(p, "*");   /* turned since (or never it): marked */
        return;
    case KT_TLEN: u2s(s, BIO_TUR_LEN[v & 7]); return;
    case KT_TCLK: str_cat(s, BIO_TUR_CLK_NAME[v & 3]); return;
    case KT_TWAIL: { static const char *const W[4] = {"OFF", "SPASM", "VOICE", "BOTH"}; str_cat(s, W[v & 3]); return; }
    case KT_TRATTLE: if (!v) { str_cat(s, "OFF"); return; } break;
    case KT_DARK: case KT_RUMBLE: case KT_PUMP: if (!v) { str_cat(s, "OFF"); return; } break;
    case KT_FX:
        if (n.idx == 0u) { str_cat(s, BIO_DIV_NAME[v < 10 ? v : 4]); return; }
        if (n.idx == 3u) { str_cat(s, v ? "ON" : "OFF"); return; }
        if (n.idx == 16u) { str_cat(s, BIO_SPACE_NAME[v < (int32_t)BIO_NSPACE ? v : 0]); return; }
        break;
    case KT_BPM: p = u2s(p, (uint32_t)v / 10u); *p++ = '.'; u2s(p, (uint32_t)v % 10u); return;
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
    for (k = 0; k < 27u; k++) {
        uint32_t b = 1u << k, w = is_white(k) ? white_index(k) : 99u;
        if (down & b) {
            if (seqh && w < 16u) {              /* the steps */
                ui.held_steps |= 1u << w;
                ui.step_used = 0;
                continue;
            }
            if (lfoh && w < 5u) {               /* the nerve's buttons */
                if (w == 0u) sq.tur.write = 1;
                if (w == 1u) sq.tur.write = 2;
                if (w == 2u) { sq.tur_reset = 1; say("RESET"); }
                if (w == 3u) { uint32_t f = seq_fuse(&sq); say(f ? "FUSED" : "NOTHING TO FUSE"); if (f) touched(); }
                if (w == 4u) { SC()->tur.loop = SC()->tur.loop >= 97u ? 50u : 100u; say(SC()->tur.loop >= 97u ? "LOCK" : "UNLOCK"); touched(); }
                ui.note_on[k] = K_MODE;
                continue;
            }
            if (arph && w < 8u) {               /* BURST; 5 .. 8: SCAR (the last pressed plays) */
                ui.burst |= 1u << w;
                if (w == 0u) ui.stutter_at = sq.ticks / SEQ_TPS * SEQ_TPS;
                if (w == 1u) sq.tur.invert = 1;
                if (w == 2u) eng.freeze = 1;
                if (w >= 4u) { eng.scar.want = SCAR_REP16 + (w - 4u); say(BIO_SCAR_NAME[SCAR_REP16 + (w - 4u)]); }
                ui.note_on[k] = K_MODE;
                continue;
            }
            if (homeh && w < 6u) {              /* the mutes */
                SC()->mix.mute[w] ^= 1u;
                touched();
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
                if (w < 8u && (ui.burst >> w & 1u)) {
                    ui.burst &= ~(1u << w);
                    if (w == 1u) sq.tur.invert = 0;
                    if (w == 2u) eng.freeze = 0;
                    if (w >= 4u && eng.scar.want == SCAR_REP16 + (w - 4u)) {   /* back to another still held, or off */
                        uint32_t h = ui.burst >> 4 & 15u, j = 4;
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
        if (st == 0xFAu || st == 0xFBu) { if (!sq.playing) seq_play(&sq); ui.dirty_draw |= D_STATUS; continue; }
        if (st == 0xFCu) { if (sq.playing) seq_stop(&sq); ui.dirty_draw |= D_STATUS; continue; }
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
/* the four scenes before anything is saved: the first four factory rituals (bio_rituals.h) */
static void defaults(void)
{
    uint32_t i;
    for (i = 0; i < SEQ_NSCENE; i++) BIO_RITUAL[i](&scenes[i]);
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
static void draw_top(void)
{
    uint32_t i;
    cv_at(0, 0, 240, 28);
    SPR(4, 0, LOGO, 16);                        /* (over the panel's frame: the plate, D30) */
    for (i = 0; i < PG_N; i++) {
        int32_t x = 108 + (int32_t)i * 21;
        if (i == ui.page) {
            cv_fillb(x, 7, 20, 13, COL_GREEN_D, 150);
            cv_frame(x, 7, 20, 13, COL_GREEN);
            cv_t5c(x + 10, 10, PG_NAME[i], COL_GREEN, 1);
        } else {
            cv_t5c(x + 10, 10, PG_NAME[i], COL_BONE_D, 1);
        }
    }
    cv_out();
}

static const char *loop_word(uint32_t l)
{
    return l >= 97u ? "LOCK" : l >= 60u ? "SLIP" : l > 40u ? "RANDOM" : l > 3u ? "INV SLIP" : "DBL LOCK";
}

static void draw_status(void)
{
    char s[40], *p;
    int32_t x, j;
    seq_scene_t *sc = SC();
    uint32_t recording = (sq.rec && sq.playing) || ui.steprec;
    cv_at(0, 29, 240, 18);
    if (recording && fm1_ms - ui.t_writing < 300u)   /* the red pulse: notes are going in now */
        cv_fillb(4, 2, 232, 14, COL_RED, 70);
    if (sq.playing) for (j = 0; j < 5; j++) cv_rect(8 + j, 4 + j, 1, 9 - 2 * j, COL_GREEN);   /* play */
    else cv_rect(7, 5, 7, 7, COL_BONE_D);                                                   /* stop */
    for (j = 0; j < 7; j++) {                   /* rec */
        int32_t w = j == 0 || j == 6 ? 3 : j == 1 || j == 5 ? 5 : 7;
        cv_rect(22 - w / 2, 5 + j, w, 1, recording ? COL_RED : COL_DIM);
    }
    x = 32;
    if (fm1_ms - ui.t_msg < 1500u && ui.msg) {
        cv_t5(x, 6, ui.msg, COL_BONE, 1);
    } else if (ui.t_loop && fm1_ms - ui.t_loop < 1500u) {
        p = str_cat(s, "LOOP ");
        p = u2s(p, sc->tur.loop);
        p = str_cat(p, "% ");
        str_cat(p, loop_word(sc->tur.loop));
        cv_t5(x, 6, s, COL_GREEN, 1);
    } else {
        const char *mode = sq.rec && sq.playing ? (set.replace ? "REPL" : "LIVE") : ui.steprec ? "STEP" : "";
        if (mode[0]) x = cv_t5(x, 6, mode, COL_RED, 1) + 6;
        x = cv_t5(x, 6, set.focus == F_KIT ? "DRUM" : ORG_NAME[set.focus - 1u], COL_GREEN, 1) + 5;
        if (set.focus == F_KIT) cv_t5(x, 6, ORG_NAME[ui.last_drum], COL_BONE_D, 1);
    }
    p = str_cat(s, "R0");
    p = u2s(p, sq.cur);
    if (sq.next != sq.cur) { p = str_cat(p, ">"); u2s(p, sq.next); }
    cv_t5(142, 6, s, COL_BONE, 1);
    p = u2s(s, sc->bpm10 / 10u);
    p = str_cat(p, ".");
    u2s(p, sc->bpm10 % 10u);
    cv_t5(172, 6, s, COL_GREEN, 1);
    cv_t5(207, 6, ui.dirty[sq.cur] ? "EDIT" : "SAVED", ui.dirty[sq.cur] ? COL_RED : COL_BONE_D, 1);
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
        cv_t5c(cx, 3, n.name, ui.shift || ui.latched ? COL_FLESH : COL_BONE, 1);
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

static void draw_seq(void)
{
    seq_scene_t *sc = SC();
    uint32_t r, c, o = cur_org();
    char s[8];
    cv_at(0, 49, 240, 134);
    for (c = 0; c < 16u; c++) {                 /* the step numbers: the beats, and a dot for the playhead */
        uint32_t st = ui.steppage * 16u + c;
        if (!(c & 3u)) { u2s(s, st + 1u); cv_t5(40 + (int32_t)c * 12, 4, s, COL_BONE_D, 1); }
        if (st == play_step(o)) cv_rect(42 + (int32_t)c * 12, 11, 4, 1, COL_GREEN);
    }
    for (r = 0; r < BIO_NORG; r++) {
        seq_track_t *t = &sc->tr[r];
        uint32_t L = t->len, ph = play_step(r);
        uint32_t page = r == o ? ui.steppage : (sq.playing ? ph / 16u : 0u), y = 13u + r * 13u;
        uint16_t lc = r == o ? COL_GREEN : t->mute ? COL_DIM : COL_BONE_D;
        cv_t5(5, (int32_t)y + 3, ORG_NAME[r], lc, 1);
        for (c = 0; c < 16u; c++) {
            uint32_t st = page * 16u + c;
            int32_t x = 38 + (int32_t)c * 12;
            if (st >= L) { cv_fillb(x + 4, (int32_t)y + 5, 3, 3, COL_DIM, 120); continue; }
            if (seq_step_events(&sq, r, st)) SPR(x, (int32_t)y, CELL_ON, t->mute || sc->mix.mute[BIO_ORG_CH[r]] ? 6u : 16u);
            else SPR(x, (int32_t)y, CELL_OFF, t->mute ? 8u : 16u);
            if (t->lock[st][0] != BIO_NOLOCK || t->lock[st][1] != BIO_NOLOCK || t->lock[st][2] != BIO_NOLOCK ||
                t->lock[st][3] != BIO_NOLOCK)
                cv_rect(x + 9, (int32_t)y, 3, 3, COL_RED);
            if (st == ph) cv_frame(x - 1, (int32_t)y - 1, 14, 14, COL_GREEN);
            if (r == o && st == ui.cursor && (!sq.playing || ui.steprec)) cv_frame(x - 1, (int32_t)y - 1, 14, 14, ui.steprec ? COL_RED : COL_BONE);
        }
        if (L > 16u) { u2s(s, page + 1u); cv_t5(233, (int32_t)y + 3, s, COL_DIM, 1); }   /* which page of it */
    }
    cv_out();
}

static void draw_mix(void)
{
    seq_scene_t *sc = SC();
    uint32_t ch;
    char s[24], *p;
    cv_at(0, 49, 240, 134);
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
    str_cat(p, "%");
    cv_t5(232 - t5w(s, 1), 121, s, COL_DIM, 1);
    cv_out();
}

static void draw_syn(void)
{
    uint32_t o = cur_org(), i;
    char s[20], *p;
    cv_at(0, 49, 240, 134);
    cv_t5(8, 5, ORG_NAME[o], COL_GREEN, 2);
    if (o < 4u || o == O_NOISE) {               /* what it is: a drum (and its SOUND, if noise), or NOISE's SOUND */
        int32_t x = 8 + t5w(ORG_NAME[o], 2) + 8;
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
        cv_t5(8 + t5w(ORG_NAME[o], 2) + 8, 11, md ? BIO_LAND_NAME[md] : BIO_OWN_NAME[o - 4u], md ? COL_FLESH : COL_BONE_D, 1);
        int32_t e = v->awake ? (v->env.v >> 24) : 0;
        cv_t5(10, 86, "ENV", COL_BONE_D, 1);
        cv_rect(34, 87, 2 + e * 120 / 64, 5, v->gate ? COL_GREEN : COL_FLESH);
        if (v->awake) {
            p = str_cat(s, NOTE_NAME[v->note % 12u]);
            i2s(p, (int32_t)v->note / 12 - 1);
            cv_t5(176, 84, s, COL_GREEN, 2);
        }
        if (o == O_VOICE) cv_t5(10, 104, "U  O  A  E  I", COL_DIM, 1);
    }
    cv_out();
}

static void draw_mut(void)
{
    seq_scene_t *sc = SC();
    uint32_t i, r = sq.tur.r, L = BIO_TUR_LEN[sc->tur.len_i & 7u];
    char s[24], *p;
    cv_at(0, 49, 240, 134);
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
        cv_t5(120, 116, s, sc->tur.wail ? COL_GREEN : COL_DIM, 1);
    }
    cv_out();
}

static void draw_fx(void)
{
    seq_scene_t *sc = SC();
    char s[48], *p;
    uint32_t t60 = sc->fx.r_decay <= 23u ? 250u + (uint32_t)sc->fx.r_decay * 2550u / 23u : 2800u + ((uint32_t)sc->fx.r_decay - 23u) * 9200u / 77u;
    cv_at(0, 49, 240, 134);
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
    cv_t5(10, 122, "SENDS A B C: HOME, HOME AGAIN", COL_DIM, 1);
    cv_out();
}

static void draw_glo(void)
{
    seq_scene_t *sc = SC();
    seq_track_t *t = &sc->tr[cur_org()];
    char s[32], *p;
    cv_at(0, 49, 240, 134);
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
    cv_t5(130, 36, ORG_NAME[cur_org()], COL_DIM, 1);
    cv_t5(14, 74, set.replace ? "REC: REPLACE" : "REC: OVERDUB", set.replace ? COL_RED : COL_BONE, 1);
    p = str_cat(s, "STEP REC LENGTH "); u2s(p, sc->steplen);
    cv_t5(14, 88, s, COL_BONE, 1);
    p = str_cat(s, "UNDO "); p = u2s(p, sq.nundo); p = str_cat(p, "  REDO "); u2s(p, sq.nredo);
    cv_t5(14, 102, s, COL_DIM, 1);
    p = str_cat(s, "RITUAL 0"); p = u2s(p, sq.cur); p = str_cat(p, " "); str_cat(p, sc->name);
    cv_t5(14, 116, s, COL_FLESH, 1);
    cv_t5(232 - t5w(APP_VER, 1), 116, APP_VER, COL_DIM, 1);
    cv_out();
}

static void draw_main(void)
{
    switch (ui.page) {
    case PG_MIX: draw_mix(); break;
    case PG_SEQ: draw_seq(); break;
    case PG_SYN: draw_syn(); break;
    case PG_MUT: draw_mut(); break;
    case PG_FX: draw_fx(); break;
    default: draw_glo();
    }
}

/* ---- the LEDs */
static void leds(void)
{
    uint32_t k, seqh = fm1_in.buttons >> BTN_SEQ & 1u, lfoh = fm1_in.buttons >> BTN_LFO & 1u, arph = fm1_in.buttons >> BTN_ARP & 1u;
    uint32_t o = cur_org(), L = SC()->tr[o].len, ph = play_step(o);
    static const uint8_t PAGE_BTN[PG_N] = {BTN_HOME, BTN_SEQ, BTN_EDIT, BTN_LFO, BTN_FX, BTN_GLO};
#if !BOARD_SMK37                                /* (the SMK-37's keys have no LEDs) */
    for (k = 0; k < 27u; k++) {
        uint32_t w = is_white(k) ? white_index(k) : 99u, on = 0, glow = 0;
        if (seqh && w < 16u) {                  /* the steps of the focused track */
            uint32_t st = ui.steppage * 16u + w;
            on = st == ph || (ui.held_steps >> w & 1u);
            glow = st < L && seq_step_events(&sq, o, st) != 0u;
        } else if (lfoh || arph) {              /* the page's keys */
            glow = w < (lfoh ? 5u : 8u);
            on = arph && w < 8u && (ui.burst >> w & 1u);
        } else if (!set.keyled) {               /* the keys held; the playhead's white key; the kit's middles */
            on = fm1_in.notes >> k & 1u;
            glow = ph != 999u && w == ph % 16u;
            if (set.focus == F_KIT && (k == KIT_MID[0] || k == KIT_MID[1] || k == KIT_MID[2] || k == KIT_MID[3])) glow = 1;
        }
        fm1_led_key(KEY_LED(k), (int)on);
        fm1_led_key_glow(KEY_LED(k), (int)(glow && !on));
    }
#else
    (void)seqh; (void)lfoh; (void)arph; (void)L; (void)ph; (void)o;
#endif
    for (k = 0; k < PG_N; k++) fm1_led_key(PAGE_BTN[k], k == ui.page);
    fm1_led_key(BTN_ENV, ui.page == PG_SYN && ui.sub == 2u);
    fm1_led_key(BTN_PLAY, (int)sq.playing);
    fm1_led_key(BTN_REC, (int)((sq.rec && sq.playing) || (ui.steprec && (fm1_ms & 512u))));
    fm1_led_key(BTN_SEL, (int)(ui.shift || ui.latched));
    fm1_led_key(BTN_SAVE, (int)ui.dirty[sq.cur]);
}

/* ---- the panel */
static void page_to(uint32_t pg)
{
    if (ui.page == pg) {                        /* again: the page under it */
        uint32_t nsub = pg == PG_GLO || pg == PG_SEQ ? 1u : pg == PG_FX ? 3u : 2u;
        ui.sub = (ui.sub + 1u) % nsub;
    } else {
        ui.page = pg;
        ui.sub = 0;
    }
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

static void buttons_in(void)
{
    uint32_t rel, b = fm1_input_edges(&rel), held = fm1_in.buttons;
    ui.shift = held >> BTN_SEL & 1u;
    if (b & (1u << BTN_SEL)) { ui.sel_down = fm1_ms; ui.sel_alone = 1; ui.dirty_draw |= D_KNOBS; }
    if (b & ~(1u << BTN_SEL)) ui.sel_alone = 0;
    if (rel & (1u << BTN_SEL)) {
        if (ui.sel_alone && fm1_ms - ui.sel_down < 400u) ui.latched ^= 1u;   /* SEL tapped: the second row stays */
        ui.dirty_draw |= D_KNOBS;
    }
    if (!b) return;
    ui.t_input = fm1_ms;
    if (b & (1u << BTN_HOME)) page_to(PG_MIX);
    if ((b & (1u << BTN_SEQ)) && ui.page != PG_SEQ) page_to(PG_SEQ);
    if (b & (1u << BTN_EDIT)) { if (ui.page == PG_SYN && ui.sub == 2u) ui.sub = 1u; page_to(PG_SYN); }
    if (b & (1u << BTN_ENV)) { ui.page = PG_SYN; ui.sub = 2u; ui.dirty_draw = D_ALL; }
    if (b & (1u << BTN_LFO)) page_to(PG_MUT);
    if (b & (1u << BTN_FX)) page_to(PG_FX);
    if (b & (1u << BTN_GLO)) page_to(PG_GLO);
    if (b & (1u << BTN_OCT_DOWN)) {
        if (ui.shift) { say(seq_undo(&sq) ? "UNDO" : "NOTHING TO UNDO"); touched(); ui.dirty_draw |= D_MAIN | D_KNOBS; }
        else if (held >> BTN_SEQ & 1u) { if (ui.steppage) ui.steppage--; ui.dirty_draw |= D_MAIN; }
        else if (set.focus != F_KIT && set.oct[set.focus] > -4) { set.oct[set.focus]--; say("OCTAVE DOWN"); }
    }
    if (b & (1u << BTN_OCT_UP)) {
        if (ui.shift) { say(seq_redo(&sq) ? "REDO" : "NOTHING TO REDO"); touched(); ui.dirty_draw |= D_MAIN | D_KNOBS; }
        else if (held >> BTN_SEQ & 1u) { if ((ui.steppage + 1u) * 16u < SC()->tr[cur_org()].len) ui.steppage++; ui.dirty_draw |= D_MAIN; }
        else if (set.focus != F_KIT && set.oct[set.focus] < 3) { set.oct[set.focus]++; say("OCTAVE UP"); }
    }
    if (b & (1u << BTN_PLAY)) {
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
        ui.dirty_draw |= D_STATUS | D_MAIN;
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
            if (ui.steprec && ui.page != PG_SEQ) { ui.page = PG_SEQ; ui.sub = 0; ui.dirty_draw = D_ALL; }
        }
        ui.dirty_draw |= D_STATUS | D_MAIN;
    }
    if (b & (1u << BTN_SAVE)) {
        uint32_t i;
        if (ui.shift) { for (i = 0; i < SEQ_NSCENE; i++) if (ui.dirty[i]) ui.save_req[i] = 1; }
        else ui.save_req[sq.cur] = 1;
        say(sq.playing ? "SAVES ON STOP" : "SAVING");
    }
}

static int32_t accel(int32_t e) { return e > 2 || e < -2 ? e * 3 : e; }   /* a fast turn goes further */

static void encoders_in(void)
{
    int32_t e;
    uint32_t k;
    if ((e = fm1_enc_take(ENC_SELECT)) != 0) {  /* the focus */
        int32_t f = ((int32_t)set.focus + e % 10 + 10) % 10;
        set.focus = (uint8_t)f;
        if (set.focus >= 1u && set.focus <= 4u) ui.last_drum = set.focus - 1u;
        sq.rec_trk = cur_org();
        if (ui.cursor >= SC()->tr[cur_org()].len) ui.cursor = 0;
        ui.steppage = ui.cursor / 16u;
        ui.dirty_draw = D_ALL;
        ui.t_input = fm1_ms;
    }
    if ((e = fm1_enc_take(ENC_ALGORITHM)) != 0) {   /* LOOP */
        seq_scene_t *sc = SC();
        sc->tur.loop = (uint8_t)bio_clamp((int32_t)sc->tur.loop + accel(e), 0, 100);
        ui.t_loop = fm1_ms;
        touched();
        ui.dirty_draw |= D_STATUS | (ui.page == PG_MUT ? D_MAIN : 0u);
    }
    if ((e = fm1_enc_take(ENC_PRESETS)) != 0 && ui.shift) { factory_load(e); ui.sel_alone = 0; }   /* (held: a latch must not load one) */
    else if (e) {                               /* the scene */
        int32_t s = ((int32_t)sq.next + e % (int32_t)SEQ_NSCENE + (int32_t)SEQ_NSCENE) % (int32_t)SEQ_NSCENE;
        sq.next = (uint32_t)s;
        if (!sq.playing) { sq.cur = sq.next; sq.tur_reset = 1; ui.cursor = 0; ui.steppage = 0; }
        set.scene = (uint8_t)sq.next;
        ui.dirty_draw = D_ALL;
        ui.t_input = fm1_ms;
    }
    for (k = 0; k < 4u; k++)
        if ((e = fm1_enc_take(ENC_KNOB1 + k)) != 0) { knob_turn(knob_for(k), accel(e)); ui.sel_alone = 0; }   /* (SEL held for it: not a tap) */
}

static void app_audio(int32_t *out, uint32_t n)
{
    uint32_t t0 = app_ticks(), d;
    t_audio = t0;
    if (!eng_off) bio_render(&eng, out, n);
    d = app_ticks() - t0;
    isr_load = (isr_load * 15u + d / 1393u) / 16u;   /* % of the 5.8 ms (139 320 ticks) */
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
    defaults();
    scenes_load();
    sq.cur = sq.next = set.scene;
    seq_attach(&sq, &eng, scenes, undo_buf, redo_buf);
    sq.rec_replace = set.replace;
    ui.last_drum = O_HEART;
    ui.page = PG_SEQ;                           /* (the grid first) */
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
        if (sq.writing != ui.writing_seen) { ui.writing_seen = sq.writing; ui.t_writing = fm1_ms; ui.dirty_draw |= D_STATUS; }
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
            if (ph != ph_seen) { ph_seen = ph; if (ui.page == PG_SEQ) ui.dirty_draw |= D_MAIN; }
            if (sq.tur.clocks != ui.tur_seen) { ui.tur_seen = sq.tur.clocks; if (ui.page == PG_MUT) ui.dirty_draw |= D_MAIN; }
            if ((ui.page == PG_MIX || ui.page == PG_SYN) && fm1_ms - t_draw > 40u) ui.dirty_draw |= D_MAIN;
            if (ui.msg && fm1_ms - ui.t_msg >= 1500u) { ui.msg = 0; ui.dirty_draw |= D_STATUS; }
            if (ui.t_loop && fm1_ms - ui.t_loop >= 1500u) { ui.t_loop = 0; ui.dirty_draw |= D_STATUS; }
            if (ui.t_writing && fm1_ms - ui.t_writing >= 300u) { ui.t_writing = 0; ui.dirty_draw |= D_STATUS; }
        }
        if (ui.dirty_draw && fm1_ms - t_draw >= 20u) {
            uint32_t d = ui.dirty_draw;
            ui.dirty_draw = 0;
            t_draw = fm1_ms;
            if (d & D_TOP) draw_top();
            if (d & D_STATUS) draw_status();
            if (d & D_MAIN) draw_main();
            if (d & D_KNOBS) draw_knobs();
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
WEB_EXPORT(web_state) uint32_t web_state(void) { return ui.page | (uint32_t)sq.playing << 4 | (uint32_t)sq.rec << 5 | (uint32_t)set.focus << 8; }
#endif
