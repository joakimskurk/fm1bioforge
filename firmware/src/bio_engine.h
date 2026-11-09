/* SPDX-License-Identifier: GPL-3.0-only */
/* BIOFORGE's engine: the nine organs, the shared rib, the six channels, the life signals, BLOOD DELAY, BILE REVERB,
 * MARROW CRUSH and the master. It renders in sub-blocks of 32 frames; before each one it asks its tick (the
 * sequencer) for that sub-block's events. No platform in here: the firmware, the host tools and the tests share it.
 * bio_t is big (the delay and the reverb live in it): the firmware keeps it in the pool. */
#ifndef BIO_ENGINE_H
#define BIO_ENGINE_H
#include "bio_organs.h"

#define BIO_DLY_N 32768u                        /* the delay: 0.74 s a side */
#define BIO_REV_L 3400u                         /* the reverb's four lines (at 22.05 kHz) */
#define BIO_PRE_N 2700u                         /* its pre-delay: 120 ms */
#define BIO_AP1 113u
#define BIO_AP2 337u
#define BIO_RUM_N 512u                          /* the rumble's two lines, at an eighth of the rate (93 ms) */

/* the mix and the effects' settings, 0..100 (written by the main loop) */
typedef struct {
    uint8_t lvl[BIO_NCH], mute[BIO_NCH], sa[BIO_NCH], sb[BIO_NCH], sc[BIO_NCH];
    uint8_t master, rib, pump;                  /* pump: HEART ducks the other channels (a side-chain), 0 .. -18 dB */
    uint8_t rumble;                             /* RUMBLE: HEART into a dark low tail of its own, ducked by it (D22) */
    uint8_t dark;                               /* DARK: the master's low-pass, off at 0, 18 kHz .. 1.2 kHz (D23) */
} bio_mix_t;
typedef struct {
    uint8_t d_time, d_fb, d_tone, d_ping, d_mix, d_rev;   /* BLOOD DELAY: time (a division), feedback, tone, ping-pong, return, to the reverb */
    uint8_t r_size, r_decay, r_damp, r_pre, r_mix;        /* BILE REVERB */
    uint8_t c_drive, c_rate, c_bits, c_fold, c_mix;       /* MARROW CRUSH */
    uint8_t r_space;                                      /* the reverb's SPACE (D21): HALL ROOM CAVE METAL VAST */
} bio_fxp_t;
#define BIO_NSPACE 5u
static const char *const BIO_SPACE_NAME[BIO_NSPACE] = {"HALL", "ROOM", "CAVE", "METAL", "VAST"};

/* the delay's divisions, in 1/96 of a beat (a 16th is 24): 1/32 1/16 3/32 1/8 3/16 1/4 3/8 1/2 3/4 1 bar */
static const uint16_t BIO_DIV[10] = {12, 24, 36, 48, 72, 96, 144, 192, 288, 384};
static const char *const BIO_DIV_NAME[10] = {"1/32", "1/16", "3/32", "1/8", "3/16", "1/4", "3/8", "1/2", "3/4", "1 BAR"};

typedef struct bio_s bio_t;
typedef void (*bio_tick_fn)(bio_t *e, void *ctx);

struct bio_s {
    /* the organs */
    bio_drum_t drum[4];
    bio_tone_t tone[4];                         /* BASS DRONE SPASM VOICE */
    uint8_t p[BIO_NORG][BIO_NP];                /* the parameters (the main loop writes them) */
    uint8_t lk[BIO_NORG][4];                    /* the macros locked by the note that sounds (BIO_NOLOCK: none) */
    bio_mix_t mix;
    bio_fxp_t fx;
    /* the nerve (the Turing's routes, set by the sequencer each sub-block) */
    int32_t nerve_cv;                           /* CV8, smoothed, x the depth, Q15 */
    int32_t nerve_tex;                          /* ROT onto TEXTURE: -50..50 */
    int32_t nerve_scrape, nerve_metal;          /* lf offsets: CLAW's band, TEETH's spread */
    int32_t nerve_air;                          /* HEART's and BONE's vent, Q15 extra (0..) */
    /* life */
    int32_t heart, drift, drift_to;
    uint32_t drift_t, beat_q16, beat_inc;       /* beats (Q16) and a sub-block's worth (the tempo) */
    uint32_t spb_q8;                            /* samples a beat, Q8 (the tempo) */
    uint32_t rnd;
    /* the rib */
    bio_mode_t rib[3];
    int32_t rib_in[BIO_SB];
    /* the delay */
    int16_t dl[BIO_DLY_N], dr[BIO_DLY_N];
    uint32_t dw;
    int32_t dtime, dtime_to;                    /* samples, Q8 */
    bio_op_t dtl, dtr;
    /* the reverb */
    int16_t rv[4][BIO_REV_L], pre[BIO_PRE_N], ap1[BIO_AP1], ap2[BIO_AP2];
    uint32_t rw, pw, a1w, a2w, rlen[4], rpre;
    int32_t rg[4], rhalf, rprev_l, rprev_r, rout_l, rout_r;
    uint32_t rlfo;
    bio_op_t rdamp[4];
    bio_op_t rhp_l, rhp_r;
    bio_op_t rin, rol, ror;                     /* the half rate's guards: in (no aliasing), out (no images) */
    /* the rumble: HEART at an eighth of the rate (5.5 kHz) through two low lines that feed each other, dark */
    int16_t rum[2][BIO_RUM_N];
    uint32_t rum_w;
    int32_t rum_acc, rum_pl, rum_pr, rum_l, rum_r;
    bio_op_t rum_in, rum_d0, rum_d1, rum_ol, rum_or;
    uint32_t freeze;
    /* the crush */
    int32_t c_hold, c_cnt;
    bio_dc_t cdc;
    /* the master */
    bio_dc_t mdc_l, mdc_r;
    int32_t lim_g;
    bio_op_t dk[4];                             /* DARK: two poles a side */
    uint32_t dark_seen;
    int32_t duck, duck_to, duck_rel;            /* PUMP: the cut now and the hit's (Q15), its recovery a sub-block (Q30) */
    uint32_t fx_seen;                           /* the effects' settings last turned into coefficients */
    /* the meters (peak per channel, the last block), the load */
    int32_t meter[BIO_NCH];
    int32_t ob[BIO_NORG][BIO_SB], os[BIO_NORG][BIO_SB];   /* each organ's sub-block, and its side (width) */
    bio_perc_t noise;                           /* the NOISE organ: the perc voice */
    /* the CPU rule (the idea document's 5.10): over budget, the drums not in focus ring on two modes, not four */
    volatile uint32_t shed, focus;
    /* the scope: an organ's own output, every fourth sample (the main loop draws it) */
    volatile uint32_t scope_org, scope_w;
    int16_t scope[256];
    /* the tick */
    bio_tick_fn tick;
    void *tick_ctx;
    uint32_t frames;
};

static const bio_mix_t BIO_MIX_INIT = {{80, 70, 58, 62, 56, 46}, {0, 0, 0, 0, 0, 0}, {12, 8, 0, 18, 10, 0},
                                       {18, 6, 30, 28, 34, 30}, {0, 0, 0, 0, 0, 0}, 75, 50, 0, 0, 0};   /* (no crush: D22) */
static const bio_fxp_t BIO_FX_INIT = {4, 28, 50, 0, 40, 30,   50, 23, 40, 23, 38,   40, 30, 40, 20, 0,   0};

static void bio_fx_coefs(bio_t *e);

static void bio_init(bio_t *e)
{
    uint32_t o, k;
    static const uint16_t RIB_F[3] = {210, 357, 791}, RIB_D[3] = {300, 220, 160};
    for (o = 0; o < BIO_NORG; o++) {
        for (k = 0; k < BIO_NP; k++) e->p[o][k] = BIO_P_INIT[o][k];
        for (k = 0; k < 4u; k++) e->lk[o][k] = BIO_NOLOCK;
    }
    e->mix = BIO_MIX_INIT;
    e->fx = BIO_FX_INIT;
    e->rnd = 0x2545F491u;
    for (k = 0; k < 3u; k++) bio_mode_set(&e->rib[k], bio_lf_hz(RIB_F[k]), bio_tau40(RIB_D[k]));
    for (k = 0; k < 4u; k++) e->tone[k].rnd = bio_hash(0xB10F0u, k);
    e->lim_g = 32767;
    e->beat_inc = 95u;                          /* 120 BPM until the sequencer sets the tempo */
    e->spb_q8 = 22050u << 8;
    e->fx_seen = ~0u;
    e->dark_seen = ~0u;
    e->drift_to = 0;
}

/* a note on: drums strike their body; the tonal organs play it. locks: the step's four macros (NULL: none) */
static void bio_trigger(bio_t *e, uint32_t org, int32_t note, uint32_t vel, uint32_t len, const uint8_t *locks, uint32_t seed)
{
    uint8_t p[BIO_NP];
    uint32_t k;
    if (org >= BIO_NORG) return;
    for (k = 0; k < BIO_NP; k++) p[k] = e->p[org][k];
    for (k = 0; k < 4u; k++) {
        e->lk[org][k] = locks ? locks[k] : BIO_NOLOCK;
        if (e->lk[org][k] != BIO_NOLOCK) p[k] = e->lk[org][k];
    }
    if (org == O_NOISE) {                       /* noise as percussion (its texture moved by the nerve too) */
        bio_perc_p_t pp;
        bio_perc_params(p, &pp);
        pp.texture = (uint8_t)bio_clamp((int32_t)pp.texture + e->nerve_tex, 0, 100);
        bio_perc_hit(&e->noise, &pp, (note - 60) * 256, vel, bio_level(p[P_LEVEL]), len, seed);
        return;
    }
    if (org == O_HEART && e->mix.pump) {         /* PUMP: the kick pushes the rest down; it comes back by the next 8th */
        e->duck_to = bio_mul15((int32_t)(e->mix.pump > 100u ? 100u : e->mix.pump) * 285, (int32_t)(vel > 127u ? 127u : vel) * 258);
        e->duck_rel = bio_decay(BIO_SB, (e->spb_q8 >> 8) / 6u);
    }
    if (org < 4u) {
        int32_t rot = e->nerve_tex;
        bio_drum_hit(&e->drum[org], org, note * 256, vel, len, p, seed, rot);
        if ((org == O_HEART || org == O_BONE) && e->nerve_air)   /* NERVE -> AIR */
            e->drum[org].v_lvl += bio_mul15(e->drum[org].v_lvl + 2000, e->nerve_air);
    } else {
        bio_tone_on(&e->tone[org - 4u], org, (uint32_t)bio_clamp(note, 0, 127), vel, len, p, seed);
    }
}
static void bio_release(bio_t *e, uint32_t org, uint32_t note)
{
    if (org == O_NOISE) bio_perc_release(&e->noise);
    else if (org < 4u) { if (e->drum[org].perc) bio_perc_release(&e->drum[org].np); }
    else if (org < BIO_NORG) bio_tone_off(&e->tone[org - 4u], note);
}
static void bio_panic(bio_t *e)                 /* every organ to silence in a few ms; the patterns are not touched */
{
    uint32_t k;
    for (k = 0; k < 4u; k++) if (e->drum[k].awake) { e->drum[k].fade = 32766; e->drum[k].np.fade = 32766; e->drum[k].kk.fade = 32766; }
    if (e->noise.awake) e->noise.fade = 32766;
    for (k = 0; k < 4u; k++) {
        bio_tone_t *v = &e->tone[k];
        v->gate = 0;
        if (v->env.st) { v->env.st = 3; v->env.kr = bio_decay(BIO_SB, BIO_MS(1)); }
    }
}
static int bio_quiet(const bio_t *e)            /* under -50 dBFS on every channel: a flash erase's gap is not heard */
{
    uint32_t k;
    for (k = 0; k < BIO_NCH; k++) if (e->meter[k] > 100) return 0;
    return 1;
}
static int bio_busy(const bio_t *e)             /* anything sounding at all */
{
    uint32_t k;
    for (k = 0; k < 4u; k++) if (e->drum[k].awake) return 1;
    for (k = 0; k < 4u; k++) if (e->tone[k].awake) return 1;
    return e->noise.awake ? 1 : 0;
}

/* the effects' coefficients from their settings (when they changed, and the delay's time on the tempo) */
static void bio_fx_coefs(bio_t *e)
{
    static const uint16_t RL[4] = {1109, 1361, 1621, 1871};
    /* SPACE: the lines' lengths and the decay (x/256), the damping (lf), more pre-delay. ROOM small and short; CAVE
       big, dark, late; METAL lines so short their echoes are a pitch: a ringing tank; VAST the biggest, long, open */
    static const uint16_t SL[BIO_NSPACE] = {256, 115, 333, 46, 420}, ST[BIO_NSPACE] = {256, 128, 410, 300, 563};
    static const int32_t SD[BIO_NSPACE] = {0, 0, -98304, 131072, 32768};
    static const uint16_t SP[BIO_NSPACE] = {0, 0, 1100, 0, 400};
    const bio_fxp_t *f = &e->fx;
    uint32_t k, t60, sp = f->r_space < BIO_NSPACE ? f->r_space : 0u;
    /* the reverb's decay: 0.25..12 s, 2.8 s at 23 (INIT) */
    t60 = f->r_decay <= 23u ? 250u + (uint32_t)f->r_decay * 2550u / 23u : 2800u + ((uint32_t)f->r_decay - 23u) * 9200u / 77u;
    t60 = t60 * ST[sp] / 256u;
    for (k = 0; k < 4u; k++) {
        uint32_t len = RL[k] * (50u + (uint32_t)f->r_size * 130u / 100u) / 100u * SL[sp] / 256u, num, den, ex;
        if (len > BIO_REV_L - 16u) len = BIO_REV_L - 16u;
        if (len < 64u) len = 64u;
        e->rlen[k] = len;
        num = len * 653135u;                    /* 3 log2(10) in Q16 */
        den = t60 * 2205u / 100u;
        ex = num / (den ? den : 1u);
        e->rg[k] = (int32_t)(bio_exp2(-(int32_t)(ex > (15u << 16) ? 15u << 16 : ex)) >> 1);   /* Q15 */
        bio_op_set(&e->rdamp[k], bio_clamp(bio_map_lf(f->r_damp, 500, 4200, 12000) + 65536 + SD[sp], bio_lf_hz(300), bio_lf_hz(20000)));   /* (half rate: one octave up) */
    }
    e->rpre = (uint32_t)f->r_pre * 2646u / 100u + SP[sp];
    if (e->rpre >= BIO_PRE_N) e->rpre = BIO_PRE_N - 1u;
    bio_op_set(&e->rhp_l, bio_lf_hz(150));
    e->rhp_r = e->rhp_l;
    bio_op_set(&e->rin, bio_lf_hz(7000));
    bio_op_set(&e->rum_in, bio_lf_hz(160 * 8));   /* (filters run at an eighth: their coefficient for 8x) */
    bio_op_set(&e->rum_d0, bio_lf_hz(220 * 8));
    e->rum_d1.g = e->rum_d0.g;
    bio_op_set(&e->rum_ol, bio_lf_hz(260));
    e->rum_or.g = e->rum_ol.g;
    bio_op_set(&e->rol, bio_lf_hz(9000));
    e->ror.g = e->rol.g;
    bio_op_set(&e->dtl, bio_map_lf(f->d_tone, 500, 3400, 12000));
    e->dtr.g = e->dtl.g;
}

/* the delay's time in samples (Q8) from the division and the tempo */
static int32_t bio_dly_time(const bio_t *e)
{
    uint32_t div = BIO_DIV[e->fx.d_time < 10u ? e->fx.d_time : 4u];
    uint32_t s = e->spb_q8 / 96u * div;         /* (the sequencer keeps spb_q8: samples a beat, Q8) */
    if (s > ((BIO_DLY_N - 64u) << 8)) s = (BIO_DLY_N - 64u) << 8;
    return (int32_t)s;
}

static inline int32_t bio_rd(const int16_t *b, uint32_t mask, uint32_t w, int32_t d_q8)   /* a fractional read back */
{
    uint32_t di = (uint32_t)d_q8 >> 8, fr = (uint32_t)d_q8 & 255u;
    int32_t a = b[(w - di) & mask], c = b[(w - di - 1u) & mask];
    return a + (((c - a) * (int32_t)fr) >> 8);
}

/* a mix level 0..100 to Q15 (a fader's law: the square) */
static inline int32_t bio_fader(uint32_t v) { if (v > 100u) v = 100u; return (int32_t)(v * v * 32767u / 10000u); }

static void bio_render(bio_t *e, int32_t *out, uint32_t n)
{
    uint32_t off, o, k;
    int32_t meter[BIO_NCH] = {0, 0, 0, 0, 0, 0};
    uint32_t fxs = (uint32_t)e->fx.r_size | (uint32_t)e->fx.r_decay << 7 | (uint32_t)e->fx.r_damp << 14 |
                   (uint32_t)e->fx.r_pre << 21 | ((uint32_t)e->fx.d_tone << 25 ^ (uint32_t)e->fx.r_space << 29);
    if (fxs != e->fx_seen) {
        e->fx_seen = fxs;
        bio_fx_coefs(e);
    }
    for (off = 0; off + BIO_SB <= n; off += BIO_SB) {
        int32_t dryl[BIO_SB], dryr[BIO_SB], sa[BIO_SB], sb[BIO_SB], sc[BIO_SB];
        int32_t chg[BIO_NCH], ga[BIO_NCH], gb[BIO_NCH], gc[BIO_NCH], peak = 0, tg, gstep, br4;
        bio_life_t life;
        uint8_t pe[BIO_NP];
        if (e->tick) e->tick(e, e->tick_ctx);
        /* life: HEART PRESSURE (fast up, 140 ms down), NERVE DRIFT (a slow random walk), the beat */
        {
            int32_t h = bio_clamp(e->drum[O_HEART].press_out * 2, 0, 32767);
            e->heart = h > e->heart ? h : bio_mul15(e->heart, 32511);   /* (140 ms at 1378 Hz) */
            if (++e->drift_t >= 700u) {         /* a new aim every half second */
                e->drift_t = 0;
                e->drift_to = bio_noise(&e->rnd);
            }
            e->drift += (e->drift_to - e->drift) >> 9;
            e->beat_q16 += e->beat_inc;
        }
        life.heart = e->heart;
        life.drift = e->drift;
        life.cv = e->nerve_cv;
        life.beat = e->beat_q16;
        for (k = 0; k < BIO_SB; k++) e->rib_in[k] = 0;
        for (o = 0; o < BIO_NORG; o++)
            for (k = 0; k < BIO_SB; k++) e->ob[o][k] = e->os[o][k] = 0;
        /* the drums (a noise one's DRIFT: its band over four bars) */
        br4 = bio_sin(e->beat_q16 * (16384u / 4u));
        for (o = 0; o < 4u; o++) {
            uint32_t nm = e->drum[o].nm;
            if (e->drum[o].perc) e->drum[o].np.lfm = bio_mul15(br4, e->drum[o].np.drift);
            bio_drum_ctl(&e->drum[o], o, e->drift, o == O_CLAW ? e->nerve_scrape : o == O_TEETH ? e->nerve_metal : 0);
            if (e->shed && o != e->focus && nm > 2u) e->drum[o].nm = 2;   /* (the transients stay; the ring thins) */
            bio_drum_render(&e->drum[o], o, e->ob[o], e->os[o], e->rib_in, BIO_SB);
            e->drum[o].nm = nm;
        }
        /* the tonal organs: their parameters with the note's locks and the nerve's ROT on TEXTURE */
        if (e->noise.awake) {                   /* NOISE: the perc voice; BREATH (D8) lifts and lowers a long one */
            int32_t br = br4, depth = (int32_t)e->p[O_NOISE][P_D8] * 160, g;   /* (4 bars) */
            e->noise.lfm = bio_mul15(br, e->noise.drift);
            bio_perc_render(&e->noise, e->ob[O_NOISE], e->os[O_NOISE], BIO_SB);
            g = bio_clamp(32767 - bio_mul15(depth, (32767 - br) / 2), 0, 32767);
            if (depth) for (k = 0; k < BIO_SB; k++) { e->ob[O_NOISE][k] = bio_mul15(e->ob[O_NOISE][k], g); e->os[O_NOISE][k] = bio_mul15(e->os[O_NOISE][k], g); }
        }
        for (o = 4u; o < O_NOISE; o++) {
            bio_tone_t *v = &e->tone[o - 4u];
            uint32_t bars;
            if (!v->awake) continue;
            for (k = 0; k < BIO_NP; k++) pe[k] = e->p[o][k];
            for (k = 0; k < 4u; k++) if (e->lk[o][k] != BIO_NOLOCK) pe[k] = e->lk[o][k];
            pe[P_TEXTURE] = (uint8_t)bio_clamp((int32_t)pe[P_TEXTURE] + e->nerve_tex, 0, 100);
            /* BREATH: DRONE's over 1..32 bars (8 at INIT), the others' 4 */
            if (o == O_DRONE) bars = e->p[o][P_D5] <= 50u ? 1u + (uint32_t)e->p[o][P_D5] * 7u / 50u : 8u + ((uint32_t)e->p[o][P_D5] - 50u) * 24u / 50u;
            else bars = 4u;
            life.breath = bio_sin(e->beat_q16 * (16384u / bars));   /* a turn every 4 x bars beats */
            bio_tone_render(v, o, pe, &life, e->ob[o], e->os[o], BIO_SB);
        }
        {                                       /* the scope */
            uint32_t so = e->scope_org < BIO_NORG ? e->scope_org : 0u, w = e->scope_w;
            for (k = 0; k < BIO_SB; k += 4u) e->scope[w++ & 255u] = (int16_t)bio_sat(e->ob[so][k]);
            e->scope_w = w;
        }
        /* the rib: the drums' shared skeleton, at most -20 dB under them */
        {
            int32_t rg = (int32_t)e->mix.rib * 66;   /* 50 -> 3300 (-20 dB) */
            uint32_t in = 0;
            for (k = 0; k < BIO_SB; k++) in |= (uint32_t)(e->rib_in[k] != 0);
            if (!in && bio_abs(e->rib[0].x2) + bio_abs(e->rib[1].x2) + bio_abs(e->rib[2].x2) + bio_abs(e->rib[0].x1) +
                       bio_abs(e->rib[1].x1) + bio_abs(e->rib[2].x1) < (16 << 8)) {
                e->rib[0].x1 = e->rib[0].x2 = e->rib[1].x1 = e->rib[1].x2 = e->rib[2].x1 = e->rib[2].x2 = 0;
                rg = 0;
            }
            if (rg) for (k = 0; k < BIO_SB; k++) {
                int32_t r = 0, x = e->rib_in[k] * 64;
                r += bio_mode_tick(&e->rib[0], x) >> 8;
                r += bio_mode_tick(&e->rib[1], x) >> 8;
                r += bio_mode_tick(&e->rib[2], x) >> 8;
                e->ob[O_HEART][k] += bio_mul15(r, rg);
            }
        }
        /* PUMP: a 2 ms attack, then the recovery */
        if (e->duck_to) {
            e->duck += (e->duck_to - e->duck) / 2;
            if (e->duck_to - e->duck < 400) { e->duck = e->duck_to; e->duck_to = 0; }
        } else if (e->duck) {
            e->duck = bio_mul30z(e->duck, e->duck_rel);
        }
        /* the channels: pan, level, mute, the sends */
        for (o = 0; o < BIO_NCH; o++) {
            int32_t g = e->mix.mute[o] ? 0 : bio_fader(e->mix.lvl[o]);
            if (o != CH_DRUM && e->duck) g = bio_mul15(g, 32767 - e->duck);
            chg[o] = g;
            ga[o] = bio_mul15(g, bio_fader(e->mix.sa[o]));
            gb[o] = bio_mul15(g, bio_fader(e->mix.sb[o]));
            gc[o] = bio_mul15(g, bio_fader(e->mix.sc[o]));
        }
        for (k = 0; k < BIO_SB; k++) dryl[k] = dryr[k] = sa[k] = sb[k] = sc[k] = 0;
        {
            int32_t chm[BIO_SB];                /* a channel's mono sum, for its sends */
            uint32_t ch, used = 0;
            for (ch = 0; ch < BIO_NCH; ch++) {
                uint32_t any = 0;
                for (k = 0; k < BIO_SB; k++) chm[k] = 0;
                for (o = 0; o < BIO_NORG; o++) {
                    uint32_t pan = e->p[o][P_PAN];
                    int32_t pl, pr, pk = 0;
                    if (BIO_ORG_CH[o] != ch || !(o < 4u ? e->drum[o].awake || o == O_HEART : o == O_NOISE ? e->noise.awake : e->tone[o - 4u].awake)) continue;
                    any = 1;
                    pan = pan > 100u ? 100u : pan;
                    pl = bio_m15(BIO_SIN[256u + pan * 256u / 100u], chg[ch]);   /* cos */
                    pr = bio_m15(BIO_SIN[pan * 256u / 100u], chg[ch]);          /* sin */
                    for (k = 0; k < BIO_SB; k++) {
                        int32_t x = bio_sat(e->ob[o][k]), sd = bio_m15(bio_sat(e->os[o][k]), chg[ch]);
                        dryl[k] += bio_m15(x, pl) + sd;   /* (the side: + left, - right) */
                        dryr[k] += bio_m15(x, pr) - sd;
                        chm[k] += x;
                        if (bio_abs(x) > pk) pk = bio_abs(x);
                    }
                    pk = bio_m15(pk, chg[ch]);
                    if (pk > meter[ch]) meter[ch] = pk;
                }
                if (!any || !chg[ch]) continue;
                used |= 1u << ch;
                if (ga[ch]) for (k = 0; k < BIO_SB; k++) sa[k] += bio_mul15(chm[k], ga[ch]);
                if (gb[ch]) for (k = 0; k < BIO_SB; k++) sb[k] += bio_mul15(chm[k], gb[ch]);
                if (gc[ch]) for (k = 0; k < BIO_SB; k++) sc[k] += bio_mul15(chm[k], gc[ch]);
            }
            (void)used;
        }
        /* BLOOD DELAY: stereo, darker each time round, ping-pong, a send on into the reverb */
        {
            int32_t fb = (int32_t)(e->fx.d_fb > 85u ? 85u : e->fx.d_fb) * 327, ret = bio_mul15(bio_fader(e->fx.d_mix), 32767 - e->duck);
            int32_t torev = (int32_t)(e->fx.d_rev > 35u ? 35u : e->fx.d_rev) * 327;
            e->dtime_to = bio_dly_time(e);
            if (!e->dtime) e->dtime = e->dtime_to;
            for (k = 0; k < BIO_SB; k++) {
                int32_t yl, yr, fl, fr2;
                e->dtime += (e->dtime_to - e->dtime) >> 11;   /* a time change slides (tape), never jumps */
                yl = bio_rd(e->dl, BIO_DLY_N - 1u, e->dw, e->dtime);
                yr = bio_rd(e->dr, BIO_DLY_N - 1u, e->dw, e->dtime + (e->fx.d_ping ? 0 : 1100 << 8 >> 4));
                fl = bio_op_lp(&e->dtl, yl);
                fr2 = bio_op_lp(&e->dtr, yr);
                e->dw = (e->dw + 1u) & (BIO_DLY_N - 1u);
                if (e->fx.d_ping) {
                    e->dl[e->dw] = (int16_t)bio_sat(sa[k] + bio_mul15(fr2, fb));
                    e->dr[e->dw] = (int16_t)bio_sat(bio_mul15(fl, fb));
                } else {
                    e->dl[e->dw] = (int16_t)bio_sat(sa[k] + bio_mul15(fl, fb));
                    e->dr[e->dw] = (int16_t)bio_sat(sa[k] + bio_mul15(fr2, fb));
                }
                dryl[k] += bio_mul15(yl, ret);
                dryr[k] += bio_mul15(yr, ret);
                sb[k] += bio_mul15((yl + yr) >> 1, torev);
            }
        }
        /* BILE REVERB: FDN4 at half rate, two diffusers, damping, a slow modulation; FREEZE */
        {
            int32_t ret = bio_mul15(bio_fader(e->fx.r_mix), 32767 - e->duck);   /* (PUMP: the returns breathe too) */
            uint32_t fz = e->freeze;
            int32_t m0 = (int32_t)(bio_sin(e->rlfo) >> 11), m2 = (int32_t)(bio_sin(e->rlfo * 3u + 0x5000000u) >> 11);
            e->rlfo += 64000u * (BIO_SB / 2u);  /* ~0.33 Hz: its movement once a sub-block */
            for (k = 0; k < BIO_SB; k++) {
                int32_t in = bio_op_lp(&e->rin, sb[k]);   /* (what would fold back at half rate, down first) */
                if (!(k & 1u)) {
                    e->rhalf = in;
                } else {
                    int32_t x = fz ? 0 : (e->rhalf + in) >> 2, y[4], a, b, c, d;
                    uint32_t j;
                    e->pre[e->pw] = (int16_t)bio_sat(x);
                    {
                        int32_t i = (int32_t)e->pw - (int32_t)e->rpre;
                        x = e->pre[i < 0 ? i + (int32_t)BIO_PRE_N : i];
                    }
                    if (++e->pw >= BIO_PRE_N) e->pw = 0;
                    /* two allpasses: the diffusion */
                    {
                        int32_t z = e->ap1[e->a1w], v = x - (z >> 1);
                        e->ap1[e->a1w] = (int16_t)bio_sat(v);
                        x = z + (v >> 1);
                        if (++e->a1w >= BIO_AP1) e->a1w = 0;
                        z = e->ap2[e->a2w];
                        v = x - (z >> 1);
                        e->ap2[e->a2w] = (int16_t)bio_sat(v);
                        x = z + (v >> 1);
                        if (++e->a2w >= BIO_AP2) e->a2w = 0;
                    }
                    for (j = 0; j < 4u; j++) {
                        int32_t dd = (int32_t)e->rlen[j] << 8;
                        if (j == 0u) dd += m0 * 16;
                        if (j == 2u) dd += m2 * 16;
                        {
                            int32_t di = dd >> 8, fr = dd & 255, i0 = (int32_t)e->rw - di, i1;
                            if (i0 < 0) i0 += (int32_t)BIO_REV_L;
                            i1 = i0 ? i0 - 1 : (int32_t)BIO_REV_L - 1;
                            y[j] = e->rv[j][i0] + (((e->rv[j][i1] - e->rv[j][i0]) * fr) >> 8);
                        }
                        y[j] = bio_op_lp(&e->rdamp[j], y[j]);
                    }
                    /* Hadamard: orthogonal, so the loop keeps what the gains say */
                    a = y[0] + y[1]; b = y[0] - y[1]; c = y[2] + y[3]; d = y[2] - y[3];
                    {
                        int32_t h0 = (a + c) >> 1, h1 = (b + d) >> 1, h2 = (a - c) >> 1, h3 = (b - d) >> 1;
                        int32_t g0 = fz ? 32702 : e->rg[0], g1 = fz ? 32702 : e->rg[1];
                        int32_t g2 = fz ? 32702 : e->rg[2], g3 = fz ? 32702 : e->rg[3];
                        e->rv[0][e->rw] = (int16_t)bio_sat((int32_t)bio_rz64((int64_t)h0 * g0, 15) + x);
                        e->rv[1][e->rw] = (int16_t)bio_sat((int32_t)bio_rz64((int64_t)h1 * g1, 15) + x);
                        e->rv[2][e->rw] = (int16_t)bio_sat((int32_t)bio_rz64((int64_t)h2 * g2, 15) - x);
                        e->rv[3][e->rw] = (int16_t)bio_sat((int32_t)bio_rz64((int64_t)h3 * g3, 15) - x);
                    }
                    if (++e->rw >= BIO_REV_L) e->rw = 0;
                    e->rprev_l = e->rout_l;
                    e->rprev_r = e->rout_r;
                    e->rout_l = y[0] + y[2];
                    e->rout_r = y[1] + y[3];
                }
                {                               /* back to the full rate: halfway, then there */
                    int32_t l = k & 1u ? e->rout_l : (e->rprev_l + e->rout_l) >> 1;
                    int32_t r = k & 1u ? e->rout_r : (e->rprev_r + e->rout_r) >> 1;
                    l = bio_op_hp(&e->rhp_l, bio_op_lp(&e->rol, l));
                    r = bio_op_hp(&e->rhp_r, bio_op_lp(&e->ror, r));
                    dryl[k] += bio_mul15(l, ret);
                    dryr[k] += bio_mul15(r, ret);
                }
            }
        }
        /* RUMBLE: HEART, an eighth of the rate, two lines of 37 and 53 ms feeding each other (orthogonal) through a
           low-pass each, about 2 s long; back up to the full rate smoothly, ducked by PUMP */
        if (e->mix.rumble) {
            int32_t rl = bio_mul15(bio_fader(e->mix.rumble), 32767 - e->duck) * 2;
            for (k = 0; k < BIO_SB; k++) {
                e->rum_acc += e->ob[O_HEART][k];
                if ((k & 7u) == 7u) {
                    int32_t x = bio_op_lp(&e->rum_in, e->rum_acc >> 4), a, b;   /* (the mean of 8, halved) */
                    uint32_t w = e->rum_w;
                    e->rum_acc = 0;
                    a = bio_op_lp(&e->rum_d0, e->rum[0][(w - 207u) & (BIO_RUM_N - 1u)]);
                    b = bio_op_lp(&e->rum_d1, e->rum[1][(w - 293u) & (BIO_RUM_N - 1u)]);
                    e->rum[0][w] = (int16_t)bio_sat((int32_t)bio_rz64((int64_t)(a + b) * 19820, 15) + x);   /* (0.857 x 0.707) */
                    e->rum[1][w] = (int16_t)bio_sat((int32_t)bio_rz64((int64_t)(a - b) * 19820, 15) + x);
                    e->rum_w = (w + 1u) & (BIO_RUM_N - 1u);
                    e->rum_pl = e->rum_l; e->rum_pr = e->rum_r;
                    e->rum_l = a; e->rum_r = b;
                }
                {
                    int32_t f = (int32_t)(k & 7u) + 1, l, r;
                    l = e->rum_pl + (((e->rum_l - e->rum_pl) * f) >> 3);
                    r = e->rum_pr + (((e->rum_r - e->rum_pr) * f) >> 3);
                    dryl[k] += bio_mul15(bio_op_lp(&e->rum_ol, l), rl);
                    dryr[k] += bio_mul15(bio_op_lp(&e->rum_or, r), rl);
                }
            }
        }
        /* MARROW CRUSH: drive, then fewer samples, fewer bits, a fold */
        {
            int32_t ret = bio_fader(e->fx.c_mix), drv = (int32_t)(bio_exp2((int32_t)e->fx.c_drive * 4 * 1089 * 6 / 10) >> 4);
            uint32_t hold = 1u + (uint32_t)e->fx.c_rate * 7u / 100u, bits = 16u - (uint32_t)e->fx.c_bits * 11u / 100u;
            int32_t fold = 32767 - (int32_t)e->fx.c_fold * 220, mask = ~((1 << (16u - bits)) - 1);
            for (k = 0; k < BIO_SB; k++) {
                int32_t x;
                if (++e->c_cnt >= (int32_t)hold) {
                    e->c_cnt = 0;
                    x = bio_flesh(sc[k], drv);
                    if (x > fold) x = 2 * fold - x;
                    if (x < -fold) x = -2 * fold - x;
                    e->c_hold = x & mask;
                }
                x = bio_dc(&e->cdc, e->c_hold);
                dryl[k] += bio_mul15(x, ret);
                dryr[k] += bio_mul15(x, ret);
            }
        }
        /* the master: its level (-6 dB at INIT), DC, a peak limiter near -1 dBFS (the sub-block's peak first), a soft
           clip after it */
        {
            int32_t mg = bio_fader(e->mix.master);   /* 75 -> 0.56: -5 dB at INIT */
            uint32_t dk = e->mix.dark > 100u ? 100u : e->mix.dark;
            if (dk != e->dark_seen) {           /* DARK: 18 kHz at 1, 1.2 kHz at 100 */
                uint32_t j;
                e->dark_seen = dk;
                bio_op_set(&e->dk[0], bio_lf_hz(18000) - (int32_t)dk * (bio_lf_hz(18000) - bio_lf_hz(1200)) / 100);
                for (j = 1; j < 4u; j++) e->dk[j].g = e->dk[0].g;
            }
            for (k = 0; k < BIO_SB; k++) {
                dryl[k] = bio_dc(&e->mdc_l, (int32_t)(((int64_t)dryl[k] * mg) >> 15));
                dryr[k] = bio_dc(&e->mdc_r, (int32_t)(((int64_t)dryr[k] * mg) >> 15));
                if (dk) {
                    dryl[k] = bio_op_lp(&e->dk[1], bio_op_lp(&e->dk[0], dryl[k]));
                    dryr[k] = bio_op_lp(&e->dk[3], bio_op_lp(&e->dk[2], dryr[k]));
                }
                if (bio_abs(dryl[k]) > peak) peak = bio_abs(dryl[k]);
                if (bio_abs(dryr[k]) > peak) peak = bio_abs(dryr[k]);
            }
            tg = peak > 29204 ? (int32_t)(29204u * 32767u / (uint32_t)peak) : 32767;
            if (tg > e->lim_g) tg = e->lim_g + ((tg - e->lim_g) >> 6);   /* up slowly (~50 ms) */
            else e->lim_g = tg;                 /* down at once, for the whole sub-block (it was too late at its start:
                                                   the peak clipped) */
            gstep = (tg - e->lim_g) / (int32_t)BIO_SB;
            for (k = 0; k < BIO_SB; k++) {
                int32_t l, r;
                e->lim_g += gstep;
                l = bio_mul15(dryl[k], e->lim_g);
                r = bio_mul15(dryr[k], e->lim_g);
                l = bio_clamp(l, -32767, 32767);
                r = bio_clamp(r, -32767, 32767);
                out[2u * (off + k)] += l;
                out[2u * (off + k) + 1u] += r;
            }
            e->lim_g = tg;
        }
        e->frames += BIO_SB;
    }
    for (o = 0; o < BIO_NCH; o++) e->meter[o] = meter[o];
}

#endif
