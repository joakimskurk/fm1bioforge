/* SPDX-License-Identifier: GPL-3.0-only */
/* BIOFORGE's nervous system: a 16-bit shift register clocked from the transport, after the documented behaviour of the
 * Music Thing Modular Turing Machine (Tom Whitwell; no code of it is used). On each clock: the bit at LENGTH comes
 * round, flipped with the chance 1 - LOOP, and goes in at the bottom. LOOP 100 % (and 97..100): locked, a period of
 * LENGTH; 0 % (and 0..3): always flipped, the pattern and its inverse, a period of 2 x LENGTH; 50 %: random. Nothing
 * random happens while it is locked. Its uses: CV8 (the low byte) onto texture, bands and air (ROT and the routes);
 * gates from its bits (RATTLE, ghost hits); notes from a pool (WAIL). */
#ifndef BIO_TURING_H
#define BIO_TURING_H
#include "bio_dsp.h"

static const uint8_t BIO_TUR_LEN[8] = {2, 3, 4, 5, 6, 8, 12, 16};
static const uint16_t BIO_TUR_DIV[4] = {48, 96, 192, 384};   /* its clock in ticks (96 a 16th): 1/32 1/16 1/8 1/4 */
static const char *const BIO_TUR_CLK_NAME[4] = {"1/32", "1/16", "1/8", "1/4"};
/* the scales (D31): bit i, the note i semitones over the root, in it. The scene's KEY (its root, D at 0) and SCALE are
   kept in its register's settings (`key`: the root over D in bits 0..3, the scale in 4..6; 0 is D PHRYGIAN, the scenes'
   own before there was a choice) */
#define BIO_NSCALE 8u
static const uint16_t BIO_SCALE_MASK[BIO_NSCALE] = {
    1u << 0 | 1u << 1 | 1u << 3 | 1u << 5 | 1u << 7 | 1u << 8 | 1u << 10,    /* PHRYGIAN */
    1u << 0 | 1u << 2 | 1u << 3 | 1u << 5 | 1u << 7 | 1u << 8 | 1u << 10,    /* AEOLIAN (natural minor) */
    1u << 0 | 1u << 1 | 1u << 3 | 1u << 5 | 1u << 6 | 1u << 8 | 1u << 10,    /* LOCRIAN */
    1u << 0 | 1u << 2 | 1u << 3 | 1u << 5 | 1u << 7 | 1u << 8 | 1u << 11,    /* HARMONIC MINOR */
    1u << 0 | 1u << 1 | 1u << 4 | 1u << 5 | 1u << 7 | 1u << 8 | 1u << 10,    /* PHRYGIAN DOMINANT */
    1u << 0 | 1u << 2 | 1u << 4 | 1u << 6 | 1u << 8 | 1u << 10,              /* WHOLE TONE */
    1u << 0 | 1u << 3 | 1u << 5 | 1u << 7 | 1u << 10,                        /* MINOR PENTATONIC */
    0xFFFu,                                                                  /* CHROMATIC */
};
static const char *const BIO_SCALE_NAME[BIO_NSCALE] = {"PHRYGIAN", "AEOLIAN", "LOCRIAN", "HARM MINOR", "PHRYG DOM",
                                                       "WHOLE TONE", "PENTA MIN", "CHROMATIC"};

/* the settings (kept in the scene) */
typedef struct {
    uint16_t seed_r;                            /* the register it starts from (RESET) */
    uint8_t len_i, loop, clock, depth;          /* LENGTH (index), LOOP 0..100, CLOCK (index), TURING DEPTH 0..100 */
    uint8_t r_scrape, r_metal, r_air, r_rot;    /* the routes' depths 0..100 */
    uint8_t rattle, wail, scale, key;           /* RATTLE 0..100 (off at 0), WAIL on SPASM/VOICE (bits 0, 1), its range;
                                                   the KEY and SCALE (D31: the root over D, bits 0..3; the scale 4..6) */
    uint32_t seed_rng;
} bio_tur_set_t;
static const bio_tur_set_t BIO_TUR_INIT = {0xB35Du, 5, 100, 2, 20, 22, 16, 11, 50, 0, 0, 50, 0, 0x6D2B79F5u};

/* the running state */
typedef struct {
    uint16_t r;
    uint32_t rng, clocks;
    int32_t cv;                                 /* CV8, smoothed (Q15, 0..32767) */
    uint8_t write;                              /* (the panel) 1: WRITE 0, 2: WRITE 1 */
    uint8_t invert;                             /* (BURST) INVERT held */
} bio_tur_t;

static void bio_tur_reset(bio_tur_t *t, const bio_tur_set_t *s)
{
    t->r = s->seed_r;
    t->rng = s->seed_rng ? s->seed_rng : 1u;
    t->clocks = 0;
    t->cv = (int32_t)(s->seed_r & 0xFFu) * 128;   /* (its smoothed CV from where it starts) */
}
/* the chance that the bit coming round is flipped (Q16) */
static uint32_t bio_tur_pflip(uint32_t loop)
{
    if (loop <= 3u) return 65536u;              /* DOUBLE LOCK: always */
    if (loop >= 97u) return 0;                  /* LOCK: never */
    return (97u - loop) * 65536u / 94u;         /* between: 1 - LOOP, the ends widened */
}
static void bio_tur_clock(bio_tur_t *t, const bio_tur_set_t *s)
{
    uint32_t len = BIO_TUR_LEN[s->len_i & 7u], tap = ((uint32_t)t->r >> (len - 1u)) & 1u, p = bio_tur_pflip(s->loop), in;
    uint32_t flip = p >= 65536u ? 1u : p == 0u ? 0u : ((bio_rnd(&t->rng) >> 16) < p);
    in = tap ^ flip;
    if (t->write == 1u) in = 0;
    if (t->write == 2u) in = 1;
    t->r = (uint16_t)(((uint32_t)t->r << 1 | in) & 0xFFFFu);
    t->clocks++;
}
static inline uint32_t bio_tur_cv8(const bio_tur_t *t) { return (t->invert ? ~(uint32_t)t->r : t->r) & 0xFFu; }
static inline uint32_t bio_tur_bit(const bio_tur_t *t, uint32_t b) { return ((t->invert ? ~(uint32_t)t->r : t->r) >> b) & 1u; }
/* the routes' effective depth: the route's %, times TURING DEPTH / 20 (so at INIT, DEPTH 20, a route is its own %) */
static inline int32_t bio_tur_eff(const bio_tur_set_t *s, uint32_t route)
{
    return bio_clamp((int32_t)(route * s->depth * 32768u / 2000u), 0, 32767);
}
/* the KEY: its root in semitones over D (0..11), the shift the scene's notes take (-5 .. +6: the nearest way), its scale */
static inline uint32_t bio_key_root(const bio_tur_set_t *s) { uint32_t r = s->key & 15u; return r < 12u ? r : 0u; }
static inline int32_t bio_key_shift(const bio_tur_set_t *s) { int32_t r = (int32_t)bio_key_root(s); return r <= 6 ? r : r - 12; }
static inline uint32_t bio_key_scale(const bio_tur_set_t *s) { return (uint32_t)(s->key >> 4) & 7u; }
static inline uint32_t bio_scale_count(uint32_t m)       /* its notes in an octave */
{
    uint32_t b = BIO_SCALE_MASK[m & 7u], c = 0;
    while (b) { c += b & 1u; b >>= 1; }
    return c;
}
static int32_t bio_scale_note(uint32_t m, uint32_t n)    /* the n-th note of the scale over its root (octaves on up) */
{
    uint32_t b = BIO_SCALE_MASK[m & 7u], c = bio_scale_count(m), i = n % c, j;
    for (j = 0; j < 12u; j++)
        if (b >> j & 1u) { if (!i) break; i--; }
    return (int32_t)(n / c * 12u + j);
}
/* WAIL: a note of the pool from CV8, about a root (the D at or under the organ's own note; the KEY moves it after) */
static int32_t bio_tur_wail(const bio_tur_t *t, const bio_tur_set_t *s, int32_t note)
{
    uint32_t m = bio_key_scale(s), c = bio_scale_count(m);
    uint32_t pool = c + 1u + (uint32_t)s->scale * c / 100u;   /* an octave and a note .. two octaves and a note */
    uint32_t i = bio_tur_cv8(t) * pool / 256u;
    int32_t root = note - ((note - 2 + 120) % 12);
    return root + bio_scale_note(m, i);
}
#endif
