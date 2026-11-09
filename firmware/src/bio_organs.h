/* SPDX-License-Identifier: GPL-3.0-only */
/* BIOFORGE's nine organs. The drums (HEART, BONE, TEETH, CLAW) are TISSUE: an excitation (PRESSURE, FRACTURE,
 * FRICTION, VENT) through damped modes, the flesh curve, a DC block. Each drum organ is one body that keeps ringing:
 * a hit excites it again, nothing is stolen (DECISIONS.md D2). The tonal organs (BASS, DRONE, SPASM, VOICE, NOISE) are
 * monophonic. Every hit's noise comes from its seed: the same seed, the same sound.
 *
 * The numbers in BIO_DRUM_DEF are the idea document's (5.3): frequencies in Hz, times in ms (decays: to -40 dB),
 * levels in % of the excitation. Parameters are 0..100 (tune 0..48, 24 = in tune; pan 0..100, 50 = centre). */
#ifndef BIO_ORGANS_H
#define BIO_ORGANS_H
#include "bio_dsp.h"

enum { O_HEART, O_BONE, O_TEETH, O_CLAW, O_BASS, O_DRONE, O_SPASM, O_VOICE, O_NOISE, BIO_NORG };
enum { CH_DRUM, CH_BASS, CH_DRONE, CH_SPASM, CH_VOICE, CH_NOISE, BIO_NCH };
static const uint8_t BIO_ORG_CH[BIO_NORG] = {CH_DRUM, CH_DRUM, CH_DRUM, CH_DRUM, CH_BASS, CH_DRONE, CH_SPASM, CH_VOICE, CH_NOISE};

/* the parameters of an organ */
enum {
    P_BODY, P_TEXTURE, P_MOTION, P_DECAY,       /* the four macros */
    P_D1, P_D2, P_D3, P_D4, P_D5, P_D6, P_D7, P_D8,   /* its own depth (SYN 2) */
    P_A, P_D, P_S, P_R,                         /* its envelope (tonal); drums: P_A = the gesture's attack */
    P_TUNE, P_LEVEL, P_PAN, P_SPARE, BIO_NP
};
#define BIO_NOLOCK 0xFFu

/* ---- the drum organs' material */
typedef struct {
    uint16_t p_f0, p_f1, p_tau, p_tail, p_on10; /* PRESSURE: the pitch body's sweep (Hz), its time constant, its tail, the
                                                   impulse's onset (0.1 ms) */
    uint8_t p_lvl, p_sub;                       /* its level (%), the share of it that is the pitch body (%) */
    uint8_t fr_n0, fr_n1;                       /* FRACTURE: micro-impulses, fewest and most */
    uint16_t fr_t0, fr_t1;                      /* their window (0.1 ms) */
    uint8_t fr_lvl, fr_slap10;                  /* level (%), the first slap's length (0.1 ms; 0 = none) */
    uint8_t fr_even;                            /* 1: evenly spaced (a rotation), 0: scattered */
    uint8_t fr_am;                              /* % of the friction cut up by the clicks (BONE, CLAW) */
    uint16_t fx_lo, fx_hi;                      /* FRICTION: its band (Hz) */
    uint16_t fx_peak;                           /* CLAW: the band's middle on the drag goes to this (Hz), 0 = still */
    uint16_t fx_att, fx_tail;                   /* ms */
    uint8_t fx_lvl, fx_ring;                    /* level (%), ring modulation by the body (%) */
    uint16_t v_f, v_at, v_len;                  /* VENT: the air's corner (Hz), when (ms), for how long (ms) */
    uint8_t v_lvl;
    uint8_t nm;                                 /* modes */
    uint16_t m_f[4], m_dec[4];
    uint8_t m_g[4];                             /* their share (%) */
    int8_t bend10;                              /* their frequency at the hit, % x10 above (BONE: the bounce) */
    uint8_t bend_ms;
    uint8_t drive10;                            /* FLESH: dB x10 */
    uint8_t dry_fric;                           /* % of the friction straight out */
    uint16_t g_att, g_drag, g_rel;              /* the gesture (CLAW): ms; 0 = a strike */
    uint8_t init[4];                            /* BODY TEXTURE MOTION DECAY at INIT (the numbers above are there) */
} bio_drum_def_t;

static const bio_drum_def_t BIO_DRUM_DEF[4] = {
    /* HEART: a heavy hydraulic piston through a wet body, a short cracking valve at the end */
    {142, 46, 39, 390, 20, 52, 70,   2, 2, 110, 250, 12, 12, 0, 0,   85, 470, 0, 2, 55, 17, 0,
     3000, 17, 14, 7,   3, {93, 171, 286, 0}, {170, 110, 64, 0}, {60, 28, 12, 0}, 0, 0,   25, 6,   0, 0, 0,
     {76, 48, 18, 46}},
    /* BONE: brittle fracture in a stretched skeleton, a resonance that bends after the blow */
    {260, 180, 8, 70, 8, 22, 15,   4, 7, 20, 330, 34, 0, 0, 55,   850, 3600, 0, 1, 70, 30, 0,
     5200, 2, 30, 7,   4, {192, 293, 671, 1510}, {210, 124, 82, 46}, {34, 28, 22, 16}, 20, 30,   18, 8,   0, 0, 0,
     {48, 74, 26, 42}},
    /* TEETH: small hard teeth biting into each other, a metal grinder with a valve */
    {1800, 900, 2, 12, 3, 8, 0,   3, 8, 150, 750, 40, 0, 1, 20,   2200, 6500, 0, 1, 30, 18, 30,
     4200, 1, 40, 22,  4, {760, 1078, 1624, 2670}, {34, 55, 24, 90}, {30, 26, 22, 22}, 0, 0,   20, 12,   0, 0, 0,
     {31, 69, 39, 29}},
    /* CLAW: a claw dragged through ribs, now and then a hollow rattling after-blow */
    {180, 120, 10, 60, 40, 12, 30,   2, 6, 0, 0, 15, 0, 0, 60,   820, 2900, 2900, 4, 18, 55, 0,
     6000, 4, 60, 6,   3, {215, 348, 580, 0}, {180, 125, 300, 0}, {36, 30, 34, 0}, 0, 0,   22, 20,   8, 120, 60,
     {61, 82, 52, 58}},
};

/* the default parameters of every organ (the four macros, the depth at 50 = the document's INIT, the envelope, tune,
   level, pan) */
static const uint8_t BIO_P_INIT[BIO_NORG][BIO_NP] = {
    {76, 48, 18, 46,  50, 50, 50, 50, 50, 50, 50, 50,  30, 0, 0, 0,  24, 90, 50, 0},   /* HEART */
    {48, 74, 26, 42,  50, 50, 50, 50, 50, 50, 50, 50,  30, 0, 0, 0,  24, 86, 44, 0},   /* BONE */
    {31, 69, 39, 29,  50, 50, 50, 50, 50, 50, 50, 50,  30, 0, 0, 0,  24, 96, 60, 0},   /* TEETH */
    {61, 82, 52, 58,  50, 50, 50, 50, 50, 50, 50, 50,  50, 0, 0, 0,  24, 80, 38, 0},   /* CLAW */
    {50, 50, 50, 50,  50, 50, 50, 50, 50, 50, 50, 50,  12, 50, 62, 40,  24, 86, 50, 0},   /* BASS */
    {50, 50, 50, 50,  50, 50, 50, 50, 50, 50, 50, 50,  70, 50, 100, 75,  24, 84, 50, 0},  /* DRONE */
    {50, 50, 50, 50,  50, 50, 50, 50, 50, 50, 50, 50,  0, 50, 0, 30,  24, 76, 58, 0},   /* SPASM */
    {50, 50, 50, 50,  50, 50, 50, 50, 50, 50, 50, 50,  60, 50, 80, 65,  24, 80, 46, 0},   /* VOICE */
    {50, 50, 50, 50,  50, 50, 50, 50, 50, 50, 50, 50,  70, 50, 100, 70,  24, 72, 50, 0},  /* NOISE */
};

/* LEVEL 0..100 as a fader: the square (80: -3.9 dB, 50: -12 dB), Q15 */
static inline int32_t bio_level(uint32_t v) { if (v > 100u) v = 100u; return (int32_t)(v * v * 32767u / 10000u); }
/* (v - init) in octaves at k units an octave, Q16: how a macro scales a time or a level from its INIT */
static inline int32_t bio_oct(int32_t v, int32_t init, int32_t k) { return (v - init) * (65536 / k); }   /* (so no constant
                                                   folds into the chip ROM's address range: docs/RULES.md) */
/* a 0..100 parameter on a log scale: lo at 0, mid at 50, hi at 100 (Hz) -> lf */
static int32_t bio_map_lf(uint32_t v, uint32_t lo, uint32_t mid, uint32_t hi)
{
    int32_t a = bio_lf_hz(lo), m = bio_lf_hz(mid), b = bio_lf_hz(hi);
    if (v > 100u) v = 100u;
    return v <= 50u ? a + (m - a) * (int32_t)v / 50 : m + (b - m) * ((int32_t)v - 50) / 50;
}
/* a 0..100 parameter on a log scale of times (ms), lo at 0, mid at 50, hi at 100 */
static uint32_t bio_map_ms(uint32_t v, uint32_t lo, uint32_t mid, uint32_t hi)
{
    int32_t lf = bio_map_lf(v, lo, mid, hi);
    return (uint32_t)(bio_exp2(lf - (14 << 16)) >> 2);   /* 2^lf, via a Q16 that stays in range: (2^(lf-14) Q16) / 4 */
}
static inline uint32_t bio_lin(uint32_t v, uint32_t lo, uint32_t hi) { if (v > 100u) v = 100u; return lo + (hi - lo) * v / 100u; }

/* ---- a drum organ: one body */
#define BIO_NCLK 10
typedef struct {
    uint32_t t, rnd, awake, exc;               /* exc: the excitation still going (else only the body rings) */
    int32_t amp;                                /* the strike's level, Q15 */
    /* PRESSURE */
    uint32_t p_on;
    int32_t p_imp, p_dec, p_inc;                /* the impulse after its onset, its decay (Q30), its rise */
    int32_t p_peak;
    uint32_t sub_ph, sub_inc, sub_end;
    int32_t sub_k, sub_env, sub_dec;            /* the sweep's factor, the body's level and decay (Q30) */
    /* FRACTURE */
    uint32_t clk_t[BIO_NCLK];
    int32_t clk_a[BIO_NCLK];
    uint32_t nclk, iclk;
    int32_t brst, brst_dec, brst_lvl, slap_dec;
    /* FRICTION */
    bio_svf_t f_svf;
    bio_op_t f_pk;                              /* the pink-ish colour */
    int32_t f_env, f_lvl, f_am, f_ring;
    uint32_t f_att, f_tail, f_hold;             /* samples: attack, tail (e-fold), the drag held (CLAW) */
    int32_t f_lf0, f_lf1, f_lf2, f_k;           /* the band's lf: start, the drag's middle, the end; its k */
    int32_t f_dec, f_inc;
    /* VENT */
    bio_op_t v_hp;
    uint32_t v_at, v_len;
    int32_t v_env, v_lvl, v_dec;
    /* the modes */
    bio_mode_t m[4];
    int32_t m_g[4], m_lf[4], gimp, gnz;
    uint32_t m_tau[4], nm;
    int32_t bend, bend_k;                       /* lf above, and its fall a sub-block */
    int32_t mot, mot_ph;                        /* the drift (MOTION) */
    /* out */
    int32_t drive, dry, last;
    bio_dc_t dc;
    int32_t fade;                               /* the release to silence (PANIC), Q15; 32767 = open */
    int32_t press_out;                          /* HEART: its pressure envelope (the life signal), Q15 */
    int32_t peak;
} bio_drum_t;

/* a hit: note = semitones from the organ's own pitch (Q8), vel 1..127, len = the gesture (samples), p = the
   parameters (locks applied), seed = this hit's */
static void bio_drum_hit(bio_drum_t *d, uint32_t org, int32_t note_q8, uint32_t vel, uint32_t len, const uint8_t *p,
                         uint32_t seed, int32_t rot)
{
    const bio_drum_def_t *df = &BIO_DRUM_DEF[org];
    int32_t body = p[P_BODY], tex = p[P_TEXTURE] + rot, mot = p[P_MOTION], dec = p[P_DECAY];
    int32_t o_dec = bio_oct(dec, df->init[3], 25) + bio_oct(p[P_D5], 50, 100);   /* DECAY: x2 each 25; RESO a little */
    int32_t o_tex = bio_oct(bio_clamp(tex, 0, 100), df->init[1], 60);
    int32_t o_body = bio_oct(body, df->init[0], 70);
    int32_t pitch = ((int32_t)p[P_TUNE] - 24) * BIO_SEMI + (int32_t)(((int64_t)note_q8 * BIO_SEMI) >> 8);
    int32_t mass = -(body - df->init[0]) * BIO_SEMI / 40;   /* more BODY: heavier, a little lower */
    int32_t hard = ((int32_t)p[P_D6] - 50);     /* HARD (up) / WET (down): brighter modes, or duller and longer */
    int32_t velq = (int32_t)(vel > 127u ? 127u : vel) * 258;   /* Q15 */
    uint32_t i, n, s = seed;
    int32_t lvl;

    d->t = 0;
    d->rnd = bio_hash(seed, org + 1u);
    d->awake = 1;
    d->exc = 1;
    d->fade = 32767;
    d->amp = bio_mul15(velq, bio_level(p[P_LEVEL]));   /* LEVEL: a fader's law */
    d->drive = (int32_t)(bio_exp2((int32_t)df->drive10 * 1089 + bio_oct(p[P_D7], 50, 25)) >> 4);   /* Q12 */

    /* PRESSURE: the impulse into the modes and the pitch-swept body (the sub) */
    lvl = (int32_t)df->p_lvl * 328 * (int32_t)(bio_exp2(bio_oct(p[P_D1], 50, 35) - o_tex / 3) >> 8) >> 8;   /* Q15 */
    d->p_on = BIO_MS(df->p_on10) / 10u + 1u;
    d->p_peak = bio_mul15(lvl, 32767 - (int32_t)df->p_sub * 327) * 2;
    d->p_imp = 0;
    d->p_inc = d->p_peak / (int32_t)d->p_on;
    d->p_dec = bio_decay(1, bio_tau(df->p_on10 / 10u + 2u));
    d->sub_ph = 0x40000000u;                    /* from its peak: the piston's push */
    d->sub_inc = bio_inc(bio_lf_hz(df->p_f0) + pitch + mass);
    d->sub_end = bio_inc(bio_lf_hz(df->p_f1) + pitch + mass);
    d->sub_k = bio_decay(1, bio_tau(df->p_tau));
    d->sub_env = (bio_mul15(lvl, (int32_t)df->p_sub * 327) * (int32_t)(bio_exp2(o_body / 2) >> 10) >> 6) << 15;   /* Q30 */
    d->sub_env = bio_clamp(d->sub_env, 0, 1 << 30);
    d->sub_dec = bio_decay(1, bio_scale(bio_tau40(df->p_tail), o_dec));

    /* FRACTURE: the clicks, seeded */
    n = df->fr_n0 + (uint32_t)(((uint32_t)(df->fr_n1 - df->fr_n0) * (uint32_t)bio_clamp(tex, 0, 100) + 50u) / 100u);
    if (org == O_TEETH) {                       /* closed (the lower keys): a short run; open (the upper): a long one */
        int32_t open = note_q8 > 0 ? 1 : 0;
        n = open ? df->fr_n1 : df->fr_n0 + 1u;
    }
    if (n > BIO_NCLK - 1u) n = BIO_NCLK - 1u;
    d->nclk = 0;
    if (df->fr_slap10) {                        /* HEART: the slap at the attack */
        d->clk_t[d->nclk] = 0;
        d->clk_a[d->nclk++] = 32767;
    }
    {
        uint32_t t0 = BIO_MS(df->fr_t0) / 10u, t1 = BIO_MS(df->fr_t1) / 10u;
        if (org == O_TEETH && note_q8 > 0)      /* open: the teeth run on */
            t1 = BIO_MS(df->fr_t1) / 10u * 3u / 2u;
        if (org == O_TEETH && note_q8 <= 0)
            t1 = t0 + (t1 - t0) / 3u;
        for (i = 0; i < n && d->nclk < BIO_NCLK; i++) {
            uint32_t at;
            if (df->fr_even)                    /* a rotation: even, each a little off (±6 %) */
                at = t0 + (t1 - t0) * i / (n ? n : 1u) + bio_rndr(&s, (t1 - t0) / (n ? n * 8u : 8u) + 1u);
            else
                at = t0 + bio_rndr(&s, t1 - t0 + 1u);
            d->clk_t[d->nclk] = at;
            d->clk_a[d->nclk++] = 9000 + (int32_t)bio_rndr(&s, 23000u);
        }
        for (i = 1; i < d->nclk; i++) {         /* in time order */
            uint32_t j = i, tt = d->clk_t[i];
            int32_t aa = d->clk_a[i];
            while (j > 0 && d->clk_t[j - 1] > tt) { d->clk_t[j] = d->clk_t[j - 1]; d->clk_a[j] = d->clk_a[j - 1]; j--; }
            d->clk_t[j] = tt;
            d->clk_a[j] = aa;
        }
    }
    d->iclk = 0;
    d->brst = 0;
    d->brst_lvl = (int32_t)df->fr_lvl * 328 * (int32_t)(bio_exp2(o_tex + bio_oct(p[P_D2], 50, 30)) >> 8) >> 8;
    d->brst_lvl = bio_mul15(d->brst_lvl, 16384 + velq / 2);
    d->brst_dec = bio_decay(1, BIO_MS(5) / 10u + 4u);   /* a click: 0.5 ms */
    d->slap_dec = bio_decay(1, BIO_MS(df->fr_slap10) / 30u + 4u);

    /* FRICTION */
    d->f_lvl = (int32_t)df->fx_lvl * 328 * (int32_t)(bio_exp2(o_tex + bio_oct(p[P_D3], 50, 30)) >> 8) >> 8;
    d->f_am = (int32_t)df->fr_am * 327;
    d->f_ring = (int32_t)df->fx_ring * 327;
    d->f_lf0 = (bio_lf_hz(df->fx_lo) + bio_lf_hz(df->fx_hi)) / 2 + pitch / 2;
    d->f_lf1 = df->fx_peak ? bio_lf_hz(df->fx_peak) + pitch / 2 : d->f_lf0;
    d->f_lf2 = df->fx_peak ? bio_lf_hz(400) + pitch / 2 : d->f_lf0;
    {
        int32_t bw = bio_lf_hz(df->fx_hi) - bio_lf_hz(df->fx_lo);   /* the band's width: Q = centre / width */
        uint32_t q10 = (uint32_t)(10 * 65536 / (bw > 16384 ? bw : 16384));
        d->f_k = (int32_t)bio_q_k(q10 < 4u ? 4u : q10);
    }
    if (df->g_drag) {                           /* CLAW: the gesture, its drag as long as the note (45..300 ms) */
        uint32_t lo = BIO_MS(45), hi = BIO_MS(300);
        d->f_att = BIO_MS(bio_lin(p[P_A], 4, 14));
        d->f_hold = len < lo ? lo : len > hi ? hi : len;
        d->f_tail = bio_scale(bio_tau(df->g_rel / 3u), o_dec);
    } else {
        d->f_att = BIO_MS(df->fx_att) + 1u;
        d->f_hold = 0;
        d->f_tail = bio_scale(bio_tau40(df->fx_tail), o_dec);
    }
    d->f_dec = bio_decay(1, d->f_tail);
    d->f_env = 0;
    d->f_inc = (int32_t)((1u << 30) / (d->f_att ? d->f_att : 1u));
    bio_svf_set(&d->f_svf, d->f_lf0, (uint32_t)d->f_k);
    bio_op_set(&d->f_pk, bio_lf_hz(df->fx_hi));

    /* VENT: air, at its moment */
    d->v_at = BIO_MS(df->v_at);
    d->v_len = BIO_MS(df->v_len) + 1u;
    d->v_lvl = (int32_t)df->v_lvl * 328 * (int32_t)(bio_exp2(o_tex + bio_oct(p[P_D4], 50, 30)) >> 8) >> 8;
    d->v_lvl = bio_mul15(d->v_lvl, 16384 + velq / 2);
    d->v_env = 0;
    d->v_dec = bio_decay(1, d->v_len / 3u + 4u);
    bio_op_set(&d->v_hp, bio_lf_hz(df->v_f) + pitch / 3);

    /* the modes: their frequencies (the bend on top), their decays, their shares */
    d->nm = df->nm;
    d->gimp = 1;
    for (i = 0; i < df->nm; i++) {
        int32_t lf = bio_lf_hz(df->m_f[i]) + pitch + mass + hard * (int32_t)i * 40;
        uint32_t tau = bio_scale(bio_tau40(df->m_dec[i]), o_dec + bio_oct(body, df->init[0], 200) -
                                 hard * (int32_t)(i + 1u) * 300);
        d->m_lf[i] = lf;
        d->m_tau[i] = tau;
        bio_mode_set(&d->m[i], lf, tau);
        d->m_g[i] = (int32_t)df->m_g[i] * 328 * (int32_t)(bio_exp2(o_body / 2 + bio_oct(p[P_D5], 50, 60)) >> 8) >> 8;
    }
    d->bend = (int32_t)df->bend10 * 94;         /* % x10 -> lf: 0.1 % is 94 */
    d->bend_k = df->bend_ms ? bio_decay(BIO_SB, bio_tau(df->bend_ms / 2u + 1u)) : 0;
    d->mot = mot;
    d->dry = (int32_t)df->dry_fric * 327;
}

/* the drum organ's control, once a sub-block: the bend, the drift, the friction's band on the gesture */
static void bio_drum_ctl(bio_drum_t *d, uint32_t org, int32_t drift, int32_t rot_lf)
{
    uint32_t i;
    int32_t g;
    if (!d->awake) return;
    if (d->bend) d->bend = bio_mul30(d->bend, d->bend_k);
    for (i = 0; i < d->nm; i++) {               /* MOTION: the modes drift apart a little while they ring */
        int32_t lf = d->m_lf[i] + d->bend + (int32_t)(((int64_t)drift * d->mot * (int32_t)(i + 1u)) >> 15);
        if (org == O_TEETH) lf += rot_lf * ((int32_t)i - 1);   /* NERVE -> METAL: the teeth's spread */
        d->m[i].f = bio_lftab(BIO_MC, lf);
    }
    if (BIO_DRUM_DEF[org].g_drag) {             /* CLAW: the band follows the gesture, 820 -> 2900 -> 400 Hz */
        int32_t t = (int32_t)(d->t >> 4), half = (int32_t)((d->f_att + d->f_hold) >> 5);
        if (half < 1) half = 1;
        if (t < half) g = d->f_lf0 + (d->f_lf1 - d->f_lf0) / 64 * (t * 64 / half);
        else if (t < 2 * half) g = d->f_lf1 + (d->f_lf2 - d->f_lf1) / 64 * ((t - half) * 64 / half);
        else g = d->f_lf2;
        bio_svf_set(&d->f_svf, g + rot_lf * 4, (uint32_t)d->f_k);   /* NERVE -> SCRAPE */
    }
}

/* n samples of the drum organ, added to out[] (mono, Q15), its excitation added to rib[] */
static void bio_drum_render(bio_drum_t *d, uint32_t org, int32_t *out, int32_t *rib, uint32_t n)
{
    uint32_t k;
    int32_t pk = d->peak;
    if (!d->awake) return;
    if (!d->exc) {                              /* the excitation is over: only the body (and the pitch body) rings */
        uint32_t i, subon = d->sub_env > (1 << 16);
        for (k = 0; k < n; k++) {
            int32_t body = 0, y;
            for (i = 0; i < d->nm; i++) body += bio_mode_tick(&d->m[i], 0) >> 8;
            if (subon) {
                d->sub_inc = d->sub_end + (uint32_t)bio_mul30((int32_t)(d->sub_inc - d->sub_end), d->sub_k);
                d->sub_ph += d->sub_inc;
                body += bio_mul15(bio_sin(d->sub_ph), d->sub_env >> 15);
                d->sub_env = bio_mul30z(d->sub_env, d->sub_dec);
            }
            y = bio_dc(&d->dc, bio_flesh(body, d->drive));
            y = bio_mul15(y, bio_mul15(d->amp, d->fade));
            if (d->fade < 32767) d->fade = d->fade > 600 ? d->fade - 600 : 0;
            out[k] += y;
            if (bio_abs(y) > pk) pk = bio_abs(y);
        }
        d->t += n;
        d->peak = pk;
        d->last = 0;
        d->press_out = bio_clamp(d->sub_env >> 15, 0, 32767);
        goto sleep;
    }
    for (k = 0; k < n; k++) {
        uint32_t t = d->t;
        int32_t w = bio_noise(&d->rnd), ex, fr, crack, vent, sub, body = 0, y;
        uint32_t i;
        /* PRESSURE */
        if (t < d->p_on) d->p_imp += d->p_inc;
        else d->p_imp = bio_mul30z(d->p_imp, d->p_dec);
        d->sub_inc = d->sub_end + (uint32_t)bio_mul30((int32_t)(d->sub_inc - d->sub_end), d->sub_k);
        d->sub_ph += d->sub_inc;
        sub = bio_mul15(bio_sin(d->sub_ph), d->sub_env >> 15);
        d->sub_env = bio_mul30z(d->sub_env, d->sub_dec);
        /* FRACTURE */
        while (d->iclk < d->nclk && d->clk_t[d->iclk] <= t) {
            int32_t a = bio_mul15(d->clk_a[d->iclk], d->brst_lvl);
            if (a > d->brst) d->brst = a;
            d->iclk++;
        }
        crack = bio_mul15(w, d->brst);
        d->brst = bio_mul30z(d->brst, t < 64u && BIO_DRUM_DEF[org].fr_slap10 ? d->slap_dec : d->brst_dec);
        /* FRICTION: the envelope (a strike's attack and tail, or the claw's drag), pink-ish noise, the band */
        if (t < d->f_att) d->f_env += d->f_inc;
        else if (t < d->f_att + d->f_hold) d->f_env = 1 << 30;
        else d->f_env = bio_mul30z(d->f_env, d->f_dec);
        {
            int32_t pink = bio_op_lp(&d->f_pk, w), e = d->f_env >> 15;
            if (d->f_am)                        /* cut up by the clicks: the fracture inside the friction */
                e = bio_mul15(e, 32767 - d->f_am + bio_clamp(d->brst * 3, 0, d->f_am * 2));
            fr = bio_mul15(bio_svf_bp(&d->f_svf, bio_mul15(pink, e)), d->f_lvl) * 2;
            if (d->f_ring)                      /* TEETH: the friction rung against the body */
                fr += bio_mul15(bio_mul15(fr, d->last), d->f_ring) * 4;
        }
        /* VENT */
        if (t == d->v_at) d->v_env = d->v_lvl << 15;
        else if (t > d->v_at + d->v_len) d->v_env = bio_mul30z(d->v_env, d->v_dec);
        vent = bio_mul15(bio_op_hp(&d->v_hp, w), d->v_env >> 15);
        /* the excitation, the modes */
        ex = d->p_imp + crack + fr + vent;
        for (i = 0; i < d->nm; i++)
            body += bio_mode_tick(&d->m[i], bio_mul15(ex, d->m_g[i]) * 256) >> 8;
        rib[k] += ex >> 3;
        d->last = bio_sat(body);
        body += sub + bio_mul15(fr, d->dry) + (crack >> 2);
        y = bio_dc(&d->dc, bio_flesh(body, d->drive));
        y = bio_mul15(y, bio_mul15(d->amp, d->fade));
        if (d->fade < 32767) d->fade = d->fade > 600 ? d->fade - 600 : 0;   /* PANIC: 2 ms to silence */
        out[k] += y;
        if (bio_abs(y) > pk) pk = bio_abs(y);
        d->t = t + 1u;
    }
    d->peak = pk;
    d->press_out = bio_clamp((d->sub_env >> 15) + (d->p_imp >> 1), 0, 32767);
    if (d->t > d->v_at + d->v_len && d->iclk >= d->nclk && d->f_env < (1 << 19) && d->p_imp < 16 && d->brst < 16 &&
        d->v_env < (1 << 19))
        d->exc = 0;                             /* (from here the fast path: the friction under -66 dB is let go) */
sleep:
    /* asleep when it has rung out (the body and the pitch body under ~-72 dB), or faded out (PANIC) */
    if (!d->fade || (!d->exc && d->sub_env < (1 << 16))) {
        uint32_t i, quiet = 1;
        for (i = 0; i < d->nm; i++)
            if (bio_abs(d->m[i].x1) > (8 << 8) || bio_abs(d->m[i].x2) > (8 << 8)) quiet = 0;
        if (quiet || !d->fade) {
            d->awake = 0;
            for (i = 0; i < d->nm; i++) d->m[i].x1 = d->m[i].x2 = 0;
            d->dc.x1 = d->dc.y1 = 0;
        }
    }
}

/* ---- an envelope, once a sub-block (Q30); the attack overshoots to 1.25 and stops at 1 */
typedef struct { int32_t v, ka, kd, kr, sus; uint8_t st; } bio_adsr_t;
static void bio_adsr_times(bio_adsr_t *e, uint32_t a_ms, uint32_t d_ms, uint32_t s_pct, uint32_t r_ms)
{
    e->ka = bio_decay(BIO_SB, bio_tau(a_ms / 2u + 1u));
    e->kd = bio_decay(BIO_SB, bio_tau(d_ms / 3u + 1u));
    e->kr = bio_decay(BIO_SB, bio_tau(r_ms / 4u + 1u));
    e->sus = (int32_t)((s_pct > 100u ? 100u : s_pct) * 10737418u);
}
static int32_t bio_adsr_step(bio_adsr_t *e)
{
    switch (e->st) {
    case 1: e->v = 1342177280 - bio_mul30(1342177280 - e->v, e->ka);
        if (e->v >= (1 << 30)) { e->v = 1 << 30; e->st = 2; }
        break;
    case 2: e->v = e->sus + bio_mul30(e->v - e->sus, e->kd); break;
    case 3: e->v = bio_mul30z(e->v, e->kr); if (e->v < (1 << 12)) { e->v = 0; e->st = 0; } break;
    default: e->v = 0;
    }
    return e->v;
}

/* ---- the tonal organs: one state for all five (each uses its part) */
typedef struct {
    uint32_t gate, note, awake;
    int32_t lf, lf_to, glide_k;
    uint32_t pa, pb, pc, pd;                    /* phases */
    bio_adsr_t env;
    bio_ramp_t amp;
    bio_svf_t f1, f2, f3, f4;
    bio_op_t o1, o2;
    bio_dc_t dc;
    int32_t pm, pm_k, pm_dec, fenv, fenv_k;     /* the PM strike (SPASM), the filter's envelope (BASS) */
    uint32_t rnd, ratchet, rt_n, rt_len, rt_t;  /* SPASM's ratchet: pulses left, their spacing */
    int32_t mouth, vel, fade, grain, gt;
    int32_t peak, last;
} bio_tone_t;

static void bio_tone_on(bio_tone_t *v, uint32_t org, uint32_t note, uint32_t vel, uint32_t len, const uint8_t *p, uint32_t seed)
{
    int32_t lf = bio_lf_note((int32_t)note * 256 + ((int32_t)p[P_TUNE] - 24) * 256);
    int32_t o_dec = bio_oct(p[P_DECAY], 50, 25);
    uint32_t legato = v->gate && v->env.st && v->env.st < 3;
    v->vel = (int32_t)(vel > 127u ? 127u : vel) * 258;
    v->fade = 32767;
    v->awake = 1;
    if (!v->rnd) v->rnd = bio_hash(seed, org);
    if (legato && note == v->note && org != O_SPASM) {   /* the same note going on where it ends: tied, not struck */
        v->gate = 1;
        return;
    }
    v->note = note;
    v->lf_to = lf;
    if (!legato || org != O_BASS || p[P_D4] == 0u)   /* BASS glides legato (GLIDE), the others jump */
        v->lf = lf;
    v->gate = 1;
    switch (org) {
    case O_BASS:
        bio_adsr_times(&v->env, bio_map_ms(p[P_A], 1, 4, 100), bio_scale(bio_map_ms(p[P_D], 30, 210, 2500), o_dec),
                       p[P_S], bio_scale(bio_map_ms(p[P_R], 5, 120, 2000), o_dec));
        v->glide_k = bio_decay(BIO_SB, bio_tau(bio_map_ms(p[P_D4], 1, 28, 300) / 2u + 1u));
        v->fenv = 1 << 30;
        v->fenv_k = bio_decay(BIO_SB, bio_tau(bio_scale(bio_map_ms(p[P_D], 30, 210, 2500), o_dec) / 3u + 1u));
        break;
    case O_DRONE:
        bio_adsr_times(&v->env, bio_scale(bio_map_ms(p[P_A], 50, 1400, 10000), o_dec), 400, 100,
                       bio_scale(bio_map_ms(p[P_R], 50, 2800, 10000), o_dec));
        break;
    case O_SPASM:                               /* a strike that falls: PM 2.4 in 75 ms, the level in 160 ms */
        bio_adsr_times(&v->env, 1, bio_scale(bio_map_ms(p[P_D], 15, 160, 1100), o_dec), 0, 30);
        v->pm = 1 << 30;
        v->pm_k = bio_decay(BIO_SB, bio_tau(bio_scale(bio_map_ms(p[P_D3], 5, 75, 300), o_dec) / 3u + 1u));
        v->pa = v->pb = 0;
        v->rt_n = p[P_D5] <= 50u ? 0u : ((uint32_t)p[P_D5] - 51u) / 17u + 1u;   /* RATCHET: 1 (INIT) .. 4 pulses */
        v->rt_len = (len ? len : BIO_MS(125)) / (v->rt_n + 1u);
        v->rt_t = 0;
        break;
    case O_VOICE:
        bio_adsr_times(&v->env, bio_scale(bio_map_ms(p[P_A], 5, 230, 2000), o_dec), 300, p[P_S],
                       bio_scale(bio_map_ms(p[P_R], 30, 1100, 5000), o_dec));
        break;
    default:                                    /* NOISE */
        bio_adsr_times(&v->env, bio_scale(bio_map_ms(p[P_A], 10, 800, 3000), o_dec), 300, p[P_S],
                       bio_scale(bio_map_ms(p[P_R], 20, 1600, 6000), o_dec));
    }
    v->env.st = 1;
}
static void bio_tone_off(bio_tone_t *v, uint32_t note)
{
    if (v->gate && (note == v->note || note > 127u)) {
        v->gate = 0;
        if (v->env.st) v->env.st = 3;
    }
}

/* the vowels' formants (Hz): U O A E I, the idea document's anchors */
static const uint16_t BIO_VOWEL[5][3] = {{350, 900, 2300}, {500, 1050, 2600}, {800, 1200, 2900}, {500, 1900, 2600}, {300, 2200, 3000}};

/* life signals and modulation that the tonal organs take, once a sub-block */
typedef struct { int32_t heart, breath, drift, cv; } bio_life_t;   /* Q15, Q15 (-1..1), Q15 (-1..1), Q15 (0..1) */

/* n samples of a tonal organ, added to out[] (mono) */
static void bio_tone_render(bio_tone_t *v, uint32_t org, const uint8_t *p, const bio_life_t *life, int32_t *out, uint32_t n)
{
    uint32_t k, inc;
    int32_t e0, e1, x, y, a, pk = v->peak, step;
    if (!v->awake) return;
    if (v->lf != v->lf_to) {                    /* the glide */
        v->lf = v->lf_to + bio_mul30(v->lf - v->lf_to, v->glide_k);
        if (bio_abs(v->lf - v->lf_to) < 8) v->lf = v->lf_to;
    }
    e0 = v->env.v >> 15;
    e1 = bio_adsr_step(&v->env) >> 15;
    a = bio_mul15(bio_level(p[P_LEVEL]), v->vel);
    if (org == O_DRONE)                         /* HEART PRESSURE ducks the drone: up to -1.5 dB */
        a = bio_mul15(a, 32767 - bio_mul15(life->heart, 5200 * (int32_t)p[P_D4] / 50));
    if (org == O_NOISE || org == O_VOICE)       /* BREATH lifts and lowers them */
        a = bio_mul15(a, 26000 + bio_mul15(life->breath, 6700 * (int32_t)p[P_MOTION] / 50));
    step = (e1 - e0) / (int32_t)n;
    switch (org) {
    case O_BASS: {
        /* OSC A sine..triangle (MORPH), phase-modulated by OSC B at 2:1; an octave under; a 2-pole low-pass with its
           envelope; the asymmetric drive. TEXTURE: the PM and the growl; BODY: the sub and the drive */
        int32_t morph = (int32_t)p[P_D5] * 393;   /* 0..100 -> 0..39300 (to the triangle), 50 = 0.60 */
        int32_t pm = (int32_t)p[P_D6] * (int32_t)p[P_D6] * 458 / 100 + ((int32_t)p[P_TEXTURE] - 50) * 120;   /* 0.55 rad at INIT */
        int32_t subl = (int32_t)p[P_D7] * 120 + (int32_t)p[P_BODY] * 100;
        int32_t drv = (int32_t)(bio_exp2(66000 + bio_oct(p[P_D8] + p[P_BODY] / 2, 75, 25)) >> 4);   /* ~4 dB */
        int32_t lfc = bio_map_lf(p[P_D1], 60, 320, 6000), famt = ((int32_t)p[P_D3] + p[P_MOTION] - 50) * 60000 / 50;
        uint32_t q10 = bio_lin(p[P_D2], 5, 50);
        v->fenv = bio_mul30(v->fenv, v->fenv_k);
        bio_svf_set(&v->f1, lfc + (int32_t)(((int64_t)famt * (v->fenv >> 15)) >> 15) + (int32_t)(((int64_t)life->cv * p[P_MOTION]) >> 9),
                    bio_q_k(q10 < 5u ? 5u : q10));
        inc = bio_inc(v->lf);
        if (morph > 32767) morph = 32767;
        for (k = 0; k < n; k++) {
            int32_t b, s, t, o;
            v->pb += inc * 2u;
            b = bio_sin(v->pb);
            v->pa += inc;
            s = bio_sin(v->pa + (uint32_t)(b * pm));
            t = bio_tri(v->pa + (uint32_t)(b * pm));
            o = s + bio_mul15(t - s, morph);
            v->pc += inc / 2u;
            o += bio_mul15(bio_sin(v->pc), subl);
            o += bio_mul15(bio_noise(&v->rnd), (int32_t)p[P_TEXTURE] * 20);   /* the membrane's noise */
            y = bio_svf(&v->f1, o >> 1, &x);
            y = bio_flesh(y, drv);
            e0 += step;
            y = bio_mul15(bio_mul15(y, e0), a);
            out[k] += y;
            if (bio_abs(y) > pk) pk = bio_abs(y);
        }
        break;
    }
    case O_DRONE: {
        /* OSC A triangle/saw (MORPH 1.25), OSC B a sine at 1.012 (the beating), light PM, two resonant bands, a
           low-pass that breathes over BREATH; HEART bends it */
        int32_t morph = (int32_t)p[P_D6] * 110 + (int32_t)p[P_TEXTURE] * 54;   /* the saw's share: 0.25 at INIT */
        uint32_t ratio = 65536u + (uint32_t)p[P_D3] * 1572u / 100u;   /* 1.012 (Q16) at 50: 21 cents of beating */
        int32_t pm = (int32_t)p[P_D7] * 75;     /* 0.18 rad at INIT */
        int32_t lfc = bio_map_lf(p[P_D1], 90, 610, 4500) + bio_mul15(life->breath, 65536 * (int32_t)p[P_MOTION] / 50) -
                      bio_mul15(life->heart, 20000) + bio_mul15(life->drift, 9000);
        int32_t band = (int32_t)p[P_D8] * 200 + (int32_t)p[P_BODY] * 120;
        uint32_t incb;
        bio_svf_set(&v->f1, lfc, bio_q_k(bio_lin(p[P_D2], 5, 80)));
        bio_svf_set(&v->f2, v->lf + 3 * 65536 + 25000, bio_q_k(60));   /* a band at ~10 x the root (D2: 760 Hz) */
        inc = bio_inc(v->lf + bio_mul15(life->drift, 300));
        incb = (uint32_t)(((uint64_t)inc * ratio) >> 16);
        for (k = 0; k < n; k++) {
            int32_t b, tr, sw, o, b2, b3;
            v->pb += incb;
            b = bio_sin(v->pb);
            v->pa += inc;
            tr = bio_tri(v->pa + (uint32_t)(b * pm));
            sw = (int32_t)((v->pa + (uint32_t)(b * pm)) >> 16) - 32768;
            o = tr + bio_mul15(sw - tr, morph);
            o = (o >> 1) + (b >> 1);
            y = bio_svf(&v->f1, o, &x);
            b2 = bio_svf_bp(&v->f2, o);
            b3 = 0;
            y += bio_mul15(b2 + b3, band);
            e0 += step;
            y = bio_mul15(bio_mul15(bio_dc(&v->dc, y), e0), a) * 2;
            out[k] += y;
            if (bio_abs(y) > pk) pk = bio_abs(y);
        }
        break;
    }
    case O_SPASM: {
        /* OSC A sine, OSC B inharmonic (1.414), a strong PM strike that falls fast, a band-pass node; the ratchet */
        uint32_t ratio = p[P_D1] <= 50u ? 65536u + (uint32_t)p[P_D1] * 543u   /* RATIO 1..6, 1.414 at 50 (Q16) */
                                        : 92682u + (uint32_t)(p[P_D1] - 50u) * 6011u;
        int32_t pmpk = (int32_t)p[P_D2] * 1001 + ((int32_t)p[P_TEXTURE] - 50) * 300;   /* 2.4 rad at INIT */
        int32_t lfb = bio_map_lf(p[P_D4], 300, 1800, 6000) + (int32_t)(((int64_t)life->cv * p[P_MOTION]) >> 8);
        uint32_t incb;
        bio_svf_set(&v->f1, lfb, bio_q_k(bio_lin(p[P_D6], 8, 90) + (p[P_BODY] > 50u ? (p[P_BODY] - 50u) : 0u)));
        v->pm = bio_mul30(v->pm, v->pm_k);
        if (v->rt_n && v->gate) {               /* the ratchet: pulses inside the step */
            v->rt_t += n;
            if (v->rt_t >= v->rt_len) {
                v->rt_t = 0;
                v->rt_n--;
                v->env.st = 1;
                v->pm = 1 << 30;
            }
        }
        inc = bio_inc(v->lf + bio_mul15(life->drift, 2000));
        incb = (uint32_t)(((uint64_t)inc * ratio) >> 16);
        {
            int32_t pmv = bio_mul15(pmpk, v->pm >> 15);
            for (k = 0; k < n; k++) {
                int32_t b, s;
                v->pb += incb;
                b = bio_sin(v->pb);
                v->pa += inc;
                s = bio_sin(v->pa + ((uint32_t)(b * (pmv >> 3)) << 3));
                y = bio_svf_bp(&v->f1, s) + (s >> 2);
                e0 += step;
                y = bio_mul15(bio_mul15(y, e0), a) * 2;
                out[k] += y;
                if (bio_abs(y) > pk) pk = bio_abs(y);
            }
        }
        break;
    }
    case O_VOICE: {
        /* a glottal pulse (a band-limited saw, falling) with a subharmonic, breath noise, three formants moving
           between U O A E I (MOUTH); BREATH and NERVE move the mouth a little by themselves */
        int32_t m = ((int32_t)p[P_D1] <= 50 ? (int32_t)p[P_D1] * 143 / 10 : 717 + ((int32_t)p[P_D1] - 50) * 67) +
                    bio_mul15(life->breath, (int32_t)p[P_MOTION] * 3) + bio_mul15(life->drift, (int32_t)p[P_MOTION] * 2);
        int32_t vi, fr, sh = ((int32_t)p[P_D3] - 50) * 1311 - 3 * BIO_SEMI - ((int32_t)p[P_BODY] - 50) * 455;   /* SHIFT ±12, BODY */
        int32_t br = (int32_t)p[P_D2] * 59 + ((int32_t)p[P_TEXTURE] - 50) * 40, sub = (int32_t)p[P_D4] * 90;
        int32_t f[3];
        uint32_t j;
        m = bio_clamp(m, 0, 4 * 1024 - 1);      /* 0.7 of the way from U to O at INIT */
        vi = m >> 10;
        fr = m & 1023;
        for (j = 0; j < 3u; j++) {
            int32_t f0 = bio_lf_hz(BIO_VOWEL[vi][j]), f1 = bio_lf_hz(BIO_VOWEL[vi < 4 ? vi + 1 : 4][j]);
            f[j] = f0 + (int32_t)(((int64_t)(f1 - f0) * fr) >> 10) + sh;
        }
        bio_svf_set(&v->f1, f[0], bio_q_k(70));
        bio_svf_set(&v->f2, f[1], bio_q_k(90));
        bio_svf_set(&v->f3, f[2], bio_q_k(110));
        inc = bio_inc(v->lf + bio_mul15(life->drift, 1200));
        for (k = 0; k < n; k++) {
            int32_t g, o, nz;
            uint32_t ph;
            v->pa += inc;
            ph = v->pa;
            g = 32767 - (int32_t)(ph >> 16);    /* a falling saw: the glottis closing */
            v->pc += inc / 2u;
            o = (g >> 1) + bio_mul15(bio_sin(v->pc), sub);
            nz = bio_mul15(bio_noise(&v->rnd), bio_clamp(br, 0, 16000));
            o += nz;
            y = bio_svf_bp(&v->f1, o) + (bio_svf_bp(&v->f2, o) >> 1) + (bio_svf_bp(&v->f3, o) >> 2);
            e0 += step;
            y = bio_mul15(bio_mul15(bio_dc(&v->dc, y), e0), a) * 2;
            out[k] += y;
            if (bio_abs(y) > pk) pk = bio_abs(y);
        }
        break;
    }
    default: {                                  /* NOISE: breath, scrape, steam */
        uint32_t mode = p[P_D1] < 34u ? 1u : p[P_D1] < 67u ? 0u : 2u;   /* MODE: SCRAPE, BREATH (INIT), STEAM */
        int32_t lfc = bio_map_lf(p[P_D2], 80, 1450, 12000) - ((int32_t)p[P_BODY] - 50) * 600 + (int32_t)(((int64_t)life->cv * p[P_MOTION]) >> 9);
        int32_t rough = (int32_t)p[P_D4] * 30 / 100 + ((int32_t)p[P_TEXTURE] - 50) / 2;   /* 15 % at INIT */
        uint32_t q10 = bio_lin(p[P_D3], 5, 90);
        if (mode == 1u) q10 = q10 * 4u;
        bio_svf_set(&v->f1, lfc, bio_q_k(q10 < 5u ? 5u : q10));
        bio_op_set(&v->o1, bio_lf_hz(3000));
        for (k = 0; k < n; k++) {
            int32_t w = bio_noise(&v->rnd), pnk = bio_op_lp(&v->o1, w), o, bp, lp;
            o = pnk + (w >> 3);
            if (--v->gt <= 0) {                 /* roughness: grains, held 1-4 ms */
                v->gt = 44 + (int32_t)bio_rndr(&v->rnd, 132u);
                v->grain = 32767 - (int32_t)bio_rndr(&v->rnd, (uint32_t)bio_clamp(rough, 0, 100) * 300u + 1u);
            }
            o = bio_mul15(o, v->grain);
            lp = bio_svf(&v->f1, o, &bp);
            y = mode == 2u ? o - (int32_t)(((int64_t)v->f1.k * bp) >> 16) - lp : bp;
            e0 += step;
            y = bio_mul15(bio_mul15(y, e0), a);
            out[k] += y;
            if (bio_abs(y) > pk) pk = bio_abs(y);
        }
    }
    }
    if (v->fade < 32767) {                      /* PANIC: out at once */
        v->env.v = 0;
        v->env.st = 0;
    }
    v->peak = pk;
    if (!v->env.st && !v->gate) v->awake = 0;
}

#endif
