// SPDX-License-Identifier: GPL-3.0
// FM1 Phase 2: licensed GPL-3.0, see LICENSE; third-party code and its licences: NOTICE.md.
/* The drum sequencer (drum track, step 2). Included by app.c (through drumrun.c) and by rack_host.c: no libm, no app
 * globals. The sounds and the MIDI notes go out through three hooks the caller sets (the firmware: rk_drum_hit, a
 * MIDI note on the lane's channel, rk_drum_run; the host check: recorders), so the same code is what runs and what the
 * check measures.
 *
 * 6 lanes (KICK, SNARE, C HAT, O HAT, CLAP, PERC) of 16 steps, one pattern (A). A step is 16 bits: on, force (GHOST SOFT
 * NORM HARD ACC), ratchet 1-4, chance (12 25 50 75 90 100 %), nudge (-4..+3 of a 64th of the step). The grid is one
 * 1/16 a step, at the tempo or the MIDI clock's (6 ticks), starting with the sequencer's PLAY and stopping with it.
 * A hit is struck at the block that holds its frame (1 ms; the voices render a block later). Static RAM: the pattern
 * 192 B, the hits waiting 192 B, the state about 60 B.
 * The kit (the sounds) is in flash: 909 ISH. */
#include <stdint.h>

#define DR_LANES 6
#define DR_STEPS 16
#define DR_NQ 24                              /* hits waiting (a step of 6 lanes with ratchets 4 is 24) */
enum { DF_GHOST, DF_SOFT, DF_NORM, DF_HARD, DF_ACC };
#define DRS_ON 1u
#define DRS_FORCE(s) (((s) >> 1) & 7u)
#define DRS_RATCH(s) ((int)(((s) >> 4) & 3u) + 1)
#define DRS_CHANCE(s) (((s) >> 6) & 7u)        /* index into DR_CHANCE */
#define DRS_NUDGE(s) ((int)(((s) >> 9) & 7u) - 4)
#define DRS_MK(on, force, ratch, chance, nudge) \
    ((uint16_t)((on) | (uint16_t)(force) << 1 | (uint16_t)((ratch) - 1) << 4 | (uint16_t)(chance) << 6 | (uint16_t)((nudge) + 4) << 9))
#define DRS_NEW DRS_MK(1u, DF_NORM, 1, 5, 0)    /* a step turned on: NORM, one hit, always, on the grid */
static const uint8_t DR_CHANCE[6] = {12, 25, 50, 75, 90, 100};
static const uint8_t DR_VEL[5] = {25, 50, 80, 105, 127};          /* MIDI velocity of each force */
static const char *const DR_FORCE[5] = {"GHOST", "SOFT", "NORM", "HARD", "ACC"};
static const char *const DR_LNAME[DR_LANES] = {"KICK", "SNARE", "C HAT", "O HAT", "CLAP", "PERC"};

/* the kit: a lane's voice (the hats share one, the clap and the perc one), model (engine/rack.cc DM_*), pitch (a MIDI
 * note), the two parameters, DECAY (10 ms), LEVEL, PAN, SEND (percent) and the General MIDI note it sends */
typedef struct { uint8_t voice, model, pitch, p1, p2, decay_cs, level, send; int8_t pan; uint8_t gm; } dr_lane_t;
static const dr_lane_t DR_KIT909[DR_LANES] = {   /* 909 ISH */
    {0, 0, 36, 50, 50, 45, 100, 5, 0, 36},    /* KICK:   the kick model */
    {1, 1, 50, 50, 60, 25, 90, 25, 0, 38},    /* SNARE:  the snare model */
    {2, 6, 70, 50, 50, 6, 55, 10, 15, 42},    /* C HAT:  clocked noise (the cheap hat) */
    {2, 6, 70, 50, 50, 35, 55, 10, 15, 46},   /* O HAT:  the same voice, a long decay: a closed hat cuts it */
    {3, 5, 62, 50, 50, 20, 80, 35, -10, 39},  /* CLAP:   filtered noise */
    {3, 0, 55, 40, 50, 25, 75, 15, -20, 45},  /* PERC:   the kick model high: a low tom */
};
#define DR_CH 9                               /* MIDI channel 10 (from 0) */
#define DR_GATE_MS 30

static uint16_t dr_pat[DR_LANES][DR_STEPS];   /* pattern A */
typedef struct { uint32_t at; uint8_t lane, force; } dr_hit_t;
static dr_hit_t dr_q[DR_NQ];
static volatile struct {
    uint8_t run, mute;                        /* playing; the lanes muted (bit per lane: still advancing, not sounding) */
    int8_t cur;                               /* the step playing, -1 stopped */
    uint8_t ring;                             /* the lanes whose MIDI note is on */
    int nq;
    uint32_t next_at, step, ticks, rng;
    uint32_t hits;                            /* struck since the start (the checks') */
    float frac;
} dr = {0, 0, -1, 0, 0, 0, 0, 0, 1, 0, 0.0f};
static uint32_t dr_flash[DR_LANES];           /* when each lane last struck (the screen's: a lit tile) */
static float dr_hz = 44100.0f;                /* frames a second */
static uint32_t dr_off_at[DR_LANES];          /* the frame each lane's MIDI note ends */

static void (*dr_hook_hit)(int voice, int model, float note, float p1, float p2, float decay, float level, float pan, float send);
static void (*dr_hook_midi)(int ch0, int note, int vel);   /* vel 0: note off */
static void (*dr_hook_run)(int on);

static float dr_rand(void)                    /* the chance's draw: its own, so a pattern plays the same on any host */
{
    dr.rng = dr.rng * 1664525u + 1013904223u;
    return (float)(dr.rng >> 8) / 16777216.0f;
}
static int dr_any(void)                       /* 1 when any step is on */
{
    int l, i;
    for (l = 0; l < DR_LANES; l++)
        for (i = 0; i < DR_STEPS; i++)
            if (dr_pat[l][i] & DRS_ON)
                return 1;
    return 0;
}

/* a hit: the voice, and the lane's MIDI note on its channel (an earlier note of the lane ends first) */
static void dr_fire(int lane, int force, uint32_t at)
{
    const dr_lane_t *k = &DR_KIT909[lane];
    float lv = (float)DR_VEL[force > 4 ? 4 : force] / 100.0f * (float)k->level * 0.01f;
    dr.hits++;
    dr_flash[lane] = at;
    if (dr_hook_hit)
        dr_hook_hit(k->voice, k->model, (float)k->pitch, (float)k->p1 * 0.01f, (float)k->p2 * 0.01f, (float)k->decay_cs * 0.01f,
                    lv, (float)k->pan * 0.01f, (float)k->send * 0.01f);
    if (dr_hook_midi) {
        if (dr.ring >> lane & 1u)
            dr_hook_midi(DR_CH, k->gm, 0);
        dr_hook_midi(DR_CH, k->gm, DR_VEL[force > 4 ? 4 : force]);
        dr.ring |= (uint8_t)(1u << lane);
        dr_off_at[lane] = at + (uint32_t)(dr_hz * (float)DR_GATE_MS * 0.001f);
    }
}
/* the hits and note-offs due before the frame end */
static void dr_due(uint32_t end)
{
    int i, j, l;
    for (i = 0; i < dr.nq;) {
        if ((int32_t)(dr_q[i].at - end) < 0) {
            dr_hit_t h = dr_q[i];
            for (j = i; j < dr.nq - 1; j++)
                dr_q[j] = dr_q[j + 1];
            dr.nq--;
            dr_fire(h.lane, h.force, h.at);
        } else {
            i++;
        }
    }
    for (l = 0; l < DR_LANES; l++)
        if ((dr.ring >> l & 1u) && (int32_t)(dr_off_at[l] - end) < 0) {
            dr.ring &= (uint8_t)~(1u << l);
            if (dr_hook_midi)
                dr_hook_midi(DR_CH, DR_KIT909[l].gm, 0);
        }
}
/* the step i, whose frame is tg and which lasts d frames: each lane's step is struck (or its ratchets queued) */
static void dr_step(int i, uint32_t tg, float d)
{
    int l, k;
    dr.cur = (int8_t)i;
    for (l = 0; l < DR_LANES; l++) {
        uint16_t s = dr_pat[l][i];
        int r;
        uint32_t t;
        if (!(s & DRS_ON) || (dr.mute >> l & 1u))
            continue;
        if (DRS_CHANCE(s) < 5 && dr_rand() * 100.0f >= (float)DR_CHANCE[DRS_CHANCE(s) > 5 ? 5 : DRS_CHANCE(s)])
            continue;
        t = tg + (uint32_t)(int32_t)((float)DRS_NUDGE(s) / 64.0f * d);
        r = DRS_RATCH(s);
        for (k = 0; k < r && dr.nq < DR_NQ; k++) {
            dr_q[dr.nq].at = t + (uint32_t)((float)k * d / (float)r);
            dr_q[dr.nq].lane = (uint8_t)l;
            dr_q[dr.nq].force = (uint8_t)DRS_FORCE(s);
            dr.nq++;
        }
    }
}
static void dr_start(uint32_t now)
{
    if (dr.run)
        return;
    dr.run = 1;
    dr.step = 0;
    dr.ticks = 0;
    dr.next_at = now;
    dr.frac = 0.0f;
    dr.nq = 0;
    dr.hits = 0;
    dr.rng = 12345u;                          /* the same draws on every start */
    if (dr_hook_run)
        dr_hook_run(1);
}
static void dr_stop(void)
{
    int l;
    if (!dr.run)
        return;
    dr.run = 0;
    dr.cur = -1;
    dr.nq = 0;
    for (l = 0; l < DR_LANES; l++)
        if (dr.ring >> l & 1u && dr_hook_midi)
            dr_hook_midi(DR_CH, DR_KIT909[l].gm, 0);
    dr.ring = 0;
    if (dr_hook_run)
        dr_hook_run(0);
}
/* each block, on the tempo: d frames a step (the caller's: the tempo's 16th); the steps that begin before the block ends */
static void dr_block(uint32_t now, uint32_t n, float d)
{
    uint32_t end = now + n;
    if (dr.run) {
        /* a step is worked out a little ahead (the largest nudge back, 4/64 of a step), so a hit nudged early is not late */
        while ((int32_t)(dr.next_at - end) < (int32_t)(d * (4.0f / 64.0f))) {
            uint32_t di = (uint32_t)d;
            if (dr.step >= DR_STEPS)
                dr.step = 0;
            dr_step((int)dr.step, dr.next_at, d);
            dr.frac += d - (float)di;         /* the fraction of a frame carried: the tempo holds over bars */
            if (dr.frac >= 1.0f) { di++; dr.frac -= 1.0f; }
            dr.next_at += di;
            dr.step++;
        }
    }
    dr_due(end);
}
/* MIDI clock: a tick, at frame at; every 6th is a step */
static void dr_tick(uint32_t at, float d)
{
    if (!dr.run)
        return;
    if (dr.ticks++ % 6u == 0u) {
        if (dr.step >= DR_STEPS)
            dr.step = 0;
        dr_step((int)dr.step, at, d);
        dr.step++;
    }
}
/* only the hits due (the MIDI clock's: the steps come from dr_tick) */
static void dr_flush(uint32_t now, uint32_t n) { dr_due(now + n); }

/* ---- editing (the page's) */
static void dr_toggle(int lane, int i)
{
    dr_pat[lane][i] = (dr_pat[lane][i] & DRS_ON) ? 0 : DRS_NEW;
}
static int dr_clamp(int v, int lo, int hi) { return v < lo ? lo : v > hi ? hi : v; }
static void dr_adjust(int lane, int i, int what, int d)   /* what: 0 force, 1 ratchet, 2 chance, 3 nudge; a step off is turned on first */
{
    uint16_t s = dr_pat[lane][i];
    int f, r, c, n;
    if (!(s & DRS_ON))
        s = DRS_NEW;
    f = (int)DRS_FORCE(s); r = DRS_RATCH(s); c = (int)DRS_CHANCE(s); n = DRS_NUDGE(s);
    if (what == 0) f = dr_clamp(f + d, 0, 4);
    else if (what == 1) r = dr_clamp(r + d, 1, 4);
    else if (what == 2) c = dr_clamp(c + d, 0, 5);
    else n = dr_clamp(n + d, -4, 3);
    dr_pat[lane][i] = DRS_MK(1u, f, r, c, n);
}
static void dr_clear_lane(int lane) { memset(dr_pat[lane], 0, sizeof dr_pat[lane]); }
/* the factory beat (909 ISH): four on the floor, a backbeat of snare and clap, offbeat hats, an open hat, a ghost kick, two toms */
static void dr_load_beat(void)
{
    static const uint8_t K[] = {0, 4, 8, 12}, S[] = {4, 12}, CH[] = {0, 2, 4, 6, 8, 10, 12}, OH[] = {14}, CL[] = {4, 12}, PC[] = {7, 11};
    int i;
    memset(dr_pat, 0, sizeof dr_pat);
    for (i = 0; i < 4; i++) dr_pat[0][K[i]] = DRS_NEW;
    dr_pat[0][10] = DRS_MK(1u, DF_GHOST, 1, 5, 0);
    for (i = 0; i < 2; i++) dr_pat[1][S[i]] = DRS_MK(1u, DF_HARD, 1, 5, 0);
    for (i = 0; i < 7; i++) dr_pat[2][CH[i]] = DRS_MK(1u, (CH[i] & 3) == 2 ? DF_HARD : DF_SOFT, 1, 5, 0);
    dr_pat[3][OH[0]] = DRS_NEW;
    for (i = 0; i < 2; i++) dr_pat[4][CL[i]] = DRS_MK(1u, DF_SOFT, 1, 5, 0);
    for (i = 0; i < 2; i++) dr_pat[5][PC[i]] = DRS_MK(1u, DF_SOFT, 1, 3, 0);
    dr_pat[5][7] = DRS_MK(1u, DF_NORM, 2, 4, 0);
}
