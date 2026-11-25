// SPDX-License-Identifier: GPL-3.0
// FM1 Phase 2: licensed GPL-3.0, see LICENSE; third-party code and its licences: NOTICE.md.
/* JAM mode's moves (update 28). Included by app.c (through play.c) and by rack_host.c: no libm, no app globals.
 *
 * Twelve moves, one on each of the keys above the SPLIT. A move is an offset layer on the sound's own parameters
 * (engine/rack.cc: set_jam_offsets; never written into the preset): each audio block jam_block() works out, from
 * each move's level (0 to 1, gliding to its target at the move's own speed, so nothing clicks), the offset of up to
 * 20 targets, and hands them to the engine. A target the preset does not have (a module not loaded: a delay is in
 * 16 factory presets in 46) is ignored there, so every move uses the parameters of whatever the preset does have: the
 * voices' level, the filter and its SWEEP, the pitch, the source's BITS and RATE, two LFO rates, the chorus, the reverb and
 * the delay. (2.48: the moves that threw grains through Clouds are rebuilt on the delay, the reverb, the chorus and the filter.)
 * A move ends exactly: its level snaps to 0 below 0.01, all the offsets are then 0 and the engine drops the layer
 * (the preset's sound again, sample for sample). HOLD: on while the key is down. LATCH: a press turns it on, the next off.
 * Amount (0-100 %, 0 to 1 here) and the key's velocity scale a move. Clocked moves (the stutters, GLITCH, FILTER RISE) count 16ths
 * from the press, at the tempo or the MIDI clock's (the caller passes the frames in a 16th). */
#include <stdint.h>

void rk_set_jam_table(const uint8_t *pairs, int n);
void rk_set_jam_offsets(const float *o);
void rk_clear_jam(void);

#define JAM_NM 12                             /* moves */
#define JAM_NT 20                             /* targets */
/* the settings the JAM page edits (floats: the page's own machinery): ON, SPLIT (the first key of the moves), then
 * each move's AMOUNT (0-1) and LATCH (0 HOLD, 1 LATCH) */
enum { JV_ON, JV_SPLIT, JV_MV0, JV_N = JV_MV0 + 2 * JAM_NM };
#define JV_AMT(m) (JV_MV0 + 2 * (m))
#define JV_LAT(m) (JV_MV0 + 2 * (m) + 1)
static float jam_v[JV_N];

/* the targets: (a slot, or a module id from 31 up for the FX slot that holds it, or 255 the voice's pitch; a parameter) */
enum {
    T_LEVEL, T_SWEEP, T_RESO, T_DRIVE, T_PITCH, T_BITS, T_BRATE, T_LFO1, T_LFO2,
    T_DFB, T_DMIX, T_DWOW, T_RSIZE, T_RDAMP, T_RMIX, T_RSHIM, T_CRATE, T_CDEPTH, T_CMIX, T_DTONE
};
static const uint8_t JAM_TAB[JAM_NT * 2] = {
    6, 7,  7, 23,  7, 10,  7, 11,  255, 0,  0, 4,  0, 5,  8, 1,  8, 5,
    31, 1,  31, 3,  31, 5,  32, 0,  32, 1,  32, 3,  32, 6,  33, 0,  33, 1,  33, 3,  31, 2
};
/* each target's span (the engine takes offsets as a fraction of it): the moves are written in the parameter's own units */
static const float JAM_SPAN[JAM_NT] = {
    1, 1, 1, 1, 1, 6, 5, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1
};
static const char *const JAM_NAME[JAM_NM] = {"DELAY THROW", "STUTTER 1/8", "STUTTER 1/16", "STUTTER 1/32", "REVERSE SCRUB", "TAPE STOP",
                                              "WASH", "FREEZE PAD", "GLITCH", "FILTER DIVE", "FILTER RISE", "CRUSH"};
/* each move's glide to its level, and back, in milliseconds */
static const uint16_t JAM_ATK[JAM_NM] = {25, 8, 8, 8, 40, 450, 600, 120, 5, 500, 5, 15};
static const uint16_t JAM_REL[JAM_NM] = {350, 60, 60, 60, 350, 150, 600, 450, 80, 300, 150, 150};

typedef struct { float lvl, age; uint8_t vel, on; } jam_mv_t;   /* level, 16ths since the press, velocity 0-255, bit 0 down, bit 1 latched */
static jam_mv_t jam_m[JAM_NM];
static float jam_off[JAM_NT];
static uint8_t jam_live;                      /* the engine has offsets: to be sent (zeros too) until they are zero */
static uint32_t jam_rng = 0x1F2E3D4Cu;
static float jam_gr[4];                       /* GLITCH's random values for this 16th, and its cell */
static int32_t jam_gcell = -1;
static float jam_gs[3];                       /* the stutters' gates, smoothed */

static void jam_defaults(void)
{
    int m;
    for (m = 0; m < JV_N; m++)
        jam_v[m] = 0.0f;
    jam_v[JV_SPLIT] = 15.0f;                  /* 12 keys of 27 for the moves: the upper 12 */
    for (m = 0; m < JAM_NM; m++)
        jam_v[JV_AMT(m)] = m == 6 || m == 7 ? 0.7f : 0.8f;
}
static void jam_init(void)                    /* once: the engine's table */
{
    rk_set_jam_table(JAM_TAB, JAM_NT);
}
static float jam_rand(void)                   /* -1 to 1 */
{
    jam_rng = jam_rng * 1664525u + 1013904223u;
    return (float)(jam_rng >> 8) * (1.0f / 8388608.0f) - 1.0f;
}
static void jam_clear(void)                   /* every move ends now: the preset's own sound */
{
    int m;
    for (m = 0; m < JAM_NM; m++) {
        jam_m[m].lvl = 0.0f;
        jam_m[m].age = 0.0f;
        jam_m[m].on = 0;
    }
    jam_gcell = -1;
    for (m = 0; m < JAM_NT; m++)
        jam_off[m] = 0.0f;
    rk_clear_jam();
    jam_live = 0;
}
static void jam_press(int m, int vel127)
{
    jam_mv_t *s;
    if (m < 0 || m >= JAM_NM)
        return;
    s = &jam_m[m];
    s->vel = (uint8_t)(vel127 < 1 ? 1 : vel127 > 127 ? 255 : vel127 * 2 + 1);
    if (jam_v[JV_LAT(m)] > 0.5f) {
        s->on = (uint8_t)(s->on & 2u ? 0u : 2u);
    } else {
        s->on |= 1u;
    }
    if (s->on && s->lvl < 0.002f) {
        s->age = 0.0f;
        if (m >= 1 && m <= 3)
            jam_gs[m - 1] = 0.0f;
    }
}
static void jam_release(int m)
{
    if (m >= 0 && m < JAM_NM)
        jam_m[m].on &= (uint8_t)~1u;
}
static float jam_level(int m) { return m >= 0 && m < JAM_NM ? jam_m[m].lvl : 0.0f; }
static int jam_active(int m) { return m >= 0 && m < JAM_NM && (jam_m[m].on || jam_m[m].lvl > 0.0f); }

#define JA(t, u) (jam_off[t] += s * (u) / JAM_SPAN[t])
/* a gate: closed in the second half of each cycle of `div` 16ths */
static float jam_gate(float age, float div) { float c = age / div; return c - (float)(int32_t)c >= 0.5f ? 1.0f : 0.0f; }

/* move m, its level and amount together s (0-1), 16ths since the press `age` */
static void jam_mv(int m, float s, float lvl, float age)
{
    switch (m) {
    case 0:   /* DELAY THROW: the sound thrown back in wobbling, smeared echoes with the feedback up, the chorus and reverb opening round them */
        JA(T_DFB, 0.55f); JA(T_DMIX, 0.45f); JA(T_DWOW, 0.6f); JA(T_DTONE, -0.25f); JA(T_CMIX, 0.4f); JA(T_CDEPTH, 0.45f); JA(T_CRATE, 0.35f);
        JA(T_RMIX, 0.35f); JA(T_RSIZE, 0.2f); JA(T_LFO1, 0.3f); JA(T_LFO2, 0.3f); JA(T_SWEEP, -0.1f);
        break;
    case 1: case 2: case 3: {   /* STUTTER: the voices and the wet of the chorus, delay and reverb chopped on the grid (8ths, 16ths, 32nds), the gate's edges
                                 * about 10 ms */
        float div = m == 1 ? 2.0f : m == 2 ? 1.0f : 0.5f, *gs = &jam_gs[m - 1];
        *gs += (jam_gate(age, div) - *gs) * 0.5f;
        JA(T_LEVEL, -*gs); JA(T_RMIX, -*gs); JA(T_DMIX, -*gs); JA(T_CMIX, -*gs);
        break;
    }
    case 4: {   /* REVERSE SCRUB: a reversed swell: the filter opens and the level rises under a growing reverb and a delay that smears it */
        float ramp = age >= 8.0f ? 1.0f : age * 0.125f;
        JA(T_RMIX, 0.45f); JA(T_RSIZE, 0.4f); JA(T_SWEEP, -0.7f * (1.0f - ramp)); JA(T_LEVEL, -0.55f * (1.0f - ramp));
        JA(T_DMIX, 0.25f); JA(T_DFB, 0.3f); JA(T_DTONE, -0.3f * (1.0f - ramp)); JA(T_CDEPTH, 0.3f); JA(T_CRATE, 0.2f);
        break;
    }
    case 5:   /* TAPE STOP: the pitch and the movement fall away, darker, the repeats sag */
        JA(T_PITCH, -2.0f); JA(T_SWEEP, -0.3f); JA(T_LFO1, -0.35f); JA(T_LFO2, -0.35f);
        JA(T_DWOW, 0.4f); JA(T_DTONE, -0.3f);
        break;
    case 6:   /* WASH: reverb and feedback swell, the filter softens */
        JA(T_RMIX, 0.5f); JA(T_RSIZE, 0.45f); JA(T_RSHIM, 0.3f); JA(T_RDAMP, 0.25f); JA(T_SWEEP, -0.3f);
        JA(T_DMIX, 0.2f); JA(T_DFB, 0.3f); JA(T_CMIX, 0.3f); JA(T_CDEPTH, 0.2f);
        break;
    case 7:   /* FREEZE PAD: the sound held and smeared wide: the delay feeds back almost to ringing, the reverb opens far, the chorus spreads it */
        JA(T_DFB, 0.65f); JA(T_DMIX, 0.4f); JA(T_DTONE, -0.2f); JA(T_RMIX, 0.65f); JA(T_RSIZE, 0.6f); JA(T_RDAMP, 0.15f); JA(T_RSHIM, 0.3f);
        JA(T_CDEPTH, 0.3f); JA(T_CMIX, 0.3f); JA(T_SWEEP, -0.2f); JA(T_LFO1, 0.15f);
        break;
    case 8: {   /* GLITCH: every 16th a new pitch, a drop-out, a crushed bit, a filter jump and a burst of the delay */
        int32_t cell = (int32_t)age;
        static const float PQ[7] = {-1.0f, -7.0f / 12.0f, -5.0f / 12.0f, 0.0f, 5.0f / 12.0f, 7.0f / 12.0f, 1.0f};
        int q;
        if (cell != jam_gcell) {
            int i;
            jam_gcell = cell;
            for (i = 0; i < 4; i++)
                jam_gr[i] = jam_rand();
        }
        q = (int)((jam_gr[2] * 0.5f + 0.5f) * 6.999f);
        JA(T_PITCH, PQ[q]); JA(T_DFB, 0.2f);
        if (jam_gr[3] < -0.5f) JA(T_LEVEL, -1.0f);
        if (jam_gr[1] > 0.3f) JA(T_BITS, -2.0f);
        JA(T_SWEEP, jam_gr[1] * 0.35f); JA(T_LFO1, 0.4f * (jam_gr[0] < 0 ? -jam_gr[0] : jam_gr[0])); JA(T_DMIX, 0.3f * (jam_gr[0] < 0 ? -jam_gr[0] : jam_gr[0])); JA(T_RMIX, 0.15f);
        break;
    }
    case 9:   /* FILTER DIVE: the cutoff falls, the resonance rises */
        JA(T_SWEEP, -0.9f); JA(T_RESO, 0.45f); JA(T_DRIVE, 0.15f); JA(T_RMIX, 0.1f);
        break;
    case 10: {   /* FILTER RISE: from closed, opening to a little brighter than the sound's own over 12 16ths */
        float ramp = age >= 12.0f ? 1.0f : age * (1.0f / 12.0f), c = (1.0f - ramp);
        JA(T_SWEEP, -0.85f * c + 0.2f * ramp); JA(T_RESO, 0.35f * c); JA(T_RMIX, 0.1f);
        break;
    }
    default:  /* CRUSH: fewer bits, a lower rate, driven */
        JA(T_BITS, -4.0f); JA(T_BRATE, -3.0f); JA(T_DRIVE, 0.45f);
        break;
    }
}

/* once per audio block: dt seconds, f16 frames in a 16th (the same unit as the block's frames: n) */
static void jam_block(float dt, float f16, float n)
{
    int m, any = 0, i;
    for (m = 0; m < JAM_NM; m++) {
        jam_mv_t *s = &jam_m[m];
        float tgt = s->on ? 1.0f : 0.0f, tc, k;
        if (!s->on && s->lvl <= 0.0f)
            continue;
        tc = (tgt > s->lvl ? JAM_ATK[m] : JAM_REL[m]) * 0.001f;
        k = dt / (tc + dt);
        s->lvl += (tgt - s->lvl) * k;
        if (tgt == 0.0f && s->lvl < 0.01f)
            s->lvl = 0.0f;
        else if (tgt > 0.0f && tgt - s->lvl < 0.002f)
            s->lvl = tgt;
        s->age += n / f16;
        if (s->lvl > 0.0f)
            any = 1;
    }
    if (!any && !jam_live)
        return;
    for (i = 0; i < JAM_NT; i++)
        jam_off[i] = 0.0f;
    for (m = 0; m < JAM_NM; m++) {
        jam_mv_t *s = &jam_m[m];
        float v, a;
        if (s->lvl <= 0.0f)
            continue;
        v = 0.4f + 0.6f * (float)s->vel * (1.0f / 255.0f);
        a = jam_v[JV_AMT(m)] * v;
        jam_mv(m, s->lvl * a, s->lvl, s->age);
    }
    rk_set_jam_offsets(jam_off);
    jam_live = (uint8_t)any;
}
