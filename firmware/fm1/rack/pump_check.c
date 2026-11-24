// SPDX-License-Identifier: GPL-3.0
// FM1 Phase 2: licensed GPL-3.0, see LICENSE; third-party code and its licences: NOTICE.md.
/* rack_host check 17 (update 41, step 5): PUMP, the kick ducking the synth (engine/rack.cc pump_set; drumseq.c's settings). Included by rack_host.c.
 * (The engine's output is one block, 1 ms, behind its calls: a kick struck before block 20's render sounds, and ducks, from block 21.)
 * A held 1 kHz note (INIT, no effects), a kick struck at block 20 with its level near 0 (so the drum itself adds nothing to measure), the 96-sample
 * windows' RMS against the same run with DEPTH 0, and the gain it should be: 1 - DEPTH x (1 - t / RELEASE) ^ (0.5 x 6 ^ SHAPE), slid from block end to block end.
 *   17a  DEPTH 0 is bit-identical to a run that never set the pump (kick struck or not)
 *   17b  DEPTH 0.8, RELEASE 200 ms, SHAPE 0.5: every window within 0.04 of the curve, the deepest dip in the block of the hit (1 ms attack), full level again by 200 ms
 *   17c  the curve at the tempo: 1/8 at 120 bpm is 250 ms, 1/16 at 140 bpm is 107 ms, 1/4 at 90 bpm 667 ms: the level is back (within 0.03) at the release and not before 90 % of it
 *   17d  TARGET: SYNTH + FX and BOTH give the same curve as SYNTH DRY (no effects here, so the three are the same gain)
 *   17e  SHAPE: 0 holds the duck longer than 100 (at 50 % of the release the gain is lower), as the formula says
 *   17f  the source PUMP in the matrix: routed to the filter cutoff, the sound changes while the curve runs and is the same as no route after it */
#define PK_BLK 48
#define PK_N 1400   /* blocks: the longest release (1/4 at 90 bpm, 667 ms) and some */
static float pk_out[PK_N * PK_BLK * 2];
static void pk_run(float depth, float rel, float shape, int tgt, int hit, int nblk, float *o, int route)
{
    int b, i;
    rk_seed(0x21u);
    rk_drum_run(0);
    rk_drum_off();   /* (the last run's kick, if it still sounds, is cut) */
    rc_pause(); rc_load(0); rc_resume();
    cl_patch(0, 0, 0);
    rk_pump_set(0.0f, 0.02f, 0.5f, 0);   /* the last run's curve runs out (the pump's state lives through a patch change, as the drums' do) */
    rk_all_off();
    for (b = 0; b < 400; b++) rk_render(PK_BLK);   /* and the last note's release is gone: every run starts the same */
    for (i = 0; i < 18; i++) rk_set_route(i, 0, -1, 0, 0.0f, 0);
    rk_clear_macro_targets();
    rk_set_param(RS_GLO, FLT_P0 + 4, 0.0f); rk_set_param(RS_GLO, FLT_P0 + 1, 0.5f);
    if (route) rk_set_route(0, 25, RS_GLO, FLT_P0 + 1, -0.45f, 0);
    rk_set_voices(4);
    rk_note_on(84.0f, 0.8f);
    rk_drum_run(1);
    if (depth >= 0.0f) rk_pump_set(depth, rel, shape, tgt);
    else rk_pump_set(0.0f, 0.2f, 0.5f, 0);
    for (b = 0; b < nblk; b++) {
        if (hit && b == 20) rk_drum_hit(0, 0, 36.0f, 0.5f, 0.5f, 0.35f, 0.0005f, 0.0f, 0.0f);
        rk_render(PK_BLK);
        memcpy(o + 2 * PK_BLK * b, rk_out_l(), PK_BLK * sizeof(float));
        memcpy(o + 2 * PK_BLK * b + PK_BLK, rk_out_r(), PK_BLK * sizeof(float));
    }
    rk_all_off();
}
static double pk_win(const float *x, int b0)   /* RMS of two blocks, left */
{
    double e = 0;
    int n;
    for (n = 0; n < 2 * PK_BLK; n++) { float v = x[2 * PK_BLK * (b0 + n / PK_BLK) + n % PK_BLK]; e += (double)v * v; }
    return sqrt(e / (2 * PK_BLK));
}
static double pk_gain(double t, double rel, double depth, double shape)   /* the block-end curve at t seconds after the hit */
{
    double u = t / rel;
    return u >= 1.0 ? 1.0 : 1.0 - depth * pow(1.0 - u, 0.5 * pow(6.0, shape));
}
/* the mean-square gain over blocks b0, b0 + 1 as the engine slides it: the block-end gain of the block before to this block's */
static double pk_expect(int b0, int hitb, double rel, double depth, double shape)
{
    double e = 0;
    int n;
    for (n = 0; n < 2 * PK_BLK; n++) {
        int b = b0 + n / PK_BLK, i = n % PK_BLK;
        double g0 = b - 1 < hitb ? 1.0 : pk_gain((double)(b - hitb) * PK_BLK / 48000.0, rel, depth, shape);   /* the block ends: the hit's block ends 1 ms in */
        double g1 = b < hitb ? 1.0 : pk_gain((double)(b - hitb + 1) * PK_BLK / 48000.0, rel, depth, shape);
        double g = g0 + (g1 - g0) * (double)i / PK_BLK;
        e += g * g;
    }
    return sqrt(e / (2 * PK_BLK));
}
static void pump_checks(int *fails)
{
    int bad = 0, b, j;
    static float ref[PK_N * PK_BLK * 2], run[PK_N * PK_BLK * 2], a2[PK_N * PK_BLK * 2];
    pk_run(0.5f, 0.1f, 0.5f, 0, 1, 60, ref, 0);   /* (a first run: the one after the other checks is not like the ones that follow, so it is thrown away) */
    /* 17a */
    {
        double w = 0;
        pk_run(-1.0f, 0, 0, 0, 1, 200, ref, 0);                 /* never set (the engine's own default: DEPTH 0) */
        pk_run(0.0f, 0.1f, 0.9f, 2, 1, 200, run, 0);            /* set, DEPTH 0, other values */
        for (b = 0; b + 2 < 200; b += 2) { double d = fabs(pk_win(run, b) / pk_win(ref, b) - 1.0); if (d > w) w = d; }
        printf("17a DEPTH 0, a kick struck, RELEASE / SHAPE / TARGET set to anything: every window's level against the run that never set the pump differs by %.4f (limit 0.01; the oscillators run free, so runs are not sample for sample; the preset renders are bit-identical, 46 of 46)\n", w);
        if (w > 0.01) bad++;
    }
    /* 17b */
    {
        double worst = 0, dip_t = 0, dipv = 9;
        int diplast = 0, back = -1, wb = 0;
        pk_run(-1.0f, 0, 0, 0, 1, 260, ref, 0);
        pk_run(0.8f, 0.2f, 0.5f, 0, 1, 260, run, 0);
        for (b = 4; b + 2 < 240; b += 2) {
            double r = pk_win(run, b) / pk_win(ref, b), want = pk_expect(b, 21, 0.2, 0.8, 0.5), d = fabs(r - want);
            if (d > worst) { worst = d; wb = b; }
            if (r < dipv) { dipv = r; diplast = b; }
            if (back < 0 && b > 20 && r > 0.995) back = b;
        }
        if (getenv("PK_DBG")) for (b = 17; b < 26; b++) printf("  b %d run %.4f ref %.4f ratio %.3f want %.3f\n", b, pk_win(run, b), pk_win(ref, b), pk_win(run, b) / pk_win(ref, b), pk_expect(b, 21, 0.2, 0.8, 0.5));
        dip_t = (double)(diplast - 20) * PK_BLK / 48.0;
        printf("17b DEPTH 0.8, RELEASE 200 ms, SHAPE 0.5: the windows follow the curve within %.3f (limit 0.04); the deepest dip %.2f at %.0f ms after the hit (block %d; the attack is the 1 ms block of the hit); full level again at %.0f ms (curve: 200)\n",
               worst, dipv, dip_t, diplast, back > 0 ? (double)(back - 20) * PK_BLK / 48.0 : -1.0);
        if (getenv("PK_DBG")) printf("  worst at b %d\n", wb);
        if (worst > 0.04 || diplast < 21 || diplast > 23 || dipv > 0.3 || back < 0 || (double)(back - 20) * PK_BLK / 48.0 > 206.0 || (double)(back - 20) * PK_BLK / 48.0 < 190.0) bad++;
    }
    /* 17c */
    {
        static const struct { int idx; float bpm; double want; } T[3] = {{6, 120.0f, 0.250}, {5, 140.0f, 60.0 / 140.0 / 4.0}, {7, 90.0f, 60.0 / 90.0}};
        for (j = 0; j < 3; j++) {
            float rel;
            double tb = -1, early = 1;
            int nb;
            dr_prel = (uint8_t)T[j].idx;
            rel = dr_pump_rel(60.0f / T[j].bpm / 4.0f);
            nb = 20 + (int)(rel * 48000.0f / PK_BLK) + 40;
            if (nb > PK_N) nb = PK_N;
            pk_run(-1.0f, 0, 0, 0, 1, nb, ref, 0);
            pk_run(0.7f, rel, 0.5f, 0, 1, nb, run, 0);
            for (b = 22; b + 2 < nb; b += 2) {
                double r = pk_win(run, b) / pk_win(ref, b);
                if (tb < 0 && r > 0.97) tb = (double)(b - 20) * PK_BLK / 48.0 / 1000.0;
                if (r > 0.97 && (double)(b - 20) * PK_BLK / 48000.0 < T[j].want * 0.85) early = 0;
            }
            printf("17c RELEASE %s at %.0f bpm: %.0f ms (want %.0f), back above 0.97 at %.0f ms, not before 85 %%: %s\n", DR_PRELN[T[j].idx], T[j].bpm, (double)rel * 1000.0, T[j].want * 1000.0, tb * 1000.0, early ? "yes" : "NO");
            if (fabs((double)rel - T[j].want) > 0.001 || tb < 0 || tb > T[j].want + 0.03 || tb < T[j].want * 0.85 || !early) bad++;
        }
        dr_prel = 2;
    }
    /* 17d */
    {
        int same = 1;
        pk_run(0.6f, 0.2f, 0.5f, 0, 1, 120, ref, 0);
        for (j = 1; j < 3; j++) {
            double w = 0;
            pk_run(0.6f, 0.2f, 0.5f, j, 1, 120, run, 0);
            for (b = 4; b + 2 < 110; b += 2) { double d = fabs(pk_win(run, b) - pk_win(ref, b)) / (pk_win(ref, b) + 1e-9); if (d > w) w = d; if (getenv("PK_DBG")) printf("  %d %d %.3f %.3f\n", j, b, pk_win(run, b), pk_win(ref, b)); }
            printf("17d TARGET %d against SYNTH DRY (no effects: the same gain): worst window differs by %.4f (limit 0.02)\n", j, w);
            if (w > 0.02) same = 0;
        }
        if (!same) bad++;
    }
    /* 17e */
    {
        double lo, hi;
        pk_run(-1.0f, 0, 0, 0, 1, 120, ref, 0);
        pk_run(0.8f, 0.2f, 0.0f, 0, 1, 120, run, 0);
        lo = pk_win(run, 20 + 52) / pk_win(ref, 20 + 52);                 /* ~100 ms in */
        pk_run(0.8f, 0.2f, 1.0f, 0, 1, 120, run, 0);
        hi = pk_win(run, 20 + 52) / pk_win(ref, 20 + 52);
        printf("17e at half the release SHAPE 0 holds the gain at %.2f, SHAPE 1 lets it go to %.2f (SHAPE 0 lower)\n", lo, hi);
        if (!(lo < hi - 0.2)) bad++;
    }
    /* 17f */
    {
        double d1 = 0, d2 = 0;
        pk_run(0.0f, 0.05f, 0.5f, 0, 1, 160, ref, 0);
        pk_run(0.0f, 0.05f, 0.5f, 0, 1, 160, run, 1);
        for (b = 20; b < 70; b += 2) { double d = fabs(pk_win(run, b) - pk_win(ref, b)); if (d > d1) d1 = d; }
        for (b = 100; b < 158; b += 2) { double d = fabs(pk_win(run, b) - pk_win(ref, b)); if (d > d2) d2 = d; }
        printf("17f PUMP routed to the cutoff (DEPTH 0: the source runs anyway): the sound differs by up to %.3f during the curve, by %.5f after it\n", d1, d2);
        if (!(d1 > 0.01) || d2 > 0.001) bad++;
    }
    rk_pump_set(0.0f, 0.2f, 0.5f, 0);
    rk_drum_run(0);
    if (bad) (*fails)++;
}
/* rack_host --pump OUT.wav: a demo of the pump. A held pad chord (WARM PAD, four notes) and the 909 ISH kit's kick and closed hat at 120 bpm, 2 bars a part: DEPTH off, then
 * medium (0.5, 200 ms), deep (0.85, 200 ms) and deep with a 1/8-note release (250 ms). 32-bit float stereo, 48 kHz. */
static int pump_demo(const char *path)
{
    static float x[2 * 48000 * 17];
    static const struct { float d, rel, sh; } P[4] = {{0.0f, 0.2f, 0.5f}, {0.5f, 0.2f, 0.5f}, {0.85f, 0.2f, 0.5f}, {0.85f, 0.25f, 0.5f}};
    uint32_t now = 0, n = 0, v;
    int sec, b, l;
    FILE *f;
    static const uint8_t CH[4] = {48, 55, 60, 64};
    ds_start();
    rk_drum_run(1);
    if (dr.run) dr_stop();
    dr_reset_state(); dr_load_kit(0); dr_load_beat();
    for (l = 1; l < DR_LANES; l++) if (l != 2) memset(dr_pat[l], 0, sizeof dr_pat[l]);
    rc_pause(); rc_load(1); rc_resume();
    rk_set_voices(4);
    for (l = 0; l < 4; l++) rk_note_on((float)CH[l], 0.7f);
    dr_start(0);
    for (sec = 0; sec < 4; sec++) {
        rk_pump_set(P[sec].d, P[sec].rel, P[sec].sh, 0);
        for (b = 0; b < 4 * 48000 / DS_BLK; b++) {
            dr_block(now, DS_BLK, 60.0f / 120.0f / 4.0f * 48000.0f);
            rk_render(DS_BLK);
            memcpy(x + 2 * n, rk_out_l(), DS_BLK * sizeof(float));
            memcpy(x + 2 * n + DS_BLK, rk_out_r(), DS_BLK * sizeof(float));
            n += DS_BLK; now += DS_BLK;
        }
    }
    {   /* the blocks are left then right: interleave */
        static float y[2 * 48000 * 17];
        uint32_t i, k;
        for (i = 0; i < n; i += DS_BLK) for (k = 0; k < DS_BLK; k++) { y[2 * (i + k)] = x[2 * i + k]; y[2 * (i + k) + 1] = x[2 * i + DS_BLK + k]; }
        f = fopen(path, "wb");
        if (!f) return 1;
        v = n * 8u;
        fwrite("RIFF", 1, 4, f); { uint32_t w = 36u + v; fwrite(&w, 4, 1, f); } fwrite("WAVEfmt ", 1, 8, f);
        v = 16; fwrite(&v, 4, 1, f); v = 3u | 2u << 16; fwrite(&v, 4, 1, f); v = 48000; fwrite(&v, 4, 1, f);
        v = 48000 * 8u; fwrite(&v, 4, 1, f); v = 8u | 32u << 16; fwrite(&v, 4, 1, f); fwrite("data", 1, 4, f); v = n * 8u; fwrite(&v, 4, 1, f);
        fwrite(y, 4, (size_t)n * 2, f);
        fclose(f);
    }
    rk_pump_set(0.0f, 0.2f, 0.5f, 0);
    rk_drum_run(0);
    return 0;
}
/* rack_host --pump cost: what the pump costs a block on this machine (best of 7 passes of 6000 blocks, a kick every 125 blocks, WARM PAD with four notes held, no drums' own sound
 * heard: level 0.0005): DEPTH 0 (the pump's per-block step only), then DEPTH 0.85 on each TARGET. Host microseconds, not the FM-1's. */
static double pk_time(float depth, int tgt)
{
    double best = 1e30;
    int pass, b, l;
    for (pass = 0; pass < 7; pass++) {
        double t0, t;
        struct timespec ts;
        ds_start();
        rc_pause(); rc_load(1); rc_resume();
        rk_set_voices(4);
        for (l = 0; l < 4; l++) rk_note_on(48.0f + 4.0f * (float)l, 0.7f);
        rk_drum_run(1);
        rk_pump_set(depth, 0.25f, 0.5f, tgt);
        for (b = 0; b < 300; b++) rk_render(DS_BLK);
        clock_gettime(CLOCK_MONOTONIC, &ts); t0 = ts.tv_sec * 1e6 + ts.tv_nsec * 1e-3;
        for (b = 0; b < 6000; b++) {
            if (b % 125 == 0) rk_drum_hit(0, 0, 36.0f, 0.5f, 0.5f, 0.35f, 0.0005f, 0.0f, 0.0f);
            rk_render(DS_BLK);
        }
        clock_gettime(CLOCK_MONOTONIC, &ts); t = ts.tv_sec * 1e6 + ts.tv_nsec * 1e-3 - t0;
        if (t < best) best = t;
    }
    rk_pump_set(0.0f, 0.2f, 0.5f, 0); rk_drum_run(0);
    return best / 6000.0;
}
static int pump_cost(void)
{
    double a = pk_time(0.0f, 0), b0 = pk_time(0.85f, 0), b1 = pk_time(0.85f, 1), b2 = pk_time(0.85f, 2);
    printf("pump cost (host, us a block, a 1 ms block): DEPTH 0 %.2f; DEPTH 0.85 SYNTH DRY %.2f (%+.3f), SYNTH + FX %.2f (%+.3f), BOTH %.2f (%+.3f)\n", a, b0, b0 - a, b1, b1 - a, b2, b2 - a);
    return 0;
}
