/* SPDX-License-Identifier: GPL-3.0-only */
/* The render on two cores (D38), on a computer: the same output, bit for bit, however the units are shared out.
 * Every factory ritual and the worst case (every organ, every effect) render 300 blocks
 *   - on one core (no second core given);
 *   - deferred: the job runs inline at the wait (as a second core that starts late), a random split each sub-block;
 *   - on a thread: a real second core, a random split, and the split by cost (the clock of this computer);
 *   - with lost jobs: now and then the second core does not answer (the wait fails, the job never ran): the units
 *     again on this core, the output unchanged;
 * and each must match the first. tests/run_bio.sh runs it, and again under ThreadSanitizer (a race is a bug on the
 * FM-1 too). */
#include <pthread.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "bio_seq.h"
#include "bio_rituals.h"

static bio_t eng;
static seq_t sq;
static seq_scene_t sc[SEQ_NSCENE];
static seq_undo_t u[SEQ_NUNDO], r[SEQ_NREDO];

/* ---- the doubles of the second core */
static void (*job_fn)(uint32_t);
static uint32_t job_arg, lose_every, jobs;
static int job_pending;
static int run_deferred(void (*fn)(uint32_t), uint32_t arg) { job_fn = fn; job_arg = arg; job_pending = 1; return 1; }
static int wait_deferred(void)
{
    jobs++;
    job_pending = 0;
    if (lose_every && jobs % lose_every == 0u) return -1;   /* lost: never ran */
    job_fn(job_arg);
    return 0;
}

static atomic_uint t_job, t_done;              /* the mailbox: job number, the last one done */
static void (*_Atomic t_fn)(uint32_t);
static _Atomic uint32_t t_arg;
static atomic_int t_quit;
static pthread_t th;
static void *core1(void *p)
{
    unsigned done = 0;
    (void)p;
    while (!atomic_load(&t_quit)) {
        unsigned j = atomic_load_explicit(&t_job, memory_order_acquire);
        if (j == done) continue;
        atomic_load(&t_fn)(atomic_load(&t_arg));
        done = j;
        atomic_store_explicit(&t_done, j, memory_order_release);
    }
    return NULL;
}
static int run_thread(void (*fn)(uint32_t), uint32_t arg)
{
    atomic_store(&t_fn, fn);
    atomic_store(&t_arg, arg);
    atomic_fetch_add_explicit(&t_job, 1u, memory_order_release);
    return 1;
}
static int wait_thread(void)
{
    unsigned j = atomic_load(&t_job);
    while (atomic_load_explicit(&t_done, memory_order_acquire) != j)
        ;
    return 0;
}
static uint32_t ticks_clock(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint32_t)(ts.tv_sec * 1000000000u + ts.tv_nsec);
}

/* ---- a render of a scene: its hash */
enum { ONE, DEFERRED, THREAD, THREAD_COST, LOST };
static uint32_t rnd = 12345u;
static uint64_t render(int scene, int mode, uint32_t blocks)
{
    static int32_t buf[512];
    uint64_t h = 1469598103934665603ull;
    uint32_t b, k;
    memset(&eng, 0, sizeof eng);
    memset(&sq, 0, sizeof sq);
    bio_init(&eng);
    seq_scene_awaken(&sc[0]);
    for (k = 1; k < SEQ_NSCENE; k++) seq_scene_blank(&sc[k], k);
    if (scene >= 0) BIO_RITUAL[scene](&sc[0]);
    else {                                      /* the worst: every organ, every effect, the Turing rattling */
        for (k = 0; k < BIO_NCH; k++) sc[0].mix.sa[k] = sc[0].mix.sb[k] = sc[0].mix.sc[k] = 50;
        sc[0].tur.rattle = 80;
        sc[0].tur.loop = 50;
        sc[0].tr[O_VOICE].len = 16;
        sc[0].tr[O_NOISE].len = 16;
        for (k = 0; k < 16u; k++) { seq_put(&sc[0], O_HEART, k, 96, 60, 100, 0); seq_put(&sc[0], O_CLAW, k, 192, 60, 100, 0); }
        seq_put(&sc[0], O_SPASM, 0, 1536, 55, 100, 0);
        seq_put(&sc[0], O_NOISE, 0, 1536, 60, 100, 0);
        sc[0].mix.rumble = 60;
    }
    seq_attach(&sq, &eng, sc, u, r);
    seq_play(&sq);
    lose_every = mode == LOST ? 7u : 0u;
    if (mode == DEFERRED || mode == LOST) { eng.par.run = run_deferred; eng.par.wait = wait_deferred; }
    if (mode == THREAD || mode == THREAD_COST) { eng.par.run = run_thread; eng.par.wait = wait_thread; }
    if (mode == THREAD_COST) { eng.par.ticks = ticks_clock; eng.par_force = -1; }
    for (b = 0; b < blocks; b++) {
        if (b == 40u) eng.scar.want = 1;        /* a BURST in the middle (bio_scar.h) */
        if (b == 80u) eng.scar.want = 0;
        if (mode != ONE && mode != THREAD_COST) {
            rnd = rnd * 1664525u + 1013904223u;
            eng.par_force = (int32_t)(rnd >> 8 & ((1u << BIO_NUNIT) - 1u));   /* a new split each block */
        }
        memset(buf, 0, sizeof buf);
        bio_render(&eng, buf, 256);
        for (k = 0; k < 512u; k++) h = (h ^ (uint32_t)buf[k]) * 1099511628211ull;
    }
    return h;
}

int main(int argc, char **argv)
{
    static const char *const MODE[] = {"one core", "deferred", "a thread", "a thread, by cost", "lost jobs"};
    uint32_t blocks = argc > 1 ? (uint32_t)atoi(argv[1]) : 300u, fail = 0, split = 0;
    int s, m;
    pthread_create(&th, NULL, core1, NULL);
    for (s = -1; s < (int)BIO_NRITUAL; s++) {
        uint64_t h0 = render(s, ONE, blocks);
        for (m = DEFERRED; m <= LOST; m++) {
            uint64_t h = render(s, m, blocks);
            if (m == THREAD_COST && eng.par_mask) split++;
            if (h != h0) {
                printf("FAIL: %s, %s: %016llx, one core %016llx\n", s < 0 ? "the worst" : BIO_RITUAL_NAME[s], MODE[m],
                       (unsigned long long)h, (unsigned long long)h0);
                fail++;
            }
        }
    }
    atomic_store(&t_quit, 1);
    pthread_join(th, NULL);
    printf("%s: %d scenes x %u blocks, the same output in every split (%u of them split by cost at the end)\n",
           fail ? "FAIL" : "ok", (int)BIO_NRITUAL + 1, blocks, split);
    return fail != 0;
}
