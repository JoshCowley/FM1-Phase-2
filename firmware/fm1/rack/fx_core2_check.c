// SPDX-License-Identifier: GPL-3.0
// FM1 Phase 2: licensed GPL-3.0, see LICENSE; third-party code and its licences: NOTICE.md.
/* Clouds on the second core (update 9, RACK_FX_CORE2), checked on a computer with a stand-in for the second core (host_cpu2.h); included by
 * rack_host.c (section 7). Every case is played by a fresh process (rack_host --fx2 ...: the engine keeps state across init(), so two
 * runs in one process would not start alike), 1.5 s in blocks of 48 frames, a chord of four Braids voices held, Clouds and sometimes a
 * second effect, and the two outputs compared:
 *   A  one core (no worker: the voices and Clouds all on the first core, as before)
 *   B  two cores, the job run at once when handed over (HC2_NOW: Clouds draws its random numbers where one core would, so the sound can
 *      be compared sample for sample), the output of every chain order one block (48 frames, 1 ms) later than A's
 *   T  two cores, the job on a real second thread (HC2_THREAD)
 *   D  two cores, the job run when the first core waits for it (HC2_DEFER: after the voices have drawn their random numbers)
 *   G  as B with the worker failing: plat_cpu2_run says no on some blocks, or the job is given up (plat_cpu2_wait says -1) on others
 * 7a  B against A, each chain order and mode: B[k + 48] == A[k]
 * 7b  the same with FREEZE switched on and off, QUALITY and MODE changed, while it plays
 * 7c  G against B (exactly the same: a given-up or declined job is done on the first core, in the same block), no NaN, no block lost
 * 7d  T against A: the same (or, where the chain has a modulated effect, its rms within a tolerance); no NaN
 * 7e  patch changes while it plays (Clouds out and back, another mode): no NaN, no step larger than the one-core run's own
 * 7f  the starting voices: a Clouds patch fits 4 voices with the reverb or delay, as app.c's start_voices asks on two cores
 * Update 10 (the second core's job now holds the 3:2 resamplers, the dry delay and the mix too, and with six voices the voices above the fourth):
 * 7a-7f are unchanged and still ask for the same samples (four voices: nothing else changed the order of any sum).
 * 7g  six voices (the fifth and sixth in the job when Clouds is in FX 1 and an effect in FX 2): D (the deferred stand-in) against A within float
 *     rounding in LOOPDLY, the same sound in GRANULR and SPECTRL (random numbers' order); the failing worker and the real thread as in 7c and 7d
 * 7h  the overload guard (rk_shed_helps) with a made-up clock: which core is the slow one, and whether the top voice is on it */
#include "host_cpu2.h"

enum { FC_C1, FC_C1_DELAY, FC_DELAY_C2, FC_C1_REVERB, FC_REVERB_C2, FC_C2, FC_C1_CHORUS, FC_NCHAIN };
static const char *const FC_CHAIN[FC_NCHAIN] = {"CLOUDS", "CLOUDS>DELAY", "DELAY>CLOUDS", "CLOUDS>REVERB", "REVERB>CLOUDS", "FX2:CLOUDS", "CLOUDS>CHORUS"};
enum { FS_STEADY, FS_FREEZE, FS_CHANGES, FS_PATCH, FS_NSCEN };
static const char *const FC_SCEN[FS_NSCEN] = {"steady", "freeze", "qual+mode", "patch"};
enum { FM_A, FM_B, FM_T, FM_D };    /* the core arrangement: one core; two cores, a job run when handed over; two cores, a real thread; two cores, a job run when waited for */
#define FC_BLOCKS 1500
#define FC_BLK 48

static void fc_effects(int chain, int *fx1, int *fx2)
{
    switch (chain) {
    case FC_C1: *fx1 = 30; *fx2 = 0; break;
    case FC_C1_DELAY: *fx1 = 30; *fx2 = 31; break;
    case FC_DELAY_C2: *fx1 = 31; *fx2 = 30; break;
    case FC_C1_REVERB: *fx1 = 30; *fx2 = 32; break;
    case FC_REVERB_C2: *fx1 = 32; *fx2 = 30; break;
    case FC_C2: *fx1 = 0; *fx2 = 30; break;
    default: *fx1 = 30; *fx2 = 33; break;
    }
}

/* the child: one case, its two output channels as floats on stdout (interleaved) */
static void fc_play(int chain, int mode, int scen, int arr, int giveup, int norun, float *buf)
{
    int fx1, fx2, s, b, i;
    const char *ev = getenv("FX2_VOICES");
    int nv = ev ? atoi(ev) : 4;                  /* update 10: 4 (three notes held, as 7a-7f) or 6 (six held) */
    fc_effects(chain, &fx1, &fx2);
    rk_seed(0x21u);
    hc2_giveup_pct = giveup; hc2_norun_pct = norun; hc2_mode = arr == FM_T ? HC2_THREAD : arr == FM_D ? HC2_DEFER : HC2_NOW;
    if (arr == FM_A) rk_set_worker(0, 0);
    else { rk_set_worker(hc2_cpu_run, hc2_cpu_wait); rk_set_worker_ex(hc2_cpu_sync, hc2_cpu_on); }
    cl_patch(fx1, fx2, 0);
    s = cl_clouds_slot();
    cl_set(s, P_POS, 0.3f); cl_set(s, P_SIZE, 0.5f); cl_set(s, P_DENS, 0.8f); cl_set(s, P_TEX, 0.4f); cl_set(s, P_BLEND, 0.8f);
    cl_set(s, P_SPREAD, 0.5f); cl_set(s, P_FB, 0.3f); cl_set(s, P_REV, 0.3f); cl_set(s, P_MODE, (float)mode);
    if (getenv("FX2_NOVOFF")) rk_fx_core2_voices(0);
    rk_set_voices(nv);
    for (i = 0; i < (nv > 4 ? nv : 3); i++)
        rk_note_on(48.0f + 4.0f * (float)i, 0.8f);
    for (b = 0; b < FC_BLOCKS; b++) {
        if (scen == FS_FREEZE && (b == 500 || b == 1000)) cl_set(s, P_FREEZE, b == 500 ? 1.0f : 0.0f);
        if (scen == FS_CHANGES && b == 400) cl_set(s, P_QUAL, 2.0f);
        if (scen == FS_CHANGES && b == 800) cl_set(s, P_MODE, (float)((mode + 1) % 3));
        if (scen == FS_CHANGES && b == 1100) cl_set(s, P_QUAL, 1.0f);
        if (scen == FS_PATCH && b == 300) rk_set_fx_bypass(s, 1);              /* Clouds switched off from the panel, and on again */
        if (scen == FS_PATCH && b == 350) rk_set_fx_bypass(s, 0);
        if (scen == FS_PATCH && b == 500) cl_swap(s, 0);                       /* Clouds out */
        if (scen == FS_PATCH && b == 800) { cl_swap(s, 30); cl_set(s, P_MODE, (float)((mode + 2) % 3)); cl_set(s, P_DENS, 0.8f); cl_set(s, P_BLEND, 0.8f); }
        if (scen == FS_PATCH && b == 1100) { cl_patch(fx1, fx2, 0); s = cl_clouds_slot(); cl_set(s, P_BLEND, 0.8f); rk_set_voices(4); rk_note_on(52.0f, 0.8f); }
        rk_render(FC_BLK);
        memcpy(buf + 2 * FC_BLK * b, rk_out_l(), FC_BLK * sizeof(float));
        memcpy(buf + 2 * FC_BLK * b + FC_BLK, rk_out_r(), FC_BLK * sizeof(float));
    }
    hc2_stop();
    if (arr == FM_T && getenv("FX2_STACK")) fprintf(stderr, "STACK %u\n", (unsigned)hc2_stack_used());
}

static int fc_nv = 4, fc_novoff = 0;      /* the voices of the next case (6: update 10's), and whether the fifth and sixth stay on the first core */
static int fc_played(const char *self, int chain, int mode, int scen, int arr, int giveup, int norun, float *buf)
{
    char cmd[512];
    FILE *f;
    size_t n;
    snprintf(cmd, sizeof cmd, "FX2_VOICES=%d %s'%s' --fx2 %d %d %d %d %d %d", fc_nv, fc_novoff ? "FX2_NOVOFF=1 " : "", self, chain, mode, scen, arr, giveup, norun);
    if (!(f = popen(cmd, "r")))
        return -1;
    n = fread(buf, sizeof(float), 2 * FC_BLK * FC_BLOCKS, f);
    pclose(f);
    return n == (size_t)(2 * FC_BLK * FC_BLOCKS) ? 0 : -1;
}

/* what the output of the left channel looks like: NaN, the largest step, the longest run of exact zeros after the first 100 ms, rms */
typedef struct { int bad; float step, peak; int zero_run; double rms; } fc_stat_t;
static fc_stat_t fc_stat(const float *x)
{
    fc_stat_t r = {0, 0, 0, 0, 0};
    int b, k, run = 0, n = 0;
    double e = 0;
    float prev = 0;
    for (b = 0; b < FC_BLOCKS; b++)
        for (k = 0; k < FC_BLK; k++) {
            float v = x[2 * FC_BLK * b + k];
            if (!(v == v) || fabsf(v) > 1e6f) { r.bad = 1; continue; }
            if (b * FC_BLK + k > 0 && fabsf(v - prev) > r.step) r.step = fabsf(v - prev);
            if (fabsf(v) > r.peak) r.peak = fabsf(v);
            if (b * FC_BLK + k > 4800) {
                run = v == 0.0f ? run + 1 : 0;
                if (run > r.zero_run) r.zero_run = run;
                e += (double)v * v; n++;
            }
            prev = v;
        }
    r.rms = n ? sqrt(e / n) : 0;
    return r;
}
/* the largest |b[k + 48] - a[k]| over both channels, from the first block on */
static float fc_delayed_diff(const float *a, const float *b)
{
    float d = 0;
    int blk, k, c;
    for (blk = 0; blk + 1 < FC_BLOCKS; blk++)
        for (c = 0; c < 2; c++)
            for (k = 0; k < FC_BLK; k++) {
                float x = a[2 * FC_BLK * blk + c * FC_BLK + k], y = b[2 * FC_BLK * (blk + 1) + c * FC_BLK + k];
                if (fabsf(x - y) > d) d = fabsf(x - y);
            }
    return d;
}

/* the rms of b[k + 48] - a[k], over both channels, as a fraction of a's rms */
static double fc_delayed_rel(const float *a, const float *b)
{
    double e = 0, n = 0;
    int blk, k, c;
    for (blk = 0; blk + 1 < FC_BLOCKS; blk++)
        for (c = 0; c < 2; c++)
            for (k = 0; k < FC_BLK; k++) {
                double x = a[2 * FC_BLK * blk + c * FC_BLK + k], y = b[2 * FC_BLK * (blk + 1) + c * FC_BLK + k];
                e += (x - y) * (x - y); n += x * x;
            }
    return n > 0 ? sqrt(e / n) : 0;
}

/* 7h's clock: the first core's work before the hand-over is 10,000 ticks, its wait at the join fc_wait_ticks */
static uint32_t fc_clk, fc_wait_ticks;
static uint32_t fc_clock(void) { return fc_clk; }
static int fc_run(void (*fn)(uint32_t), uint32_t arg) { fc_clk += 10000; return hc2_cpu_run(fn, arg); }
static int fc_wait(void) { fc_clk += fc_wait_ticks; return hc2_cpu_wait(); }

/* update 63: 7h's second half, the VOICE GUARD setting both ways on the made-up clock's one case where a voice would be shed (CLOUDS>REVERB, 6 voices, the second core
 * slow). Run last (rack_host main): it renders a few hundred blocks, and the clock-phase-dependent checks (10, 15, 16) read the engine's clock where they start */
static int check_guard_7h(void)
{
    int fails = 0, on_v = 0, off_v = 0, g, b;
            for (g = 1; g >= 0; g--) {
                rk_set_worker(fc_run, fc_wait); rk_set_worker_ex(hc2_cpu_sync, hc2_cpu_on); rk_set_ticks(fc_clock);
                hc2_mode = HC2_NOW; hc2_giveup_pct = hc2_norun_pct = 0;
                rc_pause(); rc_load(0); rc_resume();
                cl_patch(30, 32, 0);
                rc_set_guard(g);
                rc_do(RCE_VOICES, 6, 0.0f, 0.0f);
                fc_wait_ticks = 6000;
                { int32_t lr7[512]; for (b = 0; b < 40; b++) rc_render(lr7, 256, 8388607.0f); }
                if (rk_shed_helps() != 1) { printf("7h guard test: the made-up clock did not make the second core slow   FAIL\n"); fails++; }
                rc_overload();
                { int32_t lr7[512]; for (b = 0; b < 4; b++) rc_render(lr7, 256, 8388607.0f); }
                if (g) on_v = rc_patch.voices_now; else off_v = rc_patch.voices_now;
            }
            rc_set_guard(0);
            rk_set_ticks(0); rk_set_worker(hc2_cpu_run, hc2_cpu_wait);
            printf("7h guard setting: the second core slow with 6 voices: guard ON sheds to %d (expected 5), OFF keeps %d (expected 6)\n", on_v, off_v);
            if (on_v != 5 || off_v != 6) fails++;
            return fails;
}

static int check_fx_core2(const char *self)
{
    static float A[2 * FC_BLK * FC_BLOCKS], B[2 * FC_BLK * FC_BLOCKS], X[2 * FC_BLK * FC_BLOCKS];
    int fails = 0, chain, mode, scen, i;
    float worst_exact = 0, worst_mod = 0;
    double worst_rel = 0;
    int ncmp = 0;
    /* 7a, 7b */
    for (scen = FS_STEADY; scen <= FS_CHANGES; scen++)
        for (chain = 0; chain < FC_NCHAIN; chain++)
            for (mode = 0; mode < 3; mode++) {
                float d;
                int modulated = chain == FC_C1_REVERB || chain == FC_REVERB_C2 || chain == FC_C1_CHORUS;
                if (fc_played(self, chain, mode, scen, FM_A, 0, 0, A) || fc_played(self, chain, mode, scen, FM_B, 0, 0, B)) {
                    printf("7a %-14s %-8s %-9s could not play\n", FC_CHAIN[chain], CM_NAME[mode], FC_SCEN[scen]);
                    fails++;
                    continue;
                }
                d = fc_delayed_diff(A, B);
                ncmp++;
                if (modulated) { if (d > worst_mod) worst_mod = d; } else if (d > worst_exact) worst_exact = d;
                {
                    fc_stat_t sa = fc_stat(A), sb = fc_stat(B);
                    double rel = fc_delayed_rel(A, B);
                    if (modulated && rel > worst_rel) worst_rel = rel;
                    int ok = (modulated ? rel <= 0.12 : d == 0.0f) && !sb.bad && sb.zero_run <= sa.zero_run + 1 && sb.step <= sa.step * (modulated ? 1.05f : 1.0001f) + 1e-6f
                             && fabs(sb.rms - sa.rms) <= 0.005 * sa.rms + 1e-9;
                    if (!ok || (scen == FS_STEADY && mode == 0))
                        printf("7%c %-14s %-8s %-9s B[k+48] vs A[k]: max diff %g (rms %.4f%% of the signal)  output rms %.5f / %.5f  step %.4f / %.4f%s\n", scen == FS_STEADY ? 'a' : 'b',
                               FC_CHAIN[chain], CM_NAME[mode], FC_SCEN[scen], d, rel * 100.0, sb.rms, sa.rms, sb.step, sa.step, ok ? "" : "   FAIL");
                    if (!ok) fails++;
                }
            }
    printf("7a/7b %d cases (7 chain orders x 3 modes x steady / FREEZE on-off / QUALITY+MODE changes): two cores = one core delayed 48 frames; "
           "largest difference %g in the chains without a modulated effect (identical sample for sample: Clouds' random numbers come in the one-core order), and %g (%.2f%% rms of the signal at most) where the reverb or chorus follows or leads: their own LFOs run on, not delayed\n", ncmp, worst_exact, worst_mod, worst_rel * 100.0);
    /* 7c: a worker that fails on some blocks. Declined: the job is done at the hand-over on the first core, where it would have run: the same
     * samples as the working worker's (HC2_NOW). Given up (the job dropped): done at the join, after the voices: the same samples as a
     * worker that runs its job at the join (HC2_DEFER). Both together: the two orders mix, so Clouds' random numbers (GRANULR, SPECTRL) come in
     * another order: the same sound, not the same samples (LOOPDLY: the same samples) */
    {
        static const int PCT[3][3] = {{0, 40, FM_B}, {40, 0, FM_D}, {25, 25, FM_B}};   /* given up %, declined %, the reference */
        int worst_zero = 0, nexact = 0, n = 0, nex_set[3] = {0, 0, 0};
        float worst = 0;
        double worst_rel = 0;
        for (i = 0; i < 3; i++)
            for (chain = 0; chain < FC_NCHAIN; chain += 2)
                for (mode = 0; mode < 3; mode++) {
                    fc_stat_t sg, sb;
                    float d = 0;
                    double rr;
                    int k, exact, ok, modulated = chain == FC_REVERB_C2 || chain == FC_C1_CHORUS;
                    if (fc_played(self, chain, mode, FS_FREEZE, PCT[i][2], 0, 0, B) || fc_played(self, chain, mode, FS_FREEZE, PCT[i][2], PCT[i][0], PCT[i][1], X)) { fails++; printf("7c could not play\n"); continue; }
                    for (k = 0; k < 2 * FC_BLK * FC_BLOCKS; k++) if (fabsf(B[k] - X[k]) > d) d = fabsf(B[k] - X[k]);
                    sg = fc_stat(X); sb = fc_stat(B);
                    rr = fabs(sg.rms / sb.rms - 1.0);
                    exact = d == 0.0f;
                    ok = !sg.bad && sg.zero_run <= sb.zero_run + 1 &&
                         (i < 2 ? exact : (exact || (mode != CM_LOOPDLY ? rr <= 0.08 : (modulated || d == 0.0f))));
                    n++; nexact += exact; nex_set[i] += exact;
                    if (d > worst) worst = d;
                    if (sg.zero_run > worst_zero) worst_zero = sg.zero_run;
                    if (rr > worst_rel && i == 2) worst_rel = rr;
                    if (!ok) {
                        printf("7c %-14s %-8s gave up %d%% / declined %d%%: differs from the working worker by %g, rms %.2f%%, NaN %d, zero run %d   FAIL\n", FC_CHAIN[chain], CM_NAME[mode],
                               PCT[i][0], PCT[i][1], d, rr * 100.0, sg.bad, sg.zero_run);
                        fails++;
                    }
                }
        printf("7c worker failing (declined on 40%% of the blocks / given up on 40%% / 25%% each), %d cases: identical sample for sample to the working worker's output: %d of 12 declined, %d of 12 given up, %d of 12 both (the mixed set differs only in Clouds' random order: rms within %.2f%%); "
               "longest run of zero samples %d (a block lost would be 48 or more)\n", n, nex_set[0], nex_set[1], nex_set[2], worst_rel * 100.0, worst_zero);
    }
    /* 7d: a real second thread */
    {
        float worst = 0, worst_rms = 0;
        int n = 0, nexact = 0;
        for (chain = 0; chain < FC_NCHAIN; chain++)
            for (mode = 0; mode < 3; mode++) {
                fc_stat_t sa, st;
                float d;
                int modulated = chain == FC_C1_REVERB || chain == FC_REVERB_C2 || chain == FC_C1_CHORUS;
                if (fc_played(self, chain, mode, FS_FREEZE, FM_A, 0, 0, A) || fc_played(self, chain, mode, FS_FREEZE, FM_T, 0, 0, X)) { fails++; printf("7d could not play\n"); continue; }
                d = fc_delayed_diff(A, X);
                sa = fc_stat(A); st = fc_stat(X);
                n++; nexact += d == 0.0f;
                if (d > worst) worst = d;
                if (fabs(st.rms / sa.rms - 1.0) > worst_rms) worst_rms = (float)fabs(st.rms / sa.rms - 1.0);
                if (st.bad || (d != 0.0f && fabs(st.rms / sa.rms - 1.0) > 0.08) || (d != 0.0f && mode == CM_LOOPDLY && !modulated) || st.zero_run > sa.zero_run + 1) {
                    printf("7d %-14s %-8s thread: diff %g rms %.5f / %.5f NaN %d   FAIL\n", FC_CHAIN[chain], CM_NAME[mode], d, st.rms, sa.rms, st.bad);
                    fails++;
                }
            }
        printf("7d real second thread, %d cases (FREEZE on and off): %d identical to one core delayed 48 frames sample for sample (LOOPDLY, the chains with no modulated effect: all); largest difference %g; "
               "largest rms difference %.2f%%\n", n, nexact, worst, worst_rms * 100.0);
    }
    /* 7e: patch changes: Clouds switched off from the panel and on, taken out and put back (another mode), the whole patch loaded again, a note */
    {
        float worst_ratio = 0;
        int n = 0, zr = 0;
        for (chain = 0; chain < FC_NCHAIN; chain++)
            for (mode = 0; mode < 3; mode++) {
                fc_stat_t sa, sb, st;
                if (fc_played(self, chain, mode, FS_PATCH, FM_A, 0, 0, A) || fc_played(self, chain, mode, FS_PATCH, FM_B, 25, 25, B) ||
                    fc_played(self, chain, mode, FS_PATCH, FM_T, 0, 0, X)) { fails++; printf("7e could not play\n"); continue; }
                sa = fc_stat(A); sb = fc_stat(B); st = fc_stat(X);
                n++;
                if (sb.step / sa.step > worst_ratio) worst_ratio = sb.step / sa.step;
                if (st.step / sa.step > worst_ratio) worst_ratio = st.step / sa.step;
                if (sb.zero_run - sa.zero_run > zr) zr = sb.zero_run - sa.zero_run;
                if (st.zero_run - sa.zero_run > zr) zr = st.zero_run - sa.zero_run;
                if (sb.bad || st.bad || sb.step > sa.step * 1.1f || st.step > sa.step * 1.1f || sb.zero_run > sa.zero_run + FC_BLK + 8 || st.zero_run > sa.zero_run + FC_BLK + 8) {
                    printf("7e %-14s %-8s patch changes: largest step %.4f (two cores, failing) %.4f (thread) vs %.4f (one core), NaN %d/%d, zero run %d/%d/%d   FAIL\n", FC_CHAIN[chain], CM_NAME[mode],
                           sb.step, st.step, sa.step, sb.bad, st.bad, sb.zero_run, st.zero_run, sa.zero_run);
                    fails++;
                }
            }
        printf("7e Clouds off and on from the panel, taken out and back, the patch reloaded, while playing, %d cases (failing worker and thread): no NaN; largest step at most x%.3f of the one-core run's own; "
               "the silent gap where Clouds starts up is at most %d samples longer than on one core's (one block is 48; a lost block would add another 48)\n", n, worst_ratio, zr);
    }
    /* 7f */
    {
        static const int FX[3][2] = {{30, 32}, {30, 31}, {30, 0}};
        int j, lowest = 99;
        for (j = 0; j < 3; j++) {
            int v;
            rk_set_worker(hc2_cpu_run, hc2_cpu_wait);
            cl_patch(FX[j][0], FX[j][1], 0);
            rk_set_voices(4);
            v = rk_fm1_status(3);
            if (rk_fm1_status(2) < lowest) lowest = rk_fm1_status(2);
            if (v != 4 || !rk_fx_core2_active()) { printf("7f CLOUDS + %d: %d voices playing of 4 asked (%d fit), pipe %d   FAIL\n", FX[j][1], v, rk_fm1_status(2), rk_fx_core2_active()); fails++; }
        }
        printf("7f Clouds with the reverb, the delay and alone: 4 voices fit and play (fewest that fit: %d)\n", lowest);
    }
    /* 7g: six voices, the fifth and sixth in the second core's job. They draw random numbers (drift) as Clouds' grains do, so which voice gets which number
     * depends on the order the cores draw in; the deferred stand-in (D: the job runs at the join, after the first core's voices, the job's voices first) draws
     * them in the one-core order, which Clouds' LOOPDLY (no random numbers of its own) shows sample for sample, up to the order the voices are added in.
     * GRANULR and SPECTRL draw too: the same sound (rms), not the same samples, as in 7d.
     * The voices ride in the job only with Clouds in FX 1 and a live effect in FX 2 (cl_voices_ride): then the first core has that effect to carry as well */
    {
        float worst_loop = 0, worst_off = 0, worst_exact = 0;
        double worst_rms = 0, worst_rms_t = 0, worst_g = 0;
        int n = 0, nsplit = 0;
        for (chain = 0; chain < FC_NCHAIN; chain++)
            for (mode = 0; mode < 3; mode++) {
                int modulated = chain == FC_C1_REVERB || chain == FC_REVERB_C2 || chain == FC_C1_CHORUS;
                int split = chain == FC_C1_DELAY || chain == FC_C1_REVERB || chain == FC_C1_CHORUS;     /* Clouds in FX 1 and an effect in FX 2: the voices ride in the job */
                static float Y[2 * FC_BLK * FC_BLOCKS];
                float d, dn = 0;
                int bad;
                fc_stat_t sa, sd, st;
                fc_nv = 6; fc_novoff = 0;
                if (fc_played(self, chain, mode, FS_FREEZE, FM_A, 0, 0, A) || fc_played(self, chain, mode, FS_FREEZE, FM_D, 0, 0, B) ||
                    fc_played(self, chain, mode, FS_FREEZE, FM_T, 0, 0, X)) { fails++; printf("7g could not play\n"); continue; }
                fc_novoff = 1;     /* the same with the fifth and sixth voice kept on the first core (update 9's arrangement) */
                if (fc_played(self, chain, mode, FS_FREEZE, FM_D, 0, 0, Y)) { fails++; printf("7g could not play\n"); continue; }
                fc_novoff = 0;
                sa = fc_stat(A); sd = fc_stat(B); st = fc_stat(X);
                d = fc_delayed_diff(A, B); dn = fc_delayed_diff(A, Y);
                n++; nsplit += split;
                if (mode == CM_LOOPDLY && !modulated) {
                    if (split) { if (d > worst_loop) worst_loop = d; } else if (d > worst_exact) worst_exact = d;
                    if (dn > worst_off) worst_off = dn;
                }
                if (!modulated && fabs(sd.rms / sa.rms - 1.0) > worst_rms) worst_rms = fabs(sd.rms / sa.rms - 1.0);
                if (fabs(st.rms / sa.rms - 1.0) > worst_rms_t) worst_rms_t = fabs(st.rms / sa.rms - 1.0);
                /* the sums differ in the last bit; Clouds' input is 16 bits, so a last-bit difference sometimes moves a sample by a step of it (3e-5) */
                bad = sd.bad || st.bad || sd.zero_run > sa.zero_run + 1 || fabs(sd.rms / sa.rms - 1.0) > (modulated ? 0.12 : mode == CM_LOOPDLY ? 0.01 : 0.08) || fabs(st.rms / sa.rms - 1.0) > 0.08 ||
                      (mode == CM_LOOPDLY && !modulated && (d > 2e-4f || dn > 2e-4f));
                if (bad) {
                    printf("7g %-14s %-8s six voices%s: diff %g (voices kept on the first core %g), rms %.5f / %.5f, thread rms %.5f, NaN %d/%d   FAIL\n", FC_CHAIN[chain], CM_NAME[mode],
                           split ? " (two in the job)" : "", d, dn, sd.rms, sa.rms, st.rms, sd.bad, st.bad);
                    fails++;
                }
                if (mode == CM_GRANULAR && (chain == FC_C1 || chain == FC_C1_REVERB)) {   /* the failing worker (declined 25 %, given up 25 %): no NaN, no block lost, the same sound */
                    fc_stat_t sg;
                    if (fc_played(self, chain, mode, FS_FREEZE, FM_B, 25, 25, X)) { fails++; printf("7g could not play\n"); continue; }
                    sg = fc_stat(X);
                    if (fabs(sg.rms / sd.rms - 1.0) > worst_g) worst_g = fabs(sg.rms / sd.rms - 1.0);
                    if (sg.bad || sg.zero_run > sd.zero_run + 1 || fabs(sg.rms / sd.rms - 1.0) > 0.08) {
                        printf("7g %-14s %-8s six voices, failing worker: rms %.5f / %.5f NaN %d zero run %d   FAIL\n", FC_CHAIN[chain], CM_NAME[mode], sg.rms, sd.rms, sg.bad, sg.zero_run);
                        fails++;
                    }
                }
            }
        fc_nv = 4;
        printf("7g six voices, %d cases (7 chains x 3 modes, FREEZE on and off): the fifth and sixth voice ride in the second core's job in %d of them (Clouds in FX 1, an effect in FX 2, no Rings). "
               "LOOPDLY, chains with no modulated effect: two cores = one core delayed 48 frames to within %g with the two voices in the job, %g with them kept on the first core, %g in the other chains (voices on the first core) "
               "(not sample for sample: the sums are added in another order, and Clouds' 16-bit input turns a last-bit difference into a step of it now and then); "
               "GRANULR and SPECTRL: the same sound, rms within %.2f%% (the random numbers' order); real thread within %.2f%% rms; failing worker within %.2f%% rms, no NaN, no block lost\n",
               n, nsplit, worst_loop, worst_off, worst_exact, worst_rms * 100.0, worst_rms_t * 100.0, worst_g * 100.0);
    }
    /* 7h: the overload guard's question, with a made-up clock: the first core's work before it hands the job over and the time it waits at the join */
    {
        static const struct { int fx1, fx2, nv, wait, expect; const char *what; } T[] = {
            {30, 32, 6, 0, 0, "CLOUDS>REVERB, 6 voices (2 in the job), the first core slow"},
            {30, 32, 6, 6000, 1, "CLOUDS>REVERB, 6 voices, the second core slow"},
            {30, 32, 4, 0, 1, "CLOUDS>REVERB, 4 voices, the first core slow"},
            {30, 32, 4, 6000, 0, "CLOUDS>REVERB, 4 voices (none in the job), the second core slow"},
            {32, 30, 6, 0, 1, "REVERB>CLOUDS, 6 voices (all on the first core), the first core slow"},
            {32, 30, 6, 6000, 0, "REVERB>CLOUDS, 6 voices, the second core slow"},
            {30, 0, 5, 6000, 0, "CLOUDS alone, 5 voices (all on the first core), the second core slow"},
            {30, 0, 5, 0, 1, "CLOUDS alone, 5 voices (all on the first core), the first core slow"},
        };
        int j, b, bad = 0;
        for (j = 0; j < (int)(sizeof T / sizeof *T); j++) {
            int got;
            rk_set_worker(fc_run, fc_wait);
            rk_set_worker_ex(hc2_cpu_sync, hc2_cpu_on);
            rk_set_ticks(fc_clock);
            hc2_mode = HC2_NOW; hc2_giveup_pct = hc2_norun_pct = 0;
            cl_patch(T[j].fx1, T[j].fx2, 0);
            rk_set_voices(T[j].nv);
            rk_note_on(48.0f, 0.8f);
            fc_wait_ticks = (uint32_t)T[j].wait;
            for (b = 0; b < 120; b++) rk_render(FC_BLK);
            got = rk_shed_helps();
            if (got != T[j].expect) { printf("7h %s: shed helps %d, expected %d   FAIL\n", T[j].what, got, T[j].expect); fails++; bad++; }
        }
        rk_set_ticks(0);
        rk_set_worker(hc2_cpu_run, hc2_cpu_wait);
        cl_patch(30, 32, 0); rk_set_voices(4);
        if (rk_shed_helps() != 1) { printf("7h without a clock: shed helps %d, expected 1   FAIL\n", rk_shed_helps()); fails++; }
        printf("7h overload guard: %d cases with a made-up clock (the first core waits 0 or 6000 ticks of a 10,000-tick block): a voice is shed only when it is on the slow core; no clock, or no Clouds on two cores: as before; %d wrong\n", (int)(sizeof T / sizeof *T), bad);
    }
    return fails;
}
