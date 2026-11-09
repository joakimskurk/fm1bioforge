/* SPDX-License-Identifier: GPL-3.0-only */
/* BIOFORGE's deep kick (D22): a drum organ's MODEL KICK. A sine falling from a sweep to its pitch, held, then a long
 * decay; drive for weight, then two low-pass poles so it keeps almost no top; a knock (some noise, low) if asked. Its
 * phase runs on through a new hit (no click), its attack is a ramp of 1 ms or more.
 *   BODY its pitch (30 .. 48 .. 80 Hz)        TEXTURE how far it sweeps (0.5 .. 4 octaves)
 *   MOTION how fast (4 .. 25 .. 120 ms)      DECAY its tail (80 ms .. 0.45 .. 3 s, to -40 dB)
 *   D1 TONE the low-pass (60 .. 220 .. 2000 Hz)   D2 DRIVE (0 .. 18 dB)   D3 KNOCK   D4 HOLD (0 .. 200 ms)
 *   ATTACK (1 .. 20 ms), TUNE, LEVEL, PAN as a drum's */
#ifndef BIO_KICK_H
#define BIO_KICK_H
#include "bio_dsp.h"

typedef struct {
    uint32_t awake, ph, t, hold_n, stage;       /* stage: 0 attack, 1 hold, 2 decay */
    uint32_t inc_end, inc_add;                  /* the pitch, and the sweep's extra at its start */
    int32_t pe, pe_k;                           /* the sweep (Q30) and its fall a sample */
    int32_t env, att, dec, amp;                 /* the level (Q30), the attack's step, the decay a sample; the hit (Q15) */
    bio_op_t lp1, lp2, kn;
    int32_t drive, knock, kn_env;
    uint32_t rnd;
    bio_dc_t dc;
    int32_t peak, fade;
} bio_kick_t;

typedef struct { uint8_t body, texture, motion, decay, tone, drive, knock, hold, attack; int8_t tune; } bio_kick_p_t;

static void bio_kick_hit(bio_kick_t *v, const bio_kick_p_t *p, int32_t note_q8, uint32_t vel, int32_t lvl, uint32_t seed)
{
    int32_t a = bio_lf_hz(30), m = bio_lf_hz(48), b = bio_lf_hz(80), lf;
    int32_t pitch = (int32_t)p->tune * BIO_SEMI + (int32_t)(((int64_t)note_q8 * BIO_SEMI) >> 8);
    uint32_t att_n;
    lf = (p->body <= 50u ? a + (m - a) * (int32_t)p->body / 50 : m + (b - m) * ((int32_t)p->body - 50) / 50) + pitch;
    v->inc_end = bio_inc(lf);
    v->inc_add = bio_inc(lf + 32768 + (int32_t)p->texture * 229376 / 100) - v->inc_end;   /* 0.5 .. 4 octaves */
    v->pe = 1 << 30;
    {
        int32_t ms = p->motion <= 50u ? 4 + (int32_t)p->motion * 21 / 50 : 25 + ((int32_t)p->motion - 50) * 95 / 50;
        v->pe_k = bio_decay(1, BIO_MS((uint32_t)ms));
    }
    if (!v->awake) { v->ph = 0; v->env = 0; v->lp1.s = v->lp2.s = v->kn.s = 0; v->dc.x1 = v->dc.y1 = 0; }
    att_n = BIO_MS(1u + (uint32_t)p->attack * 19u / 100u);
    v->att = (int32_t)((1u << 30) / (att_n + 1u));
    v->stage = 0;
    v->hold_n = BIO_MS((uint32_t)p->hold * 2u);
    v->t = 0;
    {
        uint32_t ms = (uint32_t)(p->decay <= 50u ? bio_exp2(bio_lf_hz(80) + (bio_lf_hz(450) - bio_lf_hz(80)) * (int32_t)p->decay / 50 - (14 << 16))
                                                 : bio_exp2(bio_lf_hz(450) + (bio_lf_hz(3000) - bio_lf_hz(450)) * ((int32_t)p->decay - 50) / 50 - (14 << 16))) >> 2;
        v->dec = bio_decay(1, bio_tau40(ms));
    }
    bio_op_set(&v->lp1, p->tone <= 50u ? bio_lf_hz(60) + (bio_lf_hz(220) - bio_lf_hz(60)) * (int32_t)p->tone / 50
                                       : bio_lf_hz(220) + (bio_lf_hz(2000) - bio_lf_hz(220)) * ((int32_t)p->tone - 50) / 50);
    v->lp2.g = v->lp1.g;
    bio_op_set(&v->kn, bio_lf_hz(900));
    v->drive = (int32_t)(bio_exp2((int32_t)p->drive * 3 * 653) >> 4);   /* 0 .. 18 dB, Q12 */
    v->knock = (int32_t)p->knock * 200;
    v->kn_env = 1 << 15;
    v->amp = bio_mul15((int32_t)(vel > 127u ? 127u : vel) * 258, lvl);
    v->rnd = bio_hash(seed, 0x4B1Cu);
    v->fade = 32767;
    v->awake = 1;
}

/* n samples, added to out[]; its level now into *press (HEART's pressure: the drone's duck) */
static void bio_kick_render(bio_kick_t *v, int32_t *out, uint32_t n, int32_t *press)
{
    uint32_t k;
    int32_t pk = v->peak;
    if (!v->awake) return;
    for (k = 0; k < n; k++) {
        int32_t x;
        v->pe = bio_mul30(v->pe, v->pe_k);
        v->ph += v->inc_end + (uint32_t)(((uint64_t)v->inc_add * (uint32_t)v->pe) >> 30);
        if (v->stage == 0u) {
            v->env += v->att;
            if (v->env >= (1 << 30)) { v->env = 1 << 30; v->stage = v->hold_n ? 1u : 2u; v->t = 0; }
        } else if (v->stage == 1u) {
            if (++v->t >= v->hold_n) v->stage = 2;
        } else {
            v->env = bio_mul30(v->env, v->dec);
        }
        x = bio_mul15(bio_sin(v->ph), v->env >> 15);
        if (v->drive > 4200) x = bio_flesh(x, v->drive);
        if (v->knock && v->kn_env > 64) {       /* the knock: a little noise, low, in its first ms */
            x += bio_mul15(bio_op_lp(&v->kn, bio_noise(&v->rnd)), bio_mul15(v->knock, v->kn_env));
            v->kn_env = bio_mul15(v->kn_env, 32000);
        }
        x = bio_op_lp(&v->lp2, bio_op_lp(&v->lp1, x));
        x = bio_mul15(bio_dc(&v->dc, x), bio_mul15(v->amp, v->fade));
        out[k] += x;
        if (bio_abs(x) > pk) pk = bio_abs(x);
        if (v->fade < 32767) v->fade = v->fade > 600 ? v->fade - 600 : 0;
    }
    v->peak = pk;
    *press = v->env >> 15;
    if (!v->fade || (v->stage == 2u && v->env < (1 << 16))) v->awake = 0;
}
#endif
