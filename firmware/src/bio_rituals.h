/* SPDX-License-Identifier: GPL-3.0-only */
/* BIOFORGE's factory rituals: whole scenes (patterns, sounds, the nerve, the mix, the effects), each its own world.
 * THE AWAKENING is the idea document's start scene (bio_seq.h); the others are BIOFORGE's own. SHIFT + PRESETS loads
 * one into the scene that plays (undone by not saving: the scene in flash is the one before). */
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
static const rit_fn BIO_RITUAL[] = {rit_awaken, rit_static, rit_splice, rit_void, rit_infection, rit_empty};
static const char *const BIO_RITUAL_NAME[] = {"AWAKENING", "STATIC CULT", "SPLICE", "VOID", "INFECTION", "EMPTY BODY"};
#define BIO_NRITUAL (sizeof BIO_RITUAL / sizeof BIO_RITUAL[0])
#endif
