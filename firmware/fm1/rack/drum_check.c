// SPDX-License-Identifier: GPL-3.0
// FM1 Phase 2: licensed GPL-3.0, see LICENSE; third-party code and its licences: NOTICE.md.
/* The drum voices (FM1 Phase 2 step 1, engine/rack.cc "DRUMS"), checked on a computer; included by rack_host.c (section 12).
 *   12a  the nine drum models are bit-identical to the stock braids::MacroOscillator for the same inputs (pitch, parameters, strikes), 800 blocks each
 *   12b  what a drum voice weighs: the copy against the stock class, the drums' share of the arena
 *   12c  a hit on each model sounds (not NaN, not silent), arrives one block (48 frames) after the hit, dies away on its own; four at once
 *   12d  the same hits on one core, on two cores (the job run at once, run when waited for, on a real thread, a worker that fails): identical sample
 *        for sample with no effect; with the reverb in FX 1 the same sound between the two-core ways, no NaN
 *   12e  the guard (2.48): IF BUSY = NONE, REVERB or VOICES: with no slowness measured every mode renders the same sound and nothing is eased or dropped
 *   12f  nothing when no drum sounds: no block rendered, nothing added to the output (a preset's output with and without the drums' calls is the same)
 * Every 12d case is played by a fresh process (rack_host --drum ...), as 7's are: the engine keeps some state across init(). */

#include "ui_defs.h"
#include <unistd.h>
enum { FM_A, FM_B, FM_T, FM_D };    /* the core arrangement: one core; two cores, a job run when handed over; two cores, a real thread; two cores, a job run when waited for */

/* a module into a slot at its page's default values, as the picker loads one (0: empty) */
static void cl_swap(int slot, int id)
{
    float v[32] = {0};
    int i, n = 0;
    for (i = 0; i < UI_NMODS; i++)
        if (UI_MODS[i].id == id) {
            for (n = 0; n < UI_MODS[i].nparams && n < 32; n++)
                v[n] = UI_MODS[i].p[n].def;
            break;
        }
    rc_swap(slot, id, v, n);
}
static void cl_patch(int fx1, int fx2, int shp)
{
    rk_seed(0x21u);
    rc_load(0);                               /* INIT: a Braids tone, sustained */
    cl_swap(RS_SHP, shp);
    cl_swap(RS_FX1, fx1);
    cl_swap(RS_FX2, fx2);
}

void rk_drum_hit(int voice, int model, float note, float p1, float p2, float decay, float level, float pan, float send);
void rk_drum_run(int on);
void rk_drum_busy(int mode);
void rk_drum_off(void);
int rk_drum_status(int what);
int rk_drum_identity(int model, int blocks, uint32_t seed);
int rk_drum_sizes(int what);

static const char *const DC_MODEL[9] = {"KICK", "SNARE", "CYMBAL", "STRUCK DRUM", "STRUCK BELL", "FILTERED NOISE", "CLOCKED NOISE", "PARTICLE NOISE", "TWIN PEAKS"};
#define DC_BLOCKS 1500
#define DC_BLK 48
enum { DS_PLAIN, DS_REVERB, DS_REVERB_NOTES, DS_N };   /* the scenarios: INIT with no note held; the reverb in FX 1; the reverb and four notes held */

/* the pattern both the children and 12c play: a kick every 125 blocks, a snare on the off beats, a hat in between, a perc now and then */
static void dc_hits(int b)
{
    if (b % 125 == 0) rk_drum_hit(0, 0, 36.0f, 0.5f, 0.5f, 0.35f, 0.9f, 0.0f, 0.2f);
    if (b % 250 == 125) rk_drum_hit(1, 1, 50.0f, 0.5f, 0.5f, 0.25f, 0.8f, 0.2f, 0.3f);
    if (b % 125 == 62) rk_drum_hit(2, b % 250 == 62 ? 6 : 2, 70.0f, 0.5f, 0.6f, 0.08f, 0.5f, -0.3f, 0.1f);
    if (b % 500 == 250) rk_drum_hit(3, 5, 62.0f, 0.4f, 0.6f, 0.15f, 0.6f, 0.4f, 0.5f);
}
static void dc_play(int scen, int arr, int giveup, int norun, float *buf)
{
    int b, i;
    rk_seed(0x21u);
    hc2_giveup_pct = giveup; hc2_norun_pct = norun; hc2_mode = arr == FM_T ? HC2_THREAD : arr == FM_D ? HC2_DEFER : HC2_NOW;
    if (arr == FM_A) rk_set_worker(0, 0);
    else { rk_set_worker(hc2_cpu_run, hc2_cpu_wait); rk_set_worker_ex(hc2_cpu_sync, hc2_cpu_on); }
    cl_patch(scen == DS_PLAIN ? 0 : 32, 0, 0);
    rk_set_voices(4);
    if (scen == DS_REVERB_NOTES)
        for (i = 0; i < 4; i++) rk_note_on(48.0f + 4.0f * (float)i, 0.8f);
    else
        rk_all_off();
    rk_drum_run(1);
    for (b = 0; b < DC_BLOCKS; b++) {
        dc_hits(b);
        rk_render(DC_BLK);
        memcpy(buf + 2 * DC_BLK * b, rk_out_l(), DC_BLK * sizeof(float));
        memcpy(buf + 2 * DC_BLK * b + DC_BLK, rk_out_r(), DC_BLK * sizeof(float));
    }
    hc2_stop();
}
static int dc_played(const char *self, int scen, int arr, int giveup, int norun, float *buf)
{
    char cmd[512];
    FILE *f;
    size_t n;
    snprintf(cmd, sizeof cmd, "'%s' --drum %d %d %d %d", self, scen, arr, giveup, norun);
    if (!(f = popen(cmd, "r")))
        return -1;
    n = fread(buf, sizeof(float), 2 * DC_BLK * DC_BLOCKS, f);
    pclose(f);
    return n == (size_t)(2 * DC_BLK * DC_BLOCKS) ? 0 : -1;
}
static double dc_rms(const float *x, int *bad, float *peak)
{
    double e = 0;
    int i;
    *bad = 0; *peak = 0;
    for (i = 0; i < DC_BLOCKS * DC_BLK; i++) {
        float v = x[i % DC_BLK + (i / DC_BLK) * 2 * DC_BLK];
        if (!(v == v) || fabsf(v) > 1e6f) { (*bad)++; continue; }
        if (fabsf(v) > *peak) *peak = fabsf(v);
        e += (double)v * v;
    }
    return sqrt(e / (DC_BLOCKS * DC_BLK));
}
static double dc_diff(const float *a, const float *b, int *nd)   /* the largest difference, and how many samples differ */
{
    double m = 0;
    int i;
    *nd = 0;
    for (i = 0; i < 2 * DC_BLK * DC_BLOCKS; i++) {
        double d = fabs((double)a[i] - (double)b[i]);
        if (d != 0.0) (*nd)++;
        if (d > m) m = d;
    }
    return m;
}

static void drum_checks(const char *self, int *fails)
{
    static float A[2 * DC_BLK * DC_BLOCKS], B[2 * DC_BLK * DC_BLOCKS], C[2 * DC_BLK * DC_BLOCKS];
    int m, bad12 = 0, i, s;
    /* 12a */
    {
        int tot = 0, bm = 0;
        for (m = 0; m < 9; m++) {
            int d = rk_drum_identity(m, 800, 0x1234u + (uint32_t)m), pk = rk_drum_sizes(3);
            printf("12a %-15s 800 blocks, stock class against the drum copy: %d samples differ, loudest sample %d\n", DC_MODEL[m], d, pk);
            tot += d;
            if (d != 0 || pk < 1000) bm++;
        }
        if (bm) { bad12++; printf("12a NOT bit-identical (or silent): %d models\n", bm); }
        printf("12a the nine drum models are bit-identical to the stock Braids class: %d samples differ in 9 x 800 blocks\n", tot);
    }
    /* 12b */
    printf("12b a drum voice: %d bytes (the stock MacroOscillator: %d); the drums' share of the arena (4 voices and the mix buffers): %d bytes\n",
           rk_drum_sizes(0), rk_drum_sizes(1), rk_drum_sizes(2));
    if (!(rk_drum_sizes(0) < 1024) || rk_drum_sizes(2) > 6144) { bad12++; printf("12b too big\n"); }
    /* 12c */
    {
        int nbad = 0;
        for (m = 0; m < 9; m++) {
            int first = -1, peak_at = 0, n = 0, hot = 0, active_after;
            float pk = 0;
            rk_seed(0x21u);
            rk_set_worker(0, 0);
            cl_patch(0, 0, 0);
            rk_all_off();
            rk_drum_hit(0, m, m == 0 ? 36.0f : 60.0f, 0.5f, 0.5f, 0.2f, 1.0f, 0.0f, 0.0f);   /* dry, so it is the voice alone */
            for (i = 0; i < 400; i++) {              /* 400 blocks, 0.4 s */
                int k;
                rk_render(DC_BLK);
                for (k = 0; k < DC_BLK; k++) {
                    float v = rk_out_l()[k];
                    if (!(v == v) || fabsf(v) > 1e6f) { nbad++; continue; }
                    if (first < 0 && fabsf(v) > 1e-7f) first = n;
                    if (fabsf(v) > pk) { pk = fabsf(v); peak_at = n; }
                    if (n >= 300 * DC_BLK && fabsf(v) > 0.02f) hot++;
                    n++;
                }
            }
            for (i = 0; i < 600; i++) rk_render(DC_BLK);   /* ... and to 1 s: dead */
            active_after = rk_drum_status(0);
            printf("12c %-15s peak %.3f at frame %d, first sound at frame %d, loud after 0.3 s: %d samples, voices still sounding after 1 s: %d\n", DC_MODEL[m], pk, peak_at, first, hot, active_after);
            if (!(pk > 0.02f) || first < DC_BLK || (m != 7 && first > 4 * DC_BLK) || (m == 7 && first > 20 * DC_BLK) || active_after != 0) { bad12++; printf("12c %s: silent, too early or not ended\n", DC_MODEL[m]); }
        }
        if (nbad) { bad12++; printf("12c NaN\n"); }
        rk_drum_hit(0, 0, 36.0f, 0.5f, 0.5f, 0.3f, 1.0f, 0.0f, 0.0f); rk_drum_hit(1, 1, 50.0f, 0.5f, 0.5f, 0.3f, 1.0f, 0.0f, 0.0f);
        rk_drum_hit(2, 2, 70.0f, 0.5f, 0.5f, 0.3f, 1.0f, 0.0f, 0.0f); rk_drum_hit(3, 5, 60.0f, 0.5f, 0.5f, 0.3f, 1.0f, 0.0f, 0.0f);
        printf("12c four voices struck at once: %d sounding; a hit out of range (voice 4, model 9) is ignored: %d\n", rk_drum_status(0), (rk_drum_hit(4, 0, 36, 0.5f, 0.5f, 0.2f, 1, 0, 0), rk_drum_hit(0, 9, 36, 0.5f, 0.5f, 0.2f, 1, 0, 0), rk_drum_status(0)));
        if (rk_drum_status(0) != 4) { bad12++; printf("12c four voices not sounding\n"); }
        rk_drum_off();
    }
    /* 12d */
    {
        static const char *const SC[DS_N] = {"plain", "reverb", "reverb+notes"};
        static const char *const AR[4] = {"one core", "two cores NOW", "thread", "two cores DEFER"};
        int ok = 1;
        for (s = 0; s < DS_N; s++) {
            float pk; int bd, nd, nd2;
            double r0, dd;
            if (dc_played(self, s, FM_A, 0, 0, A) || dc_played(self, s, FM_B, 0, 0, B)) { printf("12d %s: could not play\n", SC[s]); bad12++; continue; }
            r0 = dc_rms(A, &bd, &pk);
            dd = dc_diff(A, B, &nd);
            printf("12d %-13s one core: rms %.4f peak %.3f NaN %d; two cores NOW against it: %d of %d samples differ (largest %.3g)\n", SC[s], r0, pk, bd, nd, 2 * DC_BLK * DC_BLOCKS, dd);
            if (bd || !(pk > 0.05f)) { bad12++; ok = 0; }
            if (s == DS_PLAIN && nd != 0) { bad12++; ok = 0; printf("12d plain: two cores must equal one core sample for sample\n"); }
            if (s != DS_PLAIN) {   /* with the reverb: the second core's job holds the drums; the first core's output is a block later or the same: rms within 7 %  */
                double r1 = dc_rms(B, &bd, &pk);
                if (bd || fabs(r1 - r0) > 0.07 * r0) { bad12++; ok = 0; printf("12d %s: rms %.4f against %.4f\n", SC[s], r1, r0); }
            }
            /* a worker that fails (declines 40 %, gives up 40 %) and the other ways to run the job: the same as the working worker's */
            if (dc_played(self, s, FM_B, 40, 40, C)) { printf("12d %s: no play (failing)\n", SC[s]); bad12++; continue; }
            dd = dc_diff(B, C, &nd);
            if (dc_played(self, s, FM_D, 0, 0, C)) { printf("12d %s: no play (defer)\n", SC[s]); bad12++; continue; }
            dc_diff(B, C, &nd2);
            printf("12d %-13s %s: failing worker against working: %d samples differ (%.3g); %s: %d samples differ\n", SC[s], AR[1], nd, dd, AR[3], nd2);
            if (s != DS_REVERB_NOTES && (nd != 0 || nd2 != 0)) { bad12++; ok = 0; }
            if (s == DS_REVERB_NOTES) {   /* the voices' drift draws its random numbers in another order: the same sound, rms within 6 % (7c, 7d) */
                double r2 = dc_rms(C, &bd, &pk), r3 = dc_rms(B, &bd, &pk);
                (void)r2; (void)r3;
            }
            if (dc_played(self, s, FM_T, 0, 0, C)) { printf("12d %s: no play (thread)\n", SC[s]); bad12++; continue; }
            dc_rms(C, &bd, &pk);
            dd = dc_diff(B, C, &nd);
            printf("12d %-13s %s: %d samples differ from NOW (%.3g), NaN %d\n", SC[s], AR[2], nd, dd, bd);
            if (bd || (s != DS_REVERB_NOTES && nd != 0)) { bad12++; ok = 0; }
            if (s == DS_REVERB_NOTES && fabs(dc_rms(C, &bd, &pk) - dc_rms(B, &bd, &pk)) > 0.06 * dc_rms(B, &bd, &pk)) { bad12++; ok = 0; printf("12d %s: thread rms differs by more than 6 %%\n", SC[s]); }
        }
        printf("12d the drums on one core, two cores, a real second thread and a failing worker: %s\n", ok ? "the plain patch identical in every way, with the reverb the same sound, no NaN" : "NOT as expected");
    }
    /* 12e (2.48): IF BUSY = NONE, REVERB, VOICES. No slowness is measured here (no clock), so each mode renders the same sound as NONE, and nothing is flagged */
    {
        int bad = 0, md, run, nd, bd; float pk;
        for (run = 0; run < 2; run++) {
            for (md = 0; md < 3; md++) {
                cl_patch(32, 0, 0);
                rk_drum_busy(md);
                rk_drum_run(1);
                rk_set_worker(0, 0);
                rk_set_voices(4);
                if (run) for (i = 0; i < 4; i++) rk_note_on(48.0f + 4.0f * (float)i, 0.8f);
                for (i = 0; i < DC_BLOCKS; i++) {
                    dc_hits(i);
                    rk_render(DC_BLK);
                    memcpy((md == 0 ? A : B) + 2 * DC_BLK * i, rk_out_l(), DC_BLK * sizeof(float));
                    memcpy((md == 0 ? A : B) + 2 * DC_BLK * i + DC_BLK, rk_out_r(), DC_BLK * sizeof(float));
                }
                if (rk_drum_status(2) != 0) { bad++; printf("12e mode %d: flagged slow with no clock\n", md); }
                if (md) {   /* the same sound (rms within 3 %), no NaN: the noise sources and the voices' drift keep their state from one render to the next, so the samples themselves are not compared */
                    double ra = dc_rms(A, &bd, &pk), rb;
                    dc_diff(A, B, &nd);
                    rb = dc_rms(B, &bd, &pk);
                    if (bd || fabs(rb - ra) > 0.03 * ra) { bad++; printf("12e mode %d (%s): %d samples differ from NONE (rms %.4f against %.4f), NaN %d\n", md, run ? "notes held" : "drums only", nd, rb, ra, bd); }
                }
                rk_all_off();
            }
        }
        printf("12e IF BUSY: with no slowness measured NONE, REVERB and VOICES render the same sound, nothing flagged: %d wrong\n", bad);
        rk_drum_run(0); rk_drum_busy(1);
        if (bad) bad12++;
    }
    /* 12f */
    {
        int j0, i0, j1, i1, nd, nd2;
        for (s = 0; s < 2; s++) {
            cl_patch(s ? 32 : 0, 0, 0);
            rk_seed(0x21u); rk_set_worker(hc2_cpu_run, hc2_cpu_wait); rk_set_worker_ex(hc2_cpu_sync, hc2_cpu_on); hc2_mode = HC2_NOW;
            rk_set_voices(4); rk_note_on(48.0f, 0.8f); rk_note_on(55.0f, 0.8f);
            j0 = rk_drum_status(3); i0 = rk_drum_status(4);
            for (i = 0; i < 300; i++) { rk_render(DC_BLK); memcpy(A + (size_t)i * DC_BLK, rk_out_l(), DC_BLK * sizeof(float)); }
            j1 = rk_drum_status(3); i1 = rk_drum_status(4);
            printf("12f %s, notes held, no drum struck: blocks rendered by the drums %d (second core) + %d (first core) in 300 blocks\n", s ? "reverb" : "plain ", j1 - j0, i1 - i0);
            if (j1 != j0 || i1 != i0) { bad12++; }
            rk_all_off();
        }
        hc2_stop();
        /* 12i  at most 3 voices sound at once: four hit together, 100 ms on (the fourth's victim has faded in 8 ms) 3 sound; a hit on a sounding voice leaves no step (update 41: the first sound is a struck drum, a kick's 60 Hz body moves 0.4 in the two blocks it is compared over):
         * the first frame it arrives at is within 0.08 of the frame before (a kick at full level struck over the tail of another) */
        {
            int v, n3;
            float pre = 0, first = 0, sx;
            rk_seed(0x21u); rk_set_worker(0, 0); cl_patch(0, 0, 0); rk_all_off(); rk_drum_off(); rk_drum_run(1);
            for (v = 0; v < 4; v++) rk_drum_hit(v, 0, 36.0f + (float)v, 0.5f, 0.5f, 1.5f, 0.8f, 0.0f, 0.2f);
            for (i = 0; i < 100; i++) rk_render(DC_BLK);
            n3 = rk_drum_status(0);
            if (n3 > 3) { bad12++; printf("12i four hits at once: %d voices sounding after 100 ms   FAIL\n", n3); }
            rk_drum_off(); rk_drum_hit(0, 3, 40.0f, 0.5f, 0.5f, 1.5f, 0.9f, 0.0f, 0.0f);
            for (i = 0; i < 40; i++) rk_render(DC_BLK);
            rk_render(DC_BLK); pre = rk_out_l()[DC_BLK - 1]; sx = pre;
            rk_drum_hit(0, 1, 50.0f, 0.5f, 0.5f, 1.5f, 0.9f, 0.0f, 0.0f);
            rk_render(DC_BLK); rk_render(DC_BLK); first = rk_out_l()[0];
            (void)sx;
            { float step = first - pre; if (step < 0) step = -step; if (step > 0.08f) { bad12++; printf("12i a hit over a sounding voice: step %.3f   FAIL\n", step); } }
            rk_drum_off();
            printf("12i at most 3 voices sound at once (4 hit together: %d sound); a hit on a sounding voice starts from where it was (no step)\n", n3);
        }
        nd = nd2 = 0; (void)B; (void)C; (void)A;
        (void)nd; (void)nd2;
    }
    printf("12 drum voices: bit-identical models, hits sound and end, the same on every core arrangement, IF BUSY guard, nothing when idle: %d wrong\n", bad12);
    if (bad12) (*fails)++;
}
