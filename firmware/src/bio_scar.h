/* SPDX-License-Identifier: GPL-3.0-only */
/* BIOFORGE's SCAR (D29): the master's last 0.37 s, kept, and played back while a BURST key is held (ARP held, the
 * white keys 5 .. 8). It records what the engine sends out (after the limiter: it never clips), always; a burst takes
 * the output's place (a crossfade in and out) while the music goes on underneath, in time for when the key is let go.
 *   REPEAT 1/16, 1/8  the 16th (8th) nearest on the grid, again and again, each time a little darker
 *   REVERSE           from the next 8th on: the 8th just played, backwards, again (its hit swells in at the end)
 *   SMEAR             the buffer frozen, two grains of 186 ms crossing at random places in it: the music smeared
 *   SCREW             (D35) from the next 8th on: the 8th just played at half its speed, an octave down, again
 *   DROP              (D35) nothing: a cut, the music on underneath (RUPTURE's, or a key's)
 * And WARP (D35): the output through a short tape whose speed wavers (a slow wow, a flutter, a drift), before it is
 * kept: everything a little out of true.
 * A loop's seam crossfades over 6 ms with what lies on the other side of it, so it never clicks. Stopped, the grid is
 * the press itself. */
#ifndef BIO_SCAR_H
#define BIO_SCAR_H
#include "bio_dsp.h"

#define BIO_SCAR_N 16384u                       /* 0.37 s a side (a power of two: the index wraps by a mask) */
#define BIO_SCAR_X 256u                         /* a seam's crossfade, and the burst's in and out: 5.8 ms */
#define BIO_SCAR_G 8192u                        /* SMEAR's grains: 186 ms (a power of two) */
#define BIO_SCAR_CAP (BIO_SCAR_N - 2u * BIO_SCAR_X - 2u * BIO_SB)   /* the longest loop the buffer keeps whole */
enum { SCAR_OFF, SCAR_REP16, SCAR_REP8, SCAR_REV, SCAR_SMEAR, SCAR_SCREW, SCAR_DROP, BIO_NSCAR };
static const char *const BIO_SCAR_NAME[BIO_NSCAR] = {"", "REPEAT 1/16", "REPEAT 1/8", "REVERSE", "SMEAR", "SCREW", "DROP"};
#define BIO_WARP_N 1024u                        /* WARP's tape: 23 ms a side */

typedef struct {
    int16_t l[BIO_SCAR_N], r[BIO_SCAR_N];
    uint32_t now;                               /* frames, absolute (in the buffer: now & (N - 1)) */
    uint32_t since;                             /* frames recorded without a gap (up to N) */
    uint32_t grid[3];                           /* the frames the last 16th, 8th and quarter began on (the sequencer) */
    volatile uint32_t want;                     /* the burst held (the main loop writes it) */
    uint32_t want_auto;                         /* RUPTURE's (the sequencer's): a key held comes first */
    uint32_t mode, armed;                       /* what plays; REVERSE waiting for its 8th */
    uint32_t keep;                              /* the oldest frame the burst needs: the recording stops short of it */
    uint32_t a, len, t, pass;                   /* a loop: its first frame, its length, where in it, how many times */
    int32_t dir, mix, step, lvl;                /* forward or back; the burst's share of the output (Q15), its ramp */
    bio_op_t lp_l, lp_r;                        /* REPEAT: darker each time round */
    uint32_t g_at[2], g_t, rnd;                 /* SMEAR: the grains' places, their time */
    int16_t wl[BIO_WARP_N], wr[BIO_WARP_N];     /* WARP: its tape */
    uint32_t w_wow, w_flut, w_t;                /* its phases; the drift's clock */
    int32_t w_mix, w_d, w_drift, w_aim;         /* its share (Q15), the delay last sub-block (frames Q8), the drift */
} bio_scar_t;

/* (once a sub-block, before the frames) the sequencer's grid: bit j, the j-th division began on this sub-block */
static inline void bio_scar_grid(bio_scar_t *s, uint32_t bits)
{
    uint32_t j;
    for (j = 0; j < 3u; j++)
        if (bits >> j & 1u) s->grid[j] = s->now;
}

static uint32_t bio_scar_div(uint32_t spb_q8, uint32_t ticks)   /* a division (ticks of 384 a beat) in frames */
{
    uint32_t L = (spb_q8 >> 8) * ticks / 384u;
    return L > BIO_SCAR_CAP ? BIO_SCAR_CAP : L < 4u * BIO_SCAR_X ? 4u * BIO_SCAR_X : L;
}

static void bio_scar_start(bio_scar_t *s, uint32_t m, uint32_t spb_q8, uint32_t playing)
{
    s->mode = m;
    s->t = s->pass = 0;
    s->lvl = 32767;
    s->armed = 0;
    s->step = (int32_t)(32767u / BIO_SCAR_X);
    s->lp_l.s = s->lp_r.s = 0;
    if (m == SCAR_REP16 || m == SCAR_REP8) {    /* from the last 16th (8th) on: still the live sound until it loops */
        uint32_t j = m == SCAR_REP16 ? 0u : 1u, L = bio_scar_div(spb_q8, 96u << j), t0;
        t0 = playing ? s->now - s->grid[j] : 0u;
        if (t0 >= L) t0 %= L;
        s->len = L;
        s->a = s->now - t0;
        s->t = t0;
        s->dir = 1;
        s->keep = s->a - BIO_SCAR_X;
        bio_op_set(&s->lp_l, bio_lf_hz(18000));
        s->lp_r.g = s->lp_l.g;
    } else if (m == SCAR_SCREW) {               /* the 8th just played, at half speed: on the grid (the next 8th) */
        uint32_t L = bio_scar_div(spb_q8, 192u);
        if (L > s->since) L = s->since < 4u * BIO_SCAR_X ? 4u * BIO_SCAR_X : s->since;
        s->len = L;
        s->a = s->now - L;
        s->dir = 1;
        s->keep = s->a - BIO_SCAR_X;
        bio_op_set(&s->lp_l, bio_lf_hz(5000));   /* (darker: the tape slowed) */
        s->lp_r.g = s->lp_l.g;
    } else if (m == SCAR_DROP) {                /* a cut */
        s->keep = s->now;
    } else if (m == SCAR_REV) {                 /* the 8th just played, backwards: on the grid (the next 8th) */
        uint32_t L = bio_scar_div(spb_q8, 192u);
        if (L > s->since) L = s->since < 4u * BIO_SCAR_X ? 4u * BIO_SCAR_X : s->since;
        s->len = L;
        s->a = s->now - 1u;
        s->dir = -1;
        s->keep = s->now - L;
    } else {                                    /* SMEAR: what the buffer holds, frozen */
        uint32_t S = s->since > BIO_SCAR_CAP ? BIO_SCAR_CAP : s->since;
        if (S < BIO_SCAR_G + BIO_SCAR_X) S = BIO_SCAR_G + BIO_SCAR_X;
        s->len = S;
        s->keep = s->now - S;
        s->g_t = 0;
        s->rnd = bio_hash(s->now, 0x5CA2u);
        s->g_at[0] = s->keep + bio_rndr(&s->rnd, S - BIO_SCAR_G);
        s->g_at[1] = s->keep + bio_rndr(&s->rnd, S - BIO_SCAR_G);
        s->step = (int32_t)(32767u / 2048u);     /* (in and out over 46 ms: a cloud, not a cut) */
    }
}

/* n frames (a sub-block) of the master, l[] and r[] in place: recorded, and a burst over them */
static void bio_scar_run(bio_scar_t *s, int32_t *l, int32_t *r, uint32_t n, uint32_t spb_q8, uint32_t playing, uint32_t warp)
{
    const uint32_t M = BIO_SCAR_N - 1u;
    uint32_t k, w = s->want ? s->want : s->want_auto;
    if (w >= BIO_NSCAR) w = 0;
    if (warp || s->w_mix) {                     /* WARP: the output through a short tape, its speed wavering */
        const uint32_t WM = BIO_WARP_N - 1u;
        int32_t d0 = s->w_d, d1, dd, wv = (int32_t)(warp > 100u ? 100u : warp);
        uint32_t t = s->now;
        s->w_wow += 64000u * n;                 /* (0.66 Hz) */
        s->w_flut += 64000u * n * 11u;          /* (7.2 Hz) */
        if (++s->w_t >= 400u) { s->w_t = 0; s->w_aim = bio_noise(&s->rnd); }   /* (a new drift each 0.3 s) */
        s->w_drift += (s->w_aim - s->w_drift) >> 7;
        d1 = (450 << 8) + bio_mul15(bio_sin(s->w_wow), wv * 678) + bio_mul15(bio_sin(s->w_flut), wv * 31) +
             bio_mul15(s->w_drift, wv * 305);   /* (frames Q8: 10 ms; at 100 the wow 6 ms either way, the flutter 0.3, the
                                                   drift 2.7: up to about 35 + 20 cents) */
        if (!d0) d0 = d1;
        dd = (d1 - d0) / (int32_t)n;
        for (k = 0; k < n; k++) {
            int32_t di, fr, al, ar, bl, br, xl, xr;
            uint32_t i;
            s->wl[(t + k) & WM] = (int16_t)l[k];
            s->wr[(t + k) & WM] = (int16_t)r[k];
            d0 += dd;
            di = d0 >> 8; fr = d0 & 255;
            i = t + k - (uint32_t)di;
            al = s->wl[i & WM]; ar = s->wr[i & WM]; bl = s->wl[(i - 1u) & WM]; br = s->wr[(i - 1u) & WM];
            xl = al + (((bl - al) * fr) >> 8);
            xr = ar + (((br - ar) * fr) >> 8);
            s->w_mix += warp ? (s->w_mix < 32767 ? 16 : 0) : -16;   /* (in and out over 46 ms) */
            s->w_mix = bio_clamp(s->w_mix, 0, 32767);
            l[k] += ((xl - l[k]) * s->w_mix) >> 15;
            r[k] += ((xr - r[k]) * s->w_mix) >> 15;
        }
        s->w_d = d1;
    }
    if (w != s->mode || s->armed) {             /* another burst: out of the old one first */
        if (s->mix <= 0) {
            s->mix = 0;
            s->mode = w;
            s->armed = 0;
            if (w != SCAR_OFF) {                /* on the nearest point of its grid: REPEAT pressed a little early
                                                   waits (dry) for the beat it meant; REVERSE for the next 8th */
                uint32_t j = w == SCAR_REP16 ? 0u : w == SCAR_SMEAR || w == SCAR_DROP ? 3u : 1u;
                if (playing && j < 3u && s->grid[j] != s->now) {
                    uint32_t L = bio_scar_div(spb_q8, 96u << j), t0 = s->now - s->grid[j];
                    if (w == SCAR_REV || w == SCAR_SCREW || (t0 >= L / 2u && t0 < L)) s->armed = 1;
                }
                if (!s->armed) bio_scar_start(s, w, spb_q8, playing);
            }
        }
    }
    /* the recording, unless the burst needs what it would write over (a sub-block at a time: CAP leaves room) */
    if (s->mode == SCAR_OFF || s->armed || s->now + n - s->keep < BIO_SCAR_N - 1u) {
        for (k = 0; k < n; k++) {
            s->l[(s->now + k) & M] = (int16_t)l[k];
            s->r[(s->now + k) & M] = (int16_t)r[k];
        }
        s->since = s->since + n > BIO_SCAR_N ? BIO_SCAR_N : s->since + n;
    } else {
        s->since = 0;                           /* (a gap: what is older is another time's) */
    }
    if (s->mode == SCAR_OFF || s->armed) { s->now += n; return; }
    {
        int32_t wl[BIO_SB], wr[BIO_SB];
        if (s->mode == SCAR_SMEAR) {            /* two grains, a sine's half each, crossing: their powers add to one
                                                   (the windows from the table once a sub-block, a line between) */
            uint32_t t = s->g_t, t1 = (t + BIO_SCAR_G / 2u) & (BIO_SCAR_G - 1u), p0 = s->g_at[0] + t, p1 = s->g_at[1] + t1;
            int32_t w0 = bio_sin(t << 18), w1 = bio_sin(t1 << 18);
            int32_t d0 = (bio_sin((t + n) << 18) - w0) / (int32_t)n, d1 = (bio_sin((t1 + n) << 18) - w1) / (int32_t)n;
            for (k = 0; k < n; k++) {
                uint32_t i0 = (p0 + k) & M, i1 = (p1 + k) & M;
                wl[k] = bio_sat((s->l[i0] * w0 + s->l[i1] * w1) >> 15);   /* (two alike: up to +3 dB) */
                wr[k] = bio_sat((s->r[i0] * w0 + s->r[i1] * w1) >> 15);
                w0 += d0;
                w1 += d1;
            }
            s->g_t = (t + n) & (BIO_SCAR_G - 1u);   /* (a grain begins on a sub-block: G is a multiple of it) */
            if (s->g_t == 0u) s->g_at[0] = s->keep + bio_rndr(&s->rnd, s->len - BIO_SCAR_G);
            if (s->g_t == BIO_SCAR_G / 2u) s->g_at[1] = s->keep + bio_rndr(&s->rnd, s->len - BIO_SCAR_G);
        } else if (s->mode == SCAR_DROP) {      /* a cut */
            for (k = 0; k < n; k++) wl[k] = wr[k] = 0;
        } else if (s->mode == SCAR_SCREW) {     /* half speed: a frame of it each two, between them their mean */
            for (k = 0; k < n; k++) {
                uint32_t q = s->t >> 1, pos = s->a + q, h = s->t & 1u;
                int32_t x = s->l[pos & M], y = s->r[pos & M];
                if (h) { x = (x + s->l[(pos + 1u) & M]) >> 1; y = (y + s->r[(pos + 1u) & M]) >> 1; }
                if (q + BIO_SCAR_X >= s->len) {
                    uint32_t p2 = (pos - s->len) & M;
                    int32_t f = (int32_t)(q + BIO_SCAR_X - s->len) << 7;
                    x += ((s->l[p2] - x) * f) >> 15;
                    y += ((s->r[p2] - y) * f) >> 15;
                }
                if (++s->t >= 2u * s->len) s->t = 0;
                wl[k] = bio_op_lp(&s->lp_l, x);
                wr[k] = bio_op_lp(&s->lp_r, y);
            }
        } else {                                /* a loop, forward or back; near its seam, the other side fades in */
            for (k = 0; k < n; k++) {
                uint32_t q = s->t, pos = s->a + (uint32_t)(s->dir * (int32_t)q);
                int32_t x = s->l[pos & M], y = s->r[pos & M];
                if (q + BIO_SCAR_X >= s->len) {
                    uint32_t p2 = (pos - (uint32_t)(s->dir * (int32_t)s->len)) & M;
                    int32_t f = (int32_t)(q + BIO_SCAR_X - s->len) << 7;   /* (0 .. 32767 over the crossfade) */
                    x += ((s->l[p2] - x) * f) >> 15;
                    y += ((s->r[p2] - y) * f) >> 15;
                }
                if (++s->t >= s->len) {
                    s->t = 0;
                    if (s->dir > 0 && s->pass < 12u) {   /* REPEAT: half an octave darker, 0.3 dB down, each time round */
                        int32_t lf = bio_lf_hz(18000) - (int32_t)(s->pass + 1u) * 32768;
                        s->pass++;
                        bio_op_set(&s->lp_l, lf < bio_lf_hz(1400) ? bio_lf_hz(1400) : lf);
                        s->lp_r.g = s->lp_l.g;
                        s->lvl = s->lvl > 16384 ? bio_mul15(s->lvl, 31656) : 16384;
                    }
                }
                wl[k] = x;
                wr[k] = y;
            }
            if (s->dir > 0)
                for (k = 0; k < n; k++) {
                    wl[k] = bio_mul15(bio_op_lp(&s->lp_l, wl[k]), s->lvl);
                    wr[k] = bio_mul15(bio_op_lp(&s->lp_r, wr[k]), s->lvl);
                }
        }
        if (w == s->mode && s->mix >= 32767) {  /* all of it: the burst alone */
            for (k = 0; k < n; k++) { l[k] = wl[k]; r[k] = wr[k]; }
        } else {                                /* in or out: a ramp */
            int32_t st = w == s->mode ? s->step : -s->step;
            for (k = 0; k < n; k++) {
                s->mix = bio_clamp(s->mix + st, 0, 32767);
                l[k] += ((wl[k] - l[k]) * s->mix) >> 15;
                r[k] += ((wr[k] - r[k]) * s->mix) >> 15;
            }
        }
    }
    s->now += n;
}
#endif
