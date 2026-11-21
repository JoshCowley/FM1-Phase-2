/* SPDX-License-Identifier: MPL-2.0 */
/* rack_host check 15 (FM1 Phase 2, 21 Nov 2026): the GATE PATTERN (gatepat.c, ringgen.c; the engine's GATE 1 / GATE 2 sources).
 * 15a a lane's hits fall on the Euclid steps at the tempo (1/16 at 120 bpm: 4 of 16 every half second; at 60 bpm every second;
 *     3 of 8 on 1/8: steps 0, 3, 6; TURN 1 moves them to 1, 4, 7), within one block (256 frames) of the step's frame,
 *     and the gate falls again after half a step;
 * 15b CHANCE: 0 never, 1 always, 0.5 about half of 2000 steps; RATE OFF runs nothing;
 * 15c through the engine: a hit retriggers an LFO (RETRIG GATE 1: the cycle starts again; RETRIG OFF: not), and PEAKS' envelope
 *     (GATE: GATE 1: its output rises on the hit, nothing before it);
 * 15d a lane left OFF gives the same sound as no GATE PATTERN at all (the engine's extra source reads 0). */
#include "ringgen.c"
#include "gatepat.c"
static void gc_lane(int L, int rate, int hits, int steps, int rot, float chance)
{
    gp_set_t s;
    s.rate = (uint8_t)rate; s.hits = (uint8_t)hits; s.steps = (uint8_t)steps; s.rot = (uint8_t)rot; s.chance = chance; s.mut = 0.0f;
    gp_set(L, &s);
}
static void gc_reset(void) { memset(gp, 0, sizeof gp); gp_hits_total[0] = gp_hits_total[1] = 0; gp_restart(); }
/* run nb blocks of 256 frames from frame 0; the rising edges of lane L: their block starts and ring positions */
static int gc_edges(int L, float bpm, int nb, uint32_t *at, int *pos, int *falls_ok)
{
    int b, n = 0, prev = 0;
    uint32_t t_on = 0;
    *falls_ok = 1;
    for (b = 0; b < nb; b++) {
        uint32_t now = (uint32_t)b * 256u;
        int on;
        gp_block(now, 256u, bpm, 0, 0.0f);
        on = gp[L].on;
        if (on && !prev && n < 64) { at[n] = now; pos[n] = gp[L].pos; n++; t_on = now; }
        if (!on && prev) {                    /* up for half a step (+ up to a block) */
            float half = gp_frames(L, bpm, 0, 0.0f, 44100.0f) * 0.5f;
            if ((float)(now - t_on) < half - 256.0f || (float)(now - t_on) > half + 512.0f) *falls_ok = 0;
        }
        prev = on;
    }
    return n;
}
static const char *gc_self;
static void gate_checks(int *fails)
{
    int bad = 0, i, j, n, fo;
    uint32_t at[64];
    int pos[64];
    /* 15a */
    {
        static const struct { float bpm; int rate, hits, steps, rot, nexp; double step_frames; } T[] = {
            {120.0f, 4, 4, 16, 0, 8, 5512.5}, {60.0f, 4, 4, 16, 0, 4, 11025.0}, {120.0f, 2, 3, 8, 0, 6, 11025.0}, {120.0f, 2, 3, 8, 1, 6, 11025.0}};
        for (j = 0; j < 4; j++) {
            int nb = 689, ok = 1, k, st;
            int ring[32], nr = 0;
            uint32_t m;
            gc_reset();
            gc_lane(0, T[j].rate, T[j].hits, T[j].steps, T[j].rot, 1.0f);
            n = gc_edges(0, T[j].bpm, nb, at, pos, &fo);
            m = rgn_rot(rgn_euclid(T[j].steps, T[j].hits), T[j].steps, T[j].rot);
            for (k = 0; k < T[j].steps; k++) if (m >> k & 1u) ring[nr++] = k;
            for (i = 0, st = 0; i < n; i++) {   /* each edge: the next ring hit's step, within a block of its frame */
                int want = ring[i % nr] + (i / nr) * T[j].steps;
                double t = (double)want * T[j].step_frames;
                if (pos[i] != ring[i % nr] || !((double)at[i] <= t + 0.5 && t < (double)at[i] + 256.0)) ok = 0;
                st++;
            }
            if (n != T[j].nexp || !ok || !fo) bad++;
            printf("15a %.0f bpm, rate %d, %d of %d turned %d: %d hits in 4 s (want %d), first at frames %u %u %u, on the steps and in time: %s, falls after half a step: %s\n",
                   T[j].bpm, T[j].rate, T[j].hits, T[j].steps, T[j].rot, n, T[j].nexp, n > 0 ? at[0] : 0u, n > 1 ? at[1] : 0u, n > 2 ? at[2] : 0u, ok ? "yes" : "NO", fo ? "yes" : "NO");
        }
    }
    /* 15b */
    {
        static const float CH[3] = {0.0f, 1.0f, 0.5f};
        for (j = 0; j < 3; j++) {
            uint32_t now = 0;
            int steps;
            gc_reset();
            rgn_rnd = 0x9E3779B9u;
            gc_lane(0, 6, 16, 16, 0, CH[j]);
            while (gp[0].seq < 2000u) { gp_block(now, 65536u, 120.0f, 0, 0.0f); now += 65536u; }
            steps = (int)gp[0].seq;
            n = (int)gp_hits_total[0];
            printf("15b CHANCE %.1f: %d hits of %d steps\n", CH[j], n, steps);
            if ((j == 0 && n != 0) || (j == 1 && n != steps) || (j == 2 && !(n > steps * 45 / 100 && n < steps * 55 / 100))) bad++;
        }
        gc_reset();
        gc_lane(0, 0, 16, 16, 0, 1.0f);
        for (i = 0; i < 100; i++) gp_block((uint32_t)i * 256u, 256u, 120.0f, 0, 0.0f);
        printf("15b RATE OFF: %d steps run, gate %d\n", (int)gp[0].seq, gp[0].on);
        if (gp[0].seq || gp[0].on) bad++;
    }
    /* 15c: LFO 1 -> the filter's CUTOFF (row 11), a falling saw at 1.9 Hz; the gate from lane 1 */
    {
        float before, after, drift_a, drift_b;
        int pass;
        for (pass = 0; pass < 3; pass++) {     /* 0: RETRIG GATE 1, 1: RETRIG OFF, 2: PEAKS' envelope on GATE 1 */
            rk_seed(0x21u); rc_pause(); rc_load(0); rc_resume();
            for (i = 0; i < 18; i++) rk_set_route(i, 0, -1, 0, 0.0f, 0);
            rk_clear_macro_targets();
            rk_set_param(RS_GLO, FLT_P0 + 4, 0.0f);
            rk_set_param(RS_GLO, FLT_P0 + 1, 0.5f);
            rk_set_ext(6, 0.0f);
            if (pass < 2) {
                rk_set_param(RS_LFO, 0, 2.0f); rk_set_param(RS_LFO, 1, 0.6f); rk_set_param(RS_LFO, 2, 0.0f);
                rk_set_param(RS_LFO, 3, pass == 0 ? 2.0f : 0.0f);
                rk_set_route(11, 13, RS_GLO, FLT_P0 + 1, 0.25f, 0);
            } else {
                rk_set_module(RS_MODA, 20);    /* PEAKS: FUNC ENV, GATE: GATE 1 (value 6) */
                rk_set_param(RS_MODA, 0, 0.0f); rk_set_param(RS_MODA, 3, 6.0f); rk_set_param(RS_MODA, 1, 0.1f); rk_set_param(RS_MODA, 2, 0.7f);
                rk_set_route(11, 9, RS_GLO, FLT_P0 + 1, 0.4f, 0);
            }
            rk_note_on(55.0f, 0.8f);
            for (j = 0; j < 280; j++) rk_render(48);   /* ~0.3 s: the LFO well into its cycle */
            before = rk_eff_param(RS_GLO, FLT_P0 + 1);
            for (j = 0; j < 4; j++) rk_render(48);
            drift_a = rk_eff_param(RS_GLO, FLT_P0 + 1);
            rk_set_ext(6, 1.0f);                       /* a hit */
            for (j = 0; j < 8; j++) rk_render(48);
            after = rk_eff_param(RS_GLO, FLT_P0 + 1);
            rk_set_ext(6, 0.0f);
            drift_b = after;
            printf("15c %s: cutoff %.3f before the hit (%.3f 4 ms later), %.3f 8 ms after it\n",
                   pass == 0 ? "LFO RETRIG GATE 1" : pass == 1 ? "LFO RETRIG OFF" : "PEAKS envelope, GATE 1", before, drift_a, drift_b);
            if (pass == 0 && !(after > before + 0.1f)) bad++;               /* the cycle starts again: its top */
            if (pass == 1 && !(fabsf(after - before) < 0.05f)) bad++;       /* not retriggered: it keeps falling */
            if (pass == 2 && !(after > drift_a + 0.05f && fabsf(drift_a - before) < 0.02f)) bad++;   /* the envelope rises on the hit only */
        }
    }
    /* 15d: lanes OFF and nothing connected: a preset (1 s, a chord, each run in a process of its own, as check 1) sounds sample for
     * sample as it does with the GATE PATTERN's code never run (the engine's two extra sources read 0 and nothing sets them) */
    {
        static float a[2][48000];
        int same = 1, pk;
        for (pk = 0; pk < 3; pk++) {
            char cmd[512];
            int run;
            for (run = 0; run < 2; run++) {
                FILE *f;
                size_t got;
                snprintf(cmd, sizeof cmd, "'%s' %s %d 1", gc_self, run ? "--playgate" : "--play", pk * 7);
                if (!(f = popen(cmd, "r"))) { same = 0; break; }
                got = fread(a[run], sizeof(float), 48000, f);
                pclose(f);
                if (got != 48000) same = 0;
            }
            if (memcmp(a[0], a[1], sizeof a[0])) same = 0;
        }
        if (!same) bad++;
        printf("15d GATE PATTERN lanes OFF: three presets' second of output is sample for sample the same as without it: %s\n", same ? "yes" : "NO");
    }
    printf("15 GATE PATTERN: hits on the Euclid steps at the tempo, CHANCE, RATE OFF, it retriggers an LFO and an envelope, unused it changes nothing: %d wrong\n", bad);
    if (bad)
        (*fails)++;
}
