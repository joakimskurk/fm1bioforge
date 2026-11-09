/* SPDX-License-Identifier: GPL-3.0-only */
/* BIOFORGE's noise-percussion sounds: settings for the perc voice (bio_perc.h), for the NOISE organ or for a drum
 * organ switched to MODEL NOISE. The order of p is an organ's parameters (bio_organs.h):
 *   BODY TEXTURE MOTION DECAY | SOURCE RESO MODE BURSTS CLICK DRIVE CRUSH MODEL | ATTACK GATE WIDTH RELEASE | TUNE LEVEL PAN -
 * BODY the cutoff (120 Hz .. 2.4 kHz at 50 .. 14 kHz), TEXTURE grain (or CRACKLE's density), MOTION the sweep (over
 * 50 a zap down, under it a rise), SOURCE WHITE PINK DIGI METAL CRACKLE (in fifths), MODE low-pass 0 band-pass 50
 * high-pass 100, BURSTS 1..4 (a clap), MODEL over 50 = NOISE, GATE: held while the key is (0: struck). */
#ifndef BIO_KITS_H
#define BIO_KITS_H
#include "bio_organs.h"

typedef struct { const char *name; uint8_t p[BIO_NP]; } bio_preset_t;

#define PK(b, t, m, d, src, res, mode, bur, clk, drv, cr, a, g, w, r, tn, lvl) \
    {b, t, m, d, src, res, mode, bur, clk, drv, cr, 100, a, g, w, r, tn, lvl, 50, 0, 0, 0, 0, 0}
/* (BODY 38: 1.2 kHz, 55: 2.9 kHz, 70: 4.8 kHz, 85: 8.2 kHz; DECAY 25: 22 ms to -40 dB, 40: 60 ms, 50: 120 ms,
    60: 230 ms, 80: 0.8 s; RESO 20: Q 1.3, 50: Q 5.5, 80: Q 13, 95: Q 19) */
static const bio_preset_t BIO_PERC_KIT[] = {
    /*               BODY TEX MOT DEC  SRC RES MODE BUR CLK DRV CRU  ATT GATE WID REL TUNE LVL */
    {"TICK",       PK(85, 10, 50, 24,  10, 15, 100,  0, 10,  0,  0,   0,   0, 30, 20, 24, 100)},   /* a closed hat */
    {"HISS",       PK(80, 15, 50, 58,  10, 10,  90,  0,  0,  0,  0,   0,   0, 60, 40, 24,  96)},   /* open, wide */
    {"METAL",      PK(74, 30, 50, 28,  70, 30,  75,  0, 10, 20,  0,   0,   0, 30, 20, 24, 100)},   /* a shift-register hat: tonal */
    {"DIGI SNR",   PK(55, 40, 56, 32,  50, 25,  50,  0, 40, 40,  0,   0,   0, 20, 30, 24,  96)},
    {"CLAP",       PK(38, 20, 52, 46,  10, 28,  50, 80, 10, 20,  0,   0,   0, 50, 30, 24, 100)},   /* four strikes, then its tail */
    {"ZAP",        PK(40, 10, 95, 26,  10, 80,  30,  0, 30, 30,  0,   0,   0, 10, 20, 24,  92)},   /* pinging, swept down */
    {"RISE",       PK(45, 20, 10, 88,  30, 50,  50,  0,  0, 10,  0,  60, 100, 70, 60, 24,  86)},   /* opening while held */
    {"CRUSH",      PK(58, 30, 55, 30,  10, 22,  50,  0, 30, 60, 80,   0,   0, 20, 20, 24,  92)},   /* a crushed snare */
    {"CRACKLE",    PK(75, 60, 50, 45,  90, 15,  70,  0,  0, 10,  0,   0,   0, 70, 30, 24, 100)},
    {"GRAIN",      PK(65, 92, 50, 22,  10, 20,  50,  0, 20, 20, 30,   0,   0, 40, 20, 24, 100)},   /* held in steps: digital grain */
    {"PING",       PK(28, 10, 62, 32,  10, 95,  50,  0, 30, 10,  0,   0,   0, 10, 20, 24,  96)},   /* a noise tom: the filter rings */
    {"THUMP",      PK( 0, 10, 84, 28,  30, 85,   0,  0, 70, 70,  0,   0,   0,  0, 20, 12, 100)},   /* a kick of noise: low, swept, driven */
    {"WASH",       PK(55, 20, 50, 80,  30, 20,  50,  0,  0,  0,  0,  30, 100, 90, 70, 24,  90)},   /* gated noise, held */
    {"FLAM SNR",   PK(60, 30, 55, 32,  10, 15,  60, 30, 40, 50,  0,   0,   0, 30, 30, 24,  96)},   /* two strikes: a flam */
    {"BIT HAT",    PK(86, 20, 50, 14,  50, 10, 100,  0, 20,  0, 50,   0,   0, 20, 20, 24, 100)},
    {"LASER",      PK(70, 10, 90, 22,  70, 45,  50,  0, 10, 20,  0,   0,   0, 10, 20, 24,  92)},   /* METAL, swept down */
};
#define BIO_NPERC_KIT (sizeof BIO_PERC_KIT / sizeof BIO_PERC_KIT[0])

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
