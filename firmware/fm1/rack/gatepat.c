/* SPDX-License-Identifier: MPL-2.0 */
/* FM1 Rack firmware: the GATE PATTERN (21 Nov 2026, the LFO menu): two gate lanes from the rhythm rings' maths
 * (ringgen.c: Euclid hits, turn, steps, chance, mutate), each on a rate of its own from the tempo or the MIDI clock.
 * A hit raises the lane's gate for half a step: the engine's source GATE 1 / GATE 2 (rk_set_ext 6 and 7), which the
 * matrix, LFO RETRIG, PEAKS / TIDES GATE and FUNC TRIG read. Nothing runs and nothing is sent while a lane's RATE is OFF.
 * Plain C with no firmware settings in it (play.c hands it the values), so rack_host.c checks it on a computer. */
#define GP_BEATS_N 6
static const float GP_BEATS[GP_BEATS_N] = {1.0f, 0.5f, 1.0f / 3.0f, 0.25f, 1.0f / 6.0f, 0.125f};   /* RATE 1/4 1/8 1/8T 1/16 1/16T 1/32, in beats */
static const uint8_t GP_TICKS[GP_BEATS_N] = {24, 12, 8, 6, 4, 3};                                    /* ... in MIDI clock ticks */
typedef struct { uint8_t rate, hits, steps, rot; float chance, mut; } gp_set_t;                      /* rate 0: OFF, 1..6: the note values */
static struct {
    gp_set_t s;                               /* the values now */
    uint32_t base, live, next_at, off_at, step, seq;   /* the ring as set and as mutated; the step times; counts */
    uint8_t len, pos, on, out, running, ticks;
} gp[2];
static uint32_t gp_hits_total[2];             /* hits fired since power-up (the checks') */
static void gp_set(int L, const gp_set_t *s) { gp[L].s = *s; }
static void gp_build(int L)                   /* the ring as set; a change starts it afresh */
{
    int n = gp[L].s.steps < 2 ? 2 : gp[L].s.steps > 32 ? 32 : gp[L].s.steps;
    uint32_t m = rgn_rot(rgn_euclid(n, gp[L].s.hits), n, gp[L].s.rot);
    if (m != gp[L].base || n != gp[L].len) {
        gp[L].base = gp[L].live = m;
        gp[L].len = (uint8_t)n;
    }
}
static uint32_t gp_ring(int L, int *n) { gp_build(L); *n = gp[L].len; return gp[L].live; }
static int gp_hit(int L, uint32_t k)          /* step k (counted from the start): a hit to play? */
{
    int n, i;
    uint32_t m = gp_ring(L, &n);
    i = (int)(k % (uint32_t)n);
    if (i == 0 && k) {
        rgn_mutate(&gp[L].live, gp[L].base, n, gp[L].s.mut);
        m = gp_ring(L, &n);
    }
    gp[L].pos = (uint8_t)i;
    gp[L].seq++;
    return (m >> i & 1u) && rgn_chance(gp[L].s.chance);
}
/* a step in frames: the MIDI clock's ticks (tick_frames > 0 and midi), or the tempo's */
static float gp_frames(int L, float bpm, int midi, float tick_frames, float hz)
{
    int r = (int)gp[L].s.rate - 1;
    if (r < 0 || r >= GP_BEATS_N) return 0.0f;
    if (midi && tick_frames > 0.0f) return tick_frames * (float)GP_TICKS[r];
    return GP_BEATS[r] * 60.0f / bpm * hz;
}
static float gp_tickf, gp_hz = 44100.0f;
static void gp_fire(int L, uint32_t at, float d)   /* a step at frame at: a hit raises the gate for half the step */
{
    if (gp_hit(L, gp[L].step++)) {
        gp[L].on = 1;
        gp[L].off_at = at + (uint32_t)(d * 0.5f);
        gp_hits_total[L]++;
    }
}
/* each block (n frames from `now`), before the render: the steps due on the internal tempo, the gates up or down */
static void gp_block(uint32_t now, uint32_t n, float bpm, int midi, float tick_frames)
{
    uint32_t end = now + n;
    int L;
    gp_tickf = tick_frames;
    for (L = 0; L < 2; L++) {
        if (!gp[L].s.rate) {
            gp[L].running = 0;
            gp[L].on = 0;
        } else {
            if (gp[L].on && (int32_t)(gp[L].off_at - now) <= 0)
                gp[L].on = 0;
            if (!midi) {                      /* (on MIDI clock the ticks fire the steps: gp_tick) */
                float d = gp_frames(L, bpm, 0, 0.0f, gp_hz);
                if (!gp[L].running || (int32_t)(gp[L].next_at - now) < -(int32_t)d) {
                    gp[L].running = 1;
                    gp[L].next_at = now;
                    gp[L].step = 0;
                }
                while ((int32_t)(gp[L].next_at - end) < 0) {
                    gp_fire(L, gp[L].next_at, d);
                    gp[L].next_at += (uint32_t)d;
                }
            }
        }
        if (gp[L].on != gp[L].out) {
            gp[L].out = gp[L].on;
            rk_set_ext(6 + L, gp[L].on ? 1.0f : 0.0f);
        }
    }
}
static void gp_tick(uint32_t at, float bpm, float tick_frames)   /* each MIDI clock tick: the steps due */
{
    int L;
    for (L = 0; L < 2; L++) {
        int r = (int)gp[L].s.rate - 1;
        if (r < 0) continue;
        if (!gp[L].running) { gp[L].running = 1; gp[L].step = 0; gp[L].ticks = 0; }
        if (gp[L].ticks++ % GP_TICKS[r] == 0u)
            gp_fire(L, at, gp_frames(L, bpm, 1, tick_frames, gp_hz));
    }
}
static void gp_restart(void) { gp[0].running = gp[1].running = 0; }
