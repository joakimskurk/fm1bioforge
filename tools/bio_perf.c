/* SPDX-License-Identifier: GPL-3.0-only */
/* The engine's cost on a computer (tests/run_bio.sh runs it under callgrind, built -Os as the chip's build is):
 * BLOCKS blocks of the awakening, or of everything at once (every organ sounding, every effect on: the worst case),
 * or the worst with the four drums as noise (MODEL NOISE, every 16th, driven and crushed), or a factory ritual.
 *   build/bio_perf awaken|worst|perc|ritualN BLOCKS */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "bio_seq.h"
#include "bio_rituals.h"
static bio_t eng;
static seq_t sq;
static seq_scene_t sc[SEQ_NSCENE];
static seq_undo_t u[SEQ_NUNDO], r[SEQ_NREDO];
int main(int argc, char **argv)
{
    static int32_t buf[512];
    uint32_t n = argc > 2 ? (uint32_t)atoi(argv[2]) : 690u, b, k;
    int perc = argc > 1 && !strcmp(argv[1], "perc"), worst = perc || (argc > 1 && !strcmp(argv[1], "worst"));
    bio_init(&eng);
    seq_scene_awaken(&sc[0]);
    for (k = 1; k < SEQ_NSCENE; k++) seq_scene_blank(&sc[k], k);
    if (argc > 1 && !strncmp(argv[1], "ritual", 6)) BIO_RITUAL[(uint32_t)atoi(argv[1] + 6) % BIO_NRITUAL](&sc[0]);
    if (perc) {                                 /* the heaviest of the kit on the four drums, NOISE held */
        static const char *const K[4] = {"FLAM SNR", "CLAP", "CRUSH", "RISE"};
        for (k = 0; k < 4u; k++) {
            uint32_t j;
            bio_preset_set(sc[0].p[k], bio_preset_find(K[k]), k);
            sc[0].p[k][P_S] = 80; sc[0].p[k][P_D6] = 80; sc[0].p[k][P_D7] = 60;   /* width, drive, crush: all of it */
            for (j = 0; j < 16u; j++) seq_put(&sc[0], k, j, 96, 60, 100, 0);
        }
        bio_preset_set(sc[0].p[O_NOISE], bio_preset_find("WASH"), O_NOISE);
        seq_put(&sc[0], O_NOISE, 0, 1536, 60, 100, 0);
    }
    if (worst) {                                /* every organ, every effect, the Turing rattling */
        for (k = 0; k < BIO_NCH; k++) { sc[0].mix.sa[k] = sc[0].mix.sb[k] = sc[0].mix.sc[k] = 50; }
        sc[0].tur.rattle = 80; sc[0].tur.loop = 50;
        sc[0].tr[O_VOICE].len = 16; sc[0].tr[O_NOISE].len = 16;
        for (k = 0; k < 16u; k++) { seq_put(&sc[0], O_HEART, k, 96, 60, 100, 0); seq_put(&sc[0], O_CLAW, k, 192, 60, 100, 0); }
        seq_put(&sc[0], O_SPASM, 0, 1536, 55, 100, 0);
    }
    seq_attach(&sq, &eng, sc, u, r);
    seq_play(&sq);
    for (b = 0; b < n; b++) { memset(buf, 0, sizeof buf); bio_render(&eng, buf, 256); }
    printf("%u blocks, %u frames\n", n, n * 256u);
    return 0;
}
