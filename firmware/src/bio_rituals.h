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
    {b, t, m, d, src, res, mode, bur, clk, drv, cr, 50, a, g, w, r, tn, lvl, pan, 0, scat, tone, drift, 0}
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

/* RITUAL: VOID. Slow (70 BPM) and wide: a huge HEART twice a bar, a bone far off, a fire held for bars, the
   drone in front, the throat singing long notes, crackle in the dark, a long reverb */
static void rit_void(seq_scene_t *sc)
{
    seq_scene_blank(sc, 0x7031Du);
    rit_name(sc, "VOID");
    sc->bpm10 = 700;
    sc->p[O_HEART][P_BODY] = 96; sc->p[O_HEART][P_DECAY] = 84; sc->p[O_HEART][P_TEXTURE] = 30;
    sc->p[O_BONE][P_DECAY] = 90; sc->p[O_BONE][P_BODY] = 80;
    rit_perc(sc, O_CLAW, "EMBERS", 30);
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
    static const uint8_t SL[BIO_NP] = NC(38, 20, 50, 40,  6, 30, 50, 0, 0, 20, 0,  0, 100, 20, 0,  24, 100, 50,  100, 0, 0);
    static const uint8_t LO[BIO_NP] = NC(4, 10, 50, 34,  6, 35, 0, 0, 0, 25, 0,  0, 100, 0, 0,  24, 100, 50,  25, 0, 0);
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
    static const uint8_t ST[BIO_NP] = NC(40, 66, 50, 70,  18, 34, 50, 0, 0, 35, 0,  0, 100, 30, 0,  24, 100, 46,  0, 0, 50);
    static const uint8_t LO[BIO_NP] = NC(2, 10, 50, 80,  6, 20, 0, 0, 0, 10, 0,  0, 100, 0, 40,  24, 100, 50,  0, 0, 0);
    static const uint8_t HI[BIO_NP] = NC(88, 10, 50, 40,  6, 10, 100, 0, 0, 0, 0,  0, 100, 40, 10,  24, 70, 75,  0, 0, 0);
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
   Phrygian, struck each step and held low between. Two bars of 16ths, a bar of triplets, a bar of 32nds. A machine's
   noise over it in 8ths. 128 BPM */
static void rit_wall(seq_scene_t *sc)
{
    static const uint8_t RIFF[16] = {38, 38, 41, 38, 50, 39, 38, 45, 38, 38, 41, 43, 50, 46, 45, 39};
    static const uint8_t WA[BIO_NP] = NC(75, 10, 50, 48,  6, 92, 50, 0, 0, 30, 0,  0, 4, 40, 10,  24, 100, 40,  0, 70, 0);
    static const uint8_t WI[BIO_NP] = NC(60, 10, 50, 60,  43, 30, 60, 0, 0, 0, 0,  0, 100, 20, 5,  24, 70, 70,  0, 0, 0);
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
    static const uint8_t CK[BIO_NP] = NC(84, 10, 50, 2,  6, 95, 50, 0, 100, 0, 0,  0, 0, 0, 0,  24, 100, 14,  0, 0, 0);
    static const uint8_t BU[BIO_NP] = NC(75, 10, 50, 5,  6, 12, 85, 0, 30, 0, 0,  0, 100, 0, 0,  24, 100, 50,  60, 0, 0);
    static const uint8_t DG[BIO_NP] = NC(46, 10, 50, 70,  31, 10, 10, 0, 0, 0, 0,  0, 100, 0, 0,  24, 100, 80,  0, 0, 0);
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

/* RITUAL: EMBERS. Organic crackle (D21): a fire held under it all, sparks locked into a groove (each its own,
   SCATTER), grease on two and four, a creak of wood that comes and goes, a low HEART, the drone in the dark. 100 BPM */
static void rit_embers(seq_scene_t *sc)
{
    seq_scene_blank(sc, 0xE3BE25u);
    rit_name(sc, "EMBERS");
    sc->bpm10 = 1000;
    rit_perc(sc, O_NOISE, "EMBERS", 50);
    rit_perc(sc, O_TEETH, "SPARKS", 66);
    rit_perc(sc, O_BONE, "GREASE", 40);
    rit_perc(sc, O_CLAW, "CREAK", 28);
    sc->p[O_TEETH][P_E1] = 50;                  /* each spark its own */
    sc->tr[O_NOISE].len = 64;
    seq_put(sc, O_NOISE, 0, 64 * 96 - 12, 60, 100, 0);
    rit_steps(sc, O_TEETH, "x.xx.x.xx.x.x.xx", 72, 96, 48);
    rit_steps(sc, O_BONE, "....o.......x..g", 60, 104, 48);
    sc->tr[O_CLAW].len = 32;
    seq_put(sc, O_CLAW, 8, 20 * 96, 60, 100, 0);
    sc->p[O_HEART][P_BODY] = 88; sc->p[O_HEART][P_DECAY] = 60;
    rit_steps(sc, O_HEART, "x.....x...x.....", 60, 112, 96);
    sc->tr[O_DRONE].len = 32;
    seq_put(sc, O_DRONE, 0, 32 * 96 - 12, 38, 90, 0);
    sc->p[O_DRONE][P_D1] = 30;
    sc->mix.lvl[CH_DRONE] = 42; sc->mix.lvl[CH_NOISE] = 90; sc->mix.lvl[CH_DRUM] = 88;
    sc->mix.lvl[CH_BASS] = sc->mix.lvl[CH_SPASM] = sc->mix.lvl[CH_VOICE] = 0;
    sc->fx.d_mix = 0; sc->fx.c_mix = 0; sc->fx.r_mix = 24; sc->fx.r_decay = 30; sc->fx.r_size = 50;
    sc->tur.loop = 96; sc->tur.depth = 10;
}

/* ---- landscapes (D21): the tonal organs as other bodies (MODEL), the reverb as other rooms (SPACE) */
static void rit_model(seq_scene_t *sc, uint32_t org, uint32_t model, uint32_t body, uint32_t tex, uint32_t mot, uint32_t dec)
{
    sc->p[org][P_E1] = BIO_LAND(model);
    sc->p[org][P_BODY] = (uint8_t)body; sc->p[org][P_TEXTURE] = (uint8_t)tex;
    sc->p[org][P_MOTION] = (uint8_t)mot; sc->p[org][P_DECAY] = (uint8_t)dec;
}

/* RITUAL: FOUNDRY. Industry, 112 BPM: a piston running under it all (BASS), the hum of the mains with its sparks
   (DRONE), bowed steel (VOICE), an anvil struck off the beat (SPASM as a bell of iron), steam and metal slices locked
   into a groove (NOISE), a hinge, a deep HEART; in a room of metal */
static void rit_foundry(seq_scene_t *sc)
{
    static const uint8_t SL[BIO_NP] = NC(70, 20, 50, 34,  43, 40, 70, 0, 0, 30, 20,  0, 100, 30, 0,  24, 90, 60,  70, 0, 0);
    seq_scene_blank(sc, 0xF0D41u);
    rit_name(sc, "FOUNDRY");
    sc->bpm10 = 1120;
    rit_model(sc, O_BASS, L_PISTON, 62, 20, 70, 50);
    sc->p[O_BASS][P_S] = 100;
    seq_put(sc, O_BASS, 0, 16 * 96 - 6, 26, 104, 0);
    rit_model(sc, O_DRONE, L_HUM, 40, 30, 40, 50);
    sc->p[O_DRONE][P_A] = 20;
    sc->tr[O_DRONE].len = 32;
    seq_put(sc, O_DRONE, 0, 32 * 96 - 6, 38, 90, 0);
    rit_model(sc, O_VOICE, L_STEEL, 60, 72, 50, 50);
    sc->tr[O_VOICE].len = 64;
    seq_put(sc, O_VOICE, 8, 20 * 96, 62, 80, 0);
    seq_put(sc, O_VOICE, 40, 16 * 96, 65, 80, 0);
    rit_model(sc, O_SPASM, L_BELL, 55, 70, 0, 60);
    rit_steps(sc, O_SPASM, "..x.......x..?..", 50, 100, 24);
    rit_core(sc, O_NOISE, SL);
    rit_steps(sc, O_NOISE, "x.xxx.x.xx.xx.x.", 60, 96, 60);
    rit_perc(sc, O_CLAW, "HINGE", 30);
    rit_steps(sc, O_CLAW, ".......x.......?", 60, 90, 48);
    sc->p[O_HEART][P_BODY] = 90; sc->p[O_HEART][P_DECAY] = 40;
    rit_steps(sc, O_HEART, "x.......x.x.....", 60, 112, 96);
    rit_perc(sc, O_BONE, "CRUSH", 46);
    rit_steps(sc, O_BONE, "....x.......x...", 60, 100, 48);
    sc->mix.lvl[CH_BASS] = 84; sc->mix.lvl[CH_DRONE] = 52; sc->mix.lvl[CH_VOICE] = 58; sc->mix.lvl[CH_SPASM] = 64;
    sc->mix.lvl[CH_NOISE] = 80; sc->mix.lvl[CH_DRUM] = 88;
    sc->mix.sb[CH_SPASM] = 50; sc->mix.sb[CH_VOICE] = 40; sc->mix.sb[CH_DRUM] = 24; sc->mix.sb[CH_NOISE] = 20;
    sc->fx.r_space = 3; sc->fx.r_mix = 30; sc->fx.r_decay = 30; sc->fx.d_mix = 0; sc->fx.c_mix = 0;
    sc->mix.pump = 40;
    sc->tur.loop = 95; sc->tur.depth = 10;
}

/* RITUAL: ORBIT. Cosmic, 76 BPM: glass beating slowly (DRONE), a choir of throats (VOICE) moving through vowels, bells
   far apart (SPASM), ice crackling in the cold and creaking now and then; in a vast space */
static void rit_orbit(seq_scene_t *sc)
{
    static const uint8_t ICE[BIO_NP] = NC(80, 30, 50, 30,  56, 30, 70, 0, 0, 0, 0,  0, 0, 60, 30,  24, 100, 50,  60, 0, 0);
    static const uint8_t WIND[BIO_NP] = NC(80, 25, 50, 80,  56, 20, 80, 0, 0, 0, 0,  40, 100, 30, 70,  24, 72, 50,  0, 0, 0);   /* (ice, not wind: D24) */
    seq_scene_blank(sc, 0x0B17u);
    rit_name(sc, "ORBIT");
    sc->bpm10 = 760;
    rit_model(sc, O_DRONE, L_GLASS, 40, 60, 70, 60);
    sc->tr[O_DRONE].len = 32;
    seq_put(sc, O_DRONE, 0, 16 * 96 - 6, 50, 90, 0);
    seq_put(sc, O_DRONE, 16, 16 * 96 - 6, 46, 90, 0);
    rit_model(sc, O_VOICE, L_CHOIR, 30, 30, 60, 60);
    sc->p[O_VOICE][P_A] = 70; sc->p[O_VOICE][P_R] = 80;
    sc->tr[O_VOICE].len = 64;
    seq_put(sc, O_VOICE, 0, 15 * 96, 62, 80, 0);
    seq_put(sc, O_VOICE, 16, 15 * 96, 65, 80, 0);
    seq_put(sc, O_VOICE, 32, 15 * 96, 69, 80, 0);
    seq_put(sc, O_VOICE, 48, 15 * 96, 64, 80, 0);
    rit_model(sc, O_SPASM, L_BELL, 40, 25, 30, 90);
    sc->tr[O_SPASM].len = 32;
    rit_steps(sc, O_SPASM, "......x...........?.....x.......", 74, 90, 48);
    rit_core(sc, O_TEETH, ICE);
    rit_steps(sc, O_TEETH, "x..?..x.?.x...?.", 72, 80, 24);
    rit_perc(sc, O_CLAW, "CREAK", 70);
    sc->tr[O_CLAW].len = 64;
    seq_put(sc, O_CLAW, 20, 10 * 96, 60, 80, 0);
    rit_core(sc, O_NOISE, WIND);
    sc->tr[O_NOISE].len = 64;
    seq_put(sc, O_NOISE, 0, 64 * 96 - 6, 60, 90, 0);
    sc->p[O_HEART][P_BODY] = 96; sc->p[O_HEART][P_DECAY] = 80;
    rit_steps(sc, O_HEART, "x...............", 60, 96, 96);
    sc->mix.lvl[CH_DRONE] = 74; sc->mix.lvl[CH_VOICE] = 66; sc->mix.lvl[CH_SPASM] = 62; sc->mix.lvl[CH_BASS] = 0;
    sc->mix.lvl[CH_NOISE] = 60; sc->mix.lvl[CH_DRUM] = 80;
    sc->mix.sb[CH_DRONE] = 50; sc->mix.sb[CH_VOICE] = 60; sc->mix.sb[CH_SPASM] = 80; sc->mix.sb[CH_DRUM] = 50; sc->mix.sb[CH_NOISE] = 40;
    sc->fx.r_space = 4; sc->fx.r_mix = 52; sc->fx.r_decay = 70; sc->fx.r_size = 90; sc->fx.d_time = 7; sc->fx.d_fb = 50;
    sc->fx.d_mix = 24; sc->fx.c_mix = 0;
    sc->tur.loop = 90; sc->tur.depth = 20;
}

/* RITUAL: INNARDS. The body from inside, 92 BPM: blood pulsing with the beat (DRONE), a wet squelching bass (SLIME),
   bubbles rising (VOICE), a heartbeat (HEART, lub and dub), grease on two and four, a sizzle of flesh held low, a wet
   fry gated in and out (NOISE); in a cave */
static void rit_innards(seq_scene_t *sc)
{
    static const uint8_t BR[BIO_NP] = NC(40, 40, 50, 60,  81, 30, 30, 0, 0, 10, 0,  10, 100, 20, 40,  24, 84, 50,  0, 0, 0);
    seq_scene_blank(sc, 0x1AA4D5u);
    rit_name(sc, "INNARDS");
    sc->bpm10 = 920;
    rit_model(sc, O_DRONE, L_BLOOD, 30, 50, 80, 50);
    sc->p[O_DRONE][P_A] = 20;
    sc->tr[O_DRONE].len = 32;
    seq_put(sc, O_DRONE, 0, 32 * 96 - 6, 38, 100, 0);
    rit_model(sc, O_BASS, L_SLIME, 50, 60, 40, 50);
    rit_steps(sc, O_BASS, "x..x..x...x.x...", 26, 100, 60);
    sc->tr[O_BASS].ev[1].note = 29; sc->tr[O_BASS].ev[3].note = 32; sc->tr[O_BASS].ev[4].note = 27;
    rit_model(sc, O_VOICE, L_BUBBLE, 40, 40, 60, 50);
    sc->p[O_VOICE][P_A] = 20;
    sc->tr[O_VOICE].len = 32;
    seq_put(sc, O_VOICE, 4, 24 * 96, 62, 90, 0);
    sc->p[O_HEART][P_BODY] = 84; sc->p[O_HEART][P_DECAY] = 50; sc->p[O_HEART][P_TEXTURE] = 30;
    rit_steps(sc, O_HEART, "x..g....x..g....", 60, 108, 96);
    rit_perc(sc, O_BONE, "GREASE", 44);
    rit_steps(sc, O_BONE, "....x.......x..g", 60, 100, 48);
    rit_perc(sc, O_TEETH, "SIZZLE", 60);
    sc->tr[O_TEETH].len = 64;
    seq_put(sc, O_TEETH, 0, 64 * 96 - 6, 60, 70, 0);
    rit_core(sc, O_NOISE, BR);
    rit_steps(sc, O_NOISE, "x...x...x.x.x...", 60, 90, 72);
    sc->mix.lvl[CH_DRONE] = 78; sc->mix.lvl[CH_BASS] = 80; sc->mix.lvl[CH_VOICE] = 62; sc->mix.lvl[CH_SPASM] = 0;
    sc->mix.lvl[CH_NOISE] = 56; sc->mix.lvl[CH_DRUM] = 86;
    sc->mix.sb[CH_VOICE] = 40; sc->mix.sb[CH_BASS] = 14; sc->mix.sb[CH_DRUM] = 20;
    sc->fx.r_space = 2; sc->fx.r_mix = 30; sc->fx.r_decay = 40; sc->fx.d_mix = 0; sc->fx.c_mix = 0;
    sc->mix.pump = 30;
    sc->tur.loop = 94; sc->tur.depth = 15;
}

/* ---- dark ambient meets noise, a deep kick (D22) */

/* RITUAL: ABYSS. 122 BPM, D minor. A deep kick with almost no top on the four, its RUMBLE under it; everything else
   breathes with it (PUMP): a fire crackling held, the reverb and the delay. Noise in time: dark ticks off the beat and
   ghosts locked between, sparks now and then, a low clap far away on two and four. Dub chords off the beat into a
   dark delay, a bass on the off-beats, the drone and a choir in a cave */
static void rit_abyss(seq_scene_t *sc)
{
    static const uint8_t TK[BIO_NP] = NC(72, 10, 50, 30,  6, 15, 90, 0, 0, 0, 0,  0, 0, 30, 20,  24, 78, 62,  25, 0, 0);
    static const uint8_t CL[BIO_NP] = NC(32, 20, 52, 55,  6, 30, 50, 80, 0, 10, 0,  0, 0, 40, 30,  24, 74, 46,  0, 0, 0);
    seq_scene_blank(sc, 0xAB7550u);
    rit_name(sc, "ABYSS");
    sc->bpm10 = 1220;
    rit_perc(sc, O_HEART, "DEEP KICK", 50);
    rit_steps(sc, O_HEART, "x...x...x...x...", 60, 112, 96);
    rit_core(sc, O_BONE, CL);
    rit_steps(sc, O_BONE, "....x.......x...", 60, 84, 48);
    rit_core(sc, O_TEETH, TK);
    rit_steps(sc, O_TEETH, "g.x.g.xgg.x.g.xg", 60, 88, 30);
    rit_perc(sc, O_CLAW, "SPARKS", 30);
    sc->p[O_CLAW][P_E1] = 40;
    sc->p[O_CLAW][P_LEVEL] = 80;
    sc->tr[O_CLAW].len = 32;
    rit_steps(sc, O_CLAW, ".......x.............x.....x....", 72, 90, 48);
    rit_perc(sc, O_NOISE, "EMBERS", 50);
    sc->tr[O_NOISE].len = 64;
    seq_put(sc, O_NOISE, 0, 64 * 96 - 6, 60, 96, 0);
    /* dub chords off the beat, two bars: D, D, C, B flat (a minor 7th) */
    rit_model(sc, O_SPASM, L_DUB, 30, 30, 60, 45);
    sc->p[O_SPASM][P_D] = 40;
    sc->tr[O_SPASM].len = 32;
    rit_steps(sc, O_SPASM, "...x.......x.......x.......x....", 50, 96, 30);
    sc->tr[O_SPASM].ev[2].note = 48; sc->tr[O_SPASM].ev[3].note = 46;
    /* the bass on the off-beats, low, short */
    rit_steps(sc, O_BASS, "..x...x...x...x.", 26, 92, 40);
    sc->p[O_BASS][P_D1] = 26; sc->p[O_BASS][P_D] = 18; sc->p[O_BASS][P_TEXTURE] = 36;
    /* the drone under it all, the choir far away */
    sc->tr[O_DRONE].len = 64;
    seq_put(sc, O_DRONE, 0, 32 * 96 - 6, 38, 90, 0);
    seq_put(sc, O_DRONE, 32, 32 * 96 - 6, 34, 90, 0);
    sc->p[O_DRONE][P_D1] = 26; sc->p[O_DRONE][P_MOTION] = 70;
    rit_model(sc, O_VOICE, L_CHOIR, 22, 40, 50, 60);
    sc->p[O_VOICE][P_A] = 80; sc->p[O_VOICE][P_R] = 85;
    sc->tr[O_VOICE].len = 64;
    seq_put(sc, O_VOICE, 0, 30 * 96, 62, 70, 0);
    seq_put(sc, O_VOICE, 32, 30 * 96, 60, 70, 0);
    /* the mix: the kick and its rumble, everything else breathing with it */
    sc->mix.lvl[CH_DRUM] = 92; sc->mix.lvl[CH_BASS] = 66; sc->mix.lvl[CH_DRONE] = 56; sc->mix.lvl[CH_SPASM] = 74;
    sc->mix.lvl[CH_VOICE] = 44; sc->mix.lvl[CH_NOISE] = 70;
    sc->mix.sa[CH_SPASM] = 70; sc->mix.sb[CH_SPASM] = 40; sc->mix.sb[CH_DRUM] = 22; sc->mix.sa[CH_DRUM] = 0;
    sc->mix.sb[CH_DRONE] = 40; sc->mix.sb[CH_VOICE] = 70; sc->mix.sb[CH_NOISE] = 34; sc->mix.sb[CH_BASS] = 0;
    sc->mix.sa[CH_BASS] = 0; sc->mix.sa[CH_VOICE] = 20;
    sc->mix.pump = 55; sc->mix.rumble = 62; sc->mix.master = 80;
    sc->fx.d_time = 4; sc->fx.d_fb = 56; sc->fx.d_tone = 28; sc->fx.d_mix = 34; sc->fx.d_rev = 20; sc->fx.d_ping = 1;
    sc->fx.r_space = 2; sc->fx.r_mix = 36; sc->fx.r_decay = 60; sc->fx.r_size = 80; sc->fx.r_damp = 30; sc->fx.c_mix = 0;
    sc->tur.loop = 97; sc->tur.depth = 10; sc->tur.rattle = 15;
}

/* RITUAL: ASH. 116 BPM, the ambient side of ABYSS: a sub kick, softer, its rumble bigger; no ticks, only the fire held
   and wood creaking now and then, sparks in time; glass beating under a choir, minor-ninth dub chords far apart into
   the delay; a vast space */
static void rit_ash(seq_scene_t *sc)
{
    seq_scene_blank(sc, 0xA54u);
    rit_name(sc, "ASH");
    sc->bpm10 = 1160;
    rit_perc(sc, O_HEART, "SUB KICK", 50);
    rit_steps(sc, O_HEART, "x...x...x...x...", 60, 104, 96);
    rit_perc(sc, O_CLAW, "CREAK", 26);
    sc->tr[O_CLAW].len = 64;
    seq_put(sc, O_CLAW, 12, 8 * 96, 60, 80, 0);
    seq_put(sc, O_CLAW, 44, 6 * 96, 55, 80, 0);
    rit_perc(sc, O_TEETH, "SPARKS", 70);
    sc->p[O_TEETH][P_E1] = 50; sc->p[O_TEETH][P_LEVEL] = 74;
    rit_steps(sc, O_TEETH, "..x...x?..x...x.", 72, 80, 40);
    rit_perc(sc, O_NOISE, "EMBERS", 50);
    sc->tr[O_NOISE].len = 64;
    seq_put(sc, O_NOISE, 0, 64 * 96 - 6, 60, 100, 0);
    rit_model(sc, O_SPASM, L_DUB, 26, 60, 70, 60);
    sc->p[O_SPASM][P_D] = 60;
    sc->tr[O_SPASM].len = 64;
    seq_put(sc, O_SPASM, 6, 30, 50, 96, 0);
    seq_put(sc, O_SPASM, 27, 30, 46, 90, 0);
    seq_put(sc, O_SPASM, 38, 30, 50, 96, 0);
    seq_put(sc, O_SPASM, 59, 30, 45, 90, 0);
    rit_model(sc, O_DRONE, L_GLASS, 50, 40, 60, 70);
    sc->p[O_DRONE][P_A] = 70;
    sc->tr[O_DRONE].len = 64;
    seq_put(sc, O_DRONE, 0, 32 * 96 - 6, 38, 90, 0);
    seq_put(sc, O_DRONE, 32, 32 * 96 - 6, 41, 90, 0);
    rit_model(sc, O_VOICE, L_CHOIR, 18, 50, 60, 60);
    sc->p[O_VOICE][P_A] = 90; sc->p[O_VOICE][P_R] = 90;
    sc->tr[O_VOICE].len = 64;
    seq_put(sc, O_VOICE, 0, 62 * 96, 57, 70, 0);
    sc->mix.lvl[CH_DRUM] = 88; sc->mix.lvl[CH_BASS] = 0; sc->mix.lvl[CH_DRONE] = 50; sc->mix.lvl[CH_SPASM] = 72;
    sc->mix.lvl[CH_VOICE] = 46; sc->mix.lvl[CH_NOISE] = 76;
    sc->mix.sa[CH_SPASM] = 80; sc->mix.sb[CH_SPASM] = 50; sc->mix.sb[CH_DRUM] = 26; sc->mix.sa[CH_DRUM] = 0;
    sc->mix.sb[CH_DRONE] = 50; sc->mix.sb[CH_VOICE] = 80; sc->mix.sb[CH_NOISE] = 40;
    sc->mix.pump = 60; sc->mix.rumble = 80; sc->mix.master = 80;
    sc->fx.d_time = 7; sc->fx.d_fb = 62; sc->fx.d_tone = 24; sc->fx.d_mix = 36; sc->fx.d_rev = 30; sc->fx.d_ping = 1;
    sc->fx.r_space = 4; sc->fx.r_mix = 44; sc->fx.r_decay = 70; sc->fx.r_size = 90; sc->fx.r_damp = 26; sc->fx.c_mix = 0;
    sc->tur.loop = 97; sc->tur.depth = 10;
}

/* ---- grim dark industrial ambience, with a melody and a pulse (D23: the user's reference, measured) */

/* RITUAL: RUST. 108 BPM, D Phrygian. The body is a drone: a MASS on D1 (octaves, a fifth, grit) breathing under a
   low-pass, a CLUSTER of semitones above it that moves from D to E flat; under them a smouldering rumble of noise.
   A pulse: a deep kick on one and three and a push before one, its rumble; a bone breaking far off on two and four;
   slices of dark noise locked in 16ths, low; electricity frying in the middle. A melody: a hollow bell the register plays
   from the scale (WAIL), slipping slowly, into a dark delay. DARK rolls the top off */
static void rit_rust(seq_scene_t *sc)
{
    static const uint8_t SL[BIO_NP] = NC(26, 10, 50, 30,  6, 30, 10, 0, 0, 10, 0,  2, 100, 10, 10,  24, 70, 54,  35, 0, 0);
    static const uint8_t MW[BIO_NP] = NC(52, 30, 50, 80,  81, 15, 30, 0, 0, 0, 0,  20, 100, 30, 60,  24, 58, 40,  0, 0, 0);
    static const uint8_t RB[BIO_NP] = NC(22, 10, 50, 80,  68, 20, 10, 0, 0, 10, 0,  20, 100, 0, 60,  24, 100, 50,  0, 0, 0);
    seq_scene_blank(sc, 0x2057u);
    rit_name(sc, "RUST");
    sc->bpm10 = 1080;
    /* the body */
    rit_model(sc, O_DRONE, L_MASS, 34, 50, 60, 60);
    sc->p[O_DRONE][P_A] = 70; sc->p[O_DRONE][P_R] = 70;
    sc->tr[O_DRONE].len = 64;
    seq_put(sc, O_DRONE, 0, 64 * 96 - 6, 26, 100, 0);
    rit_model(sc, O_VOICE, L_CLUSTER, 60, 40, 70, 60);
    sc->p[O_VOICE][P_A] = 90; sc->p[O_VOICE][P_R] = 85;
    sc->tr[O_VOICE].len = 64;
    seq_put(sc, O_VOICE, 0, 31 * 96, 38, 90, 0);
    seq_put(sc, O_VOICE, 32, 31 * 96, 39, 90, 0);
    sc->p[O_VOICE][P_PAN] = 30;
    rit_core(sc, O_NOISE, RB);
    sc->tr[O_NOISE].len = 64;
    seq_put(sc, O_NOISE, 0, 64 * 96 - 6, 60, 100, 0);
    /* the pulse */
    rit_perc(sc, O_HEART, "SUB KICK", 50);
    sc->p[O_HEART][P_DECAY] = 60; sc->p[O_HEART][P_LEVEL] = 92;
    rit_steps(sc, O_HEART, "x.......x.....x.", 60, 110, 96);
    sc->p[O_BONE][P_BODY] = 84; sc->p[O_BONE][P_DECAY] = 72; sc->p[O_BONE][P_LEVEL] = 62; sc->p[O_BONE][P_PAN] = 62;
    sc->p[O_BONE][P_TEXTURE] = 25;              /* (fewer clicks in its fracture: a dull break) */
    rit_steps(sc, O_BONE, "....x.......x...", 55, 92, 96);
    rit_core(sc, O_TEETH, SL);
    rit_steps(sc, O_TEETH, "x.xx.x.xx.x.x.xx", 60, 84, 72);
    rit_core(sc, O_CLAW, MW);                   /* a fry of electricity in the middle, held (not air: D24) */
    sc->tr[O_CLAW].len = 64;
    seq_put(sc, O_CLAW, 0, 64 * 96 - 6, 60, 90, 0);
    /* the melody: the register picks the bell's notes (WAIL on SPASM) */
    rit_model(sc, O_SPASM, L_BELL, 30, 40, 20, 78);
    sc->p[O_SPASM][P_PAN] = 64;
    sc->tr[O_SPASM].len = 32;
    rit_steps(sc, O_SPASM, "x..x...x..x.....x..x......x.x...", 62, 92, 48);
    sc->tur.wail = 1; sc->tur.scale = 50; sc->tur.loop = 92; sc->tur.depth = 15;
    /* the mix */
    sc->mix.lvl[CH_DRONE] = 78; sc->mix.lvl[CH_VOICE] = 58; sc->mix.lvl[CH_NOISE] = 70; sc->mix.lvl[CH_DRUM] = 86;
    sc->mix.lvl[CH_SPASM] = 62; sc->mix.lvl[CH_BASS] = 0;
    sc->mix.sa[CH_SPASM] = 60; sc->mix.sb[CH_SPASM] = 50; sc->mix.sb[CH_DRUM] = 30; sc->mix.sa[CH_DRUM] = 0;
    sc->mix.sb[CH_DRONE] = 30; sc->mix.sb[CH_VOICE] = 50; sc->mix.sb[CH_NOISE] = 30; sc->mix.sa[CH_VOICE] = 10;
    sc->mix.pump = 30; sc->mix.rumble = 45; sc->mix.dark = 0; sc->mix.master = 96;
    sc->fx.d_time = 6; sc->fx.d_fb = 50; sc->fx.d_tone = 25; sc->fx.d_mix = 30; sc->fx.d_rev = 25; sc->fx.d_ping = 1;
    sc->fx.r_space = 2; sc->fx.r_mix = 46; sc->fx.r_decay = 65; sc->fx.r_size = 85; sc->fx.r_damp = 25; sc->fx.c_mix = 0;
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
static const rit_fn BIO_RITUAL[] = {rit_awaken, rit_rust, rit_abyss, rit_ash, rit_frozen, rit_stream, rit_wall, rit_data,
                                    rit_embers, rit_foundry, rit_orbit, rit_innards, rit_static, rit_splice, rit_void,
                                    rit_infection, rit_empty};
static const char *const BIO_RITUAL_NAME[] = {"AWAKENING", "RUST", "ABYSS", "ASH", "FROZEN", "STREAM", "WALL", "DATA",
                                              "EMBERS", "FOUNDRY", "ORBIT", "INNARDS", "STATIC CULT", "SPLICE", "VOID",
                                              "INFECTION", "EMPTY BODY"};
#define BIO_NRITUAL (sizeof BIO_RITUAL / sizeof BIO_RITUAL[0])
#endif
