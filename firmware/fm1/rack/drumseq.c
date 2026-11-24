// SPDX-License-Identifier: GPL-3.0
// FM1 Phase 2: licensed GPL-3.0, see LICENSE; third-party code and its licences: NOTICE.md.
/* The drum sequencer (drum track, steps 2-4). Included by app.c (through drumrun.c) and by rack_host.c: no libm, no app
 * globals. The sounds and the MIDI notes go out through three hooks the caller sets (the firmware: rk_drum_hit, a
 * MIDI note on the lane's channel, rk_drum_run; the host check: recorders), so the same code is what runs and what the
 * check measures.
 *
 * 6 lanes (KICK, SNARE, C HAT, O HAT, CLAP, PERC) of 16 steps, 8 patterns (A-H: one live, a queued one read ahead into dr_stage and taken at the
 * bar line, the rest in the store). Each lane has its own LENGTH, DIVIDE, SHIFT and CHOKE, and the grid runs on ticks (a sixth of a 16th: the MIDI
 * clock's own), so a lane of 12 steps drifts against one of 16, a triplet divide fits, and SWING delays the odd steps. A step is 16 bits: on, force (GHOST SOFT
 * NORM HARD ACC), ratchet 1-4, chance (12 25 50 75 90 100 %), nudge (-4..+3 of a 64th of the step). The grid is one
 * 1/16 a step by default, at the tempo or the MIDI clock's, starting with the sequencer's PLAY and stopping with it.
 * A hit is struck at the block that holds its frame (1 ms; the voices render a block later). Static RAM: the pattern
 * 192 B, its feel 24 B, the queued pattern 216 B, the hits waiting 192 B, the state about 40 B.
 * The kit (the sounds): four factory kits in flash, the live one (and its edits) in RAM. */
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

/* the kit: a lane's model (engine/rack.cc DM_*), pitch (a MIDI note), the two parameters (0-100), DECAY (10 ms), LEVEL (percent),
 * SEND (percent to the FX) and PAN (-100..100). The voice is the lane's (the hats share one, the clap and the perc one) and
 * the General MIDI note it sends is the lane's. Four factory kits are in flash; the live kit (dr_kit) is in RAM, whatever was
 * loaded and edited, and is what SAVE keeps. */
typedef struct { uint8_t model, pitch, p1, p2, decay_cs, level, send; int8_t pan; } dr_snd_t;
static const uint8_t DR_VOICE[DR_LANES] = {0, 1, 2, 2, 3, 3};
static const uint8_t DR_GM[DR_LANES] = {36, 38, 42, 46, 39, 45};   /* the General MIDI notes each lane starts with */
#define DR_NKIT 4
static const char *const DR_KNAME[DR_NKIT] = {"909 ISH", "808 ISH", "LO-FI", "SPARSE"};
/* models: KICK SNARE CYMBAL STRUCK DRUM STRUCK BELL FILTERED NOISE CLOCKED NOISE PARTICLE NOISE TWIN PEAKS (rack.cc DM_*) */
#define DR_NMODEL 9
static const char *const DR_MNAME[DR_NMODEL] = {"KICK", "SNARE", "CYMBAL", "STRUCK DRUM", "STRUCK BELL", "FILTERED NOISE", "CLOCKED NOISE", "PARTICLE NOISE", "TWIN PEAKS"};
static const char *const DR_MSHORT[DR_NMODEL] = {"KICK", "SNARE", "CYMBL", "DRUM", "BELL", "FNOISE", "CLOCK", "PARTCL", "TWIN"};
static const uint8_t DR_MCPU[DR_NMODEL] = {61, 78, 80, 60, 65, 49, 32, 40, 36};   /* points of a core x 10 (the 13 Nov bench: design.md 2.1) */
static const char *const DR_P1[DR_NMODEL] = {"BODY", "TONE", "FILTER", "BALANCE", "BALANCE", "FILTER", "RATE", "DENSITY", "Q"};   /* what the two knobs do, by model (read from Braids' renderers) */
static const char *const DR_P2[DR_NMODEL] = {"CLICK", "SNAPPY", "METAL", "BRIGHT", "SPREAD", "COLOUR", "COLOUR", "SPREAD", "SPLIT"};
static const dr_snd_t DR_KITS[DR_NKIT][DR_LANES] = {
    /* update 41 step 2: the KICK model has a sine body under it (a pitch falling to its note, engine/rack.cc), the hat voice has sizzle (high-passed noise, a ring and
     * metallic partials; its second knob sets how much); kicks sit at 31-36 (49-65 Hz), closed hats stay short, open hats are longer */
    {   /* 909 ISH: the kick model with a deep body, the snare, bright hats (clocked noise), filtered-noise clap, a high kick for a low tom */
        {0, 32, 60, 55, 60, 105, 5, 0}, {1, 50, 50, 60, 25, 90, 25, 0}, {6, 74, 50, 85, 5, 55, 10, 15},
        {6, 74, 50, 85, 55, 55, 10, 15}, {5, 62, 50, 50, 20, 80, 35, -10}, {0, 55, 40, 50, 25, 75, 15, -20}},
    {   /* 808 ISH: a long low kick, a soft snare, short hats, a loose clap, a struck-drum tom */
        {0, 30, 70, 30, 120, 110, 5, 0}, {1, 47, 40, 40, 30, 85, 30, 0}, {6, 76, 60, 75, 5, 50, 10, 20},
        {6, 76, 60, 75, 48, 50, 10, 20}, {5, 58, 40, 55, 28, 85, 40, -10}, {3, 50, 40, 30, 40, 80, 20, -25}},
    {   /* LO-FI: short kick, noise snare and hats (clocked, twin peaks), bell, a lot of FX; its hats keep little sizzle */
        {0, 36, 45, 70, 30, 100, 30, 0}, {5, 60, 60, 40, 18, 85, 40, 0}, {6, 79, 80, 40, 5, 50, 30, 20},
        {8, 72, 50, 40, 26, 50, 30, 20}, {5, 56, 65, 70, 15, 75, 45, -15}, {4, 72, 40, 50, 60, 65, 45, -25}},
    {   /* SPARSE: a deep kick, a dry struck-drum rim, a quiet hat, a cymbal for the open hat, a ringing bell */
        {0, 31, 65, 40, 85, 105, 15, 0}, {3, 55, 55, 55, 12, 80, 15, 0}, {6, 70, 50, 60, 4, 40, 15, 20},
        {2, 70, 50, 70, 65, 40, 25, 20}, {5, 62, 50, 50, 15, 65, 30, -10}, {4, 79, 30, 40, 80, 60, 40, -20}},
};
static dr_snd_t dr_kit[DR_LANES];               /* the live kit */
static uint8_t dr_kit_ix;                       /* the factory kit loaded last */
static void dr_load_kit(int k) { memcpy(dr_kit, DR_KITS[k], sizeof dr_kit); dr_kit_ix = (uint8_t)k; }
#define DR_CH 9                               /* MIDI channel 10 (from 0) */
#define DR_GATE_MS 30

static uint16_t dr_pat[DR_LANES][DR_STEPS];   /* the live pattern */
/* a lane's feel (the pattern's): LENGTH 1-16 (it loops on its own: a polymeter), DIVIDE (the step's length: 1/8 1/16 1/32 8T 16T), SHIFT
 * (the pattern turned, -15..15 steps) and CHOKE (the lane whose sound it cuts: 0 none, 1-6 the lanes) */
typedef struct { uint8_t len, div, choke; int8_t shift; } dr_feel_t;
static dr_feel_t dr_feel[DR_LANES];
/* a lane's MIDI (the kit page group's DRUM MIDI): the note it sends (and MIDI in plays it on), its channel (0-15), OUT (SOUND the FM-1's voices, MIDI, ALSO both, OFF),
 * and IN (the bits of dr_in: notes on the lane's channel and note play the lane) */
enum { DO_SOUND, DO_MIDI, DO_ALSO, DO_OFF };
typedef struct { uint8_t note, ch, out; } dr_mid_t;
static dr_mid_t dr_mid[DR_LANES];
static uint8_t dr_in;                         /* the lanes MIDI in plays (bit per lane) */
static uint8_t dr_busy = 1;                   /* IF BUSY (the engine's numbering): 0 NONE, 1 CLOUD, 2 VOICES */
static uint8_t dr_style = 1;                  /* STYLE starts and stops the drums */
/* update 41, step 5: PUMP, the kick ducking the synth (the engine's pump_set, drumrun.c; the DRUM PUMP page). DEPTH 0 is off (the default) */
#define DR_PREL_N 8
static const uint16_t DR_PREL_MS[5] = {60, 120, 200, 320, 500};   /* RELEASE: these in ms, then 1/16, 1/8, 1/4 note (the tempo's) */
static uint8_t dr_pdepth = 0, dr_prel = 2, dr_pshape = 50, dr_ptgt = 0;   /* DEPTH 0-100, RELEASE 0-7, SHAPE 0-100, TARGET 0 SYNTH DRY / 1 FX RETURNS / 2 BOTH */
static float dr_pump_rel(float step_s)         /* RELEASE in seconds; the note values from a 16th's length (the tempo's, or the clock's) */
{
    return dr_prel < 5 ? (float)DR_PREL_MS[dr_prel] * 0.001f : step_s * (float)(1 << (dr_prel - 5));
}
static const char *const DR_PRELN[DR_PREL_N] = {"60 MS", "120 MS", "200 MS", "320 MS", "500 MS", "1/16", "1/8", "1/4"};
#define DR_NDIV 5
static const uint8_t DR_DIVT[DR_NDIV] = {12, 6, 3, 8, 4};   /* ticks (a sixth of a 16th: the MIDI clock's) a step */
static const char *const DR_DIVN[DR_NDIV] = {"1/8", "1/16", "1/32", "8T", "16T"};
#define DR_BAR 96                             /* ticks in a bar of 4/4 */
typedef struct { uint16_t s[DR_LANES][DR_STEPS]; dr_feel_t f[DR_LANES]; } dr_pat_t;   /* a pattern as kept: 216 B */
#define DR_NPAT 8
typedef struct { uint32_t at; uint8_t lane, force; } dr_hit_t;
static dr_hit_t dr_q[DR_NQ];
static dr_pat_t dr_stage;                     /* the next pattern, read from the store while the drums play, taken at the bar line */
static volatile uint8_t dr_staged;
static uint8_t dr_dirty;                      /* bit 0: the live pattern has edits the store has not; bit 1: the kit or the settings */
static volatile struct {
    uint8_t run, mute;                        /* playing; the lanes muted (bit per lane: still advancing, not sounding) */
    uint8_t ring;                             /* the lanes whose MIDI note is on */
    uint8_t pat, next, swing;                 /* the pattern live (0-7, A-H); the one queued for the bar line (255 none); SWING 50-75 % */
    uint8_t lstep[DR_LANES], lcnt[DR_LANES];  /* each lane's step last played (255 none yet), the ticks left in it */
    int nq;
    uint32_t next_at, ticks, rng;
    uint32_t hits;                            /* struck since the start (the checks') */
    float frac;
} dr;
static volatile uint8_t dr_aud;               /* the pages ask for a lane to sound (lane + 1), when stopped: the interrupt does it */
static uint32_t dr_flash[DR_LANES];           /* when each lane last struck (the screen's: a lit tile) */
static float dr_hz = 44100.0f;                /* frames a second */
static uint32_t dr_off_at[DR_LANES];          /* the frame each lane's MIDI note ends */

static void (*dr_hook_hit)(int voice, int model, float note, float p1, float p2, float decay, float level, float pan, float send);
static void (*dr_hook_cut)(int voice);       /* a choke: the voice dies away quickly */
static void (*dr_hook_midi)(int ch0, int note, int vel);   /* vel 0: note off */
static void (*dr_hook_run)(int on);
static void dr_feel_default(void)
{
    int l;
    for (l = 0; l < DR_LANES; l++) {
        dr_feel[l].len = 16; dr_feel[l].div = 1; dr_feel[l].shift = 0;
        dr_feel[l].choke = l == 2 ? 4 : 0;     /* the closed hat cuts the open one */
    }
}
static void dr_reset_state(void)              /* power-up (and the checks'): pattern A empty, the 909 ISH kit, the lanes at their start */
{
    int l;
    memset(dr_pat, 0, sizeof dr_pat);
    dr_feel_default();
    dr_load_kit(0);
    for (l = 0; l < DR_LANES; l++) { dr_mid[l].note = DR_GM[l]; dr_mid[l].ch = DR_CH; dr_mid[l].out = DO_ALSO; }
    dr_in = 0x3Fu; dr_busy = 1; dr_style = 1;
    dr_pdepth = 0; dr_prel = 2; dr_pshape = 50; dr_ptgt = 0;
    dr.pat = 0; dr.next = 255; dr.swing = 50; dr.mute = 0; dr.ring = 0; dr.nq = 0; dr.hits = 0;
    for (l = 0; l < DR_LANES; l++) { dr.lstep[l] = 255; dr.lcnt[l] = 0; }
    dr_staged = 0; dr_dirty = 0;
}
static void dr_touch(void)                    /* the live pattern is edited: it is the store's no more, and a queued switch (which would drop the edit) is called off */
{
    dr_dirty |= 1u;
    dr.next = 255;
    dr_staged = 0;
}

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

/* the sound of a hit (the lane's voice with the live kit's values), at velocity vel (0-127: a force's, or MIDI in's) */
static void dr_sound_v(int lane, int vel)
{
    const dr_snd_t *k = &dr_kit[lane];
    float lv = (float)vel / 100.0f * (float)k->level * 0.01f;
    if (dr_hook_hit)
        dr_hook_hit(DR_VOICE[lane], k->model, (float)k->pitch, (float)k->p1 * 0.01f, (float)k->p2 * 0.01f, (float)k->decay_cs * 0.01f,
                    lv, (float)k->pan * 0.01f, (float)k->send * 0.01f);
}
static void dr_sound(int lane, int force) { dr_sound_v(lane, DR_VEL[force > 4 ? 4 : force]); }
/* a hit: the voice, and the lane's MIDI note on its channel (an earlier note of the lane ends first) */
static void dr_fire(int lane, int force, uint32_t at)
{
    dr.hits++;
    dr_flash[lane] = at;
    if (dr_feel[lane].choke) {                /* CHOKE: the lane named is cut (its voice, if it is another one: a shared voice is cut by the new hit) */
        int t = dr_feel[lane].choke - 1;
        if (t != lane && DR_VOICE[t] != DR_VOICE[lane] && dr_hook_cut)
            dr_hook_cut(DR_VOICE[t]);
        if (t != lane && (dr.ring >> t & 1u) && dr_hook_midi) {
            dr_hook_midi(dr_mid[t].ch, dr_mid[t].note, 0);
            dr.ring &= (uint8_t)~(1u << t);
        }
    }
    if (dr_mid[lane].out == DO_SOUND || dr_mid[lane].out == DO_ALSO)
        dr_sound(lane, force);
    if (dr_hook_midi && (dr_mid[lane].out == DO_MIDI || dr_mid[lane].out == DO_ALSO)) {
        if (dr.ring >> lane & 1u)
            dr_hook_midi(dr_mid[lane].ch, dr_mid[lane].note, 0);
        dr_hook_midi(dr_mid[lane].ch, dr_mid[lane].note, DR_VEL[force > 4 ? 4 : force]);
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
                dr_hook_midi(dr_mid[l].ch, dr_mid[l].note, 0);
        }
}
/* a lane's step (column col of the pattern), struck at frame tg; it lasts lf frames (the ratchets and the nudge are fractions of it) */
static void dr_lane_step(int l, int col, uint32_t tg, float lf)
{
    uint16_t s = dr_pat[l][col];
    int r, k;
    uint32_t t;
    if (!(s & DRS_ON) || (dr.mute >> l & 1u))
        return;
    if (DRS_CHANCE(s) < 5 && dr_rand() * 100.0f >= (float)DR_CHANCE[DRS_CHANCE(s) > 5 ? 5 : DRS_CHANCE(s)])
        return;
    t = tg + (uint32_t)(int32_t)((float)DRS_NUDGE(s) / 64.0f * lf);
    r = DRS_RATCH(s);
    for (k = 0; k < r && dr.nq < DR_NQ; k++) {
        dr_q[dr.nq].at = t + (uint32_t)((float)k * lf / (float)r);
        dr_q[dr.nq].lane = (uint8_t)l;
        dr_q[dr.nq].force = (uint8_t)DRS_FORCE(s);
        dr.nq++;
    }
}
static int dr_col_of(int l, int step)         /* the column of the lane's step n: its length, shifted */
{
    int len = dr_feel[l].len < 1 ? 1 : dr_feel[l].len > DR_STEPS ? DR_STEPS : dr_feel[l].len;
    return ((step % len) + dr_feel[l].shift % len + len) % len;
}
static int dr_col(int l)                      /* the column playing (the screen's playhead), -1 stopped */
{
    return dr.run && dr.lstep[l] != 255 ? dr_col_of(l, dr.lstep[l]) : -1;
}
static uint32_t dr_start_ticks;               /* update 41 step 6: the ticks into the bar the next start begins at (0: the bar line) */
static void dr_start(uint32_t now)
{
    int l;
    if (dr.run)
        return;
    dr.run = 1;
    dr.ticks = dr_start_ticks;                /* (a recording from a step selected: the drums start in step with it) */
    dr_start_ticks = 0;
    dr.next_at = now;
    dr.frac = 0.0f;
    dr.nq = 0;
    dr.hits = 0;
    for (l = 0; l < DR_LANES; l++) { dr.lstep[l] = 255; dr.lcnt[l] = 0; }
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
    dr.nq = 0;
    for (l = 0; l < DR_LANES; l++) {
        dr.lstep[l] = 255;
        if (dr.ring >> l & 1u && dr_hook_midi)
            dr_hook_midi(dr_mid[l].ch, dr_mid[l].note, 0);
    }
    dr.ring = 0;
    if (dr_hook_run)
        dr_hook_run(0);
}
/* the bar line: a queued pattern comes in (read ahead into dr_stage), and every lane starts again from its first step */
static void dr_bar(void)
{
    int l;
    if (dr.next == 255 || !dr_staged)
        return;
    memcpy(dr_pat, dr_stage.s, sizeof dr_pat);
    memcpy(dr_feel, dr_stage.f, sizeof dr_feel);
    dr.pat = dr.next;
    dr.next = 255;
    dr_staged = 0;
    dr_dirty &= (uint8_t)~1u;
    for (l = 0; l < DR_LANES; l++) { dr.lstep[l] = 255; dr.lcnt[l] = 0; }
}
/* a tick (a sixth of a 16th: the MIDI clock's, or the tempo's, d frames a 16th) at frame at: each lane whose step ends starts its next */
static void dr_tick(uint32_t at, float d)
{
    int l;
    if (!dr.run)
        return;
    if (dr.ticks % DR_BAR == 0u)
        dr_bar();
    for (l = 0; l < DR_LANES; l++) {
        if (dr.lcnt[l] == 0) {
            int div = dr_feel[l].div >= DR_NDIV ? 1 : dr_feel[l].div, len = dr_feel[l].len < 1 ? 1 : dr_feel[l].len > DR_STEPS ? DR_STEPS : dr_feel[l].len;
            int n = dr.lstep[l] == 255 ? 0 : (dr.lstep[l] + 1) % len;
            float lf = (float)DR_DIVT[div] * d / 6.0f;
            uint32_t t = at;
            dr.lstep[l] = (uint8_t)n;
            if (dr.swing > 50 && div < 3 && (n & 1))   /* SWING: the odd steps late (not the triplets') */
                t += (uint32_t)((float)(dr.swing - 50) * 0.02f * lf);
            dr_lane_step(l, dr_col_of(l, n), t, lf);
            dr.lcnt[l] = DR_DIVT[div];
        }
        dr.lcnt[l]--;
    }
    dr.ticks++;
}
/* each block, on the tempo: d frames a 16th (the caller's); the ticks that begin before the block ends, a tick ahead (so a hit nudged early is not late) */
static void dr_block(uint32_t now, uint32_t n, float d)
{
    uint32_t end = now + n;
    float tf = d / 6.0f;
    if (dr.run) {
        while ((int32_t)(dr.next_at - end) < (int32_t)tf) {
            uint32_t di = (uint32_t)tf;
            dr_tick(dr.next_at, d);
            dr.frac += tf - (float)di;        /* the fraction of a frame carried: the tempo holds over bars */
            if (dr.frac >= 1.0f) { di++; dr.frac -= 1.0f; }
            dr.next_at += di;
        }
    }
    dr_due(end);
}
/* only the hits due (the MIDI clock's: the ticks come from dr_tick) */
static void dr_flush(uint32_t now, uint32_t n) { dr_due(now + n); }

/* ---- editing (the page's) */
static void dr_toggle(int lane, int i)
{
    dr_pat[lane][i] = (dr_pat[lane][i] & DRS_ON) ? 0 : DRS_NEW;
    dr_touch();
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
    dr_touch();
}
static void dr_clear_lane(int lane) { memset(dr_pat[lane], 0, sizeof dr_pat[lane]); dr_touch(); }
/* the factory beat (909 ISH): four on the floor, a backbeat of snare and clap, offbeat hats, an open hat, a ghost kick, two toms */
static void dr_load_beat(void)
{
    static const uint8_t K[] = {0, 4, 8, 12}, S[] = {4, 12}, CH[] = {0, 2, 4, 6, 8, 10, 12}, OH[] = {14}, CL[] = {4, 12}, PC[] = {7, 11};
    int i;
    memset(dr_pat, 0, sizeof dr_pat);
    dr_touch();
    for (i = 0; i < 4; i++) dr_pat[0][K[i]] = DRS_NEW;
    dr_pat[0][10] = DRS_MK(1u, DF_GHOST, 1, 5, 0);
    for (i = 0; i < 2; i++) dr_pat[1][S[i]] = DRS_MK(1u, DF_HARD, 1, 5, 0);
    for (i = 0; i < 7; i++) dr_pat[2][CH[i]] = DRS_MK(1u, (CH[i] & 3) == 2 ? DF_HARD : DF_SOFT, 1, 5, 0);
    dr_pat[3][OH[0]] = DRS_NEW;
    for (i = 0; i < 2; i++) dr_pat[4][CL[i]] = DRS_MK(1u, DF_SOFT, 1, 5, 0);
    for (i = 0; i < 2; i++) dr_pat[5][PC[i]] = DRS_MK(1u, DF_SOFT, 1, 3, 0);
    dr_pat[5][7] = DRS_MK(1u, DF_NORM, 2, 4, 0);
}

/* ---- DRUM GEN: a lane filled by an algorithm (a mask of the steps, rotated by TURN), then a force varied on VARY percent of its hits; edited by hand after */
enum { DG_EUCLID, DG_FOUR, DG_BACK, DG_OFFB, DG_16TH, DG_RAND, DG_BREAK, DG_CLEAR, DG_N };
static const char *const DG_NAME[DG_N] = {"EUCLID", "FOUR FLOOR", "BACKBEAT", "OFFBEAT", "16THS", "RANDOM", "BREAK", "CLEAR"};
static uint32_t dg_hash(uint32_t x) { x ^= x >> 16; x *= 0x7feb352du; x ^= x >> 15; x *= 0x846ca68bu; x ^= x >> 16; return x; }
static void dr_gen_mask(int algo, int len, int hits, int turn, uint8_t m[DR_STEPS])
{
    uint8_t t[DR_STEPS];
    int i;
    memset(m, 0, DR_STEPS);
    if (len < 1) len = 1;
    if (len > DR_STEPS) len = DR_STEPS;
    hits = hits < 0 ? 0 : hits > len ? len : hits;
    switch (algo) {
    case DG_EUCLID: for (i = 0; i < len; i++) m[i] = hits && (i * hits) % len < hits; break;   /* hits spread as evenly as the steps allow */
    case DG_FOUR: for (i = 0; i < len; i++) m[i] = i % 4 == 0; break;
    case DG_BACK: for (i = 0; i < len; i++) m[i] = i % 8 == 4; break;
    case DG_OFFB: for (i = 0; i < len; i++) m[i] = i % 4 == 2; break;
    case DG_16TH: for (i = 0; i < len; i++) m[i] = 1; break;
    case DG_BREAK: { static const uint8_t B[5] = {0, 3, 6, 10, 13}; for (i = 0; i < 5; i++) if (B[i] < len) m[B[i]] = 1; break; }
    case DG_RAND: {                            /* TURN is the seed: the same hits for the same settings */
        uint32_t h = dg_hash((uint32_t)turn * 977u + (uint32_t)hits * 31u + 7u);
        int n = 0, guard = 0;
        while (n < hits && guard++ < 400) {
            int p;
            h = dg_hash(h + 0x9e3779b9u);
            p = (int)(h % (uint32_t)len);
            if (!m[p]) { m[p] = 1; n++; }
        }
        return;
    }
    default: return;                            /* CLEAR */
    }
    if (turn % len) {                          /* TURN: the mask turned right */
        memcpy(t, m, DR_STEPS);
        for (i = 0; i < len; i++) m[(i + turn) % len] = t[i];
    }
}
static void dr_gen_write(int lane, int algo, int hits, int turn, int vary)
{
    uint8_t m[DR_STEPS];
    int i, len = dr_feel[lane].len;
    uint32_t h = dg_hash((uint32_t)(hits * 64 + turn) * 13u + (uint32_t)algo + 99u);
    dr_gen_mask(algo, len, hits, turn, m);
    for (i = 0; i < DR_STEPS; i++) {
        int f = DF_NORM;
        if (i >= len || !m[i]) { dr_pat[lane][i] = 0; continue; }
        h = dg_hash(h + (uint32_t)i);
        if ((int)(h % 100u) < vary)            /* VARY: this hit's force is another */
            f = (int)((h >> 8) % 5u);
        dr_pat[lane][i] = DRS_MK(1u, f, 1, 5, 0);
    }
    dr_touch();
}

/* ---- DRUM MIDI (step 5) */
static void dr_mid_off_now(int l)             /* a lane's note ends now (its note or channel is about to change, or the lane stops sending) */
{
    if ((dr.ring >> l & 1u) && dr_hook_midi)
        dr_hook_midi(dr_mid[l].ch, dr_mid[l].note, 0);
    dr.ring &= (uint8_t)~(1u << l);
}
static void dr_mid_set(int l, int what, int v)   /* what: 0 OUT, 1 CHANNEL (0-15), 2 NOTE (0-127); the note ringing ends first */
{
    dr_mid_off_now(l);
    if (what == 0) dr_mid[l].out = (uint8_t)dr_clamp(v, 0, 3);
    else if (what == 1) dr_mid[l].ch = (uint8_t)dr_clamp(v, 0, 15);
    else dr_mid[l].note = (uint8_t)dr_clamp(v, 0, 127);
}
/* MIDI in: a note on channel ch0 (0-15), velocity vel (0 = off): every lane with IN on, that channel and that note plays (its sound, at the velocity,
 * when its OUT has the FM-1's sound; never a MIDI note back out: no loop). 1 when a lane took it (the synth does not play it), 0 when not */
static int dr_midi_in(int ch0, int note, int vel, int is_on)
{
    int l, took = 0;
    for (l = 0; l < DR_LANES; l++) {
        if (!(dr_in >> l & 1u) || dr_mid[l].ch != ch0 || dr_mid[l].note != note)
            continue;
        took = 1;
        if (is_on && vel > 0 && (dr_mid[l].out == DO_SOUND || dr_mid[l].out == DO_ALSO)) {
            dr_sound_v(l, vel);
        }
    }
    return took;
}
/* what the kit costs, in points of a core x 10: each voice at its costliest lane's model, the four voices added (the worst case: all sounding at once) */
static int dr_load_x10(void)
{
    int v, l, sum = 0;
    for (v = 0; v < 4; v++) {
        int m = 0;
        for (l = 0; l < DR_LANES; l++)
            if (DR_VOICE[l] == v && DR_MCPU[dr_kit[l].model] > m)
                m = DR_MCPU[dr_kit[l].model];
        sum += m;
    }
    return sum;
}
