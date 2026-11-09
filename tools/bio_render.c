/* SPDX-License-Identifier: GPL-3.0-only */
/* BIOFORGE on a computer: the engine and the sequencer as the firmware has them, rendered to WAV (no platform).
 *   cc -O2 -Ifirmware/src -Ifirmware/gen -o build/bio_render tools/bio_render.c
 *   build/bio_render awaken OUT.wav [BARS]          RITUAL 00, THE AWAKENING, from PLAY
 *   build/bio_render organ N OUT.wav [NOTE] [LEN_MS] one hit (or note) of organ N (0 HEART .. 8 NOISE), dry, 2 s
 *   build/bio_render dry OUT.wav [BARS]             the awakening with no effects and no Turing (DRY START)
 *   build/bio_render ritual N OUT.wav [BARS] [SOLO] a factory ritual (bio_rituals.h: 0 AWAKENING, 1 STATIC CULT,
 *                                                   2 SPLICE, 3 VOID, 4 INFECTION, 5 EMPTY BODY); SOLO: one organ
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
        if (argc > 5) {                         /* one organ alone (its stem, through the effects) */
            uint32_t solo = (uint32_t)atoi(argv[5]);
            for (k = 0; k < BIO_NORG; k++) scenes[0].tr[k].mute = k != solo;
        }
        sq.tur_reset = 1;
        seq_play(&sq);
        return render(argv[3], bars * 4u * (uint32_t)(44100u * 60u / (scenes[0].bpm10 / 10u)) + 88200u);
    }
    if (!strcmp(argv[1], "palette") && argc > 2) {   /* the perc kit: each sound three times (its note, -5, +7) */
        static int32_t buf[512];
        int16_t pcm[512];
        uint32_t i, done = 0, frames = (uint32_t)BIO_NPERC_KIT * 66150u, j;
        FILE *f = fopen(argv[2], "wb");
        if (!f) { perror(argv[2]); return 1; }
        eng.tick = 0;
        eng.mix.lvl[CH_NOISE] = 100;
        eng.mix.sa[CH_NOISE] = 0; eng.mix.sb[CH_NOISE] = 14;
        wav_hdr(f, frames);
        for (i = 0; i < BIO_NPERC_KIT; i++) {
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
