/* SPDX-License-Identifier: GPL-3.0-only */
/* BIOFORGE's memory: the RITUAL GRID (scenes, tracks, note events, p-locks), the transport, recording (LIVE, STEP,
 * PARAM; OVERDUB, REPLACE, ERASE), undo, and the NERVOUS TAPE's clock. The transport runs in the audio interrupt, as
 * the engine's tick, once a sub-block; the main loop records and edits, inside SEQ_LOCK() (the app: interrupts off for
 * the few microseconds an edit takes).
 *
 * Time: a 16th step is 96 ticks. A live note keeps the tick it came on; its track's quantise places it when it plays
 * (non-destructive). A track has its own length (1..64 steps); each plays the transport's tick modulo its length. */
#ifndef BIO_SEQ_H
#define BIO_SEQ_H
#include "bio_engine.h"
#include "bio_turing.h"

#ifndef SEQ_LOCK
#define SEQ_LOCK() ((void)0)
#define SEQ_UNLOCK() ((void)0)
#endif

#define SEQ_TPS 96u                             /* ticks a 16th */
#define SEQ_NEV 96u                             /* events a track */
#define SEQ_MAXLEN 64u
#define SEQ_NSCENE 4u
#define SEQ_BAR (16u * SEQ_TPS)
#define SEQ_MAGIC 0x32464F42u                   /* "BOF2": the scene's format (2: the mix's PUMP) */
#define SEQ_NUNDO 16u
#define SEQ_NREDO 8u

/* an event's flags */
#define EV_ACC 0x01u                            /* accent */
#define EV_RAT_SH 1                             /* bits 1-2: the ratchet - 1 */
#define EV_CH_SH 3                              /* bits 3-5: the chance (0: always, c: (8 - c) / 8) */
#define EV_ANCHOR 0x40u                         /* protected (the Turing never plays over it) */
#define EV_FRESH 0x80u                          /* just played live: not again on this pass */

typedef struct { uint16_t start, len; uint8_t note, vel, fl, pad; } seq_ev_t;   /* len 0: still held (recording) */
typedef struct {
    seq_ev_t ev[SEQ_NEV];
    uint8_t n, len, quant, qstr, mute, pad[3];
    uint8_t lock[SEQ_MAXLEN][4];                /* BODY TEXTURE MOTION DECAY on each step, BIO_NOLOCK: none */
} seq_track_t;
typedef struct {
    uint32_t magic;
    uint16_t bpm10;
    uint8_t swing, steplen;                     /* swing 0..100; STEP REC's note length in steps */
    uint32_t seed;
    seq_track_t tr[BIO_NORG];
    uint8_t p[BIO_NORG][BIO_NP];
    bio_mix_t mix;
    bio_fxp_t fx;
    bio_tur_set_t tur;
    char name[12];
} seq_scene_t;

static const uint16_t SEQ_QGRID[5] = {0, 48, 96, 192, 384};   /* OFF 1/32 1/16 1/8 1/4 */
static const char *const SEQ_QNAME[5] = {"OFF", "1/32", "1/16", "1/8", "1/4"};

typedef struct { uint8_t org, note, vel, on; } seq_in_t;
typedef struct { int16_t trk; seq_track_t t; uint16_t group; } seq_undo_t;

typedef struct {
    bio_t *e;
    seq_scene_t *sc;                            /* SEQ_NSCENE of them (in the pool) */
    volatile uint32_t cur, next;                /* the scene that plays, the one asked for (on the next bar) */
    /* the transport (ISR) */
    volatile uint32_t playing, ticks, frac;     /* ticks since PLAY, its fraction (Q16) */
    volatile uint32_t step_seen, bar;
    uint32_t cursor[BIO_NORG], ver_seen[BIO_NORG];
    volatile uint32_t ver[BIO_NORG];            /* (main) a track was edited: the ISR looks again */
    uint32_t off_t[BIO_NORG];                   /* the sounding note's end (ticks since PLAY + 1; 0: none) */
    uint8_t off_note[BIO_NORG];
    uint32_t rat_t[BIO_NORG], rat_left[BIO_NORG], rat_dt[BIO_NORG];   /* a ratchet's next pulse */
    uint8_t rat_note[BIO_NORG], rat_vel[BIO_NORG];
    uint32_t ghosts_step, ghost_tick[32];
    uint8_t ghost_org[32], nghost, nghost_last;
    uint32_t ghost_last_tick[32];
    uint8_t ghost_last_org[32];
    bio_tur_t tur;
    volatile uint32_t tur_reset;
    /* the live input (main -> ISR) */
    seq_in_t q[32];
    volatile uint32_t qw, qr;
    /* recording (main sets, ISR reads) */
    volatile uint32_t rec, rec_trk, rec_replace, rec_started, erase_note;   /* erase_note 0xFF: none */
    volatile uint32_t rec_mask;                 /* tracks written in this pass (REPLACE clears them as it goes) */
    volatile uint32_t writing;                  /* a note was written (the red pulse on the screen) */
    /* undo */
    seq_undo_t *undo, *redo;                    /* SEQ_NUNDO, SEQ_NREDO (in the pool) */
    uint32_t nundo, nredo, undo_head;
    uint16_t group, pass_group;
    uint32_t pass_snap;                         /* tracks saved for undo in this pass */
    volatile uint32_t sound_seen;               /* the scene whose sound the engine has */
    volatile uint32_t inc32;                    /* ticks (Q16) a sub-block, from the tempo */
} seq_t;

/* ---- quantise and swing: where an event plays (ticks into its track, 0..len; len itself is the next loop's 0) */
static uint32_t seq_eff(const seq_scene_t *sc, const seq_track_t *t, uint32_t raw)
{
    uint32_t g = SEQ_QGRID[t->quant < 5u ? t->quant : 0u], q = raw, L = (uint32_t)t->len * SEQ_TPS, pair, in, d;
    if (g) {
        uint32_t G = (raw + g / 2u) / g * g;    /* the nearest grid point */
        q = raw + (uint32_t)(((int32_t)G - (int32_t)raw) * (int32_t)t->qstr / 100);
    }
    if (sc->swing) {                            /* swing: the off-beat 16ths later, the pair warped (monotonic) */
        d = (uint32_t)sc->swing * (SEQ_TPS / 3u) / 100u;   /* up to a triplet's */
        pair = q / (2u * SEQ_TPS) * (2u * SEQ_TPS);
        in = q - pair;
        q = pair + (in < SEQ_TPS ? in * (SEQ_TPS + d) / SEQ_TPS : SEQ_TPS + d + (in - SEQ_TPS) * (SEQ_TPS - d) / SEQ_TPS);
    }
    return q > L ? L : q;
}

static void seq_tempo(seq_t *s)                 /* (ISR or main) the engine's beat from the tempo */
{
    uint32_t b = s->sc[s->cur].bpm10;
    if (b < 400u) b = 400u;
    if (b > 2400u) b = 2400u;
    s->inc32 = b * 997288u >> 15;               /* ticks a sub-block, Q16 (96 x 4 x BPM / 60 / 44100 x 32) */
    s->e->beat_inc = s->inc32 / 384u;
    s->e->spb_q8 = (26460000u / b << 8) + ((26460000u % b) << 8) / b;
}

/* the scene's sound into the engine (once a block: the main loop's edits reach the sound) */
static void seq_sound(seq_t *s)
{
    const seq_scene_t *sc = &s->sc[s->cur];
    uint32_t o, k;
    for (o = 0; o < BIO_NORG; o++)
        for (k = 0; k < BIO_NP; k++) s->e->p[o][k] = sc->p[o][k];
    s->e->mix = sc->mix;
    s->e->fx = sc->fx;
    s->sound_seen = s->cur;
}

/* the first event at or after a (ticks into the track) */
static uint32_t seq_seek(const seq_scene_t *sc, const seq_track_t *t, uint32_t a)
{
    uint32_t i;
    for (i = 0; i < t->n; i++)
        if (seq_eff(sc, t, t->ev[i].start) >= a) break;
    return i;
}

static void seq_del(seq_track_t *t, uint32_t i)
{
    for (; i + 1u < t->n; i++) t->ev[i] = t->ev[i + 1u];
    if (t->n) t->n--;
}

/* a note of a track starts (ISR): the chance, the ratchet, the Turing's WAIL, the organ */
static void seq_fire(seq_t *s, uint32_t trk, const seq_ev_t *ev, uint32_t step)
{
    const seq_scene_t *sc = &s->sc[s->cur];
    const seq_track_t *t = &sc->tr[trk];
    uint32_t ch = (ev->fl >> EV_CH_SH) & 7u, vel = ev->vel, len, rat, seed;
    int32_t note = ev->note;
    const uint8_t *lk = t->lock[step % SEQ_MAXLEN];
    seed = bio_hash(sc->seed ^ (trk * 0x01000193u), (s->bar & 3u) << 8 | (step & 255u));
    if (ch && (bio_hash(seed, ev->start) >> 29) >= 8u - ch) return;   /* the chance said no */
    if (ev->fl & EV_ACC) vel = vel + 30u > 127u ? 127u : vel + 30u;
    if ((sc->tur.wail & 1u && trk == O_SPASM) || (sc->tur.wail & 2u && trk == O_VOICE))
        note = bio_tur_wail(&s->tur, &sc->tur, note);
    len = (uint32_t)(ev->len ? ev->len : SEQ_TPS) * (s->e->spb_q8 >> 8) / 384u;   /* in samples */
    bio_trigger(s->e, trk, trk < 4u ? note - 60 : note, vel, len, lk, seed);
    if (trk >= 4u) {
        s->off_t[trk] = s->ticks + (ev->len ? ev->len : SEQ_TPS) + 1u;
        s->off_note[trk] = (uint8_t)note;
    } else if (trk == O_CLAW) {
        s->off_t[trk] = s->ticks + (ev->len ? ev->len : SEQ_TPS) + 1u;
        s->off_note[trk] = (uint8_t)note;
    }
    rat = (ev->fl >> EV_RAT_SH) & 3u;
    if (rat) {                                  /* the ratchet: more pulses inside the note */
        uint32_t span = ev->len && ev->len < SEQ_TPS ? ev->len : SEQ_TPS;
        s->rat_left[trk] = rat;
        s->rat_dt[trk] = span / (rat + 1u);
        s->rat_t[trk] = s->ticks + s->rat_dt[trk];
        s->rat_note[trk] = (uint8_t)note;
        s->rat_vel[trk] = (uint8_t)(vel * 3u / 4u);
    }
}

/* the transport, one sub-block (the engine's tick, in the audio interrupt) */
static void seq_tick(bio_t *e, void *ctx)
{
    seq_t *s = (seq_t *)ctx;
    seq_scene_t *sc;
    uint32_t t0, t1, trk;
    (void)e;
    if ((s->e->frames & 255u) == 0u || s->sound_seen != s->cur) {
        seq_tempo(s);
        seq_sound(s);
    }
    /* the keys and MIDI, at once */
    while (s->qr != s->qw) {
        seq_in_t in = s->q[s->qr & 31u];
        s->qr++;
        if (in.on) {
            uint32_t len = in.org == O_NOISE || (in.org < 4u && s->e->p[in.org][P_D8] >= 50u) ? 0xFFFFFFFFu   /* (a perc voice:
                                                   held until the key is let go) */
                         : in.org == O_CLAW ? BIO_MS(300) : BIO_MS(125);
            bio_trigger(s->e, in.org, in.org < 4u ? (int32_t)in.note - 60 : (int32_t)in.note, in.vel, len, 0,
                        bio_hash(s->sc[s->cur].seed, in.note * 131u + in.org + s->e->frames));
            if (in.org >= 4u) s->off_t[in.org] = 0;   /* a held key: no sequenced end cuts it */
        } else {
            bio_release(s->e, in.org, in.note);
            if (in.org == O_CLAW && s->e->drum[O_CLAW].awake && s->e->drum[O_CLAW].t < s->e->drum[O_CLAW].f_att + s->e->drum[O_CLAW].f_hold) {
                bio_drum_t *d = &s->e->drum[O_CLAW];   /* the claw lets go: the drag ends (45 ms at least) */
                uint32_t h = d->t > d->f_att ? d->t - d->f_att : 0;
                d->f_hold = h < BIO_MS(45) ? BIO_MS(45) : h;
            }
        }
    }
    if (s->tur_reset) {
        s->tur_reset = 0;
        bio_tur_reset(&s->tur, &s->sc[s->cur].tur);
    }
    if (!s->playing) {
        s->ticks = s->frac = 0;
        goto nerve;
    }
    t0 = s->ticks;
    s->frac += s->inc32;
    t1 = t0 + (s->frac >> 16);
    s->frac &= 0xFFFFu;
    /* the next scene, on the bar */
    if (s->next != s->cur && t1 / SEQ_BAR != t0 / SEQ_BAR) {
        uint32_t o;
        for (o = 0; o < BIO_NORG; o++) bio_release(s->e, o, 255u);
        s->cur = s->next;
        for (o = 0; o < BIO_NORG; o++) s->ver_seen[o] = ~0u;
        seq_tempo(s);
        seq_sound(s);
    }
    sc = &s->sc[s->cur];
    if (t1 / SEQ_BAR != t0 / SEQ_BAR || t0 == 0u) s->bar = t1 / SEQ_BAR;
    /* the NERVOUS TAPE's clock, and RATTLE's ghosts on its gates */
    {
        uint32_t div = BIO_TUR_DIV[sc->tur.clock & 3u], c;
        for (c = (t0 + div - 1u) / div * div; c < t1 || (t0 == 0u && c == 0u && t1 == 0u); c += div) {
            bio_tur_clock(&s->tur, &sc->tur);
            if (sc->tur.rattle) {               /* RATTLE: bit 3 AND NOT bit 1 -> TEETH; bit 5 AND bit 0 -> CLAW */
                uint32_t vel = 30u + (uint32_t)sc->tur.rattle * 50u / 100u, st = c / SEQ_TPS;
                if (s->ghosts_step != st) { s->ghosts_step = st; }
                if (bio_tur_bit(&s->tur, 3) && !bio_tur_bit(&s->tur, 1) && !sc->tr[O_TEETH].mute) {
                    bio_trigger(s->e, O_TEETH, -2, vel, 0, 0, bio_hash(sc->seed, c));
                    if (s->nghost < 32u) { s->ghost_tick[s->nghost] = c; s->ghost_org[s->nghost++] = O_TEETH; }
                }
                if (bio_tur_bit(&s->tur, 5) && bio_tur_bit(&s->tur, 0) && !sc->tr[O_CLAW].mute) {
                    bio_trigger(s->e, O_CLAW, 0, vel, BIO_MS(60), 0, bio_hash(sc->seed, c + 7u));
                    if (s->nghost < 32u) { s->ghost_tick[s->nghost] = c; s->ghost_org[s->nghost++] = O_CLAW; }
                }
            }
            if (c == 0u && t1 == 0u) break;
        }
        if (t1 / SEQ_BAR != t0 / SEQ_BAR) {     /* a bar gone: its ghosts are what FUSE would write */
            uint32_t i;
            for (i = 0; i < s->nghost; i++) { s->ghost_last_tick[i] = s->ghost_tick[i]; s->ghost_last_org[i] = s->ghost_org[i]; }
            s->nghost_last = s->nghost;
            s->nghost = 0;
        }
    }
    /* the tracks: the events in [t0, t1) of each, by its own length */
    for (trk = 0; trk < BIO_NORG; trk++) {
        seq_track_t *t = &sc->tr[trk];
        uint32_t L = (uint32_t)(t->len ? t->len : 16u) * SEQ_TPS, a = t0 % L, w = t1 - t0, b = a + w, pass;
        if (s->ver_seen[trk] != s->ver[trk]) {
            s->ver_seen[trk] = s->ver[trk];
            s->cursor[trk] = seq_seek(sc, t, a);
        }
        /* REPLACE: each step the playhead enters on a track being written is cleared (not what was just played) */
        if (s->rec && s->rec_replace && s->rec_started && (s->rec_mask >> trk & 1u) && w &&
            (b - 1u) / SEQ_TPS != (a ? (a - 1u) / SEQ_TPS : ~0u)) {
            uint32_t st = (b - 1u) / SEQ_TPS % (L / SEQ_TPS), i = 0, k;
            while (i < t->n) {
                uint32_t q = seq_eff(sc, t, t->ev[i].start) / SEQ_TPS % (L / SEQ_TPS);
                if (q == st && !(t->ev[i].fl & EV_FRESH) && t->ev[i].len) {
                    seq_del(t, i);
                    if (s->cursor[trk] > i) s->cursor[trk]--;
                } else {
                    i++;
                }
            }
            for (k = 0; k < 4u; k++) t->lock[st][k] = BIO_NOLOCK;
        }
        if (!w) continue;
        for (pass = 0; pass < 2u; pass++) {
            uint32_t hi = b < L ? b : L;
            while (s->cursor[trk] < t->n) {
                seq_ev_t *ev = &t->ev[s->cursor[trk]];
                uint32_t q = seq_eff(sc, t, ev->start);
                if (q >= hi && !(pass == 0u && b >= L && q >= L)) break;
                if (q < a && pass == 0u) { s->cursor[trk]++; continue; }   /* (behind: a re-seek's edge) */
                if (ev->fl & EV_FRESH) {        /* just played live: it sounded already */
                    ev->fl &= (uint8_t)~EV_FRESH;
                    s->cursor[trk]++;
                    continue;
                }
                if (s->rec && s->erase_note == ev->note && s->rec_trk == trk && ev->len) {   /* ERASE as it passes */
                    seq_del(t, s->cursor[trk]);
                    continue;
                }
                if (!t->mute && ev->len) seq_fire(s, trk, ev, q / SEQ_TPS % SEQ_MAXLEN);
                s->cursor[trk]++;
            }
            if (b < L) break;
            s->cursor[trk] = 0;                 /* round the loop (also when the window ends right on it) */
            a = 0;
            b -= L;
        }
    }
    /* the notes' ends, the ratchets */
    for (trk = 0; trk < BIO_NORG; trk++) {
        if (s->off_t[trk] && s->off_t[trk] <= t1 + 1u) {
            s->off_t[trk] = 0;
            if (trk == O_CLAW) {
                bio_drum_t *d = &s->e->drum[O_CLAW];
                if (d->awake && d->t < d->f_att + d->f_hold) { uint32_t h = d->t > d->f_att ? d->t - d->f_att : 0; d->f_hold = h < BIO_MS(45) ? BIO_MS(45) : h; }
            } else {
                bio_release(s->e, trk, s->off_note[trk]);
            }
        }
        if (s->rat_left[trk] && s->rat_t[trk] < t1) {
            s->rat_left[trk]--;
            s->rat_t[trk] += s->rat_dt[trk];
            bio_trigger(s->e, trk, trk < 4u ? (int32_t)s->rat_note[trk] - 60 : s->rat_note[trk], s->rat_vel[trk],
                        BIO_MS(40), sc->tr[trk].lock[(t0 / SEQ_TPS) % SEQ_MAXLEN], bio_hash(sc->seed, t1 + trk));
        }
    }
    s->ticks = t1;
nerve:
    /* the nerve's routes into the engine: CV8 smoothed (~20 ms), times each route's depth */
    {
        const bio_tur_set_t *ts = &s->sc[s->cur].tur;
        int32_t cv = (int32_t)bio_tur_cv8(&s->tur) * 128, c;
        s->tur.cv += (cv - s->tur.cv) >> 5;
        c = s->tur.cv - 16384;                  /* -0.5 .. 0.5 */
        s->e->nerve_tex = (int32_t)(((int64_t)c * 50 * bio_tur_eff(ts, ts->r_rot)) >> 29);
        s->e->nerve_scrape = (int32_t)(((int64_t)c * 125829 / 2 * bio_tur_eff(ts, ts->r_scrape)) >> 29);   /* 900..3400 Hz */
        s->e->nerve_metal = (int32_t)(((int64_t)c * 8150 * 2 * bio_tur_eff(ts, ts->r_metal)) >> 29);        /* ±9 % */
        s->e->nerve_air = (int32_t)(((int64_t)s->tur.cv * 5898 >> 15) * bio_tur_eff(ts, ts->r_air) >> 15);  /* +0..18 % */
        s->e->nerve_cv = (int32_t)(((int64_t)c * 2 * bio_tur_eff(ts, ts->r_rot)) >> 15);
    }
}

/* ---- the main loop's side */
static void seq_attach(seq_t *s, bio_t *e, seq_scene_t *scenes, seq_undo_t *undo, seq_undo_t *redo)
{
    uint32_t k;
    s->e = e;
    s->sc = scenes;
    s->undo = undo;
    s->redo = redo;
    s->erase_note = 0xFFu;
    for (k = 0; k < BIO_NORG; k++) s->ver_seen[k] = ~0u;
    bio_tur_reset(&s->tur, &scenes[0].tur);
    e->tick = seq_tick;
    e->tick_ctx = s;
    seq_tempo(s);
    seq_sound(s);
}

static void seq_live(seq_t *s, uint32_t org, uint32_t note, uint32_t vel, uint32_t on)
{
    seq_in_t in;
    if (s->qw - s->qr >= 32u) return;
    in.org = (uint8_t)org;
    in.note = (uint8_t)note;
    in.vel = (uint8_t)vel;
    in.on = (uint8_t)on;
    s->q[s->qw & 31u] = in;
    s->qw++;
}

static void seq_play(seq_t *s)
{
    uint32_t k;
    SEQ_LOCK();
    s->ticks = s->frac = 0;
    s->bar = 0;
    s->nghost = 0;
    for (k = 0; k < BIO_NORG; k++) { s->cursor[k] = 0; s->ver_seen[k] = ~0u; s->off_t[k] = 0; s->rat_left[k] = 0; }
    bio_tur_reset(&s->tur, &s->sc[s->cur].tur);
    for (k = 0; k < 4u; k++) {                  /* the same start, the same sound */
        s->e->tone[k].rnd = bio_hash(s->sc[s->cur].seed, k);
        s->e->tone[k].pa = s->e->tone[k].pb = s->e->tone[k].pc = 0;
        s->e->tone[k].gt = 0;
    }
    s->e->rnd = 0x2545F491u;
    s->e->rlfo = 0;                             /* the reverb's movement and the crush's sampling, from their start */
    s->e->c_cnt = 0;
    s->e->drift = s->e->drift_to = 0;
    s->e->drift_t = 0;
    s->e->beat_q16 = 0;
    s->playing = 1;
    SEQ_UNLOCK();
}

/* close what is still held on a track (REC off, STOP): its length up to now */
static void seq_close_open(seq_t *s, uint32_t trk, uint32_t tick)
{
    seq_track_t *t = &s->sc[s->cur].tr[trk];
    uint32_t i, L = (uint32_t)t->len * SEQ_TPS;
    for (i = 0; i < t->n; i++)
        if (!t->ev[i].len) {
            uint32_t d = (tick + L - t->ev[i].start) % L;
            t->ev[i].len = (uint16_t)(d < 12u ? 12u : d);
        }
}

static void seq_stop(seq_t *s)
{
    uint32_t k;
    SEQ_LOCK();
    for (k = 0; k < BIO_NORG; k++) seq_close_open(s, k, s->ticks % ((uint32_t)s->sc[s->cur].tr[k].len * SEQ_TPS));
    s->playing = 0;
    for (k = 0; k < BIO_NORG; k++) { s->off_t[k] = 0; s->rat_left[k] = 0; }
    SEQ_UNLOCK();
    for (k = 0; k < BIO_NORG; k++) bio_release(s->e, k, 255u);   /* (safe from here: one store each) */
}

/* the transport's tick now, into a track (the main loop's view: the ISR's ticks and frames since) */
static uint32_t seq_now(const seq_t *s, uint32_t trk, uint32_t extra_frames)
{
    uint32_t L = (uint32_t)s->sc[s->cur].tr[trk].len * SEQ_TPS;
    uint32_t t = s->ticks + (uint32_t)((((uint64_t)s->inc32 * extra_frames >> 5) + s->frac) >> 16);   /* (>> 5: a sub-block) */
    return L ? t % L : 0;
}

/* ---- undo: a track as it was before an edit; one pass or one gesture is one group */
static void seq_snap(seq_t *s, uint32_t trk)
{
    seq_undo_t *u;
    if (s->nundo == SEQ_NUNDO) {                /* the oldest goes */
        uint32_t i;
        for (i = 1; i < SEQ_NUNDO; i++) s->undo[i - 1u] = s->undo[i];
        s->nundo--;
    }
    u = &s->undo[s->nundo++];
    u->trk = (int16_t)trk;
    u->t = s->sc[s->cur].tr[trk];
    u->group = s->group;
    s->nredo = 0;
}
static void seq_begin(seq_t *s) { s->group++; s->pass_snap = 0; }   /* a new gesture */
static void seq_touch(seq_t *s, uint32_t trk)   /* about to change a track in this gesture */
{
    if (!(s->pass_snap >> trk & 1u)) {
        s->pass_snap |= 1u << trk;
        seq_snap(s, trk);
    }
}
static int seq_undo(seq_t *s)
{
    uint16_t g;
    if (!s->nundo) return 0;
    g = s->undo[s->nundo - 1u].group;
    SEQ_LOCK();
    while (s->nundo && s->undo[s->nundo - 1u].group == g) {
        seq_undo_t *u = &s->undo[--s->nundo];
        if (s->nredo < SEQ_NREDO) {
            s->redo[s->nredo].trk = u->trk;
            s->redo[s->nredo].t = s->sc[s->cur].tr[u->trk];
            s->redo[s->nredo].group = g;
            s->nredo++;
        }
        s->sc[s->cur].tr[u->trk] = u->t;
        s->ver[u->trk]++;
    }
    SEQ_UNLOCK();
    s->pass_snap = 0;
    return 1;
}
static int seq_redo(seq_t *s)
{
    uint16_t g;
    if (!s->nredo) return 0;
    g = s->redo[s->nredo - 1u].group;
    SEQ_LOCK();
    while (s->nredo && s->redo[s->nredo - 1u].group == g) {
        seq_undo_t *r = &s->redo[--s->nredo];
        if (s->nundo < SEQ_NUNDO) {
            s->undo[s->nundo].trk = r->trk;
            s->undo[s->nundo].t = s->sc[s->cur].tr[r->trk];
            s->undo[s->nundo].group = g;
            s->nundo++;
        }
        s->sc[s->cur].tr[r->trk] = r->t;
        s->ver[r->trk]++;
    }
    SEQ_UNLOCK();
    s->pass_snap = 0;
    return 1;
}

/* insert an event (sorted by start); the same note at the same quantised start is replaced. -1: the track is full */
static int seq_insert(seq_t *s, uint32_t trk, uint32_t start, uint32_t len, uint32_t note, uint32_t vel, uint32_t fl)
{
    seq_scene_t *sc = &s->sc[s->cur];
    seq_track_t *t = &sc->tr[trk];
    uint32_t i, L = (uint32_t)t->len * SEQ_TPS, g = SEQ_QGRID[t->quant < 5u ? t->quant : 0u], q;
    if (start >= L) start %= L;
    if (!g) g = SEQ_TPS;                        /* the same moment: the same grid point (the step, with quantise off) */
    q = (start + g / 2u) / g % (L / g ? L / g : 1u);
    for (i = 0; i < t->n; i++)
        if (t->ev[i].note == note && (t->ev[i].start + g / 2u) / g % (L / g ? L / g : 1u) == q) {   /* no double note-on: this one wins */
            t->ev[i].start = (uint16_t)start;
            t->ev[i].len = (uint16_t)len;
            t->ev[i].vel = (uint8_t)vel;
            t->ev[i].fl = (uint8_t)fl;
            while (i > 0 && t->ev[i - 1u].start > t->ev[i].start) { seq_ev_t x = t->ev[i]; t->ev[i] = t->ev[i - 1u]; t->ev[--i] = x; }
            while (i + 1u < t->n && t->ev[i + 1u].start < t->ev[i].start) { seq_ev_t x = t->ev[i]; t->ev[i] = t->ev[i + 1u]; t->ev[++i] = x; }
            s->ver[trk]++;
            return (int)i;
        }
    if (t->n >= SEQ_NEV) return -1;
    i = t->n;
    while (i > 0 && t->ev[i - 1u].start > start) { t->ev[i] = t->ev[i - 1u]; i--; }
    t->ev[i].start = (uint16_t)start;
    t->ev[i].len = (uint16_t)len;
    t->ev[i].note = (uint8_t)note;
    t->ev[i].vel = (uint8_t)vel;
    t->ev[i].fl = (uint8_t)fl;
    t->ev[i].pad = 0;
    t->n++;
    s->ver[trk]++;
    return (int)i;
}

/* LIVE REC: a key went down / up on a track, at tick (into the track) */
static int seq_rec_on(seq_t *s, uint32_t trk, uint32_t note, uint32_t vel, uint32_t tick)
{
    int r;
    SEQ_LOCK();
    seq_touch(s, trk);
    s->rec_mask |= 1u << trk;
    s->rec_started = 1;
    r = seq_insert(s, trk, tick, 0, note, vel, EV_FRESH);
    s->writing++;
    SEQ_UNLOCK();
    return r;
}
static void seq_rec_off(seq_t *s, uint32_t trk, uint32_t note, uint32_t tick)
{
    seq_track_t *t = &s->sc[s->cur].tr[trk];
    uint32_t i, L = (uint32_t)t->len * SEQ_TPS;
    SEQ_LOCK();
    for (i = 0; i < t->n; i++)
        if (!t->ev[i].len && t->ev[i].note == note) {
            uint32_t d = (tick + L - t->ev[i].start) % L;
            t->ev[i].len = (uint16_t)(d < 12u ? 12u : d);
            s->ver[trk]++;
        }
    SEQ_UNLOCK();
}

/* the events that play in a step (quantised): their mask (bit i: event i), up to 32 */
static uint32_t seq_step_events(const seq_t *s, uint32_t trk, uint32_t step)
{
    const seq_scene_t *sc = &s->sc[s->cur];
    const seq_track_t *t = &sc->tr[trk];
    uint32_t i, m = 0, L = (uint32_t)t->len * SEQ_TPS;
    for (i = 0; i < t->n && i < 32u; i++) {
        uint32_t q = seq_eff(sc, t, t->ev[i].start) % (L ? L : 1u);
        if ((q + SEQ_TPS / 2u) / SEQ_TPS % t->len == step) m |= 1u << i;
    }
    return m;
}

/* STEP REC: a note at a step, its length in steps */
static int seq_step_write(seq_t *s, uint32_t trk, uint32_t step, uint32_t note, uint32_t vel, uint32_t len_steps)
{
    int r;
    SEQ_LOCK();
    seq_touch(s, trk);
    r = seq_insert(s, trk, step * SEQ_TPS, (len_steps ? len_steps : 1u) * SEQ_TPS, note, vel, 0);
    s->writing++;
    SEQ_UNLOCK();
    return r;
}
/* a step on or off (SEQ held + a white key): off if anything plays there, else the note */
static void seq_step_toggle(seq_t *s, uint32_t trk, uint32_t step, uint32_t note, uint32_t vel)
{
    seq_track_t *t = &s->sc[s->cur].tr[trk];
    uint32_t m = seq_step_events(s, trk, step), i;
    SEQ_LOCK();
    seq_touch(s, trk);
    if (m) {
        for (i = 32u; i-- > 0u;) if (m >> i & 1u) seq_del(t, i);
        s->ver[trk]++;
    } else {
        seq_insert(s, trk, step * SEQ_TPS, SEQ_TPS, note, vel, 0);
    }
    SEQ_UNLOCK();
}
/* ERASE a note's events at a step (STEP REC with SHIFT) */
static void seq_step_erase(seq_t *s, uint32_t trk, uint32_t step, uint32_t note)
{
    seq_track_t *t = &s->sc[s->cur].tr[trk];
    uint32_t m = seq_step_events(s, trk, step), i;
    SEQ_LOCK();
    seq_touch(s, trk);
    for (i = 32u; i-- > 0u;) if ((m >> i & 1u) && t->ev[i].note == note) seq_del(t, i);
    s->ver[trk]++;
    SEQ_UNLOCK();
}
static void seq_lock_set(seq_t *s, uint32_t trk, uint32_t step, uint32_t k, uint32_t v)
{
    seq_track_t *t = &s->sc[s->cur].tr[trk];
    if (step >= SEQ_MAXLEN || k >= 4u) return;
    seq_touch(s, trk);
    t->lock[step][k] = (uint8_t)(v > 100u ? 100u : v);
    s->writing++;
}

/* FUSE: the ghosts of the last bar into their tracks, as notes (one undo) */
static uint32_t seq_fuse(seq_t *s)
{
    uint32_t i, n = 0;
    seq_begin(s);
    for (i = 0; i < s->nghost_last; i++) {
        uint32_t o = s->ghost_last_org[i], L = (uint32_t)s->sc[s->cur].tr[o].len * SEQ_TPS;
        SEQ_LOCK();
        seq_touch(s, o);
        if (seq_insert(s, o, s->ghost_last_tick[i] % L, o == O_CLAW ? 48u : 24u, o == O_TEETH ? 58u : 60u, 70u, 0) >= 0) n++;
        SEQ_UNLOCK();
    }
    return n;
}

/* ---- the scenes */
static void seq_track_clear(seq_track_t *t, uint32_t len)
{
    uint32_t i, k;
    t->n = 0;
    t->len = (uint8_t)len;
    t->quant = 2;                               /* 1/16 */
    t->qstr = 75;                               /* at 75 % */
    t->mute = 0;
    for (i = 0; i < SEQ_MAXLEN; i++) for (k = 0; k < 4u; k++) t->lock[i][k] = BIO_NOLOCK;
}
static void seq_scene_blank(seq_scene_t *sc, uint32_t seed)
{
    uint32_t o, k;
    sc->magic = SEQ_MAGIC;
    sc->bpm10 = 1200;
    sc->swing = 0;
    sc->steplen = 1;
    sc->seed = seed;
    for (o = 0; o < BIO_NORG; o++) {
        seq_track_clear(&sc->tr[o], 16);
        for (k = 0; k < BIO_NP; k++) sc->p[o][k] = BIO_P_INIT[o][k];
    }
    sc->mix = BIO_MIX_INIT;
    sc->fx = BIO_FX_INIT;
    sc->tur = BIO_TUR_INIT;
    for (k = 0; k < sizeof sc->name; k++) sc->name[k] = 0;
}
static void seq_put(seq_scene_t *sc, uint32_t trk, uint32_t step, uint32_t len_ticks, uint32_t note, uint32_t vel, uint32_t fl)
{
    seq_track_t *t = &sc->tr[trk];
    uint32_t i = t->n;
    if (t->n >= SEQ_NEV) return;
    while (i > 0 && t->ev[i - 1u].start > step * SEQ_TPS) { t->ev[i] = t->ev[i - 1u]; i--; }
    t->ev[i].start = (uint16_t)(step * SEQ_TPS);
    t->ev[i].len = (uint16_t)len_ticks;
    t->ev[i].note = (uint8_t)note;
    t->ev[i].vel = (uint8_t)vel;
    t->ev[i].fl = (uint8_t)fl;
    t->ev[i].pad = 0;
    t->n++;
}
/* RITUAL 00, THE AWAKENING: 120 BPM, 16 steps, D Phrygian; the idea document's grid (9) */
static void seq_scene_awaken(seq_scene_t *sc)
{
    static const uint8_t HEART[] = {0, 4, 8, 12}, BONE[] = {4, 12}, TEETH[] = {2, 6, 10, 14}, CLAW[] = {7, 15};
    uint32_t i;
    seq_scene_blank(sc, 0xA3A4E0u);
    for (i = 0; i < 4u; i++) seq_put(sc, O_HEART, HEART[i], SEQ_TPS, 60, 110, EV_ANCHOR);
    for (i = 0; i < 2u; i++) seq_put(sc, O_BONE, BONE[i], SEQ_TPS, 60, 100, 0);
    for (i = 0; i < 4u; i++) seq_put(sc, O_TEETH, TEETH[i], SEQ_TPS / 2u, 60, 84, 0);
    for (i = 0; i < 2u; i++) seq_put(sc, O_CLAW, CLAW[i], SEQ_TPS * 2u, 60, 92, 0);
    /* BASS: D1 on 1 (4 steps), Eb1 on 7 (2), D1 on 9 (4), C1 on 15 (2), used sparingly */
    seq_put(sc, O_BASS, 0, 4u * SEQ_TPS, 26, 100, 0);
    seq_put(sc, O_BASS, 6, 2u * SEQ_TPS, 27, 92, 0);
    seq_put(sc, O_BASS, 8, 4u * SEQ_TPS, 26, 100, 0);
    seq_put(sc, O_BASS, 14, 2u * SEQ_TPS, 24, 92, 0);
    /* DRONE holds D2 over the 16 steps, tied round the loop */
    seq_put(sc, O_DRONE, 0, 16u * SEQ_TPS, 38, 90, 0);
    /* SPASM: short nerve impulses, G3 on 12 and 16, 55-75 % */
    seq_put(sc, O_SPASM, 11, SEQ_TPS / 2u, 55, 70, 0);
    seq_put(sc, O_SPASM, 15, SEQ_TPS / 2u, 55, 95, 0);
    /* VOICE: A2, held back: once every fourth bar */
    sc->tr[O_VOICE].len = 64;
    seq_put(sc, O_VOICE, 0, 12u * SEQ_TPS, 45, 80, 0);
    /* NOISE breathes faintly: a long breath over four bars */
    sc->tr[O_NOISE].len = 64;
    seq_put(sc, O_NOISE, 0, 64u * SEQ_TPS, 50, 70, 0);
    sc->tr[O_DRONE].quant = sc->tr[O_VOICE].quant = sc->tr[O_NOISE].quant = 0;
    sc->name[0] = 'A'; sc->name[1] = 'W'; sc->name[2] = 'A'; sc->name[3] = 'K'; sc->name[4] = 'E';
    sc->name[5] = 'N'; sc->name[6] = 'I'; sc->name[7] = 'N'; sc->name[8] = 'G';
}
#endif
