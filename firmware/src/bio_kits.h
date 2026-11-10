/* SPDX-License-Identifier: GPL-3.0-only */
/* BIOFORGE's noise-percussion sounds: settings for the perc voice (bio_perc.h), for the NOISE organ or for a drum
 * organ switched to MODEL NOISE. The order of p is an organ's parameters (bio_organs.h):
 *   BODY TEXTURE MOTION DECAY | SOURCE RESO MODE BURSTS CLICK DRIVE CRUSH MODEL | ATTACK GATE WIDTH RELEASE | TUNE LEVEL PAN -
 * BODY the cutoff (120 Hz .. 2.4 kHz at 50 .. 14 kHz), TEXTURE grain (or CRACKLE's density), MOTION the sweep (over
 * 50 a zap down, under it a rise), SOURCE in eighths (bio_perc.h: WHITE PINK DIGI METAL, the crackles DUST FIRE FRY
 * CREAK; TEXTURE their density), MODE low-pass 0 band-pass 50
 * high-pass 100, BURSTS 1..4 (a clap), MODEL over 50 = NOISE, GATE: held while the key is (0: struck). */
#ifndef BIO_KITS_H
#define BIO_KITS_H
#include "bio_organs.h"

typedef struct { const char *name; uint8_t p[BIO_NP]; } bio_preset_t;

#define PK(b, t, m, d, src, res, mode, bur, clk, drv, cr, a, g, w, r, tn, lvl) \
    {b, t, m, d, src, res, mode, bur, clk, drv, cr, 50, a, g, w, r, tn, lvl, 50, 0, 0, 0, 0, 0}
/* (BODY 38: 1.2 kHz, 55: 2.9 kHz, 70: 4.8 kHz, 85: 8.2 kHz; DECAY 25: 22 ms to -40 dB, 40: 60 ms, 50: 120 ms,
    60: 230 ms, 80: 0.8 s; RESO 20: Q 1.3, 50: Q 5.5, 80: Q 13, 95: Q 19) */
static const bio_preset_t BIO_PERC_KIT[] = {
    /*               BODY TEX MOT DEC  SRC RES MODE BUR CLK DRV CRU  ATT GATE WID REL TUNE LVL */
    {"TICK",       PK(85, 10, 50, 24,  4, 15, 100,  0, 10,  0,  0,   0,   0, 30, 20, 24, 100)},   /* a closed hat */
    {"HISS",       PK(80, 15, 50, 58,  4, 10,  90,  0,  0,  0,  0,   0,   0, 60, 40, 24,  96)},   /* open, wide */
    {"METAL",      PK(74, 30, 50, 28,  32, 30,  75,  0, 10, 20,  0,   0,   0, 30, 20, 24, 100)},   /* a shift-register hat: tonal */
    {"DIGI SNR",   PK(55, 40, 56, 32,  22, 25,  50,  0, 40, 40,  0,   0,   0, 20, 30, 24,  96)},
    {"CLAP",       PK(38, 20, 52, 46,  4, 28,  50, 80, 10, 20,  0,   0,   0, 50, 30, 24, 100)},   /* four strikes, then its tail */
    {"ZAP",        PK(40, 10, 95, 26,  4, 80,  30,  0, 30, 30,  0,   0,   0, 10, 20, 24,  92)},   /* pinging, swept down */
    {"RISE",       PK(45, 20, 10, 88,  13, 50,  50,  0,  0, 10,  0,  60, 100, 70, 60, 24,  86)},   /* opening while held */
    {"CRUSH",      PK(58, 30, 55, 30,  4, 22,  50,  0, 30, 60, 80,   0,   0, 20, 20, 24,  92)},   /* a crushed snare */
    {"CRACKLE",    PK(75, 60, 50, 45,  41, 15,  70,  0,  0, 10,  0,   0,   0, 70, 30, 24, 100)},
    {"GRAIN",      PK(65, 92, 50, 22,  4, 20,  50,  0, 20, 20, 30,   0,   0, 40, 20, 24, 100)},   /* held in steps: digital grain */
    {"PING",       PK(28, 10, 62, 32,  4, 95,  50,  0, 30, 10,  0,   0,   0, 10, 20, 24,  96)},   /* a noise tom: the filter rings */
    {"THUMP",      PK( 0, 10, 84, 28,  13, 85,   0,  0, 70, 70,  0,   0,   0,  0, 20, 12, 100)},   /* a kick of noise: low, swept, driven */
    {"WASH",       PK(55, 20, 50, 80,  13, 20,  50,  0,  0,  0,  0,  30, 100, 90, 70, 24,  90)},   /* gated noise, held */
    {"FLAM SNR",   PK(60, 30, 55, 32,  4, 15,  60, 30, 40, 50,  0,   0,   0, 30, 30, 24,  96)},   /* two strikes: a flam */
    {"BIT HAT",    PK(86, 20, 50, 14,  22, 10, 100,  0, 20,  0, 50,   0,   0, 20, 20, 24, 100)},
    {"LASER",      PK(70, 10, 90, 22,  32, 45,  50,  0, 10, 20,  0,   0,   0, 10, 20, 24,  92)},   /* METAL, swept down */
    /* the organic crackles (D21): held, a bed; struck, a burst of it */
    {"EMBERS",     PK(62, 45, 50, 70,  50, 15,  40,  0,  0, 15,  0,  20, 100, 70, 60, 24, 100)},   /* a fire, held */
    {"SPARKS",     PK(75, 88, 50, 44,  50, 20,  70,  0, 20, 10,  0,   0,   0, 50, 20, 24, 100)},
    {"SIZZLE",     PK(74, 30, 50, 70,  59, 10,  70,  0,  0,  0,  0,  10, 100, 80, 50, 24,  92)},   /* a pan of fat, held */
    {"GREASE",     PK(70, 70, 50, 35,  59, 20,  60,  0,  0, 20,  0,   0,   0, 40, 20, 24, 100)},
    {"CREAK",      PK(40, 40, 50, 75,  68, 70,  50,  0,  0, 20,  0,  10, 100, 30, 40, 24, 100)},   /* wood, held: it creaks, then is still */
    {"HINGE",      PK(30, 70, 50, 45,  68, 80,  50,  0,  0, 10,  0,   0,   0, 20, 30, 24, 100)},
    /* noise that is not white (D25): ALIAS, SIX, BYTE */
    {"ALIAS",      PK(70, 60, 50, 28,  78, 20,  70,  0,  0, 10,  0,   0,   0, 20, 20, 24, 100)},   /* FM folding past Nyquist */
    {"SIX HAT",    PK(80, 30, 50, 22,  87, 15,  95,  0,  0,  0,  0,   0,   0, 20, 20, 24, 100)},   /* six squares: a metal hat */
    {"SIX RING",   PK(76, 90, 50, 55,  87, 20,  80,  0,  0, 10,  0,   0,   0, 30, 30, 24,  96)},   /* their ring: open, harsh */
    {"CIRCUIT",    PK(60, 25, 50, 50,  96, 20,  40,  0,  0, 10,  0,  10, 100, 20, 30, 24,  90)},   /* a byte of a formula, held */
    /* the deep kicks (D22): MODEL KICK, on a drum organ only (bio_kick.h) */
#define KK(b, t, m, d, tone, drv, kn, hold, a, lvl) {b, t, m, d, tone, drv, kn, hold, 0, 0, 0, 100, a, 0, 0, 0, 24, lvl, 50, 0, 0, 0, 0, 0}
    {"DEEP KICK",  KK(40, 45, 30, 55,  30, 35, 10, 20,  5, 100)},   /* 44 Hz, two octaves down in 13 ms, half a second */
    {"SUB KICK",   KK(30, 30, 45, 75,  20, 20,  0, 40,  8, 100)},   /* lower, longer, softer: the floor itself */
    {"DUB KICK",   KK(50, 55, 25, 45,  40, 50, 25, 10,  4, 100)},   /* tighter, warmer, a knock */
};
#define BIO_NPERC_KIT (sizeof BIO_PERC_KIT / sizeof BIO_PERC_KIT[0])
#define BIO_NKICK_KIT 3u                        /* (the last three: a drum organ's only) */

/* into an organ: a drum organ is switched to MODEL NOISE; the NOISE organ keeps its D8 (BREATH) */
static void bio_preset_set(uint8_t *dst, const bio_preset_t *k, uint32_t org)
{
    uint32_t i;
    for (i = 0; i < BIO_NP; i++)
        if (i != P_D8 || org != O_NOISE) dst[i] = k->p[i];
}
static const bio_preset_t *bio_preset_find(const char *name)
{
    uint32_t i;
    for (i = 0; i < BIO_NPERC_KIT; i++) {
        const char *a = BIO_PERC_KIT[i].name, *b = name;
        while (*a && *a == *b) { a++; b++; }
        if (!*a && !*b) return &BIO_PERC_KIT[i];
    }
    return &BIO_PERC_KIT[0];
}
#endif
