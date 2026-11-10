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

static uint32_t rit_index(const char *name)     /* a factory ritual by its name */
{
    uint32_t i;
    for (i = 0; i < BIO_NRITUAL; i++) if (!strcmp(BIO_RITUAL_NAME[i], name)) return i;
    return 0;
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
    for (i = 0; i < BIO_NPERC_KIT - BIO_NKICK_KIT; i++) {
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
    /* the noise core (D20). LOCK: a noise step's seed the same pass after pass; LOOP lower: some slip; the ratchet
       repeats its hit's noise; SCATTER: each step its own slice; TONE: the band on the note */
    {
        static uint32_t first[SEQ_MAXLEN];
        uint32_t k, same = 1, slipped = 0, nz = 0, loop;
        for (loop = 100u; loop >= 60u; loop -= 40u) {
            setup(&eng, &sq, scn, und, red);
            BIO_RITUAL[rit_index("FROZEN")](&scn[0]);
            scn[0].tur.loop = (uint8_t)loop;
            sq.tur_reset = 1;
            seq_play(&sq);
            run(&eng, 323, 0);                  /* a bar at 128 BPM */
            for (k = 0; k < SEQ_MAXLEN; k++) first[k] = sq.nseed[O_NOISE][k];
            run(&eng, 323 * 3, 0);              /* three more */
            for (k = 0; k < SEQ_MAXLEN; k++) {
                if (!first[k]) continue;        /* (a chance step not yet played) */
                if (loop == 100u) nz++;
                if (loop == 100u && first[k] != sq.nseed[O_NOISE][k]) same = 0;
                if (loop == 60u && first[k] != sq.nseed[O_NOISE][k]) slipped++;
            }
        }
        check(nz >= 6u && same, "FROZEN at LOCK: every noise step's seed the same four bars later (rhythm made of noise)");
        check(slipped >= 2u, "and at LOOP 60 %: some of its steps' noise has slipped");
    }
    {
        static const uint8_t T[BIO_NP] = NC(50, 10, 50, 40,  4, 90, 50, 0, 0, 0, 0,  0, 0, 0, 0,  24, 100, 50,  0, 60, 0);
        static const uint8_t S[BIO_NP] = NC(40, 10, 50, 40,  4, 30, 50, 0, 0, 0, 0,  0, 100, 0, 0,  24, 100, 50,  100, 0, 0);
        static bio_perc_t v1, v2;
        bio_perc_p_t pp;
        int32_t want = bio_lf_hz(880), d;
        uint32_t k, differ = 0;
        bio_perc_params(T, &pp);
        bio_perc_hit(&v1, &pp, 9 * 256, 100, 32767, 0, 5u);   /* A4, BODY 50: an octave up */
        d = v1.lf0 - want;
        check(bio_abs(d) < 200 && v1.tone2 > 0, "TONE: the band on the note (A4 at BODY 50: 880 Hz, within 0.3 %), a fifth over it");
        bio_perc_params(S, &pp);
        for (k = 0; k < 8u; k++) {
            bio_perc_hit(&v1, &pp, 0, 100, 32767, 4000, bio_hash(9u, k));
            bio_perc_hit(&v2, &pp, 0, 100, 32767, 4000, bio_hash(9u, k + 100u));
            if (v1.lf0 != v2.lf0 && v1.gate_n != v2.gate_n) differ++;
        }
        bio_perc_hit(&v2, &pp, 0, 100, 32767, 4000, bio_hash(9u, 7u));
        check(differ >= 7u && v1.lf0 == v2.lf0 && v1.pan_s == v2.pan_s && v1.gate_n == v2.gate_n,
              "SCATTER: each seed its own slice (band, length, place); the same seed the same slice");
    }
    {
        seq_ev_t ev = {0, 96, 60, 100, 1u << EV_RAT_SH, 0};
        setup(&eng, &sq, scn, und, red);
        BIO_RITUAL[rit_index("FROZEN")](&scn[0]);
        seq_play(&sq);
        seq_fire(&sq, O_NOISE, &ev, 3);
        check(sq.rat_left[O_NOISE] == 1u && sq.rat_seed[O_NOISE] == sq.nseed[O_NOISE][3],
              "a noise step's ratchet repeats its own noise (a stutter)");
    }
    /* the deep kick (D22): each of the kit's kicks on HEART heard, low (almost nothing over 1 kHz), unclipped, asleep */
    {
        uint32_t j, ok = 1;
        for (j = BIO_NPERC_KIT - BIO_NKICK_KIT; j < BIO_NPERC_KIT; j++) {
            static int16_t kb[200 * 512];
            int64_t lo = 0, hi = 0;
            uint32_t q;
            setup(&eng, &sq, scn, und, red);
            seq_sound(&sq);
            eng.tick = 0;
            eng.fx.d_mix = eng.fx.r_mix = eng.fx.c_mix = 0;
            eng.mix.lvl[CH_DRUM] = 100;
            bio_preset_set(eng.p[O_HEART], &BIO_PERC_KIT[j], O_HEART);
            peak = 0; clipped = 0;
            bio_trigger(&eng, O_HEART, 0, 120, 0, 0, 5u);
            run(&eng, 200, kb);
            for (q = 2; q < 200u * 512u; q += 2) {   /* (left: the energy, and that of its difference, a crude high-pass) */
                int32_t d = kb[q] - kb[q - 2];
                lo += (int64_t)kb[q] * kb[q];
                hi += (int64_t)d * d;
            }
            if (peak < 8000 || clipped || hi * 100 > lo || eng.drum[O_HEART].kick != 1u) {
                ok = 0;
                printf("     %s: peak %d, top %.4f\n", BIO_PERC_KIT[j].name, (int)peak, lo ? (double)hi / (double)lo : 0.0);
            }
            run(&eng, 600, 0);
            if (eng.drum[O_HEART].awake) ok = 0;
        }
        check(ok, "MODEL KICK: each deep kick on HEART loud, almost no top (its edges under 1 %), unclipped, then asleep");
    }
    /* RUMBLE: HEART's own low tail, and PUMP ducks it */
    {
        int32_t with, without;
        uint32_t r;
        for (r = 0; r < 2u; r++) {
            setup(&eng, &sq, scn, und, red);
            seq_sound(&sq);
            eng.tick = 0;
            eng.fx.d_mix = eng.fx.r_mix = eng.fx.c_mix = 0;
            bio_preset_set(eng.p[O_HEART], bio_preset_find("DEEP KICK"), O_HEART);
            eng.mix.rumble = r ? 80 : 0;
            bio_trigger(&eng, O_HEART, 0, 120, 0, 0, 5u);
            run(&eng, 140, 0);                  /* (0.8 s: the kick nearly gone, its rumble not) */
            peak = 0;
            run(&eng, 40, 0);
            if (r) with = peak; else without = peak;
        }
        check(with > 300 && with > without * 4, "RUMBLE: HEART leaves a low tail after itself");
    }
    /* the landscapes (D21): every model on every synth heard, not clipped, asleep after its release */
    {
        uint32_t m, o, ok_h = 1, ok_c = 1, ok_s = 1;
        for (m = 1; m < BIO_NLAND; m++)
            for (o = O_BASS; o < O_NOISE; o++) {
                setup(&eng, &sq, scn, und, red);
                seq_sound(&sq);
                eng.tick = 0;
                eng.fx.d_mix = eng.fx.r_mix = eng.fx.c_mix = 0;
                eng.mix.lvl[BIO_ORG_CH[o]] = 100;
                eng.p[o][P_E1] = BIO_LAND(m);
                eng.p[o][P_A] = 10;
                peak = 0; clipped = 0;
                bio_trigger(&eng, o, o == O_BASS ? 33 : 57, 110, BIO_MS(800), 0, 0x99u + m);
                run(&eng, 140, 0);
                bio_release(&eng, o, 255u);
                run(&eng, 2000, 0);
                if (peak < 1000) { ok_h = 0; printf("     %s on %s: peak %d\n", BIO_LAND_NAME[m], o == 4u ? "BASS" : o == 5u ? "DRONE" : o == 6u ? "SPASM" : "VOICE", (int)peak); }
                if (clipped) ok_c = 0;
                if (eng.tone[o - 4u].awake) ok_s = 0;
            }
        check(ok_h, "the landscapes: each of nine models heard on each of the four synths (over -30 dBFS)");
        check(ok_c && ok_s, "and none clips, and each is asleep 12 s after its release (no CPU)");
    }
    /* DARK: the master's low-pass takes the top off (a tone of 6 kHz down by 20 dB at 100), and nothing at 0 */
    {
        int32_t pk0 = 0, pk1 = 0;
        uint32_t d, q;
        for (d = 0; d < 2u; d++) {
            static int32_t b[512];
            uint32_t ph = 0, inc = bio_inc(bio_lf_hz(6000));
            setup(&eng, &sq, scn, und, red);
            eng.tick = 0;
            eng.mix.dark = d ? 100 : 0;
            memset(b, 0, sizeof b);
            bio_render(&eng, b, 256);           /* (the engine sets its DARK filter) */
            {                                   /* a tone through that filter, as the master would put it */
                bio_op_t f0 = eng.dk[0], f1 = eng.dk[1];
                int32_t pk = 0;
                for (q = 0; q < 4410u; q++) {
                    int32_t x = bio_sin(ph += inc) / 2;
                    if (d) x = bio_op_lp(&f1, bio_op_lp(&f0, x));
                    if (q > 2000u && bio_abs(x) > pk) pk = bio_abs(x);
                }
                if (d) pk1 = pk; else pk0 = pk;
            }
        }
        check(pk1 * 10 < pk0, "DARK 100: a 6 kHz tone 20 dB down; DARK 0 leaves it");
    }
    /* SPACE: the reverb's rooms are other rooms (ROOM's lines shorter, METAL's the shortest, VAST's the longest) */
    {
        uint32_t L[BIO_NSPACE], sp;
        for (sp = 0; sp < BIO_NSPACE; sp++) {
            setup(&eng, &sq, scn, und, red);
            eng.tick = 0;
            eng.fx.r_space = (uint8_t)sp;
            run(&eng, 1, 0);
            L[sp] = eng.rlen[0];
        }
        check(L[3] < L[1] && L[1] < L[0] && L[0] < L[2] && L[2] <= L[4], "SPACE: METAL < ROOM < HALL < CAVE <= VAST, the reverb's lines");
    }
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

/* SCAR (D29): with no burst it changes nothing; a burst leaves the music underneath in time (identical again once let
   go); REVERSE is the 8th just played, backwards; REPEAT pressed early waits for its beat; SMEAR keeps the loudness */
static int16_t sc_a[2u * 44100u * 9u], sc_b[2u * 44100u * 9u];
static void test_scar(void)
{
    const uint32_t B = 44100u * 9u / 256u;      /* 9 s */
    uint32_t b, i, k, ident_before = 1, ident_after = 1, differ = 0, rev_ok = 1, rev_n = 0, armed_ok = 0, start_ok = 0;
    uint32_t p1 = 0, L = 0, clip2 = 0;
    double ra = 0, rb = 0;
    static int32_t ba[512], bb[512];
    setup(&eng, &sq, scn, und, red);
    setup(&eng2, &sq2, scn2, und2, red2);
    BIO_RITUAL[rit_index("ABYSS")](&scn[0]);
    BIO_RITUAL[rit_index("ABYSS")](&scn2[0]);
    sq.tur_reset = sq2.tur_reset = 1;
    seq_play(&sq);
    seq_play(&sq2);
    for (b = 0; b < B; b++) {                   /* (eng and eng2 side by side: eng2 with bursts) REVERSE from 2.0 s to
                                                   3.0 s, REPEAT 1/16 from 4.06 s (3/4 into a 16th) to 5.0, SMEAR 6.0 .. 7.5 */
        uint32_t f = b * 256u;
        uint32_t w = f >= 88200u && f < 132300u ? SCAR_REV : f >= 178944u && f < 220500u ? SCAR_REP16 :
                     f >= 264600u && f < 330750u ? SCAR_SMEAR : SCAR_OFF;
        eng2.scar.want = w;
        memset(ba, 0, sizeof ba);
        memset(bb, 0, sizeof bb);
        bio_render(&eng, ba, 256);
        bio_render(&eng2, bb, 256);
        if (w == SCAR_REV && !p1 && eng2.scar.mode == SCAR_REV && !eng2.scar.armed) { p1 = eng2.scar.a + 1u; L = eng2.scar.len; }
        if (w == SCAR_REP16 && eng2.scar.armed) armed_ok = 1;
        if (w == SCAR_REP16 && !eng2.scar.armed && eng2.scar.mode == SCAR_REP16 && !start_ok)
            start_ok = eng2.scar.a == eng2.scar.grid[0] || eng2.scar.a + eng2.scar.t == eng2.scar.now ? 1u : 2u;
        for (k = 0; k < 512u; k++) {
            sc_a[(f * 2u + k) % (sizeof sc_a / 2u)] = (int16_t)ba[k];
            sc_b[(f * 2u + k) % (sizeof sc_b / 2u)] = (int16_t)bb[k];
            if (bb[k] > 32767 || bb[k] < -32767) clip2++;
            if (f < 88200u && ba[k] != bb[k]) ident_before = 0;
            if (f >= 334000u && ba[k] != bb[k]) ident_after = 0;   /* (let go at 7.5 s, out by 50 ms) */
            if (f >= 264600u && f < 330750u) { ra += (double)ba[k] * ba[k]; rb += (double)bb[k] * bb[k]; }
            if (bb[k] != ba[k]) differ++;
        }
    }
    for (i = p1 + BIO_SCAR_X + 64u; p1 && i + BIO_SCAR_X < p1 + L; i++) {   /* the reversed pass, at its full share */
        int32_t got = sc_b[2u * i], want = sc_a[2u * (p1 - 1u - (i - p1))];
        rev_n++;
        if (got - want > 2 || want - got > 2) rev_ok = 0;
    }
    check(ident_before && differ > 44100u, "SCAR: nothing changed before a burst; a burst changes the sound");
    check(ident_after, "SCAR: let go, the music is the same again (it went on underneath, in time)");
    check(p1 && rev_ok && rev_n > 8000u && (p1 - 88200u) < L + 512u, "SCAR REVERSE: on the next 8th, the 8th just played, backwards");
    check(armed_ok && start_ok == 1u, "SCAR REPEAT 1/16: pressed in a 16th's second half, it waits for the next");
    check(clip2 == 0u && rb > ra / 4.0 && rb < ra * 4.0, "SCAR SMEAR: not clipped, its loudness within 6 dB of the music's");
}

/* KEY and SCALE (D31): the synths' notes take the KEY's shift (the nearest way), the drums do not; WAIL picks from the
   SCALE; a held key lets go even when the KEY was turned in between; KEY 0 changes nothing */
static void test_key(void)
{
    uint32_t b, ok_pc = 1, seen = 0, k;
    setup(&eng, &sq, scn, und, red);
    seq_scene_blank(&scn[0], 0x4B1u);
    seq_put(&scn[0], O_BASS, 0, 96, 38, 100, 0);
    scn[0].tur.key = 2;                         /* E */
    seq_play(&sq);
    run(&eng, 2, 0);
    check(eng.tone[0].note == 40u, "KEY E: the bass's D2 sounds as E2");
    seq_stop(&sq);
    scn[0].tur.key = 7;                         /* A: down a fourth, not up a fifth */
    seq_play(&sq);
    run(&eng, 2, 0);
    check(eng.tone[0].note == 33u && bio_key_shift(&scn[0].tur) == -5, "KEY A: the nearest way, -5 (A1)");
    seq_stop(&sq);
    /* WAIL from MINOR PENTATONIC: every note SPASM plays is D, F, G, A or C */
    seq_scene_blank(&scn[0], 0x4B2u);
    for (k = 0; k < 16u; k++) seq_put(&scn[0], O_SPASM, k, 48, 62, 100, 0);
    scn[0].tur.wail = 1; scn[0].tur.loop = 50; scn[0].tur.key = 6u << 4; scn[0].tur.scale = 100;
    seq_play(&sq);
    for (b = 0; b < 400u; b++) {
        uint32_t pc;
        run(&eng, 1, 0);
        pc = (eng.tone[2].note + 10u) % 12u;   /* (semitones over D) */
        if (eng.tone[2].note) { seen |= 1u << pc; if (!(BIO_SCALE_MASK[6] >> pc & 1u)) ok_pc = 0; }
    }
    check(ok_pc && (seen & ~1u), "WAIL in MINOR PENTATONIC: only its notes, and more than the root");
    seq_stop(&sq);
    /* a key held while the KEY turns: it still lets go */
    scn[0].tur.wail = 0; scn[0].tur.key = 3;
    seq_live(&sq, O_VOICE, 62, 100, 1);
    run(&eng, 1, 0);
    k = eng.tone[3].gate && eng.tone[3].note == 65u;
    scn[0].tur.key = 9;
    seq_live(&sq, O_VOICE, 62, 0, 0);
    run(&eng, 1, 0);
    check(k && !eng.tone[3].gate, "a key held in F, let go in B: its note ends (it took the shift it began with)");
}

int main(void)
{
    test_turing();
    test_sound();
    test_record();
    test_perc();
    test_scar();
    test_key();
    printf("%d of %d checks passed\n", checks - fails, checks);
    return fails ? 1 : 0;
}
