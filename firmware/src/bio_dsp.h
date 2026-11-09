/* SPDX-License-Identifier: GPL-3.0-only */
/* BIOFORGE's arithmetic: integers only (the chip has no FPU, no 64-bit division; docs/RULES.md). Frequencies are
 * lf = log2(Hz) in Q16; levels Q15; coefficients Q30. The building blocks: the magic-circle resonator, the TPT
 * state-variable filter, one-poles, decays, noise, sine, the flesh curve. Shared by the firmware and the host tools. */
#ifndef BIO_DSP_H
#define BIO_DSP_H
#include <stdint.h>
#include "bio_tables.h"

#define BIO_FS 44100u
#define BIO_SB 32u                              /* a sub-block: events, envelopes, coefficients at 1378 Hz */
#define BIO_LF_A4 575919                        /* log2(440) in Q16 */
#define BIO_SEMI 5461                           /* a semitone in lf (5461.33) */
#define BIO_MS(ms) ((uint32_t)(ms) * 441u / 10u)   /* samples in ms */

static inline int32_t bio_clamp(int32_t x, int32_t lo, int32_t hi) { return x < lo ? lo : x > hi ? hi : x; }
static inline int32_t bio_sat(int32_t x) { return bio_clamp(x, -32767, 32767); }
/* x >> s, rounded toward zero (in a feedback loop, so it dies out instead of buzzing at -1) */
static inline int64_t bio_rz64(int64_t v, int s) { return (v + ((v >> 63) & ((((int64_t)1) << s) - 1))) >> s; }
static inline int32_t bio_mul30(int32_t a, int32_t b) { return (int32_t)(((int64_t)a * b) >> 30); }
static inline int32_t bio_mul30z(int32_t a, int32_t b) { return (int32_t)bio_rz64((int64_t)a * b, 30); }
static inline int32_t bio_mul15(int32_t a, int32_t b) { return (int32_t)(((int64_t)a * b) >> 15); }
static inline int32_t bio_abs(int32_t x) { return x < 0 ? -x : x; }

/* ---- noise: xorshift32 (never 0) */
static inline uint32_t bio_rnd(uint32_t *s)
{
    uint32_t x = *s;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    return *s = x;
}
static inline int32_t bio_noise(uint32_t *s) { return (int32_t)(int16_t)(bio_rnd(s) >> 16); }   /* Q15, flat */
static inline uint32_t bio_hash(uint32_t a, uint32_t b)   /* a seed from two numbers; never 0 */
{
    uint32_t x = a * 0x9E3779B1u ^ (b + 0x7F4A7C15u) * 0x85EBCA77u;
    x ^= x >> 15;
    x *= 0x2C1B3C6Du;
    x ^= x >> 12;
    return x ? x : 0x1234567u;
}
static inline uint32_t bio_rndr(uint32_t *s, uint32_t n) { return (uint32_t)(((uint64_t)(bio_rnd(s) >> 8) * n) >> 24); }

/* ---- sine of a 32-bit phase, Q15, interpolated */
static inline int32_t bio_sin(uint32_t ph)
{
    uint32_t i = ph >> 22, fr = (ph >> 6) & 0xFFFFu;
    int32_t a = BIO_SIN[i], b = BIO_SIN[i + 1u];
    return a + (int32_t)(((b - a) * (int32_t)fr) >> 16);
}
static inline int32_t bio_tri(uint32_t ph)      /* in phase with bio_sin, Q15 */
{
    int32_t v = (int32_t)((ph + 0x40000000u) >> 16) - 32768;
    return bio_clamp(32767 - 2 * bio_abs(v), -32767, 32767);
}

/* ---- exp2, log2 (Q16) */
static uint32_t bio_exp2(int32_t x)             /* 2^(x / 65536) in Q16, x in -16..15 octaves */
{
    int32_t o = x >> 16;
    uint32_t f = (uint32_t)x & 0xFFFFu, j = f >> 8, r = f & 255u;
    uint32_t v = BIO_EXP2[j] + (((BIO_EXP2[j + 1u] - BIO_EXP2[j]) * r) >> 8);
    if (o >= 0) return o > 15 ? 0xFFFFFFFFu : v << o;
    return o < -16 ? 0u : v >> -o;
}
static int32_t bio_log2(uint32_t v)             /* log2(v) in Q16, v >= 1 */
{
    int32_t o = 31;
    uint32_t m, j, r;
    if (!v) return 0;
    while (!(v & 0x80000000u)) { v <<= 1; o--; }
    m = (v >> 15) & 0xFFFFu;                    /* the mantissa's 16 bits under the top one */
    j = m >> 8;
    r = m & 255u;
    return (o << 16) + (int32_t)(BIO_LOG2[j] + (((BIO_LOG2[j + 1u] - BIO_LOG2[j]) * r) >> 8));
}
static inline int32_t bio_lf_hz(uint32_t hz) { return bio_log2(hz ? hz : 1u); }
static inline int32_t bio_lf_note(int32_t note_q8)   /* a MIDI note (Q8: 1/256 semitone) to lf */
{
    return BIO_LF_A4 + (int32_t)(((int64_t)(note_q8 - 69 * 256) * 5461) >> 8);
}
static uint32_t bio_inc(int32_t lf)             /* the phase increment for lf */
{
    int32_t o = lf >> 16;
    uint32_t f = (uint32_t)lf & 0xFFFFu, j = f >> 8, r = f & 255u;
    uint32_t v = BIO_INC1[j] + (uint32_t)(((uint64_t)(BIO_INC1[j + 1u] - BIO_INC1[j]) * r) >> 8);
    if (o < 0) return v >> (o < -20 ? 20 : -o);
    return o > 14 ? v << 14 : v << o;
}
static int32_t bio_lftab(const int32_t *t, int32_t lf)   /* BIO_TAN or BIO_MC at lf, interpolated, clamped */
{
    int32_t d = lf - BIO_LF_LO, i, r;
    if (d < 0) d = 0;
    i = d >> BIO_LF_SHIFT;
    if (i >= BIO_LF_N - 1) return t[BIO_LF_N - 1];
    r = d & ((1 << BIO_LF_SHIFT) - 1);
    return t[i] + (int32_t)(((int64_t)(t[i + 1] - t[i]) * r) >> BIO_LF_SHIFT);
}

/* ---- decays. n = samples per e-fold (tau * FS). exp(-step / n) in Q30, by its series (n >= 4) */
static int32_t bio_decay(uint32_t step, uint32_t n)
{
    int64_t x, y;
    if (n < 4u) n = 4u;
    x = (int64_t)((1u << 30) / n) * step;       /* step / n, Q30 */
    if (x > (3 << 29)) x = 3 << 29;             /* (steps longer than 1.5 tau: the series would go wrong) */
    y = (1 << 30) - x;
    {                                           /* (each term under 2^31: divided as 32-bit numbers) */
        int32_t x2 = (int32_t)((x * x) >> 30), x3 = (int32_t)(((int64_t)x2 * x) >> 30);
        int32_t x4 = (int32_t)(((int64_t)x3 * x) >> 30);
        y += x2 / 2 - x3 / 6 + x4 / 24;
    }
    return (int32_t)(y < 0 ? 0 : y);
}
/* samples per e-fold for a time to -40 dB given in ms (the document's "decay" numbers) */
static inline uint32_t bio_tau40(uint32_t ms) { uint32_t n = BIO_MS(ms) * 10u / 46u; return n < 4u ? 4u : n; }
static inline uint32_t bio_tau(uint32_t ms) { uint32_t n = BIO_MS(ms); return n < 4u ? 4u : n; }   /* an e-fold in ms */
/* x * 2^(oct/65536): a time or a level scaled by octaves */
static inline uint32_t bio_scale(uint32_t x, int32_t oct) { return (uint32_t)(((uint64_t)x * bio_exp2(oct)) >> 16); }

/* ---- the magic circle: a damped mode, stable at any frequency in integers (its determinant is D: a turn of the
 * phasor loses exactly what D says). x1, x2 are Q15 << 8. f = 2 sin(pi F / FS) (Q30), D = the per-sample energy
 * factor (Q30; the amplitude falls by sqrt(D) a sample). */
typedef struct { int32_t x1, x2, f, d; } bio_mode_t;
static inline int32_t bio_mode_tick(bio_mode_t *m, int32_t in)
{
    int32_t x1 = bio_mul30z(m->d, m->x1) - bio_mul30(m->f, m->x2) + in;
    m->x1 = x1;
    m->x2 += bio_mul30(m->f, x1);
    return m->x2;
}
static inline void bio_mode_set(bio_mode_t *m, int32_t lf, uint32_t tau_n)   /* tau_n: samples per e-fold */
{
    int32_t dd = bio_decay(1, tau_n);
    m->f = bio_lftab(BIO_MC, lf);
    m->d = bio_mul30(dd, dd);                   /* energy: the square of the amplitude's factor */
}

/* ---- the TPT state-variable filter (Zavalishin). g = tan(pi f / FS) Q24, k = 1/Q Q16. Q15 signals. */
typedef struct { int32_t ic1, ic2, a1, a2, a3, k; } bio_svf_t;
static inline uint32_t bio_recip16(uint32_t den_q24)   /* 1/den in Q16, den >= 1.0 (Q24) */
{
    uint32_t d = den_q24 >> 8;
    return d ? 0xFFFFFFFFu / d : 0xFFFFu;
}
static void bio_svf_set(bio_svf_t *s, int32_t lf, uint32_t k_q16)
{
    int32_t g = bio_lftab(BIO_TAN, lf);
    int64_t den = (1 << 24) + (((int64_t)g * (g + ((int64_t)k_q16 << 8))) >> 24);
    uint32_t inv = bio_recip16(den > 0x7FFFFFFF ? 0x7FFFFFFFu : (uint32_t)den);   /* Q16 */
    int32_t a1 = (int32_t)((uint64_t)inv << 14);   /* Q30 */
    s->a1 = a1 > (1 << 30) ? (1 << 30) : a1;
    s->a2 = (int32_t)(((int64_t)g * s->a1) >> 24);
    s->a3 = (int32_t)(((int64_t)g * s->a2) >> 24);
    s->k = (int32_t)k_q16;
}
/* one sample: the band-pass in *bp, the low-pass returned (high-pass: x - k bp - lp) */
static inline int32_t bio_svf(bio_svf_t *s, int32_t x, int32_t *bp)
{
    int32_t v3 = x - s->ic2;
    int32_t v1 = bio_mul30(s->a1, s->ic1) + bio_mul30(s->a2, v3);
    int32_t v2 = s->ic2 + bio_mul30(s->a2, s->ic1) + bio_mul30(s->a3, v3);
    s->ic1 = bio_clamp(2 * v1 - s->ic1, -(1 << 24), 1 << 24);
    s->ic2 = bio_clamp(2 * v2 - s->ic2, -(1 << 24), 1 << 24);
    *bp = v1;
    return v2;
}
static inline int32_t bio_svf_bp(bio_svf_t *s, int32_t x) { int32_t bp; (void)bio_svf(s, x, &bp); return bp; }
static inline int32_t bio_svf_hp(bio_svf_t *s, int32_t x)
{
    int32_t bp, lp = bio_svf(s, x, &bp);
    return x - (int32_t)(((int64_t)s->k * bp) >> 16) - lp;
}
static inline uint32_t bio_q_k(uint32_t q_x10) { return q_x10 ? 655360u / q_x10 : 65536u; }   /* Q (x10) to k Q16 */

/* ---- the TPT one-pole: G = g / (1 + g), Q30 */
typedef struct { int32_t s, g; } bio_op_t;
static void bio_op_set(bio_op_t *o, int32_t lf)
{
    int32_t g = bio_lftab(BIO_TAN, lf);
    uint32_t inv = bio_recip16((1u << 24) + (uint32_t)g);
    o->g = (int32_t)(((int64_t)g * inv) >> 10);   /* Q24 * Q16 >> 10 = Q30 */
}
static inline int32_t bio_op_lp(bio_op_t *o, int32_t x)
{
    int32_t v = bio_mul30(o->g, x - o->s), y = v + o->s;
    o->s = y + v;
    return y;
}
static inline int32_t bio_op_hp(bio_op_t *o, int32_t x) { return x - bio_op_lp(o, x); }

/* ---- the flesh curve: an asymmetric soft saturation (cubic), Q15 in and out; drive Q12 (4096 = 0 dB). The
 * positive half is pushed a little harder than the negative (a membrane stretched one way): its DC is the DC block's. */
static inline int32_t bio_flesh(int32_t x, int32_t drive_q12)
{
    int32_t v = (int32_t)(((int64_t)x * drive_q12) >> 12), y;
    if (v > 0) v += v >> 3;
    v = bio_clamp(v, -46340, 46340);            /* the cubic's knee at 1.414 */
    y = v - (int32_t)((((int64_t)((v * v) >> 15)) * v) >> 15) / 6;   /* v - v^3/6: 0.94 at the knee */
    return bio_sat(y);
}
/* DC block: y = x - x1 + R y1, R = 0.9985 (10 Hz) */
typedef struct { int32_t x1, y1; } bio_dc_t;
static inline int32_t bio_dc(bio_dc_t *d, int32_t x)
{
    int32_t y = x - d->x1 + (int32_t)bio_rz64((int64_t)d->y1 * 32719, 15);
    d->x1 = x;
    d->y1 = y;
    return y;
}

/* a level from dB x10 (Q15: 0 dB = 32768) */
static inline int32_t bio_db(int32_t db10) { return (int32_t)(bio_exp2(db10 * 1089) >> 1); }   /* (1089: 0.1 dB in octaves, Q16) */

/* linear ramp over a sub-block */
typedef struct { int32_t v, step; } bio_ramp_t;
static inline void bio_ramp_to(bio_ramp_t *r, int32_t target) { r->step = (target - r->v) / (int32_t)BIO_SB; }
static inline int32_t bio_ramp(bio_ramp_t *r) { r->v += r->step; return r->v; }

#endif
