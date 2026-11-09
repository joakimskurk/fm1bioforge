/* SPDX-License-Identifier: GPL-3.0-only */
/* BIOFORGE's drawing: the background (tools/gen_art.py) under everything, sprites with their alpha, the 5 x 7 font,
 * FONT_S blended over the picture, the eye knobs' green arcs. Everything is drawn into the canvas and blitted; the
 * canvas knows where on the screen it sits (cvx, cvy) so that it can start from the background there. Included by
 * bioforge.c after platform.c (gfx.c's canvas). */
#ifndef BIO_UI_H
#define BIO_UI_H
#include "bio_art.h"

#define COL_GREEN RGB(140, 255, 42)
#define COL_GREEN_D RGB(56, 110, 24)
#define COL_RED RGB(230, 40, 52)
#define COL_RED_D RGB(110, 22, 30)
#define COL_BONE RGB(214, 200, 176)
#define COL_BONE_D RGB(140, 120, 110)
#define COL_DIM RGB(84, 62, 60)
#define COL_FLESH RGB(150, 70, 80)

static int32_t cvx, cvy;                        /* where the canvas sits on the screen */

static inline uint32_t unsw(uint32_t c) { return swap16(c); }
static inline uint16_t mix565(uint32_t a, uint32_t b, uint32_t t)   /* a to b by t / 256 */
{
    uint32_t r = ((a >> 11) * (256u - t) + (b >> 11) * t) >> 8;
    uint32_t g = (((a >> 5) & 63u) * (256u - t) + ((b >> 5) & 63u) * t) >> 8;
    uint32_t bl = ((a & 31u) * (256u - t) + (b & 31u) * t) >> 8;
    return (uint16_t)(r << 11 | g << 5 | bl);
}
static inline uint16_t bg_px(int32_t x, int32_t y)
{
    if ((uint32_t)x >= 240u || (uint32_t)y >= 240u) return 0;
    return BG_PAL[BG_IDX[(uint32_t)y * 240u + (uint32_t)x]];
}
/* a canvas of w x h at the screen's (x, y), the background in it */
static void cv_at(int32_t x, int32_t y, uint32_t w, uint32_t h)
{
    uint32_t i, j;
    cv_begin(w, h, 0);
    cvx = x;
    cvy = y;
    for (j = 0; j < cv_h; j++) {
        const uint8_t *row = &BG_IDX[(uint32_t)(y + (int32_t)j) * 240u + (uint32_t)x];
        uint16_t *d = &cv_px[j * cv_w];
        for (i = 0; i < cv_w; i++) d[i] = swap16(BG_PAL[row[i]]);
    }
}
static void cv_out(void) { cv_blit((uint32_t)cvx, (uint32_t)cvy); }
static inline void cv_blend(int32_t x, int32_t y, uint16_t c, uint32_t t)   /* t / 256 of c over the pixel */
{
    if ((uint32_t)x < cv_w && (uint32_t)y < cv_h) {
        uint16_t *d = &cv_px[(uint32_t)y * cv_w + (uint32_t)x];
        *d = swap16(mix565(unsw(*d), c, t));
    }
}
static void cv_fillb(int32_t x, int32_t y, int32_t w, int32_t h, uint16_t c, uint32_t t)   /* a blended rectangle */
{
    int32_t i, j;
    for (j = 0; j < h; j++) for (i = 0; i < w; i++) cv_blend(x + i, y + j, c, t);
}
static void cv_frame(int32_t x, int32_t y, int32_t w, int32_t h, uint16_t c)
{
    cv_rect(x, y, w, 1, c); cv_rect(x, y + h - 1, w, 1, c);
    cv_rect(x, y, 1, h, c); cv_rect(x + w - 1, y, 1, h, c);
}
/* a sprite with its alpha (0..15), times fade / 16 */
static void cv_spr(int32_t x, int32_t y, const uint16_t *c, const uint8_t *a, uint32_t w, uint32_t h, uint32_t fade)
{
    uint32_t i, j, k = 0;
    for (j = 0; j < h; j++)
        for (i = 0; i < w; i++, k++) {
            uint32_t al = (k & 1u) ? (a[k >> 1] & 15u) : (a[k >> 1] >> 4);
            if (al) cv_blend(x + (int32_t)i, y + (int32_t)j, c[k], al * fade);
        }
}
#define SPR(x, y, N, fade) cv_spr((x), (y), N, N##_A, N##_W, N##_H, (fade))

/* the 5 x 7 font: s at (x, y), scale 1 or 2; returns the x after it */
static int32_t t5w(const char *s, int32_t scale) { int32_t n = 0; while (*s++) n++; return n ? (n * 6 - 1) * scale : 0; }
static int32_t cv_t5(int32_t x, int32_t y, const char *s, uint16_t c, int32_t scale)
{
    for (; *s; s++) {
        uint32_t ch = (uint8_t)*s, gy, gx;
        if (ch >= 'a' && ch <= 'z') ch -= 32u;
        if (ch < 32u || ch > 95u) ch = '?';
        for (gy = 0; gy < 7u; gy++) {
            uint32_t row = F5[ch - 32u][gy];
            for (gx = 0; gx < 5u; gx++)
                if (row & (16u >> gx)) {
                    if (scale == 1) cv_pset(x + (int32_t)gx, y + (int32_t)gy, c);
                    else cv_rect(x + (int32_t)gx * scale, y + (int32_t)gy * scale, scale, scale, c);
                }
        }
        x += 6 * scale;
    }
    return x;
}
static int32_t cv_t5c(int32_t cx, int32_t y, const char *s, uint16_t c, int32_t scale)   /* centred on cx */
{
    return cv_t5(cx - t5w(s, scale) / 2, y, s, c, scale);
}
/* FONT_S (16 px) blended over the picture */
static int32_t cv_ts(int32_t x, int32_t y, const felucca_font_t *f, const char *s, uint16_t c)
{
    for (; *s; s++) {
        uint32_t gi = glyph(f, (uint8_t)*s), gx, gy, w = f->bw[gi], bpr = (w + 1u) / 2u;
        const uint8_t *gd = f->data + f->off[gi];
        for (gy = 0; gy < f->h; gy++)
            for (gx = 0; gx < w; gx++) {
                uint32_t v = gd[gy * bpr + gx / 2u];
                v = (gx & 1u) ? (v & 15u) : (v >> 4);
                if (v) cv_blend(x - f->pad + (int32_t)gx, y + (int32_t)gy, c, v * 17u);
            }
        x += f->adv[gi];
    }
    return x;
}
/* an arc about (cx, cy), radius r, from 225 degrees clockwise over 270 * v / 100 (the knob's value) */
static void cv_arc(int32_t cx, int32_t cy, int32_t r, uint32_t v, uint16_t on, uint16_t off)
{
    uint32_t k, n = 54u, step = 0xC0000000u / 54u;
    for (k = 0; k <= n; k++) {
        uint32_t ph = 0xA0000000u + k * step;   /* (225 degrees on, three quarters of a turn) */
        int32_t s = bio_sin(ph), c = bio_sin(ph + 0x40000000u);
        int32_t x = cx + (int32_t)((s * r) >> 15), y = cy - (int32_t)((c * r) >> 15);
        uint16_t col = k * 100u <= v * n ? on : off;
        cv_pset(x, y, col);
        cv_pset(x, y + 1, col);
    }
}

/* small numbers to text */
static char *u2s(char *p, uint32_t v)
{
    char d[10];
    uint32_t n = 0;
    do { d[n++] = (char)('0' + v % 10u); v /= 10u; } while (v && n < 10u);
    while (n) *p++ = d[--n];
    *p = 0;
    return p;
}
static char *i2s(char *p, int32_t v) { if (v < 0) { *p++ = '-'; v = -v; } return u2s(p, (uint32_t)v); }
static char *str_cat(char *p, const char *s) { while (*s) *p++ = *s++; *p = 0; return p; }
#endif
