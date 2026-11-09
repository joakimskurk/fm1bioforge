/* SPDX-License-Identifier: GPL-3.0-only */
/* The engine's cost on a computer (tests/run_bio.sh runs it under callgrind, built -Os as the chip's build is):
 * BLOCKS blocks of the awakening, or of everything at once (every organ sounding, every effect on: the worst case).
 *   build/bio_perf awaken|worst BLOCKS */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "bio_seq.h"
static bio_t eng;
static seq_t sq;
static seq_scene_t sc[SEQ_NSCENE];
static seq_undo_t u[SEQ_NUNDO], r[SEQ_NREDO];
int main(int argc, char **argv)
{
    static int32_t buf[512];
    uint32_t n = argc > 2 ? (uint32_t)atoi(argv[2]) : 690u, b, k;
    int worst = argc > 1 && !strcmp(argv[1], "worst");
    bio_init(&eng);
    seq_scene_awaken(&sc[0]);
    for (k = 1; k < SEQ_NSCENE; k++) seq_scene_blank(&sc[k], k);
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
