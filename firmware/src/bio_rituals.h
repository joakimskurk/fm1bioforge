/* SPDX-License-Identifier: GPL-3.0-only */
/* BIOFORGE's factory rituals: whole scenes (patterns, sounds, the nerve, the mix, the effects), each its own world.
 * THE AWAKENING is the idea document's start scene (bio_seq.h); the others are BIOFORGE's own. SHIFT + PRESETS loads
 * one into the scene that plays (the scene in flash stays the one before until SAVE). */
#ifndef BIO_RITUALS_H
#define BIO_RITUALS_H
#include "bio_seq.h"
#include "bio_kits.h"

static void rit_name(seq_scene_t *sc, const char *n) { uint32_t k; for (k = 0; k < 11u && n[k]; k++) sc->name[k] = n[k]; sc->name[k] = 0; }
static void rit_perc(seq_scene_t *sc, uint32_t org, const char *kit, uint32_t pan)   /* an organ becomes a perc sound */
{
    bio_preset_set(sc->p[org], bio_preset_find(kit), org);
    sc->p[org][P_PAN] = (uint8_t)pan;
}
/* an organ as the noise core, its parameters given (a drum one switched to MODEL NOISE, NOISE's BREATH off) */
#define NC(b, t, m, d, src, res, mode, bur, clk, drv, cr, a, g, w, r, tn, lvl, pan, scat, tone, drift) \
    {b, t, m, d, src, res, mode, bur, clk, drv, cr, 100, a, g, w, r, tn, lvl, pan, 0, scat, tone, drift, 0}
static void rit_core(seq_scene_t *sc, uint32_t org, const uint8_t *v)
{
    uint32_t k;
    for (k = 0; k < BIO_NP; k++) sc->p[org][k] = v[k];
    if (org == O_NOISE) sc->p[org][P_D8] = 0;
}
/* a note anywhere (ticks, 96 a step), in order: triplets, 32nds, a gate inside a step */
static void rit_tick(seq_scene_t *sc, uint32_t trk, uint32_t tick, uint32_t len, uint32_t note, uint32_t vel, uint32_t fl)
{
    seq_track_t *t = &sc->tr[trk];
    uint32_t i = t->n;
    if (t->n >= SEQ_NEV) return;
    while (i > 0 && t->ev[i - 1u].start > tick) { t->ev[i] = t->ev[i - 1u]; i--; }
    t->ev[i].start = (uint16_t)tick; t->ev[i].len = (uint16_t)len; t->ev[i].note = (uint8_t)note;
    t->ev[i].vel = (uint8_t)vel; t->ev[i].fl = (uint8_t)fl; t->ev[i].pad = 0;
    t->n++;
}
static void rit_dry(seq_scene_t *sc)              /* the effects off but a little room; the master up (sparse, dry) */
{
    sc->fx.d_mix = 0; sc->fx.c_mix = 0; sc->fx.r_mix = 12; sc->fx.r_decay = 12; sc->fx.r_size = 40;
    sc->mix.master = 88;
}

/* notes from a string of steps: 'x' a hit (vel), 'o' an accent, '.' rest, 'g' a ghost (vel/2), 'r' a ratchet of 2,
   'R' of 3, '?' a 50 % chance */
static void rit_steps(seq_scene_t *sc, uint32_t trk, const char *s, uint32_t note, uint32_t vel, uint32_t len)
{
    uint32_t i;
    for (i = 0; s[i] && i < SEQ_MAXLEN; i++) {
        uint32_t fl = 0, v = vel;
        switch (s[i]) {
        case '.': continue;
        case 'o': fl = EV_ACC; break;
        case 'g': v = vel / 2u; break;
        case 'r': fl = 1u << EV_RAT_SH; break;
        case 'R': fl = 2u << EV_RAT_SH; break;
        case '?': fl = 4u << EV_CH_SH; break;
        default: break;
        }
        seq_put(sc, trk, i, len, note, v, fl);
    }
}

/* RITUAL: STATIC CULT. Noise as percussion, 132 BPM: HEART a tight body, BONE a flam snare of noise, TEETH ticking
   sixteenths, CLAW metal on the off-beats, NOISE an open hiss, a rolling bass under it; the nerve rattles a little */
static void rit_static(seq_scene_t *sc)
{
    seq_scene_blank(sc, 0x5747C1u);
    rit_name(sc, "STATIC CULT");
    sc->bpm10 = 1320;
    sc->p[O_HEART][P_BODY] = 82; sc->p[O_HEART][P_DECAY] = 30; sc->p[O_HEART][P_TEXTURE] = 40; sc->p[O_HEART][P_D7] = 70;
    rit_perc(sc, O_BONE, "FLAM SNR", 50);
    rit_perc(sc, O_TEETH, "TICK", 62);
    rit_perc(sc, O_CLAW, "METAL", 36);
    rit_perc(sc, O_NOISE, "HISS", 70);
    rit_steps(sc, O_HEART, "o...x...o...x...", 60, 110, 48);
    rit_steps(sc, O_BONE, "....o.......o..g", 60, 105, 48);
    rit_steps(sc, O_TEETH, "xgxgxgxgxgxgxgxr", 60, 96, 24);
    rit_steps(sc, O_CLAW, "..x...x..x....x.", 60, 100, 24);
    rit_steps(sc, O_NOISE, "..x...x...x...x.", 60, 100, 48);
    rit_steps(sc, O_BASS, "..x.x.x.x.x.x.xx", 26, 96, 40);
    sc->tr[O_BASS].ev[2].note = 38;             /* (an octave up now and then) */
    sc->tr[O_BASS].ev[6].note = 29;
    rit_steps(sc, O_SPASM, ".......?.......x", 62, 70, 24);
    sc->p[O_BASS][P_D1] = 38; sc->p[O_BASS][P_D] = 22; sc->p[O_BASS][P_TEXTURE] = 64; sc->p[O_BASS][P_D8] = 70;
    sc->p[O_SPASM][P_D2] = 80;
    sc->mix.lvl[CH_DRUM] = 88; sc->mix.lvl[CH_BASS] = 72; sc->mix.lvl[CH_DRONE] = 0; sc->mix.lvl[CH_VOICE] = 0;
    sc->mix.lvl[CH_NOISE] = 86; sc->mix.lvl[CH_SPASM] = 54;
    sc->mix.sb[CH_DRUM] = 10; sc->mix.sa[CH_DRUM] = 14; sc->mix.sb[CH_NOISE] = 12; sc->mix.sa[CH_SPASM] = 40;
    sc->fx.d_time = 4; sc->fx.d_fb = 34; sc->fx.d_mix = 34; sc->fx.r_mix = 22; sc->fx.r_decay = 14; sc->fx.c_mix = 0;
    sc->tur.rattle = 30; sc->tur.loop = 90; sc->tur.r_rot = 60; sc->tur.depth = 30;
    sc->tr[O_TEETH].qstr = 100;
    sc->mix.pump = 55;
}

/* RITUAL: SPLICE. Broken and glitched, 100 BPM: a 32-step body, a crushed snare, grains and zaps with ratchets and
   chances, a bit-hat on 12 steps against it all, the voice in short stabs; the nerve slips */
static void rit_splice(seq_scene_t *sc)
{
    seq_scene_blank(sc, 0x5B11CEu);
    rit_name(sc, "SPLICE");
    sc->bpm10 = 1000;
    sc->swing = 18;
    sc->tr[O_HEART].len = sc->tr[O_BONE].len = sc->tr[O_TEETH].len = sc->tr[O_CLAW].len = 32;
    sc->p[O_HEART][P_BODY] = 70; sc->p[O_HEART][P_DECAY] = 36;
    rit_perc(sc, O_BONE, "CRUSH", 46);
    rit_perc(sc, O_TEETH, "GRAIN", 64);
    rit_perc(sc, O_CLAW, "ZAP", 30);
    rit_perc(sc, O_NOISE, "BIT HAT", 56);
    rit_steps(sc, O_HEART, "o..x......x.....o.....x...x..x..", 60, 110, 48);
    rit_steps(sc, O_BONE, "....o.......g..?....o..g.....r..", 60, 104, 48);
    rit_steps(sc, O_TEETH, "..r...?.R..g...x..?...r..?..R.g.", 62, 86, 24);
    rit_steps(sc, O_CLAW, ".......x..........x..........x..", 60, 90, 24);
    sc->tr[O_NOISE].len = 12;
    rit_steps(sc, O_NOISE, "x?x.x?x.x?xx", 60, 100, 12);
    rit_steps(sc, O_BASS, "x.......x...........x.....x.....", 33, 100, 192);
    sc->tr[O_BASS].len = 32;
    sc->tr[O_BASS].ev[1].note = 31; sc->tr[O_BASS].ev[2].note = 26; sc->tr[O_BASS].ev[3].note = 36;
    sc->p[O_BASS][P_D4] = 60;                   /* glide */
    sc->tr[O_VOICE].len = 32;
    rit_steps(sc, O_VOICE, "......x...........x.......x.....", 57, 90, 36);
    sc->p[O_VOICE][P_A] = 4; sc->p[O_VOICE][P_R] = 20; sc->p[O_VOICE][P_D1] = 80; sc->p[O_VOICE][P_MOTION] = 80;
    sc->tr[O_SPASM].len = 7;
    rit_steps(sc, O_SPASM, "x..?.g.", 55, 84, 24);
    sc->tur.wail = 3; sc->tur.loop = 62; sc->tur.rattle = 40; sc->tur.clock = 1; sc->tur.depth = 40;
    sc->mix.lvl[CH_DRONE] = 0; sc->mix.lvl[CH_DRUM] = 86; sc->mix.lvl[CH_VOICE] = 60; sc->mix.lvl[CH_NOISE] = 84;
    sc->mix.sc[CH_DRUM] = 30; sc->mix.sc[CH_SPASM] = 50; sc->mix.sa[CH_VOICE] = 40; sc->mix.sa[CH_DRUM] = 20;
    sc->fx.c_mix = 50; sc->fx.c_rate = 50; sc->fx.c_bits = 60; sc->fx.d_time = 2; sc->fx.d_fb = 50; sc->fx.d_ping = 1;
    sc->fx.d_mix = 40; sc->fx.r_mix = 26; sc->fx.r_decay = 10;
    sc->mix.pump = 30;
}

/* RITUAL: VOID. Slow (70 BPM) and wide: a huge HEART twice a bar, a bone far off, gated noise washes held for bars, the
   drone in front, the throat singing long notes, crackle in the dark, a long reverb */
static void rit_void(seq_scene_t *sc)
{
    seq_scene_blank(sc, 0x7031Du);
    rit_name(sc, "VOID");
    sc->bpm10 = 700;
    sc->p[O_HEART][P_BODY] = 96; sc->p[O_HEART][P_DECAY] = 84; sc->p[O_HEART][P_TEXTURE] = 30;
    sc->p[O_BONE][P_DECAY] = 90; sc->p[O_BONE][P_BODY] = 80;
    rit_perc(sc, O_CLAW, "WASH", 30);
    rit_perc(sc, O_NOISE, "CRACKLE", 64);
    sc->p[O_NOISE][P_DECAY] = 90; sc->p[O_NOISE][P_D] = 100;
    sc->p[O_TEETH][P_DECAY] = 70;
    rit_steps(sc, O_HEART, "x.........x.....", 60, 112, 96);
    rit_steps(sc, O_BONE, "........x.......", 55, 70, 96);
    rit_steps(sc, O_TEETH, "...?.......?...g", 72, 60, 24);
    sc->tr[O_CLAW].len = 32;
    rit_steps(sc, O_CLAW, "x...............x...............", 60, 80, 14 * 96);
    sc->tr[O_NOISE].len = 64;
    seq_put(sc, O_NOISE, 0, 60 * 96, 60, 80, 0);
    sc->tr[O_DRONE].len = 32;
    seq_put(sc, O_DRONE, 0, 16 * 96, 38, 96, 0);
    seq_put(sc, O_DRONE, 16, 16 * 96, 36, 96, 0);
    sc->tr[O_VOICE].len = 32;
    seq_put(sc, O_VOICE, 2, 12 * 96, 50, 80, 0);
    seq_put(sc, O_VOICE, 18, 10 * 96, 53, 80, 0);
    sc->tr[O_DRONE].quant = sc->tr[O_VOICE].quant = sc->tr[O_NOISE].quant = sc->tr[O_CLAW].quant = 0;
    rit_steps(sc, O_SPASM, "......?.....x...", 67, 60, 24);
    sc->p[O_SPASM][P_D1] = 90; sc->p[O_SPASM][P_DECAY] = 80;
    sc->p[O_DRONE][P_D1] = 60; sc->p[O_DRONE][P_MOTION] = 80;
    sc->p[O_VOICE][P_D1] = 30; sc->p[O_VOICE][P_MOTION] = 90;
    sc->mix.lvl[CH_DRONE] = 76; sc->mix.lvl[CH_VOICE] = 64; sc->mix.lvl[CH_NOISE] = 70; sc->mix.lvl[CH_DRUM] = 82;
    sc->mix.sb[CH_DRUM] = 40; sc->mix.sb[CH_VOICE] = 60; sc->mix.sb[CH_SPASM] = 70; sc->mix.sb[CH_NOISE] = 50;
    sc->fx.r_decay = 70; sc->fx.r_size = 90; sc->fx.r_mix = 56; sc->fx.d_time = 7; sc->fx.d_fb = 46; sc->fx.d_mix = 30;
    sc->tur.loop = 85; sc->tur.r_rot = 70;
    sc->mix.pump = 70;                          /* (the drone breathes with the heart) */
}

/* ---- rhythm made of noise (D20): the four sketches the user chose, in the noise core */

/* RITUAL: FROZEN. Each 16th a slice of noise, each its own band, length, shape and place (SCATTER), the slices locked
   by the register: the same noise comes back every bar and is the groove; LOOP 92 % lets a slice slip now and then.
   Low slices of noise on the downbeats (HEART as noise). 128 BPM, dry */
static void rit_frozen(seq_scene_t *sc)
{
    static const uint8_t SL[BIO_NP] = NC(38, 20, 50, 40,  10, 30, 50, 0, 0, 20, 0,  0, 100, 20, 0,  24, 100, 50,  100, 0, 0);
    static const uint8_t LO[BIO_NP] = NC(4, 10, 50, 34,  10, 35, 0, 0, 0, 25, 0,  0, 100, 0, 0,  24, 100, 50,  25, 0, 0);
    seq_scene_blank(sc, 0xF2023Eu);
    rit_name(sc, "FROZEN");
    sc->bpm10 = 1280;
    rit_core(sc, O_NOISE, SL);
    rit_core(sc, O_HEART, LO);
    rit_steps(sc, O_NOISE, "..xx.?x...xx.x?x", 60, 100, 96);
    rit_steps(sc, O_HEART, "x...x...x.......", 60, 104, 86);
    sc->tur.loop = 92; sc->tur.depth = 0;
    sc->mix.lvl[CH_NOISE] = 92; sc->mix.lvl[CH_DRUM] = 90;
    rit_dry(sc);
    sc->fx.r_mix = 0;
}

/* RITUAL: STREAM. One noise that never stops (pink, grained, its band drifting over four bars), cut by a gate: the
   rhythm is where it is open. Accents open its band; the end of every other bar stutters in 32nds (the same noise
   again: its step's locked seed). Under it a low noise that breathes with the beat (PUMP, from a HEART you do not hear),
   a thin high noise on the off-beat 8ths. 128 BPM */
static void rit_stream(seq_scene_t *sc)
{
    static const uint8_t G[32] = {4, 0, 2, 2, 4, 1, 0, 3, 4, 0, 2, 0, 4, 1, 1, 1,   4, 0, 2, 2, 4, 1, 0, 3, 4, 2, 0, 2, 9, 9, 9, 9};
    static const uint8_t ACC[16] = {1, 0, 0, 1, 0, 0, 0, 1, 1, 0, 0, 1, 0, 0, 1, 0};
    static const uint8_t ST[BIO_NP] = NC(40, 66, 50, 70,  30, 34, 50, 0, 0, 35, 0,  0, 100, 30, 0,  24, 100, 46,  0, 0, 70);
    static const uint8_t LO[BIO_NP] = NC(2, 10, 50, 80,  10, 20, 0, 0, 0, 10, 0,  0, 100, 0, 40,  24, 100, 50,  0, 0, 0);
    static const uint8_t HI[BIO_NP] = NC(88, 10, 50, 40,  10, 10, 100, 0, 0, 0, 0,  0, 100, 40, 10,  24, 70, 75,  0, 0, 0);
    uint32_t i;
    seq_scene_blank(sc, 0x57E4A3u);
    rit_name(sc, "STREAM");
    sc->bpm10 = 1280;
    rit_core(sc, O_BONE, ST);
    rit_core(sc, O_NOISE, LO);
    rit_core(sc, O_TEETH, HI);
    sc->tr[O_BONE].len = 32;
    sc->tr[O_BONE].quant = 1;                   /* (1/32: the stutter's gates stay where they are) */
    for (i = 0; i < 32u; i++) {
        uint32_t fl = ACC[i & 15u] ? EV_ACC : 0u;
        if (G[i] == 9u) {                       /* two gates of a 64th in the step, the same noise */
            rit_tick(sc, O_BONE, i * 96u, 24, 60, 96, fl);
            rit_tick(sc, O_BONE, i * 96u + 48u, 24, 60, 90, 0);
        } else if (G[i]) {
            rit_tick(sc, O_BONE, i * 96u, G[i] * 24u, 60, 96, fl);
        }
    }
    seq_put(sc, O_NOISE, 0, 16 * 96, 60, 100, 0);
    sc->p[O_HEART][P_LEVEL] = 0;                /* (heard only in the PUMP) */
    rit_steps(sc, O_HEART, "x...x...x...x...", 60, 120, 48);
    rit_steps(sc, O_TEETH, "..x...x...x...x.", 60, 90, 72);
    sc->mix.pump = 95;
    sc->mix.lvl[CH_DRUM] = 92; sc->mix.lvl[CH_NOISE] = 88;
    rit_dry(sc);
    sc->tur.depth = 0;
}

/* RITUAL: WALL. A wall of noise through a band that sits on the note (TONE, a fifth over it): a riff of hiss in D
   Phrygian, struck each step and held low between. Two bars of 16ths, a bar of triplets, a bar of 32nds. A wind of
   noise over it in 8ths, drifting. 128 BPM */
static void rit_wall(seq_scene_t *sc)
{
    static const uint8_t RIFF[16] = {38, 38, 41, 38, 50, 39, 38, 45, 38, 38, 41, 43, 50, 46, 45, 39};
    static const uint8_t WA[BIO_NP] = NC(75, 10, 50, 48,  10, 92, 50, 0, 0, 30, 0,  0, 4, 40, 10,  24, 100, 40,  0, 70, 0);
    static const uint8_t WI[BIO_NP] = NC(52, 10, 50, 60,  10, 8, 50, 0, 0, 0, 0,  0, 100, 60, 5,  24, 76, 70,  0, 0, 60);
    uint32_t i;
    seq_scene_blank(sc, 0x3A11Eu);
    rit_name(sc, "WALL");
    sc->bpm10 = 1280;
    rit_core(sc, O_NOISE, WA);
    rit_core(sc, O_CLAW, WI);
    sc->tr[O_NOISE].len = 64;
    sc->tr[O_NOISE].quant = 0;
    for (i = 0; i < 32u; i++) rit_tick(sc, O_NOISE, i * 96u, 96, RIFF[i & 15u], 100, 0);                 /* 16ths */
    for (i = 0; i < 12u; i++) rit_tick(sc, O_NOISE, 3072u + i * 128u, 128, RIFF[i], 100, 0);          /* triplets */
    for (i = 0; i < 32u; i++) rit_tick(sc, O_NOISE, 4608u + i * 48u, 48, RIFF[(i / 2u) & 15u], 96, 0);  /* 32nds */
    rit_steps(sc, O_CLAW, "x.x.x.x.x.x.x.x.", 60, 90, 96);
    sc->mix.lvl[CH_NOISE] = 92; sc->mix.lvl[CH_DRUM] = 80;
    rit_dry(sc);
    sc->fx.r_mix = 18;
    sc->tur.depth = 0;
}

/* RITUAL: DATA. Clinical and digital: clicks with a high ping, white bursts of a few ms scattered left and right,
   shift-register noise at four clocks (pitched, digital), stutters, and silence. A bar of each of four patterns.
   128 BPM, dry */
static void rit_data(seq_scene_t *sc)
{
    static const char *const P[4] = {"c.b.d..c.bd.c.s.", "c.b.d.cc.b..d.bc", "c..bd..cdb..c.s.", "cbcb.d.c..bdd..s"};
    static const uint8_t DN[4] = {48, 60, 67, 79};
    static const uint8_t CK[BIO_NP] = NC(84, 10, 50, 2,  10, 95, 50, 0, 100, 0, 0,  0, 0, 0, 0,  24, 100, 14,  0, 0, 0);
    static const uint8_t BU[BIO_NP] = NC(75, 10, 50, 5,  10, 12, 85, 0, 30, 0, 0,  0, 100, 0, 0,  24, 100, 50,  60, 0, 0);
    static const uint8_t DG[BIO_NP] = NC(46, 10, 50, 70,  50, 10, 10, 0, 0, 0, 0,  0, 100, 0, 0,  24, 100, 80,  0, 0, 0);
    uint32_t b, i;
    seq_scene_blank(sc, 0xDA7Au);
    rit_name(sc, "DATA");
    sc->bpm10 = 1280;
    rit_core(sc, O_TEETH, CK);
    rit_core(sc, O_BONE, BU);
    rit_core(sc, O_CLAW, DG);
    sc->tr[O_TEETH].len = sc->tr[O_BONE].len = sc->tr[O_CLAW].len = 64;
    sc->tr[O_TEETH].quant = sc->tr[O_BONE].quant = sc->tr[O_CLAW].quant = 0;
    for (b = 0; b < 4u; b++)
        for (i = 0; i < 16u; i++) {
            uint32_t t = (b * 16u + i) * 96u, h = bio_hash(0xDA7Au, b * 16u + i);
            switch (P[b][i]) {
            case 'c': rit_tick(sc, O_TEETH, t, 12, 60, 127, 0); break;
            case 'b': rit_tick(sc, O_BONE, t, 8, 60, 120, 0); break;
            case 'd': rit_tick(sc, O_CLAW, t, 16u + (h & 31u) + (h >> 8 & 7u), DN[h >> 16 & 3u], 120, 0); break;
            case 's': {                         /* a stutter: six of the same digital 32nd */
                uint32_t k;
                for (k = 0; k < 6u; k++) rit_tick(sc, O_CLAW, t + k * 16u, 12, DN[h >> 16 & 3u], 104 - k * 8u, 0);
                break;
            }
            default: break;
            }
        }
    sc->mix.lvl[CH_DRUM] = 96;
    rit_dry(sc);
    sc->fx.r_mix = 0;
    sc->mix.master = 96;
    sc->tur.depth = 0;
}

/* the factory list (SHIFT + PRESETS) */
typedef void (*rit_fn)(seq_scene_t *);
static void rit_awaken(seq_scene_t *sc) { seq_scene_awaken(sc); }
static void rit_infection(seq_scene_t *sc)
{
    seq_scene_awaken(sc);
    sc->tur.loop = 80; sc->tur.depth = 30; sc->tur.rattle = 45; sc->tur.len_i = 7;
    sc->seed = 0xA5F01u;
    rit_name(sc, "INFECTION");
}
static void rit_empty(seq_scene_t *sc) { seq_scene_blank(sc, 0x7E3u); rit_name(sc, "EMPTY BODY"); }
/* (the first four are the scenes before anything is saved: the document's start, then rhythm made of noise, D20) */
static const rit_fn BIO_RITUAL[] = {rit_awaken, rit_frozen, rit_stream, rit_wall, rit_data, rit_static, rit_splice,
                                    rit_void, rit_infection, rit_empty};
static const char *const BIO_RITUAL_NAME[] = {"AWAKENING", "FROZEN", "STREAM", "WALL", "DATA", "STATIC CULT", "SPLICE",
                                              "VOID", "INFECTION", "EMPTY BODY"};
#define BIO_NRITUAL (sizeof BIO_RITUAL / sizeof BIO_RITUAL[0])
#endif
