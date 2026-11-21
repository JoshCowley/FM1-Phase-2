// SPDX-License-Identifier: GPL-3.0
// FM1 Phase 2: licensed GPL-3.0, see LICENSE; third-party code and its licences: NOTICE.md.
/* What a block costs the two cores, on a computer (included by rack_host.c as section 14; built alone with -DDT_STANDALONE for the older trees).
 * The second core is the host_cpu2.h stand-in that runs its job when the first core waits for it (HC2_DEFER) and the engine's clock (rk_set_ticks,
 * nanoseconds here) gives the job's own time: c2 = the job, c1 = the block less the job. A block of the two cores takes max(c1, c2) (the first
 * core waits for the job at the join, so a slow job stalls it). Every block is timed in 5 passes and the least of each kept (the same sound every
 * pass, so block n does the same work: the least is the one the machine did not disturb).
 * Host nanoseconds are not the FM-1's: DT_SCALE turns them into points of one FM-1 core (100 = the 1 ms block) with one number, the device's 4 held
 * Braids voices (23.1 points over the idle floor, 13 Nov bench; README Update 8) against the same four on this machine (INIT, no effects). An estimate:
 * good to a ratio, not to a point.
 *   14a  drums stopped cost nothing: a preset's two-core block with drum_run(1) but no hit equals the one without the drums' calls (the engine
 *        adds no work: the job and the first core, within noise), and the drums' render count stays 0
 *   14b  the drums playing (the 909 ISH kit, 4 voices, 120 bpm) on the heaviest 4-voice preset: the first core's block (max of the two) stays
 *        under 90 points (the overload guard's, rack_core.c rc_overload) and no block holds a spike: the worst block with a hit is within
 *        1.5 x the worst without
 */
#include <time.h>
#if defined(__SSE__) || defined(__x86_64__)
#include <xmmintrin.h>
#define DT_FTZ() _mm_setcsr(_mm_getcsr() | 0x8040u)   /* flush denormals: a decaying tail on this machine's FPU takes 8 x the time (the FM-1's does not), which would swamp the timing */
#else
#define DT_FTZ() ((void)0)
#endif

#define DT_BLK 48
#define DT_N 1500
#define DT_PASS 5
static uint32_t dt_clk(void) { struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t); return (uint32_t)((uint64_t)t.tv_sec * 1000000000ull + (uint64_t)t.tv_nsec); }
typedef struct { double c1, c2, blk, p99, max, maxhit, maxnohit, mean_hit; } dt_t;
static float dt_c1[DT_N], dt_c2[DT_N];
#ifndef DT_NODRUMS
static void dt_hit_lane(int l, float vel)
{
    const dr_snd_t *k = &dr_kit[l];
    rk_drum_hit(DR_VOICE[l], k->model, (float)k->pitch, (float)k->p1 * 0.01f, (float)k->p2 * 0.01f, (float)k->decay_cs * 0.01f,
                vel * (float)k->level * 0.01f, (float)k->pan * 0.01f, (float)k->send * 0.01f);
}
/* a 16th is 125 blocks (120 bpm): kick 0 6 8 10, snare 4 12, closed hat every 2nd (the open hat on 14), clap 12, perc 7 11 */
static int dt_hits(int b)
{
    int st, n = 0;
    if (b % 125) return 0;
    st = (b / 125) & 15;
    if (st == 0 || st == 6 || st == 8 || st == 10) { dt_hit_lane(0, 1.0f); n++; }
    if (st == 4 || st == 12) { dt_hit_lane(1, 0.9f); n++; }
    if (st == 14) { dt_hit_lane(3, 0.6f); n++; }
    else if ((st & 1) == 0) { dt_hit_lane(2, 0.6f); n++; }
    if (st == 12) { dt_hit_lane(4, 0.8f); n++; }
    if (st == 7 || st == 11) { dt_hit_lane(5, 0.8f); n++; }
    return n;
}
#endif
/* preset < 0: INIT (no effects). nv voices held (four notes). drums: 0 the drums' calls absent, 1 drum_run(1) and no hit, 2 the pattern. */
static int dt_hitblk[DT_N];
static dt_t dt_run(int preset, int nv, int drums)
{
    dt_t r;
    int pass, b, i;
    double s1 = 0, s2 = 0, sh = 0, hs = 0;
    int nh = 0;
    static float tmp[DT_N];
    memset(&r, 0, sizeof r);
    DT_FTZ();
    for (b = 0; b < DT_N; b++) { dt_c1[b] = dt_c2[b] = 1e30f; dt_hitblk[b] = 0; }
    for (pass = 0; pass < DT_PASS; pass++) {
        uint32_t jb = 0, wb = 0, ib = 0, j0;
        rk_seed(0x21u);
        hc2_mode = HC2_DEFER; hc2_giveup_pct = hc2_norun_pct = 0;
        rk_set_worker(hc2_cpu_run, hc2_cpu_wait); rk_set_worker_ex(hc2_cpu_sync, hc2_cpu_on);
        rk_set_ticks(dt_clk);
        if (preset == -2 || preset == -3) {   /* Clouds in FX 1 and the reverb in FX 2, GRANULR, DENSITY 0.8 (-2) or 1.0 (-3) */
            int s;
            cl_patch(30, 32, 0); s = cl_clouds_slot();
            cl_set(s, P_POS, 0.3f); cl_set(s, P_SIZE, 0.5f); cl_set(s, P_DENS, preset == -2 ? 0.8f : 1.0f); cl_set(s, P_TEX, 0.4f); cl_set(s, P_BLEND, 0.8f);
        } else if (preset == -4) { rk_init(); rk_fm1_fit(); }   /* the engine's own defaults: the same patch in every release */
        else if (preset < 0) cl_patch(0, 0, 0); else rc_load(preset);
        rk_set_voices(nv);
        for (i = 0; i < 4; i++) rk_note_on(48.0f + 4.0f * (float)i, 0.8f);
#ifndef DT_NODRUMS
        dr_reset_state();
        rk_drum_off();
        rk_drum_run(drums ? 1 : 0);
#endif
        for (b = 0; b < 300; b++) rk_render(DT_BLK);                  /* the notes settle (the pattern starts at block 0 of the timed run) */
        rk_core_stats_reset();
        rk_core_stats(&j0, &wb, &ib);
        for (b = 0; b < DT_N; b++) {
            uint32_t t0, t1, j1;
#ifndef DT_NODRUMS
            if (drums == 2 && dt_hits(b)) dt_hitblk[b] = 1;
#endif
            t0 = dt_clk();
            rk_render(DT_BLK);
            t1 = dt_clk();
            rk_core_stats(&j1, &wb, &ib);
            jb = j1 - j0; j0 = j1;
            {
                float tot = (float)(t1 - t0), c2 = (float)jb, c1 = tot - c2;
                if (c1 < dt_c1[b]) dt_c1[b] = c1;
                if (c2 < dt_c2[b]) dt_c2[b] = c2;
            }
        }
        hc2_stop();
    }
    rk_set_ticks(0);
    for (b = 0; b < DT_N; b++) {
        double c1 = dt_c1[b], c2 = dt_c2[b], m = c1 > c2 ? c1 : c2;
        tmp[b] = (float)m;
        s1 += c1; s2 += c2; sh += m;
        if (m > r.max) r.max = m;
        if (dt_hitblk[b]) { if (m > r.maxhit) r.maxhit = m; hs += m; nh++; } else if (m > r.maxnohit) r.maxnohit = m;
    }
    r.c1 = s1 / DT_N; r.c2 = s2 / DT_N; r.blk = sh / DT_N; r.mean_hit = nh ? hs / nh : 0;
    {   /* the 99th percentile of the blocks */
        int j, k;
        for (j = 1; j < DT_N; j++) { float v = tmp[j]; for (k = j - 1; k >= 0 && tmp[k] > v; k--) tmp[k + 1] = tmp[k]; tmp[k + 1] = v; }
        r.p99 = tmp[DT_N * 99 / 100];
    }
    return r;
}
/* host ns to points of an FM-1 core: k = the device's 4 held Braids voices (23.1 points over the 7.9 idle floor) over this machine's ns for the same */
static double dt_k(void)
{
    double best = 1e30;
    int i;
    if (getenv("DT_K")) return atof(getenv("DT_K"));   /* a fixed scale: to compare two builds on the same ruler */
    for (i = 0; i < 3; i++) { dt_t r = dt_run(-1, 4, 0); if (r.blk < best) best = r.blk; }
    return 23.1 / best;
}
static double dt_pts(double ns, double k) { return 7.9 + ns * k; }
#define DT_GUARD 90.0   /* rack_core.c rc_overload: a block over 90 % of its time sheds a voice */
static void dt_line(const char *what, dt_t r, double k)
{
    printf("%-34s  first core %5.1f  second core %5.1f  block (max) mean %5.1f p99 %5.1f max %5.1f points\n", what, dt_pts(r.c1, k), dt_pts(r.c2, k), dt_pts(r.blk, k), dt_pts(r.p99, k), dt_pts(r.max, k));
}
static void dt_spikes(int p)
{
    double k = dt_k(); int b;
    dt_run(p, 4, 0);
    for (b = 0; b < DT_N; b++) { double m = dt_c1[b] > dt_c2[b] ? dt_c1[b] : dt_c2[b]; if (dt_pts(m, k) > 100.0) printf("block %d: %.0f points (first %.0f, second %.0f)\n", b, dt_pts(m, k), dt_pts(dt_c1[b], k), dt_pts(dt_c2[b], k)); }
}
#ifndef DT_NODRUMS
/* the output's peak, its DC and the samples at the limiter's ceiling over 6 s of the pattern: drums alone, a pad alone, both */
static void dt_levels(int preset, int notes, int drums, const char *what)
{
    int b, i, n = 0, hot = 0;
    double pk = 0, dc = 0, rms = 0;
    rk_seed(0x21u);
    hc2_mode = HC2_DEFER; rk_set_worker(hc2_cpu_run, hc2_cpu_wait); rk_set_worker_ex(hc2_cpu_sync, hc2_cpu_on);
    if (preset == -2) { int s; cl_patch(30, 32, 0); s = cl_clouds_slot(); cl_set(s, P_DENS, 0.8f); cl_set(s, P_BLEND, 0.5f); } else if (preset < 0) cl_patch(0, 0, 0); else rc_load(preset);
    rk_set_voices(4);
    dr_reset_state(); rk_drum_off(); rk_drum_run(drums);
    for (i = 0; i < notes; i++) rk_note_on(48.0f + 4.0f * (float)i, 0.9f);
    for (b = 0; b < 6000; b++) {
        if (drums) dt_hits(b);
        rk_render(DT_BLK);
        if (b < 500) continue;
        for (i = 0; i < DT_BLK; i++) {
            double v = rk_out_l()[i], a = v < 0 ? -v : v;
            if (a > pk) pk = a;
            if (a >= 0.98) hot++;
            dc += v; rms += v * v; n++;
        }
    }
    hc2_stop();
    printf("%-30s peak %.3f  samples at the ceiling %d  DC %+.4f  rms %.3f\n", what, pk, hot, dc / n, sqrt(rms / n));
}
static void dt_levels_all(int preset)
{
    dt_levels(preset, 0, 2, "drums alone");
    dt_levels(preset, 4, 0, "four notes alone");
    dt_levels(preset, 4, 2, "four notes + drums");
}
#endif
/* --dtime scan | --dtime K NV: the numbers by hand */
static int dt_cli(int argc, char **argv)
{
    double k = dt_k();
    int p, nv;
    printf("scale: %.4f points per host ns (4 INIT voices = 23.1 points over the idle 7.9)\n", k);
    if (argc >= 3 && !strcmp(argv[2], "scan")) {
        for (p = 0; p < NPRESETS; p++) {
            char nm[40];
            dt_t r = dt_run(p, 4, 0);
            snprintf(nm, sizeof nm, "%2d %s", p, PRESETS[p].name);
            dt_line(nm, r, k);
        }
        return 0;
    }
#ifndef DT_NODRUMS
    if (argc >= 3 && !strcmp(argv[2], "levels")) { dt_levels_all(argc >= 4 ? atoi(argv[3]) : -1); return 0; }
#endif
    if (argc >= 4 && !strcmp(argv[2], "spikes")) { dt_spikes(atoi(argv[3])); return 0; }
    p = argc >= 3 ? atoi(argv[2]) : -1;
    if (argc >= 3 && argv[2][0] >= 'A' && argv[2][0] <= 'Z') {   /* by name (underscores for spaces: the numbers differ between releases) */
        int q, i; char nm[40];
        for (i = 0; argv[2][i] && i < 39; i++) nm[i] = argv[2][i] == '_' ? ' ' : argv[2][i];
        nm[i] = 0; p = -1;
        for (q = 0; q < NPRESETS; q++) if (!strcmp(PRESETS[q].name, nm)) p = q;
        if (p < 0) { printf("no preset %s\n", nm); return 1; }
    }
    nv = argc >= 4 ? atoi(argv[3]) : 4;
    dt_line("no drum calls", dt_run(p, nv, 0), k);
#ifndef DT_NODRUMS
    dt_line("drum_run(1), nothing struck", dt_run(p, nv, 1), k);
    {
        dt_t r = dt_run(p, nv, 2);
        dt_line("909 ISH pattern playing", r, k);
        printf("   worst block %.1f points with a hit, %.1f without; mean of the hit blocks %.1f\n", dt_pts(r.maxhit, k), dt_pts(r.maxnohit, k), dt_pts(r.mean_hit, k));
    }
#endif
    return 0;
}
#ifndef DT_NODRUMS
static int dt_by_name(const char *n) { int q; for (q = 0; q < NPRESETS; q++) if (!strcmp(PRESETS[q].name, n)) return q; return -1; }
static void dt_checks(int *fails)
{
    static const char *const HEAVY[4] = {"TIDE PAD", "ATMOSPHERE", "STRING BLOOM", "ROTARY ORGAN"};
    double k = dt_k();
    int i, bad14a = 0, bad14b = 0;
    /* 14a: drums stopped cost nothing: with drum_run(1) and no hit, no block is rendered by either core (exact), and the time is the same as with no drum calls (within 4 %: host noise) */
    {
        uint32_t j0, w0, i0, j1, w1, i1;
        int n3, n4;
        dt_t a, b;
        for (i = 0; i < 2; i++) {
            int p = i == 0 ? -1 : dt_by_name("ATMOSPHERE");
            a = dt_run(p, 4, 0); b = dt_run(p, 4, 1);
            if (b.c1 > a.c1 * 1.04 + 20.0 || b.c2 > a.c2 * 1.04 + 20.0) { printf("14a %s: drum_run(1), no hit: first core %.1f vs %.1f, second core %.1f vs %.1f points   FAIL\n", p < 0 ? "INIT" : "ATMOSPHERE", dt_pts(b.c1, k), dt_pts(a.c1, k), dt_pts(b.c2, k), dt_pts(a.c2, k)); bad14a++; }
        }
        rk_seed(0x21u); rk_set_worker(hc2_cpu_run, hc2_cpu_wait); rk_set_worker_ex(hc2_cpu_sync, hc2_cpu_on); hc2_mode = HC2_DEFER;
        cl_patch(30, 0, 0); rk_set_voices(4); for (i = 0; i < 4; i++) rk_note_on(48.0f + 4.0f * (float)i, 0.8f);
        rk_drum_off(); rk_drum_run(1);
        rk_core_stats(&j0, &w0, &i0);
        n3 = rk_drum_status(3); n4 = rk_drum_status(4);   /* (cumulative counters: the change over the run is what counts) */
        for (i = 0; i < 1000; i++) rk_render(DT_BLK);
        rk_core_stats(&j1, &w1, &i1);
        if (rk_drum_status(3) != n3 || rk_drum_status(4) != n4 || rk_drum_status(0) != 0) { printf("14a drum_run(1) and no hit: %d blocks rendered in a job, %d on the first core, %d voices sounding   FAIL\n", rk_drum_status(3) - n3, rk_drum_status(4) - n4, rk_drum_status(0)); bad14a++; }
        hc2_stop();
        printf("14a drums stopped: no block rendered (job 0, first core 0), and the two cores' time with drum_run(1) and no hit equals the time with no drum calls (INIT and ATMOSPHERE): %s\n", bad14a ? "FAIL" : "ok");
    }
    /* 14b: the drums playing on a heavy preset with four voices held: the first core's block (the slower of the two cores) stays under the overload guard's 90 % (99th percentile; rack_core.c
     * rc_overload), the first core's own work is not more than the preset's own plus 25 points, and a block with a hit is not a spike (the worst block with a hit within 1.25 x the worst without) */
    for (i = 0; i < 4; i++) {
        int p = dt_by_name(HEAVY[i]);
        dt_t a = dt_run(p, 4, 0), b = dt_run(p, 4, 2);
        int ok = 1;
        if (dt_pts(b.p99, k) > DT_GUARD) ok = 0;
        if (dt_pts(b.c1, k) > dt_pts(a.c1, k) + 25.0) ok = 0;
        if (b.maxhit > b.maxnohit * 1.25) ok = 0;
        printf("14b %-13s 4 voices: first core %.1f -> %.1f, second core %.1f -> %.1f, block p99 %.1f max %.1f points (guard %.0f); worst block with a hit %.1f, without %.1f%s\n", HEAVY[i], dt_pts(a.c1, k), dt_pts(b.c1, k), dt_pts(a.c2, k), dt_pts(b.c2, k), dt_pts(b.p99, k), dt_pts(b.max, k), DT_GUARD, dt_pts(b.maxhit, k), dt_pts(b.maxnohit, k), ok ? "" : "   FAIL");
        if (!ok) bad14b++;
    }
    *fails += bad14a + bad14b;
    printf("14 timing (host ns scaled to points of an FM-1 core by 4 INIT voices = 23.1 over the idle 7.9; an estimate): drums stopped cost nothing; drums playing on 4 heavy presets stay under the guard: %d wrong\n", bad14a + bad14b);
}
#endif
