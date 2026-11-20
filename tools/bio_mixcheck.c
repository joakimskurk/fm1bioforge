/* SPDX-License-Identifier: GPL-3.0-only */
/* BIOFORGE's mix, measured (the host; no platform): for a factory ritual, where the engine runs out of room (the
 * organs' outputs, the delay's and the reverb's 16-bit lines, the rumble: how often each clipped and its peak), how hard
 * the master's limiter works, how loud it is, how wet, and each channel's loudness in it (rendered alone).
 *   cc -O2 -DBIO_STATS -Ifirmware/src -Ifirmware/gen -o build/bio_mixcheck tools/bio_mixcheck.c -lm
 *   build/bio_mixcheck N [BARS]       ritual N (bio_rituals.h); all of them without N
 *   build/bio_mixcheck check          every ritual, 8 bars: no organ past the mix's room (twice full scale: D41), the
 *                                     limiter under 1 dB on average (tests/run_bio.sh) */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "bio_rituals.h"

static bio_t eng;
static seq_t sq;
static seq_scene_t sc[SEQ_NSCENE];
static seq_undo_t u[SEQ_NUNDO], r[SEQ_NREDO];
static const char *const ORG[9] = {"HEART", "BONE", "TEETH", "CLAW", "BASS", "DRONE", "SPASM", "VOICE", "NOISE"};
static const char *const BS_NAME[BS_N] = {"organ", "side", "delay", "rev pre", "rev diff", "rev line ap", "rev FDN", "rumble"};

typedef struct { double rms, peak, gr_mean, gr_max, gr_1db, gr_3db; } res_t;

/* render the ritual; mute: channels muted (a mask); dry: the delay's and the reverb's returns off */
static res_t render(uint32_t rit, uint32_t bars, uint32_t mute, int dry)
{
    static int32_t buf[512];
    res_t o = {0, 0, 0, 0, 0, 0};
    double s2 = 0, n = 0, gsum = 0;
    uint32_t b, k, blocks, nb = 0;
    memset(&eng, 0, sizeof eng);
    memset(&sq, 0, sizeof sq);
    bio_init(&eng);
    seq_scene_awaken(&sc[0]);
    for (k = 1; k < SEQ_NSCENE; k++) seq_scene_blank(&sc[k], k);
    BIO_RITUAL[rit](&sc[0]);
    for (k = 0; k < BIO_NCH; k++) if (mute >> k & 1u) sc[0].mix.mute[k] = 1;
    if (dry) { sc[0].fx.d_mix = 0; sc[0].fx.r_mix = 0; }
    seq_attach(&sq, &eng, sc, u, r);
    seq_play(&sq);
    blocks = (uint32_t)((double)bars * 4.0 * 60.0 / (sc[0].bpm10 / 10.0) * 44100.0 / 256.0);
    for (b = 0; b < blocks; b++) {
        memset(buf, 0, sizeof buf);
        bio_render(&eng, buf, 256);
        if (b < blocks / 8u) continue;          /* (the start: the reverb filling) */
        for (k = 0; k < 512u; k++) {
            double v = buf[k] / 32768.0;
            s2 += v * v;
            if (fabs(v) > o.peak) o.peak = fabs(v);
        }
        n += 512;
        {
            double gr = -20.0 * log10((eng.lim_g > 0 ? eng.lim_g : 1) / 32767.0);
            gsum += gr;
            if (gr > o.gr_max) o.gr_max = gr;
            if (gr > 1.0) o.gr_1db++;
            if (gr > 3.0) o.gr_3db++;
            nb++;
        }
    }
    o.rms = 10.0 * log10(s2 / (n ? n : 1) + 1e-12);
    o.peak = 20.0 * log10(o.peak + 1e-9);
    o.gr_mean = gsum / (nb ? nb : 1);
    o.gr_1db = 100.0 * o.gr_1db / (nb ? nb : 1);
    o.gr_3db = 100.0 * o.gr_3db / (nb ? nb : 1);
    return o;
}

static void check(uint32_t rit, uint32_t bars)
{
    static const char *const CH[BIO_NCH] = {"DRUM", "BASS", "DRONE", "SPASM", "VOICE", "NOISE"};
    res_t full, dry;
    uint32_t i, c;
    memset(bio_st_n, 0, sizeof bio_st_n);
    memset(bio_st_over, 0, sizeof bio_st_over);
    memset(bio_st_pk, 0, sizeof bio_st_pk);
    full = render(rit, bars, 0, 0);
    printf("== %s: RMS %.1f dBFS, peak %.1f; the limiter %.1f dB on average, %.1f at most, over 1 dB %.0f %% of the "
           "time, over 3 dB %.0f %%\n", BIO_RITUAL_NAME[rit], full.rms, full.peak, full.gr_mean, full.gr_max, full.gr_1db,
           full.gr_3db);
    printf("   clipped:");
    for (i = 0; i < BS_N; i++)
        if (bio_st_n[i])
            printf(" %s %.3f %% (peak %.1f dBFS)", BS_NAME[i], 100.0 * bio_st_over[i] / bio_st_n[i],
                   20.0 * log10(bio_st_pk[i] / 32767.0 + 1e-9));
    printf("\n   organs over full scale:");
    for (i = 0; i < 9u; i++)
        if (bio_st_over[BS_N + i])
            printf(" %s %.3f %% (peak %+.1f dBFS)", ORG[i], 100.0 * bio_st_over[BS_N + i] / bio_st_n[BS_N + i],
                   20.0 * log10(bio_st_pk[BS_N + i] / 32767.0 + 1e-9));
    printf("\n");
    dry = render(rit, bars, 0, 1);
    printf("   wet: the returns add %.1f dB (dry %.1f dBFS)\n", full.rms - dry.rms, dry.rms);
    printf("   each channel alone:");
    for (c = 0; c < BIO_NCH; c++) {
        res_t a;
        if (!sc[0].mix.lvl[c] && c) continue;
        a = render(rit, bars, 0x3Fu & ~(1u << c), 0);
        if (a.rms < -80.0) continue;
        printf(" %s %.1f", CH[c], a.rms - full.rms);
    }
    printf(" (dB against the whole)\n");
}

int main(int argc, char **argv)
{
    uint32_t bars = argc > 2 ? (uint32_t)atoi(argv[2]) : 8u, i, bad = 0;
    if (argc > 1 && !strcmp(argv[1], "check")) {
        for (i = 0; i + 1u < BIO_NRITUAL; i++) {
            res_t a;
            memset(bio_st_n, 0, sizeof bio_st_n);
            memset(bio_st_over, 0, sizeof bio_st_over);
            a = render(i, 8, 0, 0);
            if (bio_st_over[BS_ORG] || a.gr_mean > 1.0) {
                printf("FAIL %s: %u samples past the mix's room, the limiter %.1f dB on average\n", BIO_RITUAL_NAME[i],
                       (unsigned)bio_st_over[BS_ORG], a.gr_mean);
                bad++;
            }
        }
        printf("%s: %u rituals, no organ past the mix's room, the limiter under 1 dB on average\n", bad ? "FAIL" : "ok",
               (unsigned)(BIO_NRITUAL - 1u));
        return bad != 0;
    }
    if (argc > 1) check((uint32_t)atoi(argv[1]) % BIO_NRITUAL, bars);
    else for (i = 0; i + 1u < BIO_NRITUAL; i++) check(i, bars);
    return 0;
}
