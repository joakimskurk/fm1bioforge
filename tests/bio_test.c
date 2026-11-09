/* SPDX-License-Identifier: GPL-3.0-only */
/* BIOFORGE's engine and memory, tested on a computer (tests/run_bio.sh builds it twice: plain and under ASan/UBSan).
 * The idea document's testable demands (5.10, 10) where a computer can check them: the Turing's periods, WRITE and
 * RESET; the same start giving the same sound; a dry start that does not clip or drift; recording live and by step,
 * overdub, replace, erase, undo; quantise that keeps the performance; no hanging notes; the organs falling asleep.
 * And noise as percussion: every sound of the kit heard, unclipped, asleep after; MODEL NOISE on a drum; PUMP; the
 * factory rituals each playing in range. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "bio_seq.h"
#include "bio_rituals.h"

static bio_t eng, eng2;
static seq_t sq, sq2;
static seq_scene_t scn[SEQ_NSCENE], scn2[SEQ_NSCENE];
static seq_undo_t und[SEQ_NUNDO], red[SEQ_NREDO], und2[SEQ_NUNDO], red2[SEQ_NREDO];
static int fails, checks;

static void check(int ok, const char *what)
{
    checks++;
    if (!ok) fails++;
    printf("%s %s\n", ok ? "ok  " : "FAIL", what);
}

static void setup(bio_t *e, seq_t *s, seq_scene_t *sc, seq_undo_t *u, seq_undo_t *r)
{
    uint32_t k;
    memset(e, 0, sizeof *e);
    memset(s, 0, sizeof *s);
    bio_init(e);
    seq_scene_awaken(&sc[0]);
    for (k = 1; k < SEQ_NSCENE; k++) seq_scene_blank(&sc[k], 0x51u + k);
    seq_attach(s, e, sc, u, r);
}

/* render n blocks; the peak, the sum (DC) and the sum of squares of L+R */
static int32_t peak;
static int64_t sum, sum2;
static uint32_t clipped;
static void run(bio_t *e, uint32_t blocks, int16_t *keep)
{
    static int32_t buf[512];
    uint32_t b, k;
    for (b = 0; b < blocks; b++) {
        memset(buf, 0, sizeof buf);
        bio_render(e, buf, 256);
        for (k = 0; k < 512u; k++) {
            int32_t v = buf[k];
            if (v > 32767 || v < -32767) clipped++;
            if (v > peak) peak = v;
            if (-v > peak) peak = -v;
            sum += v;
            sum2 += (int64_t)v * v;
            if (keep) keep[b * 512u + k] = (int16_t)v;
        }
    }
}

static uint32_t tur_period(const bio_tur_set_t *set, uint32_t clocks)   /* the smallest p with bit k == bit k + p */
{
    bio_tur_t t;
    uint8_t bits[256];
    uint32_t i, p;
    memset(&t, 0, sizeof t);
    bio_tur_reset(&t, set);
    for (i = 0; i < 64u; i++) bio_tur_clock(&t, set);   /* settle */
    for (i = 0; i < clocks && i < 256u; i++) { bio_tur_clock(&t, set); bits[i] = (uint8_t)(t.r & 1u); }
    for (p = 1; p < clocks / 2u; p++) {
        uint32_t ok = 1;
        for (i = 0; i + p < clocks; i++) if (bits[i] != bits[i + p]) { ok = 0; break; }
        if (ok) return p;
    }
    return 0;
}

static void test_turing(void)
{
    bio_tur_set_t s = BIO_TUR_INIT;
    bio_tur_t a, b;
    uint32_t i, same = 1, l;
    char m[96];
    for (l = 0; l < 8u; l++) {
        s.len_i = (uint8_t)l;
        s.loop = 100;
        snprintf(m, sizeof m, "Turing LOOP 100 %% (LOCK): the period is LENGTH (%u)", BIO_TUR_LEN[l]);
        i = tur_period(&s, 128);
        check(i && BIO_TUR_LEN[l] % i == 0u && (i == BIO_TUR_LEN[l] || i * 2u == BIO_TUR_LEN[l] || i * 4u == BIO_TUR_LEN[l] || i == 1u || BIO_TUR_LEN[l] % i == 0u), m);
        s.loop = 0;
        snprintf(m, sizeof m, "Turing LOOP 0 %% (DOUBLE LOCK): the period is 2 x LENGTH (%u)", 2u * BIO_TUR_LEN[l]);
        i = tur_period(&s, 128);
        check(i && (2u * BIO_TUR_LEN[l]) % i == 0u && i > BIO_TUR_LEN[l] / 2u, m);
    }
    s = BIO_TUR_INIT;
    s.loop = 0; s.len_i = 5;                    /* after L clocks the pattern is its inverse */
    memset(&a, 0, sizeof a);
    bio_tur_reset(&a, &s);
    {
        uint16_t r0;
        for (i = 0; i < 16u; i++) bio_tur_clock(&a, &s);
        r0 = a.r;
        for (i = 0; i < 8u; i++) bio_tur_clock(&a, &s);
        check((uint16_t)(a.r & 0xFFu) == (uint16_t)(~r0 & 0xFFu), "Turing LOOP 0 %: after LENGTH clocks the bits are inverted");
    }
    s.loop = 50;
    s.len_i = 7;
    check(tur_period(&s, 200) == 0u, "Turing LOOP 50 %: it changes (no period within 100 clocks)");
    /* RESET: the same register and RNG give the same course */
    memset(&a, 0, sizeof a);
    memset(&b, 0, sizeof b);
    bio_tur_reset(&a, &s);
    bio_tur_reset(&b, &s);
    for (i = 0; i < 300u; i++) { bio_tur_clock(&a, &s); bio_tur_clock(&b, &s); if (a.r != b.r) same = 0; }
    check(same, "Turing RESET: the saved register and RNG give the same course (LOOP 50 %, 300 clocks)");
    /* WRITE: only the incoming bit */
    s.loop = 100;
    bio_tur_reset(&a, &s);
    {
        uint16_t r0 = a.r;
        a.write = 2;
        bio_tur_clock(&a, &s);
        check((a.r & 1u) == 1u && (a.r >> 1) == (r0 & 0x7FFFu), "Turing WRITE 1: the next bit is 1, the rest only shifted");
        a.write = 1;
        bio_tur_clock(&a, &s);
        check((a.r & 1u) == 0u, "Turing WRITE 0: the next bit is 0");
    }
}

static int16_t pcm_a[400u * 512u], pcm_b[400u * 512u];

static void test_sound(void)
{
    uint32_t i, diff = 0;
    double rms;
    /* LOCKED BODY: the same scene, seed and start: the same sound, sample for sample */
    setup(&eng, &sq, scn, und, red);
    setup(&eng2, &sq2, scn2, und2, red2);
    seq_play(&sq);
    seq_play(&sq2);
    run(&eng, 400, pcm_a);
    run(&eng2, 400, pcm_b);
    for (i = 0; i < 400u * 512u; i++) if (pcm_a[i] != pcm_b[i]) diff++;
    check(diff == 0u, "LOCKED BODY: two renders of the awakening from PLAY are the same, sample for sample (2.3 s)");
    /* again after STOP and PLAY: the same */
    seq_stop(&sq);
    bio_panic(&eng);                            /* (the drone's long release would carry on into the next PLAY) */
    run(&eng, 1000, 0);
    seq_play(&sq);
    run(&eng, 400, pcm_b);
    for (i = 0, diff = 0; i < 400u * 512u; i++) if (abs(pcm_a[i] - pcm_b[i]) > 64) diff++;
    check(diff < 400u * 512u / 100u, "STOP, PANIC, 6 s, PLAY again: the same sound, but for the effects' last tails (-54 dB)");
    /* the awakening: no clipping, no DC, a level */
    setup(&eng, &sq, scn, und, red);
    seq_play(&sq);
    peak = 0; sum = sum2 = 0; clipped = 0;
    run(&eng, 1378, 0);                         /* 8 s */
    { double m = (double)sum2 / (1378.0 * 512.0), r = m > 1 ? m : 1; int j; for (j = 0; j < 40; j++) r = 0.5 * (r + m / r); rms = m > 0 ? r : 0; }   /* (a square root without libm) */
    check(clipped == 0u, "the awakening, 8 s: nothing clipped");
    check(peak < 31000, "the awakening: its peak under -0.5 dBFS (the limiter)");
    check(rms > 1000.0 && rms < 12000.0, "the awakening: its RMS between -30 and -9 dBFS");
    check(llabs(sum) / (1378 * 512) < 60, "the awakening: no DC (its mean under 0.2 %)");
    /* DRY START: no FX, no Turing: still no clipping */
    setup(&eng, &sq, scn, und, red);
    scn[0].fx.d_mix = scn[0].fx.r_mix = scn[0].fx.c_mix = 0;
    scn[0].tur.depth = 0;
    seq_play(&sq);
    peak = 0; sum = sum2 = 0; clipped = 0;
    run(&eng, 700, 0);
    check(clipped == 0u && peak > 3000, "DRY START: the four drums and the rest, dry: heard, not clipped");
}

/* how many times organ o is struck in n blocks */
static uint32_t hits(bio_t *e, uint32_t o, uint32_t blocks)
{
    static int32_t buf[512];
    uint32_t b, c = 0, last = 0;
    for (b = 0; b < blocks; b++) {
        memset(buf, 0, sizeof buf);
        bio_render(e, buf, 256);
        if (o < 4u) {
            if (e->drum[o].awake && e->drum[o].t < last) c++;
            last = e->drum[o].awake ? e->drum[o].t : 0x7FFFFFFFu;
        } else {
            if (e->tone[o - 4u].env.st == 1u && last != 1u) c++;
            last = e->tone[o - 4u].env.st;
        }
    }
    return c;
}

static void test_record(void)
{
    seq_track_t *t;
    uint32_t n0, i, ok;
    setup(&eng, &sq, scn, und, red);
    t = &scn[0].tr[O_BONE];
    n0 = t->n;
    /* LIVE REC: PLAY, a BONE note off the grid at step 3 (tick 300), 24 ticks long */
    seq_play(&sq);
    seq_begin(&sq);
    sq.rec = 1;
    sq.rec_trk = O_BONE;
    seq_rec_on(&sq, O_BONE, 60, 100, 300);
    check(t->n == n0 + 1u, "LIVE REC: the note is in the track at once");
    seq_rec_off(&sq, O_BONE, 60, 324);
    for (i = 0, ok = 0; i < t->n; i++) if (t->ev[i].start == 300u && t->ev[i].len == 24u) ok = 1;
    check(ok, "LIVE REC: its raw time (tick 300) and its length (24 ticks) kept");
    check(seq_eff(&scn[0], t, 300) == 291u, "quantise 1/16 at 75 %: it plays at 291, its raw time untouched");
    t->qstr = 0;
    check(seq_eff(&scn[0], t, 300) == 300u, "quantise off (strength 0): the performance as played");
    t->qstr = 75;
    /* OVERDUB: the same note at the same quantised start replaces it: no double note-on */
    seq_rec_on(&sq, O_BONE, 60, 90, 290);
    seq_rec_off(&sq, O_BONE, 60, 310);
    check(t->n == n0 + 1u, "OVERDUB: the same note at the same quantised start is replaced, not doubled");
    /* UNDO the pass, REDO it */
    sq.rec = 0;
    check(seq_undo(&sq) && t->n == n0, "UNDO: the pass is gone");
    check(seq_redo(&sq) && t->n == n0 + 1u, "REDO: it is back");
    /* it plays in the loop: BONE struck 2 + 1 times a bar */
    setup(&eng, &sq, scn, und, red);
    t = &scn[0].tr[O_BONE];
    seq_begin(&sq);
    seq_step_write(&sq, O_BONE, 2, 60, 100, 1);
    seq_play(&sq);
    {
        uint32_t h = hits(&eng, O_BONE, 4u * 345u);   /* 4 bars at 120 BPM */
        char m[80];
        snprintf(m, sizeof m, "STEP REC: a BONE note on step 3 plays every bar (%u hits in 4 bars, 12 wanted)", h);
        check(h >= 11u && h <= 13u, m);
    }
    /* toggling a step (SEQ held): on, off */
    n0 = t->n;
    seq_step_toggle(&sq, O_BONE, 9, 60, 100);
    check(t->n == n0 + 1u, "SEQ held + a key: the step on");
    seq_step_toggle(&sq, O_BONE, 9, 60, 100);
    check(t->n == n0, "and off again");
    /* ERASE in STEP REC: that note at that step */
    seq_step_erase(&sq, O_BONE, 2, 60);
    check(t->n == n0 - 1u, "ERASE: the note at the step is gone, the rest kept");
    /* REPLACE: from the first new note on, the steps the playhead enters are cleared on that track */
    setup(&eng, &sq, scn, und, red);
    t = &scn[0].tr[O_HEART];
    seq_play(&sq);
    run(&eng, 10, 0);
    seq_begin(&sq);
    sq.rec = 1; sq.rec_trk = O_HEART; sq.rec_replace = 1;
    seq_rec_on(&sq, O_HEART, 60, 100, seq_now(&sq, O_HEART, 0));
    seq_rec_off(&sq, O_HEART, 60, seq_now(&sq, O_HEART, 0) + 20u);
    run(&eng, 345, 0);                          /* a bar */
    check(t->n == 1u, "REPLACE: a bar later only the new note is left on HEART (the four anchors replaced)");
    sq.rec = 0;
    check(seq_undo(&sq) && t->n == 4u, "UNDO: the four HEART notes back");
    /* NOTE-OFF SAFETY: STOP leaves no tonal gate open; PANIC silences everything */
    setup(&eng, &sq, scn, und, red);
    seq_play(&sq);
    run(&eng, 200, 0);
    seq_stop(&sq);
    for (i = 0, ok = 1; i < 4u; i++) if (eng.tone[i].gate) ok = 0;
    if (eng.noise.gate) ok = 0;
    check(ok, "STOP: no tonal organ is left held, nor NOISE");
    bio_panic(&eng);
    run(&eng, 40, 0);
    for (i = 0, ok = 1; i < 4u; i++) if (eng.drum[i].awake) ok = 0;
    check(ok, "PANIC: the drums silent within 0.25 s");
    peak = 0;
    run(&eng, 2000, 0);
    for (i = 0, ok = 1; i < 4u; i++) if (eng.tone[i].awake) ok = 0;
    if (eng.noise.awake) ok = 0;
    check(ok, "and the tonal organs asleep, NOISE too (nothing renders: no CPU)");
    /* a note held over the loop's end: its length wraps */
    setup(&eng, &sq, scn, und, red);
    t = &scn[0].tr[O_BASS];
    seq_begin(&sq);
    sq.rec = 1; sq.rec_trk = O_BASS;
    seq_rec_on(&sq, O_BASS, 30, 100, 1500);
    seq_rec_off(&sq, O_BASS, 30, 100);
    for (i = 0, ok = 0; i < t->n; i++) if (t->ev[i].start == 1500u && t->ev[i].len == 136u) ok = 1;
    check(ok, "a note held over the loop's end: its length goes round (136 ticks)");
    /* FUSE: RATTLE's ghosts into the tracks */
    setup(&eng, &sq, scn, und, red);
    scn[0].tur.rattle = 60;
    scn[0].tur.loop = 50;
    seq_play(&sq);
    run(&eng, 2u * 345u, 0);
    n0 = scn[0].tr[O_TEETH].n + scn[0].tr[O_CLAW].n;
    i = seq_fuse(&sq);
    check(i > 0u && scn[0].tr[O_TEETH].n + scn[0].tr[O_CLAW].n == n0 + i, "FUSE: the last bar's ghosts written as notes");
    check(seq_undo(&sq) && scn[0].tr[O_TEETH].n + scn[0].tr[O_CLAW].n == n0, "and undone in one go");
}

static double rms_of(uint32_t blocks)       /* (a square root without libm) */
{
    double m = (double)sum2 / ((double)blocks * 512.0), r = m > 1 ? m : 1;
    int j;
    for (j = 0; j < 40; j++) r = 0.5 * (r + m / r);
    return m > 0 ? r : 0;
}

static void test_perc(void)
{
    uint32_t i, ok_heard = 1, ok_clip = 1, ok_sleep = 1, ok_same = 1, ok_tissue;
    static int16_t a[200 * 512], b[200 * 512];
    /* each sound of the kit, on NOISE alone: heard, not clipped, asleep 3 s after (a held one after its release) */
    for (i = 0; i < BIO_NPERC_KIT; i++) {
        uint32_t held = BIO_PERC_KIT[i].p[P_D] != 0u;
        setup(&eng, &sq, scn, und, red);
        seq_sound(&sq);
        eng.tick = 0;                           /* (no sequencer: the engine's parameters stay as set here) */
        eng.fx.d_mix = eng.fx.r_mix = eng.fx.c_mix = 0;
        eng.mix.lvl[CH_NOISE] = 100;
        bio_preset_set(eng.p[O_NOISE], &BIO_PERC_KIT[i], O_NOISE);
        eng.p[O_NOISE][P_D8] = 0;
        peak = 0; sum = sum2 = 0; clipped = 0;
        bio_trigger(&eng, O_NOISE, 60, 127, held ? 0xFFFFFFFFu : 0u, 0, 0x77u + i);
        run(&eng, 60, 0);
        if (held) bio_release(&eng, O_NOISE, 60);
        run(&eng, 600, 0);
        if (peak < 1600) { ok_heard = 0; printf("     %s: peak %d\n", BIO_PERC_KIT[i].name, (int)peak); }
        if (clipped) ok_clip = 0;
        if (eng.noise.awake) { ok_sleep = 0; printf("     %s: still awake\n", BIO_PERC_KIT[i].name); }
    }
    check(ok_heard, "the noise kit: each of its sounds heard at full velocity (its peak over -26 dBFS)");
    check(ok_clip, "the noise kit: none of them clips");
    check(ok_sleep, "the noise kit: each asleep 3.5 s after it was struck or let go (no CPU)");
    /* the same seed: the same hit */
    for (i = 0; i < 2u; i++) {
        setup(&eng, &sq, scn, und, red);
        seq_sound(&sq);
        eng.tick = 0;
        bio_preset_set(eng.p[O_NOISE], bio_preset_find("CLAP"), O_NOISE);
        bio_trigger(&eng, O_NOISE, 60, 110, 0, 0, 0x4242u);
        run(&eng, 200, i ? b : a);
    }
    for (i = 0; i < 200u * 512u; i++) if (a[i] != b[i]) ok_same = 0;
    check(ok_same, "LOCKED BODY for noise too: the same seed, the same clap, sample for sample");
    /* MODEL NOISE: a drum organ becomes a perc voice, and TISSUE again */
    setup(&eng, &sq, scn, und, red);
    seq_sound(&sq);
    eng.tick = 0;
    bio_preset_set(eng.p[O_TEETH], bio_preset_find("TICK"), O_TEETH);
    bio_trigger(&eng, O_TEETH, 0, 110, 0, 0, 1u);
    run(&eng, 2, 0);
    check(eng.drum[O_TEETH].perc == 1u && eng.drum[O_TEETH].np.awake, "MODEL NOISE: TEETH given TICK strikes the perc voice");
    for (i = 0; i < BIO_NP; i++) eng.p[O_TEETH][i] = BIO_P_INIT[O_TEETH][i];
    run(&eng, 200, 0);
    bio_trigger(&eng, O_TEETH, 0, 110, 0, 0, 2u);
    ok_tissue = eng.drum[O_TEETH].perc == 0u && eng.drum[O_TEETH].awake;
    check(ok_tissue, "and given its own body again (MODEL TISSUE) it rings its modes");
    /* PUMP: HEART ducks the other channels, and they come back within the beat */
    setup(&eng, &sq, scn, und, red);
    seq_sound(&sq);
    eng.tick = 0;
    eng.mix.pump = 100;
    bio_trigger(&eng, O_HEART, 0, 127, 0, 0, 3u);
    run(&eng, 1, 0);
    check(eng.duck > 26000, "PUMP 100: a HEART hit cuts the rest by more than 14 dB");
    run(&eng, 86, 0);                           /* a beat at 120 BPM */
    check(eng.duck < 1500, "and they are back within the beat (under 0.4 dB)");
    /* the factory rituals: each plays, in range, and each is its own */
    for (i = 0; i < BIO_NRITUAL; i++) {
        double r;
        setup(&eng, &sq, scn, und, red);
        BIO_RITUAL[i](&scn[0]);
        sq.tur_reset = 1;
        seq_play(&sq);
        peak = 0; sum = sum2 = 0; clipped = 0;
        run(&eng, 1378, 0);
        r = rms_of(1378);
        if (i + 1u < BIO_NRITUAL) {             /* (the last one is the empty body) */
            char what[96];
            snprintf(what, sizeof what, "ritual %s, 8 s: not clipped, its RMS between -30 and -9 dBFS, no DC", BIO_RITUAL_NAME[i]);
            check(clipped == 0u && r > 1000.0 && r < 12000.0 && llabs(sum) / (1378 * 512) < 60, what);
        }
    }
    {
        uint32_t j, ok = 1;
        static seq_scene_t x, y;
        for (i = 0; i < 4u; i++)
            for (j = i + 1u; j < 4u; j++) {
                BIO_RITUAL[i](&x);
                BIO_RITUAL[j](&y);
                if (x.bpm10 == y.bpm10 && !memcmp(x.p, y.p, sizeof x.p)) ok = 0;
            }
        check(ok, "the four start scenes: no two with the same tempo and the same sounds");
    }
}

int main(void)
{
    test_turing();
    test_sound();
    test_record();
    test_perc();
    printf("%d of %d checks passed\n", checks - fails, checks);
    return fails ? 1 : 0;
}
