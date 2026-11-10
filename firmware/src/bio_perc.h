/* SPDX-License-Identifier: GPL-3.0-only */
/* BIOFORGE's noise core: one voice, struck and choked, that the NOISE organ always is and any drum organ can become
 * (its MODEL). Rhythm made of noise (D20): the sequencer locks each step's noise (its seed) and the Turing's LOOP lets
 * it slip; SCATTER gives every step its own slice of it (band, length, shape, level, place); TONE puts the band on the
 * note (a riff of hiss, a second band a fifth up); DRIFT moves the band over four bars. A source (WHITE, PINK, DIGI: a long shift-register noise clocked at the note, METAL: the
 * short one, tonal; CRACKLE: sparse impulses), grain (sample and hold), a resonant state-variable filter that morphs
 * low-pass, band-pass, high-pass and pings at high resonance, a sweep on its own envelope (a zap down, a rise up),
 * bursts (a clap), a click, an envelope that strikes or gates (held while the key is), drive, a crusher, and a
 * decorrelated side signal for width. Integers only; its noise from the hit's seed. */
#ifndef BIO_PERC_H
#define BIO_PERC_H
#include "bio_dsp.h"

/* the sources: SOURCE 0..100 in elevenths (bio_ps_of). Four crackle, organic (D21): DUST a sparse crackle, FIRE pops
   in clusters over a smoulder, FRY a dense sizzle, CREAK stick-slip, a gliding train of slips. Three are noise that is
   not a white noise generator's (D25), tied to the note: ALIAS, FM so deep its sidebands run past Nyquist and fold
   back (TEXTURE its depth); SIX, six squares at a cymbal's ratios, not band-limited (TEXTURE from their sum to their
   ring); BYTE, a byte of a formula on a counter clocked at the note, rhythm inside the noise (TEXTURE: four of them) */
enum { PS_WHITE, PS_PINK, PS_DIGI, PS_METAL, PS_DUST, PS_FIRE, PS_FRY, PS_CREAK, PS_ALIAS, PS_SIX, PS_BYTE, BIO_NPS };
static const char *const BIO_PS_NAME[BIO_NPS] = {"WHITE", "PINK", "DIGI", "METAL", "DUST", "FIRE", "FRY", "CREAK",
                                                 "ALIAS", "SIX", "BYTE"};
#define BIO_PS(i) ((uint8_t)(((i) * 101u + 50u) / BIO_NPS))   /* a source as SOURCE's value (the middle of its share) */
#define BIO_PS_CRACKLE(s) ((s) >= PS_DUST && (s) <= PS_CREAK)
static inline uint32_t bio_ps_of(uint32_t v) { v = v * BIO_NPS / 101u; return v < BIO_NPS ? v : BIO_NPS - 1u; }

typedef struct {
    uint32_t awake, t, rnd, rnd2, gate, stage;  /* stage: 0 attack, 1 decay / hold, 2 release */
    int32_t amp;                                /* the hit's level, Q15 */
    uint32_t src;
    uint32_t lfsr, lph, linc;                   /* DIGI, METAL: the register and its clock */
    int32_t lout, pink, pk;                     /* the register's output; PINK's state and coefficient (Q15) */
    uint32_t crk;                               /* the crackles: an impulse when the draw is under this */
    int32_t heat, heat_to, pop, pop_d, rum;     /* FIRE, FRY: the fire's heat (Q15), a pop's level and its decay, the smoulder */
    uint32_t boost, ctl;                        /* a pop makes the next likelier (clusters); the control's countdown */
    uint32_t cph, cinc, cinc_to;                /* CREAK: the slips' phase, their rate and where it glides */
    uint32_t xp[6], xi[6], xt;                  /* ALIAS, SIX, BYTE: their phases and steps, BYTE's counter */
    int32_t xidx, xmix;                         /* ALIAS's depth (phase units a Q15 sine's 1); SIX's ring (Q15); BYTE's
                                                   formula */
    int32_t camp, camp_to;                      /* its level, gliding in and out (a creak, then still) */
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
    int32_t hz[32];                             /* WIDTH: its own sound, half a millisecond back (D24: no air of its own) */
    uint32_t hw;
    bio_dc_t dc;
    int32_t mk_lp, mk_bp, mk;                   /* the make-up (octaves, Q16): a low-pass's, a band-pass's; its gain now (Q12) */
    bio_svf_t f2;                               /* TONE: the second band, a fifth up */
    int32_t tone2;                              /* its level (Q10; 0: off) */
    int32_t drift, lfm;                         /* DRIFT: its depth (lf, Q16); the band's offset now (the engine steps it
                                                   bar by bar, D24) */
    int32_t pan_s;                              /* SCATTER's place for this hit: its share sent to the side (Q15) */
    int32_t fade, peak, lvl;                    /* lvl: the last block's envelope (the screen) */
} bio_perc_t;

/* the parameters (0..100) a perc voice reads: the four macros, its depth, its envelope */
typedef struct { uint8_t body, texture, motion, decay, source, reso, mode, bursts, click, drive, crush;
                 uint8_t attack, gate, width, release; int8_t tune; uint8_t scatter, tone, drift; } bio_perc_p_t;

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
    int32_t velq = (int32_t)(vel > 127u ? 127u : vel) * 258, sc = p->scatter > 100u ? 100 : p->scatter;
    uint32_t dms, i, reso = p->reso, shape = 0;  /* shape: 0 as set, 1 a chop (held, cut), 2 struck, 3 a swell */
    uint32_t r1 = bio_hash(seed, 0x5CA7u), r2 = bio_hash(seed, 0x5A9Eu);   /* SCATTER's draws: this step's own */
    v->awake = 1;
    v->t = 0;
    v->rnd = bio_hash(seed, 0x51A7u);
    v->rnd2 = bio_hash(seed, 0xC0DEu);
    v->gate = len ? 1u : 0u;
    v->gate_n = len;
    v->fade = 32767;
    v->amp = bio_mul15(velq, lvl);
    v->pan_s = 0;
    if (sc) {                                   /* SCATTER: this step's slice, from its (locked) seed */
        reso = (uint32_t)bio_clamp((int32_t)reso + ((int32_t)(r1 >> 16 & 255u) - 128) * sc / 300, 0, 100);
        if ((r2 & 1023u) < (uint32_t)sc * 10u) shape = 1u + (r2 >> 10) % 3u;
        if (len && len != 0xFFFFFFFFu)          /* (a key held stays held) */
            len -= (uint32_t)(((uint64_t)len * (uint32_t)((r2 >> 12) & 255u) * (uint32_t)sc * 3u) >> 18) / 100u;   /* down to 1/4 */
        v->amp = bio_mul15(v->amp, 32767 - (int32_t)((r2 >> 20) & 255u) * sc * 64 / 100);   /* down to -6 dB */
        v->pan_s = ((int32_t)(r1 >> 24) - 128) * sc * 181 / 100;   /* (to 0.707: hard left or right) */
        v->gate_n = len;
    }
    v->src = bio_ps_of(p->source);
    /* the cutoff: BODY, the note, a little brighter for a harder hit */
    a = bio_lf_hz(120); m = bio_lf_hz(2400); b = bio_lf_hz(14000);
    if (p->tone) {                              /* TONE: the band on the note (C4 at 262 Hz), BODY its octave (-2 .. +3) */
        v->lf0 = bio_lf_hz(262) + pitch + (((int32_t)p->body * 5 + 50) / 100 - 2) * 65536;
        v->tone2 = (int32_t)(p->tone > 100u ? 100u : p->tone) * 717 / 100;   /* the fifth: up to 0.7 */
    } else {
        v->lf0 = (p->body <= 50u ? a + (m - a) * (int32_t)p->body / 50 : m + (b - m) * ((int32_t)p->body - 50) / 50) + pitch +
                 ((int32_t)vel - 100) * 1200;   /* (an accent opens it: +30 is half an octave) */
        v->tone2 = 0;
    }
    if (sc) v->lf0 += ((int32_t)(r1 & 0xFFFFu) - 32768) * 6 * sc / 100;   /* SCATTER: the band, +-3 octaves */
    v->drift = (int32_t)(p->drift > 100u ? 100u : p->drift) * 983;          /* DRIFT: to +-1.5 octaves */
    v->k = (int32_t)bio_q_k(5u + reso * reso / 50u);   /* Q 0.5 .. 20 (it pings) */
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
    if (shape == 1u || (shape == 3u && len)) {   /* a chop: open while the note is, then cut (5 ms); a swell: rising to its end */
        v->sus = 1 << 30;
        v->rel = bio_decay(1, bio_tau40(5));
        v->stage = shape == 3u ? 0u : 1u;
        v->env = shape == 3u ? 0 : 1 << 30;
        v->att_inc = shape == 3u ? (int32_t)((1u << 30) / (len + 1u)) : 1 << 30;
    } else if (shape == 2u) {                   /* struck: its decay, the key let go at once */
        v->sus = 0;
        v->env = 1 << 30;
        v->stage = 1;
    }
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
    {   /* a crackle's density (TEXTURE): DUST 60 .. 9000 a second, FIRE 4 .. 600, FRY 400 .. 20000, CREAK the slips'
           range; the others' grain: TEXTURE above 50 holds */
        static const uint16_t LO[4] = {60, 4, 400, 30}, HI[4] = {9000, 600, 20000, 600};
        uint32_t c = BIO_PS_CRACKLE(v->src) ? v->src - PS_DUST : 0u, per_s;
        per_s = (uint32_t)(bio_exp2(bio_lf_hz(LO[c]) + (int32_t)p->texture * (bio_lf_hz(HI[c]) - bio_lf_hz(LO[c])) / 100 - (14 << 16)) >> 2);
        v->crk = per_s * 97391u;                /* (2^32 / 44100 a sample) */
        v->sh_n = p->texture <= 50u || v->src >= PS_DUST ? 1u : 1u + ((uint32_t)p->texture - 50u) * ((uint32_t)p->texture - 50u) / 80u;
        if (v->src == PS_ALIAS) {               /* FM: the carrier two octaves under the band, the modulator a tritone
                                                   over it; 2 .. 40 rad */
            v->xi[0] = bio_inc(v->lf0 - 2 * 65536);
            v->xi[1] = bio_inc(v->lf0 - 2 * 65536 + 32768);
            v->xidx = (2 + (int32_t)p->texture * 38 / 100) * 20861;
            v->xp[0] = v->xp[1] = 0;
        } else if (v->src == PS_SIX) {          /* six squares, three octaves under the band, at a cymbal's ratios */
            static const int32_t SIX[6] = {0, 34905, 45444, 61984, 86737, 92607};   /* 1 1.447 1.617 1.927 2.503 2.664 */
            uint32_t j;
            for (j = 0; j < 6u; j++) { v->xi[j] = bio_inc(v->lf0 - 3 * 65536 + SIX[j]); v->xp[j] = j * 0x2A3B4C5Du; }
            v->xmix = (int32_t)p->texture * 327;
        } else if (v->src == PS_BYTE) {         /* a counter at the band's rate; TEXTURE the formula */
            v->xi[0] = bio_inc(v->lf0);
            v->xp[0] = 0;
            v->xt = 0;
            v->xmix = (int32_t)p->texture * 4 / 101;
        }
        v->heat = v->heat_to = 20000; v->boost = 0; v->pop = 0; v->pop_d = 0; v->ctl = 1; v->rum = 0;
        v->cinc = v->cinc_to = (uint32_t)per_s * 97391u; v->cph = 0; v->camp = 0; v->camp_to = 0;
        if (v->src == PS_CREAK) {               /* a creak starts with the hit (and runs 0.15 .. 0.5 s before it rests) */
            v->camp = v->camp_to = 22000;
            v->ctl = 200u + (seed & 511u);
        }
        if (sc && (r2 >> 28) * 100u < (uint32_t)sc * 5u) v->sh_n = 2u + (r1 >> 8 & 7u);   /* SCATTER: a slice in grains */
        v->sh_c = 0;
    }
    /* the make-up: a filtered noise keeps its loudness wherever BODY and RESO put the filter. A low-pass passes power
     * in proportion to its cutoff, so it gains half an octave for every octave its cutoff is under the source's top
     * (WHITE, CRACKLE: 22 kHz; PINK: its corner); a band-pass's power grows with Q, so it loses half Q's octaves. DIGI
     * and METAL are clocked at the cutoff: their power is in the pass band already. */
    v->mk_lp = v->src == PS_DIGI || v->src == PS_METAL || v->src >= PS_CREAK ? 0 : v->src == PS_PINK ? bio_lf_hz(3000) : bio_lf_hz(22050);
    v->mk_bp = v->src == PS_CREAK ? 0 : (bio_log2((uint32_t)v->k) - (16 << 16)) / 2;   /* (CREAK: slips ring a body) */
    bio_svf_set(&v->f, v->lf0 + bio_mul30(v->sweep, v->fenv), (uint32_t)v->k);
    if (v->tone2) bio_svf_set(&v->f2, v->lf0 + bio_mul30(v->sweep, v->fenv) + 7 * BIO_SEMI, (uint32_t)v->k);
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
    lf = v->lf0 + bio_mul30(v->sweep, v->fenv) + v->lfm;
    bio_svf_set(&v->f, lf, (uint32_t)v->k);
    if (v->tone2) bio_svf_set(&v->f2, lf + 7 * BIO_SEMI, (uint32_t)v->k);
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
    gs = side ? v->width : 0;                   /* the side's level */
    if (v->src >= PS_FIRE && v->src <= PS_CREAK && --v->ctl == 0u) {   /* the crackles' life: a new heat, a new creak */
        uint32_t r = bio_rnd(&v->rnd2), u = r >> 17;   /* (0..32767) */
        if (v->src == PS_CREAK) {               /* a creak (0.15 .. 0.8 s), or still (to 0.5 s); its rate wanders */
            v->camp_to = v->camp_to ? 0 : 14000 + (int32_t)(u & 16383u);
            v->ctl = v->camp_to ? 200u + (r & 1023u) : 60u + (r & 511u);
            v->cinc_to = (uint32_t)(((uint64_t)v->crk * (uint32_t)(1434u + (r >> 4 & 4095u) * 5u / 4u)) >> 12);   /* 0.35 .. 1.6 x */
        } else {                                /* FIRE: a new heat every 0.05 .. 0.4 s (FRY: faster), mostly low */
            v->heat_to = 3000 + (int32_t)(((uint32_t)u * u) >> 15) * 29 / 32;
            v->ctl = (v->src == PS_FRY ? 8u : 64u) + (r & (v->src == PS_FRY ? 63u : 511u));
        }
    }
    if (v->src >= PS_FIRE && v->src <= PS_CREAK) {   /* (the crackles only) */
        v->heat += (v->heat_to - v->heat) >> 4;
        v->camp += (v->camp_to - v->camp) >> 5;
        if (v->cinc != v->cinc_to) { int32_t d = (int32_t)(v->cinc - v->cinc_to); v->cinc = v->cinc_to + (uint32_t)(d - d / 16); }
    }
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
        case PS_DUST: {                         /* sparse impulses of random size and sign */
            uint32_t r = bio_rnd(&v->rnd2);
            x = r < v->crk ? (int32_t)(int16_t)(r >> 8) * 2 : 0;
            break;
        }
        case PS_FIRE: case PS_FRY: {            /* pops: likelier with the heat and after a pop (clusters); most small,
                                                   a few big (the cube of a draw), each a snap of noise; under them the
                                                   smoulder (FIRE) or a sizzle (FRY) */
            uint32_t r = bio_rnd(&v->rnd2);
            uint64_t p = (((uint64_t)v->crk * (uint32_t)v->heat) >> 14) + v->boost;
            if (r < p) {
                uint32_t u = r & 0xFFFFu, z = (u * u >> 16) * u >> 16;   /* 0 .. 65535, most small */
                int32_t a = (int32_t)(z >> 1) + 600;
                v->pop = a;
                v->pop_d = v->src == PS_FRY ? 28000 : 30200 + (int32_t)(z >> 5);   /* FRY 0.15 ms; FIRE 0.3 .. 2 ms */
                x = r & (1u << 16) ? a : -a;
                if (v->boost < (v->crk > 0x15555555u ? 0x7FFFFFFFu : v->crk * 6u)) v->boost += v->crk >> (v->src == PS_FRY ? 2 : 0);
            } else {
                x = bio_mul15(w, v->pop);
            }
            v->pop = bio_mul15(v->pop, v->pop_d);
            v->boost -= v->boost >> 9;          /* (12 ms) */
            v->rum += (w - v->rum) >> 6;        /* (110 Hz) */
            x += v->src == PS_FRY ? bio_mul15(w, v->heat) >> 6 : bio_mul15(v->rum, v->heat) >> 1;
            break;
        }
        case PS_ALIAS: {                        /* FM past Nyquist: folding back, inharmonic, digital */
            v->xp[0] += v->xi[0]; v->xp[1] += v->xi[1];
            x = bio_sin(v->xp[0] + (uint32_t)((int64_t)bio_sin(v->xp[1]) * v->xidx));
            break;
        }
        case PS_SIX: {                          /* their sum, their ring (the parity of their signs), between */
            uint32_t j, par = 0;
            int32_t a = 0;
            for (j = 0; j < 6u; j++) {
                uint32_t hi;
                v->xp[j] += v->xi[j];
                hi = v->xp[j] >> 31;
                a += hi ? -5400 : 5400;
                par ^= hi;
            }
            x = a + bio_mul15((par ? -30000 : 30000) - a, v->xmix);
            break;
        }
        case PS_BYTE: {                         /* a byte of a formula on a counter (bytebeat) */
            uint32_t t, b;
            v->xp[0] += v->xi[0];
            if (v->xp[0] < v->xi[0]) v->xt++;
            t = v->xt;
            switch (v->xmix) {
            case 0: b = (t * ((t >> 9 | t >> 13) & 25u & t >> 6)) ^ (t >> 4); break;
            case 1: b = (t * 5u & t >> 7) | (t * 3u & t >> 10); break;
            case 2: b = t * (t >> 11 & t >> 8 & 123u & t >> 3); break;
            default: b = (t >> 6 | t | t >> 9) * 10u + ((t >> 11) & 7u); break;
            }
            x = ((int32_t)(b & 255u) - 128) * 200;
            break;
        }
        default: {                              /* CREAK: a train of slips, its rate gliding, now and then a slip missed */
            v->cph += v->cinc;
            x = 0;
            if (v->cph < v->cinc) {             /* a slip; the next comes up to a third early (stick-slip is not a clock) */
                uint32_t r = bio_rnd(&v->rnd2);
                if ((r & 15u) != 0u) x = bio_mul15(v->camp, 20000 + (int32_t)(r >> 19)) * 12;   /* (a slip is one sample: loud) */
                v->cph = (r >> 2) / 3u;
            }
            x += bio_mul15(w, v->camp) >> 5;    /* the grind between */
        }
        }
        if (v->sh_n > 1u) {                     /* grain */
            if (++v->sh_c >= v->sh_n) { v->sh_c = 0; v->sh_v = x; }
            x = v->sh_v;
        }
        /* the filter: low-pass at 0, band-pass at 50, high-pass at 100; the make-up */
        x >>= 1;
        lp = bio_svf(&v->f, x, &bp);
        if (v->tone2) { int32_t bp2; (void)bio_svf(&v->f2, x, &bp2); bp += (bp2 * v->tone2) >> 10; }   /* TONE's fifth */
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
        if (gs) {                               /* WIDTH: the sound less itself half a millisecond back, to the side (it
                                                   widens what is there; a hiss of its own was a wind) */
            int32_t yd = v->hz[(v->hw - 23u) & 31u];
            v->hz[v->hw & 31u] = y;
            v->hw++;
            side[k] += bio_mul15(y - yd, gs) >> 1;
        }
        if (v->pan_s && side) side[k] += bio_mul15(y, v->pan_s);   /* SCATTER's place: + left, - right */
        if (v->fade < 32767) v->fade = v->fade > 600 ? v->fade - 600 : 0;
        if (bio_abs(y) > pk) pk = bio_abs(y);
        v->t++;
    }
    v->peak = pk;
    v->lvl = v->env >> 15;
    if (!v->fade || (v->stage >= 1u && !(v->gate && v->sus) && v->env < (1 << 15) && !v->bursts)) {
        v->awake = 0;
        v->f.ic1 = v->f.ic2 = v->f2.ic1 = v->f2.ic2 = 0;
        v->dc.x1 = v->dc.y1 = 0;
    }
}
#endif
