/* SPDX-License-Identifier: GPL-3.0-only */
/* BIOFORGE's landscapes (D21): models any tonal organ (BASS, DRONE, SPASM, VOICE) can be instead of its own voice,
 * its MODEL (E1). The user chose three worlds: INDUSTRY (HUM, an electric hum with sparks; STEEL, bowed metal; PISTON,
 * a machine that runs while the key is held), COSMIC (GLASS, inharmonic partials beating; BELL, FM; CHOIR, three
 * throats through formants) and BODY (BLOOD, a pulse with the beat; BUBBLE, rising drops; SLIME, a wet squelch). The
 * organ keeps its own envelope, note and role; the four macros mean what each model says below. Integers only, the same
 * seed the same sound; each wide in its own way (D32: the plate's modes apart, sparks and drops anywhere, the low end in
 * the middle). And DUB (D22): a dark chord through a closing low-pass, for the delay to carry. MASS
 * and CLUSTER (D23): the grim drone's body, a stack of octaves and a fifth, or a cluster of semitones, thick under a
 * breathing low-pass, with grit. */
#ifndef BIO_LAND_H
#define BIO_LAND_H
#include "bio_dsp.h"

enum { L_OWN, L_HUM, L_STEEL, L_PISTON, L_GLASS, L_BELL, L_CHOIR, L_BLOOD, L_BUBBLE, L_SLIME, L_DUB, L_MASS, L_CLUSTER,
       BIO_NLAND };
static const char *const BIO_LAND_NAME[BIO_NLAND] = {"OWN", "HUM", "STEEL", "PISTON", "GLASS", "BELL", "CHOIR", "BLOOD",
                                                     "BUBBLE", "SLIME", "DUB", "MASS", "CLUSTER"};
static const char *const BIO_OWN_NAME[4] = {"MUSCLE", "WOMB", "NERVE", "THROAT"};   /* BASS DRONE SPASM VOICE as they are */
#define BIO_LAND(i) ((uint8_t)(((i) * 101u + 50u) / BIO_NLAND))   /* a model as MODEL's value (the middle of its share) */
static inline uint32_t bio_land_of(uint32_t v) { v = v * BIO_NLAND / 101u; return v < BIO_NLAND ? v : BIO_NLAND - 1u; }

/* a note starts: the model's own state (the organ's envelope is set already) */
static void bio_land_on(bio_tone_t *v, uint32_t model, const uint8_t *p, uint32_t seed)
{
    uint32_t k;
    if (v->env.v < (1 << 22)) {                 /* (from silence only: a note over a sounding one keeps its phases and
                                                   filters, so it does not click) */
        for (k = 0; k < 8u; k++) v->lx[k] = 0;
        for (k = 0; k < 4u; k++) v->lu[k] = 0;
        v->pa = v->pb = v->pc = v->pd = 0;
        v->f1.ic1 = v->f1.ic2 = v->f2.ic1 = v->f2.ic2 = v->f3.ic1 = v->f3.ic2 = v->f4.ic1 = v->f4.ic2 = 0;
    }
    v->rnd = bio_hash(seed, 0x1A2Du);
    switch (model) {
    case L_HUM: bio_op_set(&v->o1, bio_lf_hz(6000)); break;
    case L_PISTON: bio_op_set(&v->o1, bio_lf_hz(3000)); v->lu[0] = ~0u; v->lu[1] = 0; break;
    case L_BELL: {                              /* the index: BODY to 5 rad, falling over DECAY's time */
        v->lx[0] = (int32_t)p[P_BODY] * 327;
        v->lx[1] = bio_decay(BIO_SB, bio_tau(bio_map_ms(p[P_DECAY], 40, 400, 4000) / 2u + 1u));
        break;
    }
    case L_CHOIR: bio_op_set(&v->o1, bio_lf_hz(5000)); break;
    case L_BLOOD: bio_op_set(&v->o1, bio_map_lf(p[P_BODY], 80, 300, 1500)); break;
    case L_SLIME: v->lx[1] = 32767; break;      /* a squelch at the start */
    case L_MASS: case L_CLUSTER: bio_op_set(&v->o1, bio_lf_hz(90 * 32)); break;   /* (the grit: a slow noise, at the
                                                                                     sub-block rate: 90 Hz) */
    case L_DUB:                                 /* the filter opens with the note and DECAY closes it */
        v->lx[0] = 32767;
        v->lx[1] = bio_decay(BIO_SB, bio_tau(bio_map_ms(p[P_DECAY], 20, 150, 1500) / 3u + 1u));
        break;
    default: break;
    }
}

/* the steel's modes (a plate's), the glass's partials, the bell's ratios: their log2 (Q16) */
static const int32_t BIO_STEEL_LF[4] = {0, 44018, 71711, 92144};       /* 1, 1.593, 2.135, 2.65 */
static const int32_t BIO_GLASS_LF[4] = {0, 79568, 136806, 178848};     /* 1, 2.32, 4.25, 6.63 */
static const int32_t BIO_BELL_LF[6] = {0, 32756, 65536, 95830, 118446, 159483};   /* 1, 1.414, 2, 2.756, 3.5, 5.404 */
static const uint8_t BIO_PISTON_RATE[6] = {1, 2, 3, 4, 6, 8};          /* strokes a beat */

/* n samples of a landscape: e0 the envelope at the start, step its slope, a the level; added to out[] */
static void bio_land_render(bio_tone_t *v, uint32_t model, const uint8_t *p, const bio_life_t *life, int32_t e0,
                            int32_t step, int32_t a, int32_t *out, int32_t *side, uint32_t n)
{
    uint32_t k, inc = bio_inc(v->lf + bio_mul15(life->drift, 600));
    int32_t pk = v->peak, y, tex = p[P_TEXTURE], body = p[P_BODY], mot = p[P_MOTION];
    switch (model) {
    case L_HUM: {   /* a pulse (TEXTURE narrows it) and its octave, smoothed; a resonant band (BODY) that flutters
                       (MOTION); now and then a spark (TEXTURE) */
        uint32_t w = 2147483648u - (uint32_t)tex * 17179869u;   /* 0.5 .. 0.1 */
        int32_t spk = tex * tex * 970;           /* (sparks: 9 a second at 30, 100 at 100) */
        if (++v->lu[1] >= 40u) { v->lu[1] = 0; v->lx[1] = bio_noise(&v->rnd); v->lx[2] = bio_noise(&v->rnd); }   /* the
                                                   flutter's aim, 34 a second; the band's place in the field (D32) */
        v->lx[0] += (v->lx[1] - v->lx[0]) >> 2;
        v->lx[3] += (v->lx[2] - v->lx[3]) >> 4;
        bio_svf_set(&v->f1, bio_map_lf((uint32_t)body, 150, 900, 5000) + bio_mul15(v->lx[0], mot * 600), bio_q_k(25));
        for (k = 0; k < n; k++) {
            int32_t x, bp, sd;
            v->pa += inc;
            x = (bio_pulse(v->pa, inc, w) >> 1) + bio_mul15(bio_sin(v->pa * 2u), 6000);
            x = bio_op_lp(&v->o1, x);
            (void)bio_svf(&v->f1, x, &bp);
            y = (x >> 1) + bp;
            sd = bio_mul15(bp, v->lx[3]) / 2;
            if ((bio_rnd(&v->rnd) >> 8) < (uint32_t)spk >> 8) {
                int32_t sp = (v->rnd & 1u) ? 14000 : -14000;
                y += sp;
                sd += bio_mul15(sp, (int32_t)(v->rnd >> 16) - 32768);   /* (a spark: anywhere from left to right) */
            }
            y = bio_mul15(y, 32767 - bio_abs(bio_mul15(v->lx[0], mot * 200)));
            e0 += step;
            if (side) side[k] += bio_mul15(bio_mul15(sd, e0), a) * 2;
            y = bio_mul15(bio_mul15(bio_dc(&v->dc, y), e0), a) * 2;
            out[k] += y;
            if (bio_abs(y) > pk) pk = bio_abs(y);
        }
        break;
    }
    case L_STEEL: {   /* bowed metal: noise (a bow, MOTION its jitter) into a plate's four modes; TEXTURE how long they
                         ring (Q 8 .. 200), BODY an octave down or up */
        uint32_t q10 = 80u + (uint32_t)(tex * tex) * 192u / 1000u, j;
        int32_t lf = v->lf + 2 * 65536 + (body - 50) * 1311, g = (int32_t)(bio_exp2((bio_log2(bio_q_k(q10)) - (16 << 16)) / 2 + 3 * 65536) >> 4);
        for (j = 0; j < 4u; j++) bio_svf_set(j == 0u ? &v->f1 : j == 1u ? &v->f2 : j == 2u ? &v->f3 : &v->f4, lf + BIO_STEEL_LF[j], bio_q_k(q10));
        if (++v->lu[1] >= 8u) { v->lu[1] = 0; v->lx[1] = bio_noise(&v->rnd) / 2 + 16384; }   /* the bow's pressure */
        v->lx[0] += (v->lx[1] - v->lx[0]) >> 3;
        for (k = 0; k < n; k++) {
            int32_t x = bio_mul15(bio_noise(&v->rnd) >> 3, 32767 - bio_mul15(mot * 327, 32767 - v->lx[0])), b1, b2, b3, b4;
            (void)bio_svf(&v->f1, x, &b1); (void)bio_svf(&v->f2, x, &b2);
            (void)bio_svf(&v->f3, x, &b3); (void)bio_svf(&v->f4, x, &b4);
            y = (int32_t)(((int64_t)(b1 + ((b2 * 23) >> 5) + (b3 >> 1) + ((b4 * 11) >> 5)) * g) >> 12);
            y = bio_clamp(y, -60000, 60000);
            e0 += step;
            if (side) {                         /* (the plate's modes apart: 1 and 3 one side, 2 and 4 the other, D32) */
                int32_t sd = (int32_t)(((int64_t)((b1 >> 1) - ((b2 * 23) >> 5) + (b3 >> 1) - ((b4 * 11) >> 5)) * g) >> 13);
                side[k] += bio_mul15(bio_mul15(bio_clamp(sd, -40000, 40000), e0), a) * 2;
            }
            y = bio_mul15(bio_mul15(bio_dc(&v->dc, y), e0), a) * 2;
            out[k] += y;
            if (bio_abs(y) > pk) pk = bio_abs(y);
        }
        break;
    }
    case L_PISTON: {   /* a machine, running while the key is held: strokes on the beat (TEXTURE: 1 .. 8 a beat), each
                          a thump down the note and a clank of metal (BODY its brightness), steam between (MOTION) */
        uint32_t rate = BIO_PISTON_RATE[(uint32_t)tex * 6u / 101u], cyc = (uint32_t)(((uint64_t)life->beat * rate) >> 16);
        uint32_t frac = (life->beat * rate) & 0xFFFFu;
        int32_t imp = 0;
        if (cyc != v->lu[0]) { v->lu[0] = cyc; v->lx[0] = 32767; v->lx[1] = 32767; v->lu[1] = 0; imp = 24000; }
        if (frac > 0x8000u && !v->lu[1]) { v->lu[1] = 1; v->lx[2] = mot * 327; }   /* the steam, half a stroke on */
        bio_svf_set(&v->f1, v->lf + 2 * 65536 + (body - 50) * 1311, bio_q_k(400));
        bio_svf_set(&v->f2, v->lf + 2 * 65536 + 159483 + (body - 50) * 1311, bio_q_k(300));
        for (k = 0; k < n; k++) {
            int32_t th, cl, st, b1, b2, hp, w = bio_noise(&v->rnd);
            v->pa += inc + (uint32_t)(((uint64_t)inc * (uint32_t)v->lx[1]) >> 15);   /* the thump falls an octave */
            th = bio_mul15(bio_sin(v->pa), v->lx[1]);
            cl = imp + bio_mul15(w, v->lx[0]) / 4;
            imp = 0;
            (void)bio_svf(&v->f1, cl, &b1); (void)bio_svf(&v->f2, cl, &b2);
            hp = w - bio_op_lp(&v->o1, w);
            st = bio_mul15(hp, v->lx[2]) >> 2;
            if (side) {                         /* (the clank's partials apart, the steam on both sides its own: D32) */
                int32_t w2 = bio_noise(&v->rnd);
                v->lx[3] += (w2 - v->lx[3]) >> 2;
                side[k] += bio_mul15(bio_mul15(((b1 - b2) >> 1) + (bio_mul15(w2 - v->lx[3], v->lx[2]) >> 2), e0 + step), a) * 2;
            }
            v->lx[0] = bio_mul15(v->lx[0], 32440);   /* (the clank's noise: 3 ms) */
            v->lx[1] = bio_mul15(v->lx[1], 32742);   /* (the thump: 30 ms) */
            v->lx[2] = bio_mul15(v->lx[2], 32750);   /* (the steam: 45 ms) */
            y = th + ((b1 + b2) >> 1) + st;
            e0 += step;
            y = bio_mul15(bio_mul15(bio_dc(&v->dc, y), e0), a) * 2;
            out[k] += y;
            if (bio_abs(y) > pk) pk = bio_abs(y);
        }
        break;
    }
    case L_GLASS: {   /* four inharmonic partials of glass, each beating on its own (MOTION); TEXTURE the upper ones'
                         level, BODY a sub-octave under them; its base a twin a little flat (one each side), the first
                         partial into the second's phase, driven (D36: it was clean) */
        uint32_t inc2, inc3, inc4, inc0;
        int32_t l1 = 16000 + tex * 80, l2 = tex * 130, l3 = tex * 80, sub = body * 160;
        if (++v->lu[1] >= 300u) {               /* new aims for the beating, 4 a second */
            v->lu[1] = 0;
            v->lx[4] = bio_noise(&v->rnd); v->lx[5] = bio_noise(&v->rnd); v->lx[6] = bio_noise(&v->rnd);
        }
        v->lx[1] += (v->lx[4] - v->lx[1]) >> 7; v->lx[2] += (v->lx[5] - v->lx[2]) >> 7; v->lx[3] += (v->lx[6] - v->lx[3]) >> 7;
        inc = bio_inc(v->lf + 2 * 65536 + bio_mul15(life->drift, 600));   /* (glass two octaves up; BODY's sub under it) */
        inc2 = bio_inc(v->lf + 2 * 65536 + BIO_GLASS_LF[1] + bio_mul15(v->lx[1], mot * 6));
        inc3 = bio_inc(v->lf + 2 * 65536 + BIO_GLASS_LF[2] + bio_mul15(v->lx[2], mot * 8));
        inc4 = bio_inc(v->lf + 2 * 65536 + BIO_GLASS_LF[3] + bio_mul15(v->lx[3], mot * 10));
        inc0 = inc / 4u;
        {
            uint32_t incb = bio_inc(v->lf + 2 * 65536 - 1500 - mot * 12 + bio_mul15(life->drift, 600));   /* (its twin a little
                                                   flat: 16 .. 30 cents, D36) */
            int32_t pm = 9000 + mot * 250;      /* (the first partial into the second's phase: rough) */
            for (k = 0; k < n; k++) {
                int32_t b0, b1, s2, s3, s4;
                v->pa += inc + (uint32_t)(bio_noise(&v->rnd) * 900); v->pb += inc2; v->pc += inc3; v->pd += inc4;
                v->lu[2] += inc0; v->lu[3] += incb;   /* (the base shaken a little: grit) */
                b0 = bio_sin(v->pa); b1 = bio_sin(v->lu[3]);
                s2 = bio_mul15(bio_sin(v->pb + (uint32_t)((int64_t)b0 * pm * 4)), l1) / 2;
                s3 = bio_mul15(bio_sin(v->pc), l2) / 2;
                s4 = bio_mul15(bio_sin(v->pd + (uint32_t)(s2 * 3000)), l3) / 2;
                y = bio_mul15(bio_flesh(((b0 + b1) >> 2) + s2 + s3 + s4 + bio_mul15(bio_sin(v->lu[2]), sub) / 2, 11000), 22000);
                e0 += step;
                if (side) side[k] += bio_mul15(bio_mul15((s2 - s3 + s4) * 3 / 4 + ((b0 - b1) >> 3), e0), a) * 2;   /* (the partials
                                                   apart, and the twins: D32, D36) */
                y = bio_mul15(bio_mul15(y, e0), a) * 2;
                out[k] += y;
                if (bio_abs(y) > pk) pk = bio_abs(y);
            }
        }
        break;
    }
    case L_BELL: {   /* FM: two carriers a few cents apart (one each side) and a modulator at a ratio (TEXTURE: 1, 1.41,
                        2, 2.76, 3.5, 5.4), fed back on itself and shaken by noise; the index from BODY falling with
                        DECAY to a floor of grit; driven; MOTION a slow vibrato and how far apart (D36: it was clean) */
        uint32_t im = bio_inc(v->lf + BIO_BELL_LF[(uint32_t)tex * 6u / 101u]), inc2;
        int32_t idx, dt, fb = 9000 + body * 120;   /* (the modulator's feedback: about 0.6 .. 1.4 rad, D36) */
        v->lx[0] = bio_mul30(v->lx[0], v->lx[1]);
        idx = (int32_t)(((int64_t)(v->lx[0] + 3000 + body * 40) * 104305) >> 15);   /* (5 rad at full; a floor that never
                                                   falls away: 0.45 .. 1 rad, the grit that stays, D36) */
        v->pc += bio_inc(bio_lf_hz(5)) * n;
        v->pd += bio_inc(bio_lf_hz(1) - 65536) * n;   /* (a slow drift of the detune) */
        dt = 1200 + mot * 14 + bio_mul15(bio_sin(v->pd), 500);   /* (the carriers apart: 13 .. 28 cents, drifting) */
        inc = bio_inc(v->lf + bio_mul15(bio_sin(v->pc), mot * 20) + dt);
        inc2 = bio_inc(v->lf + bio_mul15(bio_sin(v->pc), mot * 20) - dt);
        for (k = 0; k < n; k++) {
            int32_t m, cl, cr;
            uint32_t ph;
            v->pa += inc; v->lu[0] += inc2;
            v->pb += im + (uint32_t)(bio_noise(&v->rnd) * 1500);   /* (the modulator shaken a little: grit) */
            m = bio_sin(v->pb + (uint32_t)((int64_t)v->lx[2] * fb * 2));   /* (fed back on itself: its sidebands rough) */
            v->lx[2] = m;
            ph = (uint32_t)((int64_t)m * idx);
            cl = bio_sin(v->pa + ph);
            cr = bio_sin(v->lu[0] + ph);
            y = bio_mul15(bio_flesh((cl + cr) >> 2, 9000), 21000);   /* (driven: about +7 dB into the flesh curve; as
                                                   loud as it was) */
            e0 += step;
            if (side) side[k] += bio_mul15(bio_mul15((cl - cr) >> 2, e0), a) * 2;   /* (the two carriers apart: wide) */
            y = bio_mul15(bio_mul15(y, e0), a) * 2;
            out[k] += y;
            if (bio_abs(y) > pk) pk = bio_abs(y);
        }
        break;
    }
    case L_CHOIR: {   /* three throats a little apart (MOTION) through three formants: BODY the vowel (U O A E I),
                         TEXTURE their breath */
        int32_t m = bio_clamp(body * 41 + bio_mul15(life->breath, 300), 0, 4 * 1024 - 1), f[3], d = mot * 7;
        uint32_t vi = (uint32_t)m >> 10, j, i2 = bio_inc(v->lf + d), i3 = bio_inc(v->lf - d);
        int32_t fr = m & 1023;
        for (j = 0; j < 3u; j++) {
            int32_t a0 = bio_lf_hz(BIO_VOWEL[vi][j]), a1 = bio_lf_hz(BIO_VOWEL[vi < 4u ? vi + 1u : 4u][j]);
            f[j] = a0 + (int32_t)(((int64_t)(a1 - a0) * fr) >> 10);
        }
        bio_svf_set(&v->f1, f[0], bio_q_k(60));
        bio_svf_set(&v->f2, f[1], bio_q_k(80));
        bio_svf_set(&v->f3, f[2], bio_q_k(100));
        for (k = 0; k < n; k++) {
            int32_t o;
            v->pa += inc; v->pb += i2; v->pc += i3;
            int32_t sb = bio_saw(v->pb, i2), sc2 = bio_saw(v->pc, i3);
            o = (bio_saw(v->pa, inc) + sb + sc2) / 4;
            o = bio_op_lp(&v->o1, o) + bio_mul15(bio_noise(&v->rnd), tex * 90);
            y = bio_svf_bp(&v->f1, o) + (bio_svf_bp(&v->f2, o) >> 1) + (bio_svf_bp(&v->f3, o) >> 2);
            e0 += step;
            if (side) side[k] += bio_mul15(bio_mul15((sb - sc2) / 8, e0), a);   /* (the throats apart: wide) */
            y = bio_mul15(bio_mul15(bio_dc(&v->dc, y), e0), a) * 2;
            out[k] += y;
            if (bio_abs(y) > pk) pk = bio_abs(y);
        }
        break;
    }
    case L_BLOOD: {   /* a pulse on the beat, lub and dub (MOTION how deep): a sine under the note and its octave, the
                         rush of blood (TEXTURE) through a low band (BODY) */
        uint32_t b = life->beat & 0xFFFFu, b2 = (b - 19661u) & 0xFFFFu;   /* (the dub 0.3 of a beat on) */
        int32_t lub = (int32_t)(bio_exp2(-(int32_t)(b * 12u)) >> 1), dub = (int32_t)(bio_exp2(-(int32_t)(b2 * 14u)) >> 1);
        int32_t pulse = bio_clamp(lub + bio_mul15(dub, 20000), 0, 32767), dep = mot * 327, am;
        am = 32767 - dep + bio_mul15(dep, pulse);
        for (k = 0; k < n; k++) {
            int32_t r;
            v->pa += inc; v->pb += inc * 2u;
            r = bio_op_lp(&v->o1, bio_noise(&v->rnd));
            y = (bio_sin(v->pa) >> 1) + (bio_sin(v->pb) >> 3) + bio_mul15(r, tex * 600) / 8;
            y = bio_mul15(y, am);
            e0 += step;
            if (side) {                         /* (the rush its own each side; the pulse in the middle: D32) */
                v->lx[3] += (bio_noise(&v->rnd) - v->lx[3]) >> 5;
                side[k] += bio_mul15(bio_mul15(bio_mul15(bio_mul15(v->lx[3] * 4, tex * 600) / 8, am), e0), a) * 2;
            }
            y = bio_mul15(bio_mul15(y, e0), a) * 2;
            out[k] += y;
            if (bio_abs(y) > pk) pk = bio_abs(y);
        }
        break;
    }
    case L_BUBBLE: {   /* drops rising (2 at a time): TEXTURE how many (2 .. 80 a second), MOTION how far from the
                          note they scatter, BODY their size (bigger: lower, longer) */
        uint32_t per = (uint32_t)(bio_exp2(bio_lf_hz(2) + tex * (bio_lf_hz(80) - bio_lf_hz(2)) / 100 - (14 << 16)) >> 2) * 97391u;
        for (k = 0; k < n; k++) {
            uint32_t r = bio_rnd(&v->rnd);
            if (r < per) {                      /* a new drop, in the older slot */
                uint32_t s = v->lu[3] ^= 1u, sz = (uint32_t)body;
                int32_t lf = v->lf + 3 * 65536 - (body - 50) * 1311 + bio_mul15(bio_noise(&v->rnd), mot * 655);   /* (3 octaves up) */
                v->lu[s] = bio_inc(lf);
                v->lx[s] = 22000 + (int32_t)(r >> 21);
                v->lx[2 + s] = bio_decay(1, BIO_MS(6u + sz * sz / 200u));   /* (6 .. 56 ms) */
                v->lx[4 + s] = (int32_t)(bio_rnd(&v->rnd) >> 17) - 16384;   /* (its place: D32) */
                if (s) v->pb = 0; else v->pa = 0;
            }
            v->pa += v->lu[0]; v->pb += v->lu[1];
            v->lu[0] += v->lu[0] >> 12; v->lu[1] += v->lu[1] >> 12;   /* rising: an octave in 64 ms */
            if (v->lu[0] > (1u << 29)) v->lx[0] = 0;
            if (v->lu[1] > (1u << 29)) v->lx[1] = 0;
            {
                int32_t d0 = bio_mul15(bio_sin(v->pa), v->lx[0]), d1 = bio_mul15(bio_sin(v->pb), v->lx[1]);
                y = d0 + d1;
                if (side) side[k] += bio_mul15(bio_mul15((bio_mul15(d0, v->lx[4]) + bio_mul15(d1, v->lx[5])) * 2, e0 + step), a) * 2;
            }
            v->lx[0] = bio_mul30(v->lx[0], v->lx[2]); v->lx[1] = bio_mul30(v->lx[1], v->lx[3]);
            e0 += step;
            y = bio_mul15(bio_mul15(y, e0), a) * 2;
            out[k] += y;
            if (bio_abs(y) > pk) pk = bio_abs(y);
        }
        break;
    }
    case L_SLIME: {   /* a saw and a square under it (BODY) through a wet resonant low-pass: a squelch at the note and
                         at random (MOTION: to 8 a second), its cutoff gurgling (TEXTURE) */
        uint32_t q10 = 40u + (uint32_t)tex * 80u / 100u, psq = (uint32_t)mot * 4u;   /* (a sub-block's chance, of 2^16) */
        int32_t lfc;
        if ((bio_rnd(&v->rnd) >> 16) < psq) v->lx[1] = 26000 + (int32_t)(v->rnd >> 26) * 100;
        v->lx[1] = bio_mul15(v->lx[1], 32000);  /* (the squelch: 40 ms) */
        if (++v->lu[1] >= 20u) { v->lu[1] = 0; v->lx[2] = bio_noise(&v->rnd); }
        v->lx[0] += (v->lx[2] - v->lx[0]) >> 3;
        lfc = v->lf + 65536 + bio_mul15(v->lx[1], 3 * 65536) + bio_mul15(v->lx[0], tex * 900);
        bio_svf_set(&v->f1, lfc, bio_q_k(q10));
        for (k = 0; k < n; k++) {
            int32_t o;
            v->pa += inc; v->pb += inc / 2u;
            o = bio_saw(v->pa, inc) / 2 + bio_mul15(bio_pulse(v->pb, inc / 2u, 1u << 31) * 3 / 4, body * 327);
            {
                int32_t bp;
                y = bio_svf(&v->f1, o, &bp);
                if (side) side[k] += bio_mul15(bio_mul15(bio_clamp(bio_mul15(bp, v->lx[0]), -30000, 30000), e0 + step), a);   /* (the
                                                   squelch wanders with its gurgle; the body in the middle: D32) */
            }
            y = bio_clamp(y, -50000, 50000);
            e0 += step;
            y = bio_mul15(bio_mul15(bio_dc(&v->dc, y), e0), a) * 2;
            out[k] += y;
            if (bio_abs(y) > pk) pk = bio_abs(y);
        }
        break;
    }
    case L_DUB: {   /* a chord (TEXTURE: minor, minor 7th, minor 9th, fifths) of two saws a note, one each side, a few
                       cents apart and drifting (a slow chorus), driven a little, each side through its own resonant
                       low-pass: BODY its cutoff, MOTION how far the note opens it; the breath and the nerve move it too
                       (D27: it was four saws in the middle, still, flat) */
        static const int8_t CH[4][4] = {{0, 3, 7, 12}, {0, 3, 7, 10}, {0, 3, 10, 14}, {0, 7, 12, 19}};
        uint32_t c = (uint32_t)tex * 4u / 101u, j, il[4], ir[4];
        int32_t ch, d, lfc;
        v->lph += bio_inc(bio_lf_hz(1) - 32768) * n;   /* (the chorus: 0.7 Hz) */
        ch = bio_sin(v->lph);
        d = 382 + bio_mul15(ch, 218);           /* 7 cents, +-4 */
        for (j = 0; j < 4u; j++) {
            int32_t dj = d + d * (int32_t)j * 5 / 32 + bio_mul15(life->drift, 300);
            il[j] = bio_inc(v->lf + CH[c][j] * BIO_SEMI + dj);
            ir[j] = bio_inc(v->lf + CH[c][j] * BIO_SEMI - dj);
        }
        lfc = bio_map_lf((uint32_t)body, 200, 900, 4000) + bio_mul15(v->lx[0], mot * 1966) +
              bio_mul15(life->breath, 26214) + bio_mul15(life->cv, 16384);   /* (to 3 octaves; 0.4 and 0.25 more) */
        v->lx[0] = bio_mul30(v->lx[0], v->lx[1]);
        bio_svf_set(&v->f1, lfc, bio_q_k(32));
        bio_svf_set(&v->f2, lfc + 600, bio_q_k(32));
        for (k = 0; k < n; k++) {
            int32_t ol, or_, bp, yl, yr;
            v->pa += il[0]; v->pb += il[1]; v->pc += il[2]; v->pd += il[3];
            v->lu[0] += ir[0]; v->lu[1] += ir[1]; v->lu[2] += ir[2]; v->lu[3] += ir[3];
            ol = (bio_saw(v->pa, il[0]) + bio_saw(v->pb, il[1]) + bio_saw(v->pc, il[2]) + bio_saw(v->pd, il[3])) / 4;
            or_ = (bio_saw(v->lu[0], ir[0]) + bio_saw(v->lu[1], ir[1]) + bio_saw(v->lu[2], ir[2]) + bio_saw(v->lu[3], ir[3])) / 4;
            yl = bio_svf(&v->f1, bio_flesh(ol, 6000), &bp);
            yr = bio_svf(&v->f2, bio_flesh(or_, 6000), &bp);
            e0 += step;
            y = bio_mul15(bio_mul15(bio_dc(&v->dc, (yl + yr) / 2), e0), a) * 2;
            if (side) side[k] += bio_mul15(bio_mul15((yl - yr) / 2, e0), a);
            out[k] += y;
            if (bio_abs(y) > pk) pk = bio_abs(y);
        }
        break;
    }
    case L_MASS: case L_CLUSTER: {   /* MASS: the note, its fifth and octave up, a sine an octave under; CLUSTER: the note
                                        and the two semitones over it, the octave up. Three saws a few cents apart under a
                                        two-pole low-pass (BODY: 60 .. 400 .. 4000 Hz, Q 1.4) that breathes with the
                                        breath (MOTION, to 1.5 octaves); TEXTURE grit: a slow noise roughening it */
        uint32_t cl = model == L_CLUSTER;
        uint32_t i1 = bio_inc(v->lf + (cl ? BIO_SEMI + 40 : 7 * BIO_SEMI - 50));
        uint32_t i2 = bio_inc(v->lf + (cl ? 2 * BIO_SEMI - 60 : 12 * BIO_SEMI + 30)), i3 = inc / 2u;
        int32_t lfc = bio_map_lf((uint32_t)body, 60, 400, 4000) + bio_mul15(life->breath, mot * 983);
        int32_t grit = tex * 300, g0 = v->lx[5], gs;
        bio_svf_set(&v->f1, lfc, bio_q_k(14));
        v->lx[5] = bio_op_lp(&v->o1, bio_noise(&v->rnd)) * 6;   /* (the grit: a slow noise, a value a sub-block, ramped) */
        gs = (v->lx[5] - g0) / (int32_t)n;
        for (k = 0; k < n; k++) {
            int32_t o, bp, g, s1, s2;
            v->pa += inc; v->pb += i1; v->pc += i2; v->pd += i3;
            s1 = bio_saw(v->pb, i1); s2 = bio_saw(v->pc, i2);
            o = (bio_saw(v->pa, inc) + (s1 * 3) / 4 + (s2 * (cl ? 3 : 2)) / 4) / 3;
            if (side) {                         /* (its upper voices to either side, low-passed by the band's state:
                                                   the drone wide, its root in the middle) */
                v->lx[6] += (((s1 - s2) / 6) - v->lx[6]) >> 3;
                side[k] += bio_mul15(bio_mul15(v->lx[6], e0), a);
            }
            g = g0 += gs;                       /* (a slow rough noise, about +-1) */
            o = bio_mul15(o, 32767 - bio_mul15(grit, bio_abs(bio_clamp(g, -32767, 32767))) / 2) + bio_mul15(g, grit) / 16;
            y = bio_svf(&v->f1, o, &bp) + (cl ? 0 : bio_sin(v->pd) / 3);   /* (MASS: the sine under it) */
            e0 += step;
            y = bio_mul15(bio_mul15(bio_dc(&v->dc, y), e0), a) * 2;
            out[k] += y;
            if (bio_abs(y) > pk) pk = bio_abs(y);
        }
        break;
    }
    default: break;
    }
    v->peak = pk;
}
#endif
