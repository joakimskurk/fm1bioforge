/* SPDX-License-Identifier: GPL-3.0-only */
/* BIOFORGE on a computer: the engine and the sequencer as the firmware has them, rendered to WAV (no platform).
 *   cc -O2 -Ifirmware/src -Ifirmware/gen -o build/bio_render tools/bio_render.c
 *   build/bio_render awaken OUT.wav [BARS]          RITUAL 00, THE AWAKENING, from PLAY
 *   build/bio_render organ N OUT.wav [NOTE] [LEN_MS] one hit (or note) of organ N (0 HEART .. 8 NOISE), dry, 2 s
 *   build/bio_render dry OUT.wav [BARS]             the awakening with no effects and no Turing (DRY START)
 *   build/bio_render ritual N OUT.wav [BARS] [SOLO] a factory ritual (bio_rituals.h, BIO_RITUAL_NAME); SOLO: one organ;
 *                         [k=R] [s=M]             or its KEY (R semitones over D) and SCALE (bio_turing.h's M) instead;
 *                         [arp=T:M:R:G:C:Q]       an ARP on track T (mode, rate, range, chord, gate as the knobs show)
 *                         [m=T:L] [a=T:V] [nowail] track T's MODEL a landscape L, its ATTACK V; WAIL off
 *                         [lvl=C:V]               channel C's level
 *                         [warp=V] [rup=V]        WARP (the tape wavering) and RUPTURE (the register's cuts), 0..100
 *   build/bio_render scar N OUT.wav BARS M@B+L ...  ritual N with SCAR bursts (bio_scar.h): mode M (1 REPEAT 1/16,
 *                                                   2 REPEAT 1/8, 3 REVERSE, 4 SMEAR) held from beat B for L beats
 *   build/bio_render palette OUT.wav                the noise kit (bio_kits.h), each sound three times
 *   build/bio_render perf                           instructions: see tests/run_bio.sh (callgrind) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "bio_rituals.h"

static bio_t eng;
static seq_t sq;
static seq_scene_t scenes[SEQ_NSCENE];
static seq_undo_t undo[SEQ_NUNDO], redo[SEQ_NREDO];

static void wav_hdr(FILE *f, uint32_t frames)
{
    uint32_t bytes = frames * 4u, v;
    fwrite("RIFF", 1, 4, f); v = 36u + bytes; fwrite(&v, 4, 1, f);
    fwrite("WAVEfmt ", 1, 8, f); v = 16; fwrite(&v, 4, 1, f);
    {
        uint16_t fmt = 1, ch = 2, bits = 16, align = 4;
        uint32_t rate = 44100, br = 44100 * 4;
        fwrite(&fmt, 2, 1, f); fwrite(&ch, 2, 1, f); fwrite(&rate, 4, 1, f); fwrite(&br, 4, 1, f);
        fwrite(&align, 2, 1, f); fwrite(&bits, 2, 1, f);
    }
    fwrite("data", 1, 4, f); fwrite(&bytes, 4, 1, f);
}

static double dbfs(int32_t peak)                 /* (no libm: from the engine's own log2) */
{
    return peak > 0 ? 6.0206 * ((double)bio_log2((uint32_t)peak) / 65536.0 - 14.99996) : -999.0;
}

static uint32_t sch_n, sch_mode[16], sch_at[16], sch_end[16];   /* SCAR's bursts, in frames */

static int render(const char *path, uint32_t frames)
{
    FILE *f = fopen(path, "wb");
    static int32_t buf[512];
    int16_t pcm[512];
    uint32_t done = 0, k, clip = 0;
    int32_t peak = 0;
    if (!f) { perror(path); return 1; }
    wav_hdr(f, frames);
    while (done < frames) {
        memset(buf, 0, sizeof buf);
        if (sch_n) {                            /* the bursts held now (the last listed wins) */
            uint32_t j, m = 0;
            for (j = 0; j < sch_n; j++) if (done >= sch_at[j] && done < sch_end[j]) m = sch_mode[j];
            eng.scar.want = m;
        }
        bio_render(&eng, buf, 256);
        for (k = 0; k < 512u; k++) {
            int32_t v = buf[k];
            if (v > 32767 || v < -32767) clip++;
            if (v > peak) peak = v;
            if (-v > peak) peak = -v;
            pcm[k] = (int16_t)(v > 32767 ? 32767 : v < -32767 ? -32767 : v);
        }
        fwrite(pcm, 2, 512, f);
        done += 256;
    }
    fclose(f);
    printf("%s: %u frames, peak %d (%.1f dBFS), %u clipped\n", path, frames, peak, dbfs(peak), clip);
    return 0;
}

int main(int argc, char **argv)
{
    uint32_t k;
    if (argc < 3) { fprintf(stderr, "usage: see the top of tools/bio_render.c\n"); return 2; }
    bio_init(&eng);
    seq_scene_awaken(&scenes[0]);
    for (k = 1; k < SEQ_NSCENE; k++) seq_scene_blank(&scenes[k], 0x51u + k);
    seq_attach(&sq, &eng, scenes, undo, redo);
    if (!strcmp(argv[1], "ritual") && argc > 3) {   /* a factory ritual (bio_rituals.h) from PLAY, BARS bars */
        uint32_t r = (uint32_t)atoi(argv[2]), bars = argc > 4 ? (uint32_t)atoi(argv[4]) : 8u;
        if (r >= BIO_NRITUAL) r = 0;
        BIO_RITUAL[r](&scenes[0]);
        for (k = 5; k < (uint32_t)argc; k++) {
            if (!strncmp(argv[k], "k=", 2)) scenes[0].tur.key = (uint8_t)((scenes[0].tur.key & 0xF0u) | ((uint32_t)atoi(argv[k] + 2) % 12u));
            else if (!strncmp(argv[k], "s=", 2)) scenes[0].tur.key = (uint8_t)((scenes[0].tur.key & 0x0Fu) | ((uint32_t)atoi(argv[k] + 2) & 15u) << 4);
            else if (!strncmp(argv[k], "arp=", 4)) {   /* arp=TRK:MODE:RATE:RANGE:CHORD:GATE (as the knobs show them) */
                unsigned t, m, r, g, c, q;
                if (sscanf(argv[k] + 4, "%u:%u:%u:%u:%u:%u", &t, &m, &r, &g, &c, &q) == 6 && t >= 4u && t < BIO_NORG) {
                    seq_track_t *tr = &scenes[0].tr[t];
                    tr->arp = (uint8_t)m;
                    tr->arp_rr = (uint8_t)((r + 4u) % 6u | ((g + 1u) % 3u) << 4);
                    tr->arp_cg = (uint8_t)(c % SEQ_NCHORD | ((q + 4u) % 10u) << 4);
                }
            }
            else if (!strncmp(argv[k], "m=", 2)) {   /* m=TRK:MODEL (a landscape, bio_land.h) */
                unsigned t, m;
                if (sscanf(argv[k] + 2, "%u:%u", &t, &m) == 2 && t >= 4u && t < O_NOISE && m < BIO_NLAND) scenes[0].p[t][P_E1] = BIO_LAND(m);
            }
            else if (!strncmp(argv[k], "a=", 2)) {   /* a=TRK:ATTACK */
                unsigned t, v;
                if (sscanf(argv[k] + 2, "%u:%u", &t, &v) == 2 && t < BIO_NORG) scenes[0].p[t][P_A] = (uint8_t)v;
            }
            else if (!strcmp(argv[k], "nowail")) scenes[0].tur.wail = 0;
            else if (!strncmp(argv[k], "warp=", 5)) scenes[0].mix.warp = (uint8_t)atoi(argv[k] + 5);
            else if (!strncmp(argv[k], "rup=", 4)) scenes[0].tur.rupture = (uint8_t)atoi(argv[k] + 4);
            else if (!strncmp(argv[k], "lvl=", 4)) {   /* lvl=CH:LEVEL (a channel's) */
                unsigned c, v;
                if (sscanf(argv[k] + 4, "%u:%u", &c, &v) == 2 && c < BIO_NCH) scenes[0].mix.lvl[c] = (uint8_t)v;
            }
            else {                              /* one organ alone (its stem, through the effects) */
                uint32_t solo = (uint32_t)atoi(argv[k]), j;
                for (j = 0; j < BIO_NORG; j++) scenes[0].tr[j].mute = j != solo;
            }
        }
        sq.tur_reset = 1;
        seq_play(&sq);
        return render(argv[3], bars * 4u * (uint32_t)(44100u * 60u / (scenes[0].bpm10 / 10u)) + 88200u);
    }
    if (!strcmp(argv[1], "scar") && argc > 4) {   /* a ritual, and SCAR's bursts on it */
        uint32_t r = (uint32_t)atoi(argv[2]), bars = (uint32_t)atoi(argv[4]), spb;
        int i;
        if (r >= BIO_NRITUAL) r = 0;
        BIO_RITUAL[r](&scenes[0]);
        spb = 26460000u / scenes[0].bpm10;
        for (i = 5; i < argc && sch_n < 16u; i++) {
            unsigned m = 0;
            double b = 0, l = 0;
            if (sscanf(argv[i], "%u@%lf+%lf", &m, &b, &l) != 3) { fprintf(stderr, "%s: M@B+L\n", argv[i]); return 2; }
            sch_mode[sch_n] = m; sch_at[sch_n] = (uint32_t)(b * spb); sch_end[sch_n] = (uint32_t)((b + l) * spb);
            printf("%-11s %6.2f s .. %6.2f s\n", BIO_SCAR_NAME[m < BIO_NSCAR ? m : 0], b * spb / 44100.0, (b + l) * spb / 44100.0);
            sch_n++;
        }
        sq.tur_reset = 1;
        seq_play(&sq);
        return render(argv[3], bars * 4u * spb + 88200u);
    }
    if (!strcmp(argv[1], "lands") && argc > 2) {   /* the landscapes (bio_land.h), each 3 s on an organ it suits */
        static const uint8_t ORG[BIO_NLAND] = {0, O_DRONE, O_DRONE, O_BASS, O_DRONE, O_SPASM, O_VOICE, O_BASS, O_VOICE, O_BASS, O_SPASM,
                                               O_DRONE, O_DRONE};
        static const uint8_t PH[4][4] = {{26, 29, 26, 33}, {38, 38, 38, 38}, {62, 67, 69, 74}, {50, 50, 50, 50}};
        static int32_t buf[512];
        int16_t pcm[512];
        uint32_t m, done = 0, frames = (BIO_NLAND - 1u) * 132300u;
        FILE *f = fopen(argv[2], "wb");
        if (!f) { perror(argv[2]); return 1; }
        eng.tick = 0;
        for (k = 0; k < BIO_NCH; k++) { eng.mix.lvl[k] = 100; eng.mix.sb[k] = 16; }
        wav_hdr(f, frames);
        for (m = 1; m < BIO_NLAND; m++) {
            uint32_t o = ORG[m], t0, held = o == O_DRONE || o == O_VOICE, j;
            for (j = 0; j < BIO_NP; j++) eng.p[o][j] = BIO_P_INIT[o][j];
            eng.p[o][P_E1] = BIO_LAND(m);
            if (o == O_DRONE) eng.p[o][P_A] = 40;
            printf("%5.2f s  %s on %u\n", done / 44100.0, BIO_LAND_NAME[m], (unsigned)o);
            for (t0 = 0; t0 < 132300u; t0 += 256u) {
                uint32_t i;
                for (i = 0; i < 4u; i++)
                    if (t0 == (i * 26624u) && (!held || i == 0u))
                        bio_trigger(&eng, o, PH[o - 4u][i], 108, held ? BIO_MS(2400) : BIO_MS(500), 0, 0x77u + m * 4u + i);
                if (held && t0 == 105984u) bio_release(&eng, o, PH[o - 4u][0]);
                if (!held && (t0 % 26624u) == 22528u) bio_release(&eng, o, 255u);
                memset(buf, 0, sizeof buf);
                bio_render(&eng, buf, 256);
                for (i = 0; i < 512u; i++) pcm[i] = (int16_t)(buf[i] > 32767 ? 32767 : buf[i] < -32767 ? -32767 : buf[i]);
                fwrite(pcm, 2, 512, f);
                done += 256u;
            }
            bio_panic(&eng);
        }
        fclose(f);
        return 0;
    }
    if (!strcmp(argv[1], "palette") && argc > 2) {   /* the perc kit: each sound three times (its note, -5, +7) */
        static int32_t buf[512];
        int16_t pcm[512];
        uint32_t i, done = 0, frames = (uint32_t)(BIO_NPERC_KIT - BIO_NKICK_KIT) * 66150u, j;
        FILE *f = fopen(argv[2], "wb");
        if (!f) { perror(argv[2]); return 1; }
        eng.tick = 0;
        eng.mix.lvl[CH_NOISE] = 100;
        eng.mix.sa[CH_NOISE] = 0; eng.mix.sb[CH_NOISE] = 14;
        wav_hdr(f, frames);
        for (i = 0; i < BIO_NPERC_KIT - BIO_NKICK_KIT; i++) {
            uint32_t t0 = 0, hold = BIO_PERC_KIT[i].p[P_D] ? 33075u : 0u;
            for (j = 0; j < BIO_NP; j++) eng.p[O_NOISE][j] = BIO_PERC_KIT[i].p[j];
            eng.p[O_NOISE][P_D8] = 0;                /* (no BREATH) */
            printf("%5.2f s  %s\n", done / 44100.0, BIO_PERC_KIT[i].name);
            for (t0 = 0; t0 < 66150u; t0 += 256u) {
                uint32_t k;
                if (t0 == 0u) bio_trigger(&eng, O_NOISE, 60, 110, hold ? 0xFFFFFFFFu : BIO_MS(100), 0, 0x9E37u + i);
                if (t0 == 11008u && !hold) bio_trigger(&eng, O_NOISE, 55, 90, BIO_MS(100), 0, 0x9E38u + i);
                if (t0 == 22016u && !hold) bio_trigger(&eng, O_NOISE, 67, 100, BIO_MS(100), 0, 0x9E39u + i);
                if (hold && t0 == hold) bio_release(&eng, O_NOISE, 60);
                memset(buf, 0, sizeof buf);
                bio_render(&eng, buf, 256);
                for (k = 0; k < 512u; k++) pcm[k] = (int16_t)(buf[k] > 32767 ? 32767 : buf[k] < -32767 ? -32767 : buf[k]);
                fwrite(pcm, 2, 512, f);
                done += 256u;
            }
        }
        fclose(f);
        printf("%s: %u sounds\n", argv[2], (unsigned)BIO_NPERC_KIT);
        return 0;
    }
    if (!strcmp(argv[1], "stem") && argc > 3) {   /* the awakening, one channel alone, dry: its level in the mix */
        uint32_t ch = (uint32_t)atoi(argv[2]);
        for (k = 0; k < BIO_NCH; k++) scenes[0].mix.mute[k] = k != ch;
        scenes[0].fx.d_mix = scenes[0].fx.r_mix = scenes[0].fx.c_mix = 0;
        seq_play(&sq);
        return render(argv[3], 4u * 88200u);
    }
    if (!strcmp(argv[1], "awaken") || !strcmp(argv[1], "dry")) {
        uint32_t bars = argc > 3 ? (uint32_t)atoi(argv[3]) : 8u;
        if (!strcmp(argv[1], "dry")) {
            scenes[0].fx.d_mix = scenes[0].fx.r_mix = scenes[0].fx.c_mix = 0;
            scenes[0].tur.depth = 0;
        }
        seq_play(&sq);
        return render(argv[2], bars * 88200u);
    }
    if (!strcmp(argv[1], "organ") && argc > 3) {
        uint32_t o = (uint32_t)atoi(argv[2]);
        int32_t note = argc > 4 ? atoi(argv[4]) : (o < 4u ? 60 : o == O_BASS ? 26 : o == O_DRONE ? 38 : o == O_SPASM ? 55 : o == O_VOICE ? 45 : 50);
        uint32_t len_ms = argc > 5 ? (uint32_t)atoi(argv[5]) : (o < 4u ? 125u : 600u);
        scenes[0].fx.d_mix = scenes[0].fx.r_mix = scenes[0].fx.c_mix = 0;
        for (k = 0; k < BIO_NCH; k++) scenes[0].mix.lvl[k] = 100;
        seq_sound(&sq);
        bio_trigger(&eng, o, o < 4u ? note - 60 : note, 110, BIO_MS(len_ms), 0, 0x1234u);
        {
            FILE *f = fopen(argv[3], "wb");
            static int32_t buf[512];
            int16_t pcm[512];
            uint32_t done = 0, frames = 88200u, j, off = BIO_MS(len_ms);
            int32_t peak = 0;
            if (!f) { perror(argv[3]); return 1; }
            wav_hdr(f, frames);
            while (done < frames) {
                memset(buf, 0, sizeof buf);
                if (done <= off && done + 256u > off) bio_release(&eng, o, (uint32_t)note);
                bio_render(&eng, buf, 256);
                for (j = 0; j < 512u; j++) {
                    int32_t v = buf[j];
                    if (v > peak) peak = v;
                    if (-v > peak) peak = -v;
                    pcm[j] = (int16_t)(v > 32767 ? 32767 : v < -32767 ? -32767 : v);
                }
                fwrite(pcm, 2, 512, f);
                done += 256;
            }
            fclose(f);
            printf("%s: organ %u, peak %d (%.1f dBFS)\n", argv[3], o, peak, dbfs(peak));
        }
        return 0;
    }
    fprintf(stderr, "bio_render: what is %s?\n", argv[1]);
    return 2;
}
