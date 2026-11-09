/* SPDX-License-Identifier: GPL-3.0-only */
/* BIOFORGE's noise as percussion: one voice, struck and choked, that the NOISE organ always is and any drum organ
 * can become (its MODEL). A source (WHITE, PINK, DIGI: a long shift-register noise clocked at the note, METAL: the
 * short one, tonal; CRACKLE: sparse impulses), grain (sample and hold), a resonant state-variable filter that morphs
 * low-pass, band-pass, high-pass and pings at high resonance, a sweep on its own envelope (a zap down, a rise up),
 * bursts (a clap), a click, an envelope that strikes or gates (held while the key is), drive, a crusher, and a
 * decorrelated side signal for width. Integers only; its noise from the hit's seed. */
#ifndef BIO_PERC_H
#define BIO_PERC_H
#include "bio_dsp.h"

enum { PS_WHITE, PS_PINK, PS_DIGI, PS_METAL, PS_CRACKLE };
static const char *const BIO_PS_NAME[5] = {"WHITE", "PINK", "DIGI", "METAL", "CRACKLE"};

typedef struct {
    uint32_t awake, t, rnd, rnd2, gate, stage;  /* stage: 0 attack, 1 decay / hold, 2 release */
    int32_t amp;                                /* the hit's level, Q15 */
    uint32_t src;
    uint32_t lfsr, lph, linc;                   /* DIGI, METAL: the register and its clock */
    int32_t lout, pink, pk;                     /* the register's output; PINK's state and coefficient (Q15) */
    uint32_t crk;                               /* CRACKLE: an impulse when the draw is under this */
    uint32_t sh_n, sh_c;
    int32_t sh_v;
    bio_svf_t f;
    int32_t lf0, sweep, fenv, fdec, k, mode, wlo, whi;   /* the cutoff, the sweep at full, its envelope (Q30), its decay;
                                                   the mode and its two weights (Q10) */
    int32_t env, dec, att_inc, rel, sus, bdec;  /* the level's envelope (Q30), a burst's own quick decay */
    uint32_t gate_n;                            /* the gate closes after this many samples (a sequenced note) */
    uint32_t bursts, bgap, bnext;               /* a clap: the bursts still to come, the gap, the next */
    int32_t click, drive, bits, width;
    uint32_t crush_n, crush_c;
    int32_t crush_v, crush_s;
    bio_op_t hp2;
    bio_dc_t dc;
    int32_t mk_lp, mk_bp, mk;                   /* the make-up (octaves, Q16): a low-pass's, a band-pass's; its gain now (Q12) */
    int32_t fade, peak, lvl;                    /* lvl: the last block's envelope (the screen) */
} bio_perc_t;

/* the parameters (0..100) a perc voice reads: the four macros, its depth, its envelope */
typedef struct { uint8_t body, texture, motion, decay, source, reso, mode, bursts, click, drive, crush;
                 uint8_t attack, gate, width, release; int8_t tune; } bio_perc_p_t;

static uint32_t bio_perc_ms(uint32_t v, uint32_t lo, uint32_t mid, uint32_t hi)   /* (as bio_map_ms) */
{
    int32_t a = bio_lf_hz(lo), m = bio_lf_hz(mid), b = bio_lf_hz(hi), lf;
    if (v > 100u) v = 100u;
    lf = v <= 50u ? a + (m - a) * (int32_t)v / 50 : m + (b - m) * ((int32_t)v - 50) / 50;
    return (uint32_t)(bio_exp2(lf - (14 << 16)) >> 2);
}

/* a strike: note_q8 semitones (Q8) from the voice's own pitch, vel 1..127 (louder and brighter), lvl its LEVEL (Q15),
   len samples held (0: a one-shot) */
static void bio_perc_hit(bio_perc_t *v, const bio_perc_p_t *p, int32_t note_q8, uint32_t vel, int32_t lvl, uint32_t len,
                         uint32_t seed)
{
    int32_t a, b, m, pitch = (int32_t)p->tune * BIO_SEMI + (int32_t)(((int64_t)note_q8 * BIO_SEMI) >> 8);
    int32_t velq = (int32_t)(vel > 127u ? 127u : vel) * 258;
    uint32_t dms, i;
    v->awake = 1;
    v->t = 0;
    v->rnd = bio_hash(seed, 0x51A7u);
    v->rnd2 = bio_hash(seed, 0xC0DEu);
    v->gate = len ? 1u : 0u;
    v->gate_n = len;
    v->fade = 32767;
    v->amp = bio_mul15(velq, lvl);
    v->src = p->source < 20u ? PS_WHITE : p->source < 40u ? PS_PINK : p->source < 60u ? PS_DIGI : p->source < 80u ? PS_METAL : PS_CRACKLE;
    /* the cutoff: BODY, the note, a little brighter for a harder hit */
    a = bio_lf_hz(120); m = bio_lf_hz(2400); b = bio_lf_hz(14000);
    v->lf0 = (p->body <= 50u ? a + (m - a) * (int32_t)p->body / 50 : m + (b - m) * ((int32_t)p->body - 50) / 50) + pitch +
             ((int32_t)vel - 100) * 300;
    v->k = (int32_t)bio_q_k(5u + (uint32_t)p->reso * (uint32_t)p->reso / 50u);   /* Q 0.5 .. 20 (it pings) */
    v->mode = p->mode > 100u ? 100 : p->mode;
    v->wlo = v->mode <= 50 ? v->mode * 1024 / 50 : 1024;
    v->whi = v->mode > 50 ? (v->mode - 50) * 1024 / 50 : 0;
    v->sweep = ((int32_t)p->motion - 50) * 65536 * 3 / 50;   /* up to three octaves: above 50 a zap down, under 50 a rise */
    dms = bio_perc_ms(p->decay, 4, 120, 3000);
    v->dec = bio_decay(1, bio_tau40(dms));
    v->fdec = bio_decay(BIO_SB, bio_tau40(dms / 2u + 2u));
    v->fenv = 1 << 30;
    v->att_inc = p->attack ? (int32_t)((1u << 30) / (BIO_MS((uint32_t)p->attack * 4u) + 1u)) : 1 << 30;
    v->env = p->attack ? 0 : 1 << 30;
    v->stage = p->attack ? 0u : 1u;
    v->sus = (int32_t)((uint32_t)p->gate * 10737418u);     /* held at GATE % while the key is down */
    v->rel = bio_decay(1, bio_tau40(bio_perc_ms(p->release, 5, 150, 2000)));
    v->bursts = p->bursts < 25u ? 0u : p->bursts < 50u ? 1u : p->bursts < 75u ? 2u : 3u;   /* 1..4 strikes: a clap */
    v->bgap = BIO_MS(7u + (seed & 7u));
    v->bdec = bio_decay(1, BIO_MS(15) / 10u);   /* a burst: 1.5 ms an e-fold */
    v->bnext = v->bgap;
    v->click = (int32_t)p->click * 300;
    v->drive = (int32_t)(bio_exp2((int32_t)p->drive * 3 * 1089) >> 4);   /* 0..30 dB, Q12 */
    v->crush_n = 1u + (uint32_t)p->crush * (uint32_t)p->crush / 900u;   /* 1..12 samples held */
    i = 16u - (uint32_t)p->crush * 11u / 100u;                          /* 16..5 bits */
    v->bits = ~((1 << (16u - i)) - 1);
    v->crush_c = 0;
    v->width = (int32_t)p->width * 327;
    /* the source */
    if (v->src == PS_DIGI || v->src == PS_METAL) {
        v->lfsr = 0x7FFFu ^ (seed & 0x3FFFu);
        if (!v->lfsr) v->lfsr = 1;
        v->linc = bio_inc(v->lf0 + (v->src == PS_METAL ? 2 * 65536 : 65536));   /* clocked at twice (METAL: 4x) the cutoff */
    }
    v->pk = 9000;
    {   /* CRACKLE's density (TEXTURE): 60 .. 9000 impulses a second; the others' grain: TEXTURE above 50 holds */
        uint32_t per_s = (uint32_t)(bio_exp2(bio_lf_hz(60) + (int32_t)p->texture * (bio_lf_hz(9000) - bio_lf_hz(60)) / 100 - (14 << 16)) >> 2);
        v->crk = per_s * 97391u;                /* (2^32 / 44100 a sample) */
        v->sh_n = p->texture <= 50u ? 1u : 1u + ((uint32_t)p->texture - 50u) * ((uint32_t)p->texture - 50u) / 80u;
        v->sh_c = 0;
    }
    /* the make-up: a filtered noise keeps its loudness wherever BODY and RESO put the filter. A low-pass passes power
     * in proportion to its cutoff, so it gains half an octave for every octave its cutoff is under the source's top
     * (WHITE, CRACKLE: 22 kHz; PINK: its corner); a band-pass's power grows with Q, so it loses half Q's octaves. DIGI
     * and METAL are clocked at the cutoff: their power is in the pass band already. */
    v->mk_lp = v->src == PS_DIGI || v->src == PS_METAL ? 0 : v->src == PS_PINK ? bio_lf_hz(3000) : bio_lf_hz(22050);
    v->mk_bp = (bio_log2((uint32_t)v->k) - (16 << 16)) / 2;
    bio_svf_set(&v->f, v->lf0 + bio_mul30(v->sweep, v->fenv), (uint32_t)v->k);
    bio_op_set(&v->hp2, bio_lf_hz(3000));
}

static void bio_perc_release(bio_perc_t *v)
{
    v->gate = 0;
    if (v->stage < 2u) v->stage = 2;
}

/* a soft knee for the voice at 0 dB of DRIVE: straight up to -2.5 dBFS, then rounded into full scale (a parabola that
   meets the line's slope): cheap where nearly all of it is, under the knee */
static inline int32_t bio_knee(int32_t x)
{
    int32_t a = x < 0 ? -x : x, d;
    if (a <= 24576) return x;
    d = a - 24576;
    if (d >= 16382) a = 32767;
    else a = 24576 + d - (int32_t)(((uint32_t)d * (uint32_t)d) >> 15);   /* (d^2 / 32764: the slope 0 at full scale) */
    return x < 0 ? -a : a;
}

/* n samples, added: the voice into out[], its width into side[] */
static void bio_perc_render(bio_perc_t *v, int32_t *out, int32_t *side, uint32_t n)
{
    uint32_t k;
    int32_t pk = v->peak, lf, ca, cb, cx, gs;
    int32_t soft = v->drive <= 4200, dc = !soft || v->crush_n > 1u || v->bits != -1;
    if (!v->awake) return;
    /* the sweep: once a sub-block */
    v->fenv = bio_mul30(v->fenv, v->fdec);
    lf = v->lf0 + bio_mul30(v->sweep, v->fenv);
    bio_svf_set(&v->f, lf, (uint32_t)v->k);
    {   /* the make-up for this cutoff: the low-pass's (up to x8: full down to 345 Hz), then across MODE */
        int32_t elp = v->mk_lp ? bio_clamp((v->mk_lp - lf) / 2, 0, 3 << 16) : 0, ebp = elp + v->mk_bp, e;
        e = v->mode <= 50 ? elp + (int32_t)(((int64_t)(ebp - elp) * v->wlo) >> 10) : ebp - (int32_t)(((int64_t)ebp * v->whi) >> 10);
        v->mk = (int32_t)(bio_exp2(e) >> 4);
    }
    /* MODE and the make-up as one sum, Q12: under 50 low-pass to band-pass, over it band-pass to high-pass
       (hp = x - k bp - lp, so y = bp (1 - w) + w hp = -w lp + (1 - w - w k) bp + w x) */
    if (v->mode <= 50) {
        ca = (int32_t)(((int64_t)(1024 - v->wlo) * v->mk) >> 10);
        cb = (int32_t)(((int64_t)v->wlo * v->mk) >> 10);
        cx = 0;
    } else {
        int32_t wk = (int32_t)(((int64_t)v->whi * v->k) >> 16);   /* w k, Q10 */
        ca = -(int32_t)(((int64_t)v->whi * v->mk) >> 10);
        cb = (int32_t)(((int64_t)(1024 - v->whi - wk) * v->mk) >> 10);
        cx = -ca;
    }
    gs = side && v->width ? bio_mul15(v->width, bio_mul15(v->amp, v->fade)) : 0;   /* the side's level */
    for (k = 0; k < n; k++) {
        int32_t w = bio_noise(&v->rnd), x, lp, bp, y;
        /* the source */
        switch (v->src) {
        case PS_WHITE: x = w; break;
        case PS_PINK: v->pink += (int32_t)(((w - v->pink) * v->pk) >> 15); x = v->pink * 2; break;
        case PS_DIGI: case PS_METAL:
            v->lph += v->linc;
            if (v->lph < v->linc) {             /* a clock: the register steps (METAL feeds back short: a tone) */
                uint32_t fb = (v->lfsr ^ (v->lfsr >> (v->src == PS_METAL ? 6 : 1))) & 1u;
                v->lfsr = (v->lfsr >> 1) | fb << 14;
                v->lout = v->lfsr & 1u ? 22000 : -22000;
            }
            x = v->lout;
            break;
        default: {                              /* CRACKLE: sparse impulses of random size and sign */
            uint32_t r = bio_rnd(&v->rnd2);
            x = r < v->crk ? (int32_t)(int16_t)(r >> 8) * 2 : 0;
        }
        }
        if (v->sh_n > 1u) {                     /* grain */
            if (++v->sh_c >= v->sh_n) { v->sh_c = 0; v->sh_v = x; }
            x = v->sh_v;
        }
        /* the filter: low-pass at 0, band-pass at 50, high-pass at 100; the make-up */
        x >>= 1;
        lp = bio_svf(&v->f, x, &bp);
        y = (int32_t)(((int64_t)lp * ca + (int64_t)bp * cb + (int64_t)x * cx) >> 12);
        /* the envelope: attack, then a decay (to the gate's level while held), the release; the bursts */
        if (v->bursts && v->t >= v->bnext) {    /* a clap: struck again */
            v->bursts--;
            v->bnext = v->t + v->bgap + (bio_rnd(&v->rnd2) & 127u);
            v->env = 1 << 30;
            v->stage = 1;
        }
        if (v->stage == 0u) {
            v->env += v->att_inc;
            if (v->env >= (1 << 30)) { v->env = 1 << 30; v->stage = 1; }
        } else if (v->stage == 1u) {
            if (v->gate && v->gate_n && v->t >= v->gate_n) { v->gate = 0; v->stage = 2; }   /* the note's end */
            if (v->bursts) v->env = bio_mul30(v->env, v->bdec);   /* (between a clap's strikes: short) */
            else if (v->gate && v->sus && v->env <= v->sus) v->env = v->sus;
            else v->env = bio_mul30(v->env, v->dec);
        } else {
            v->env = bio_mul30(v->env, v->rel);   /* (env >= 0: the floor is toward zero) */
        }
        if (v->t < 2u) y += v->t ? -v->click : v->click;   /* the click */
        y = bio_mul15(y, v->env >> 15);
        y = soft ? bio_knee(y) : bio_flesh(y, v->drive);   /* (at 0 dB of DRIVE still a knee: its peaks rounded) */
        if (v->crush_n > 1u || v->bits != -1) {
            if (++v->crush_c >= v->crush_n) { v->crush_c = 0; v->crush_v = y & v->bits; }
            y = v->crush_v;
        }
        if (dc) y = bio_dc(&v->dc, y);         /* (a driven or crushed hit has DC; a clean one none) */
        y = bio_mul15(y, bio_mul15(v->amp, v->fade));
        out[k] += y;
        if (gs) side[k] += bio_mul15(bio_mul15(bio_op_hp(&v->hp2, bio_noise(&v->rnd2)), v->env >> 15), gs) >> 2;
        if (v->fade < 32767) v->fade = v->fade > 600 ? v->fade - 600 : 0;
        if (bio_abs(y) > pk) pk = bio_abs(y);
        v->t++;
    }
    v->peak = pk;
    v->lvl = v->env >> 15;
    if (!v->fade || (v->stage >= 1u && !(v->gate && v->sus) && v->env < (1 << 15) && !v->bursts)) {
        v->awake = 0;
        v->f.ic1 = v->f.ic2 = 0;
        v->dc.x1 = v->dc.y1 = 0;
    }
}
#endif
