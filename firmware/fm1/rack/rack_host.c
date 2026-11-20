// SPDX-License-Identifier: GPL-3.0
// FM1 Phase 2: licensed GPL-3.0, see LICENSE; third-party code and its licences: NOTICE.md.
/* FM1 Rack firmware: the sound side checked on a computer (rack_core.c with the engine built as the FM-1's).
 *   1. every preset, loaded as the firmware loads it (presets.h: INIT, its differences, each module's ranges),
 *      sounds the same as INIT then the preset's own calls (presets_full.h, as the page loads one), sample for
 *      sample (each played by a fresh process: the engine keeps some state across init());
 *   2. the resampler keeps the pitch (the same cycles in the same time at 48 and 44.1 kHz), no step at block edges;
 *   3. every preset makes sound through rc_render, with a chord, no NaN;
 *   4. the unison modes play 2 and 4 voices per key;
 *   5. the overload shed;
 *   6. CLOUDS: its modes sound, the arena holds, the reverb gives way first (clouds_check.c);
 *   7. CLOUDS on the second core (update 9, RACK_FX_CORE2): the same sound as one core, a block late (fx_core2_check.c).
 *   8. the knob layer (update 18): the curve maths, and MACRO TEST's eight K1 targets (presets_macrotest.h).
 *   10. the filter's SWEEP (update 26): darker down, brighter up, the matrix's LFO on the cutoff still moves it.
 *   12. the drum voices (step 1 of the drum track, drum_check.c): bit-identical models, hits, cores, IF BUSY.
 *   13. the drum sequencer (step 2, drumseq_check.c): hits on the steps at the tempo and the clock, MIDI out per lane, ratchet, nudge, chance, mute, idle.
 *   11. JAM's moves (update 28, jam.c): each changes its parameters while on and gives back exactly the preset's own values
 *       (HOLD and LATCH, and jam_clear), moves stack, the clocked ones follow the tempo, the velocity scales them.
 * Build and run: sh build_host.sh */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "rack_core.c"
#include "clouds_check.c"
#include "fx_core2_check.c"
#include "drum_check.c"
#include "drumseq.c"
#include "drumseq_check.c"
#include "jam.c"

typedef struct { uint8_t fn, a, b, c, d; float x, y, z, w; } full_op_t;
#include "presets_full.h"
#include "presets_macrotest.h"
float rk_eff_param(int slot, int param);
float rk_eff_param_nojam(int slot, int param);

static void run_full(const full_op_t *o, int n)
{
    int i;
    for (i = 0; i < n; i++, o++)
        switch (o->fn) {
        case 1: rk_set_module(o->a, o->b); break;
        case 2: rk_set_param(o->a, o->b, o->x); break;
        case 13: rk_set_meta(o->a, o->b, o->x, o->y, o->z); break;
        case 4: rk_set_route(o->a, (int8_t)o->b, (int8_t)o->c, o->d, o->x, (int)o->y); break;
        case 5: rk_set_macro(o->a, o->x); break;
        case 6: rk_set_master(o->x); break;
        case 7: rk_set_master_fx(o->x, o->y, o->z, o->w); break;
        case 8: rk_set_legato((int)o->x); break;
        case 9: rk_set_vintage(o->x); break;
        case 10: rk_set_ext_slew(o->a, o->x); break;
        case 11: rk_set_tune(o->a, o->x); break;
        case 12: rk_set_macro_target(o->a, o->b, (int8_t)o->c, o->d, (int)o->x, (int)o->y, (int)o->z, (int)o->w); break;
        }
}

/* a preset, loaded one way or the other, a chord, 1 s of the engine's own output (left) */
static void play_engine(int k, int full, float *out)
{
    int i, f;
    rk_seed(0x21u);
    if (full) {
        rk_init();
        run_full(FULL[0].ops, FULL[0].n);
        if (k > 0)
            run_full(FULL[k].ops, FULL[k].n);
        rk_fm1_fit();                          /* as rc_load does after the preset's calls */
        rk_set_voices(PRESETS[k].mode == RVM_MONO || PRESETS[k].mode == RVM_LEGATO ? 1 : PRESETS[k].voices > RC_MAXV ? RC_MAXV : PRESETS[k].voices);
    } else {
        rc_load(k);
    }
    for (i = 0; i < 3; i++)
        rk_note_on(48.0f + 4.0f * (float)i, 0.8f);
    for (f = 0; f < 48000; f += 480) {
        rk_render(480);
        memcpy(out + f, rk_out_l(), 480 * sizeof(float));
    }
}

static int played(const char *self, int k, int full, float *buf)
{
    char cmd[512];
    FILE *f;
    size_t n;
    snprintf(cmd, sizeof cmd, "'%s' --play %d %d", self, k, full);
    if (!(f = popen(cmd, "r")))
        return -1;
    n = fread(buf, sizeof(float), 48000, f);
    pclose(f);
    return n == 48000 ? 0 : -1;
}

static int32_t lr[2 * 44100 + 512];

/* through the firmware's path: rc_render in 256-frame blocks (APP_BLOCK), 1 s at 44.1 kHz, gain 1 */
static void play_codec(int k, int notes, const int *chord, float *peak, int *bad)
{
    int i, b;
    rk_seed(0x21u);
    rc_pause();
    rc_load(k);
    rc_resume();
    for (i = 0; i < notes; i++)
        rc_note_on(chord[i], 0.8f);
    *peak = 0;
    *bad = 0;
    for (b = 0; b + 256 <= 44100; b += 256) {
        float l[512];
        int j;
        rc_render(lr + 2 * b, 256, 8388607.0f);
        (void)l;
        for (j = 0; j < 512; j++) {
            float v = (float)lr[2 * b + j] / 8388607.0f;
            if (v != v)
                *bad = 1;
            if (fabsf(v) > *peak)
                *peak = fabsf(v);
        }
    }
}


/* check 10's helpers. The centroid (Hz) of 1024 samples, Hann, DFT bins 1-511 */
static float centroid(const float *b)
{
    double sm = 0.0, sw = 0.0;
    int w, j;
    for (w = 1; w < 512; w++) {
        double re = 0.0, im = 0.0;
        for (j = 0; j < 1024; j++) {
            double h = 0.5 - 0.5 * cos(6.283185307179586 * j / 1024.0), ph = 6.283185307179586 * (double)w * j / 1024.0;
            re += b[j] * h * cos(ph); im -= b[j] * h * sin(ph);
        }
        sm += re * re + im * im; sw += (re * re + im * im) * (double)w * 48000.0 / 1024.0;
    }
    return sm > 0.0 ? (float)(sw / sm) : 0.0f;
}
/* a note on a patch, the sweep set: nwin windows of 1024 samples (60 ms apart when more than one, else after 300 blocks); buf: the
 * last window's samples; cmin/cmax: the centroids' range, emin/emax: the cutoff as the engine has it (matrix included). A first,
 * warm-up pass with the sweep centred: the engine keeps some state from the patch played before it, so each run follows one of
 * its own kind. clean: the preset's matrix rows and knob targets cleared and ENV AMT 0; plain: the filter set to mid cutoff;
 * lfo: the amount of a row from LFO 1 (a sine) to the cutoff */
static void patch_run(int pk, float sw, int clean, int plain, float lfo, int nwin, float *buf, float *cmin, float *cmax, float *emin, float *emax)
{
    int pass, i, j, n;
    for (pass = 0; pass < 2; pass++) {
        rk_seed(0x21u);
        rc_pause(); rc_load(pk); rc_resume();
        if (clean) {
            for (i = 0; i < 18; i++) rk_set_route(i, 0, -1, 0, 0.0f, 0);
            rk_clear_macro_targets();
            rk_set_param(RS_GLO, FLT_P0 + 4, 0.0f);
        }
        if (plain) rk_set_param(RS_GLO, FLT_P0 + 1, 0.5f);
        rk_set_param(RS_GLO, FLT_SWEEP, pass ? sw : 0.0f);
        if (lfo != 0.0f) {
            rk_set_param(RS_LFO, 0, 0.0f); rk_set_param(RS_LFO, 1, 0.55f);
            rk_set_route(11, 13, RS_GLO, FLT_P0 + 1, lfo, 0);
        }
        rk_note_on(55.0f, 0.8f);
        for (j = 0; j < (nwin > 1 ? 100 : 300); j++) rk_render(48);
        if (!pass)
            continue;
        *cmin = 1e9f; *cmax = 0.0f; *emin = 1e9f; *emax = -1e9f;
        for (n = 0; n < nwin; n++) {
            float c, e;
            if (nwin > 1) for (j = 0; j < 60; j++) rk_render(48);
            for (j = 0; j < 22; j++) { rk_render(48); memcpy(buf + j * 48, rk_out_l(), 48 * sizeof(float)); }
            c = centroid(buf); e = rk_eff_param(RS_GLO, FLT_P0 + 1);
            if (c < *cmin) *cmin = c;
            if (c > *cmax) *cmax = c;
            if (e < *emin) *emin = e;
            if (e > *emax) *emax = e;
        }
    }
}

int main(int argc, char **argv)
{
    static float a[48000], b[48000];
    static const int CHORD[6] = {48, 52, 55, 59, 62, 65};
    int k, i, fails = 0;
    if (argc == 8 && !strcmp(argv[1], "--fx2")) {   /* update 9: one two-core case, its output on stdout (fx_core2_check.c) */
        static float o[2 * 48 * 1500];
        fc_play(atoi(argv[2]), atoi(argv[3]), atoi(argv[4]), atoi(argv[5]), atoi(argv[6]), atoi(argv[7]), o);
        fwrite(o, sizeof(float), 2 * 48 * 1500, stdout);
        return 0;
    }
    if (argc == 6 && !strcmp(argv[1], "--drum")) {   /* FM1 Phase 2 step 1: one drum case, its output on stdout (drum_check.c) */
        static float o[2 * DC_BLK * DC_BLOCKS];
        dc_play(atoi(argv[2]), atoi(argv[3]), atoi(argv[4]), atoi(argv[5]), o);
        fwrite(o, sizeof(float), 2 * DC_BLK * DC_BLOCKS, stdout);
        return 0;
    }
    if (argc == 2 && !strcmp(argv[1], "--drums")) {   /* section 12 alone */
        drum_checks(argv[0], &fails);
        printf(fails ? "%d FAILED\n" : "all passed\n", fails);
        return fails != 0;
    }
    if (argc == 2 && !strcmp(argv[1], "--dkit")) {    /* 12g alone */
        dkit_checks(&fails);
        return fails != 0;
    }
    if (argc == 2 && !strcmp(argv[1], "--dseq")) {    /* section 13 alone */
        dseq_checks(&fails);
        printf(fails ? "%d FAILED\n" : "all passed\n", fails);
        return fails != 0;
    }
    if (argc == 4 && !strcmp(argv[1], "--play")) {
        play_engine(atoi(argv[2]), atoi(argv[3]), a);
        fwrite(a, sizeof(float), 48000, stdout);
        return 0;
    }
    /* 1 */
    for (k = 0; k < NPRESETS; k++) {
        float d = 0;
        if (played(argv[0], k, 0, a) || played(argv[0], k, 1, b)) {
            printf("load  %-14s could not play\n", PRESETS[k].name);
            fails++;
            continue;
        }
        for (i = 0; i < 48000; i++)
            if (fabsf(a[i] - b[i]) > d)
                d = fabsf(a[i] - b[i]);
        if (d != 0.0f) {
            printf("load  %-14s differs from its own calls by %g\n", PRESETS[k].name, d);
            fails++;
        }
    }
    printf("1 loading: %d presets checked against their own calls\n", NPRESETS);
    /* 2: the same note through the engine alone (48 kHz) and through rc_render (44.1 kHz): one second of each must
     * hold the same number of cycles (rising zero crossings), so the pitch is kept; and no step at block edges
     * larger than inside blocks */
    {
        int up48 = 0, up44 = 0;
        float jump_in = 0, jump_edge = 0;
        static float e[48000];
        rk_seed(0x21u);
        rc_pause();
        rc_load(0);
        rc_resume();
        rc_note_on(69, 0.8f);
        for (i = 0; i + 256 <= 44100; i += 256)
            rc_render(lr + 2 * i, 256, 8388607.0f);
        for (i = 4410 + 1; i < 44032; i++) {
            float v = (float)lr[2 * i] / 8388607.0f, w = (float)lr[2 * i - 2] / 8388607.0f;
            if (w < 0 && v >= 0)
                up44++;
            if (i % 256 == 0) { if (fabsf(v - w) > jump_edge) jump_edge = fabsf(v - w); }
            else if (fabsf(v - w) > jump_in) jump_in = fabsf(v - w);
        }
        rk_seed(0x21u);
        rc_load(0);
        rk_note_on(69.0f, 0.8f);
        for (i = 0; i < 48000; i += 480) {
            rk_render(480);
            memcpy(e + i, rk_out_l(), 480 * sizeof(float));
        }
        for (i = 4800 + 1; i < 48000 * 44032 / 44100; i++)
            if (e[i - 1] < 0 && e[i] >= 0)
                up48++;
        printf("2 resampler: %d cycles at 48 kHz, %d at 44.1 kHz over the same time; largest step %.3f inside "
               "blocks, %.3f at block edges\n", up48, up44, jump_in, jump_edge);
        if (abs(up48 - up44) > 2 || jump_edge > jump_in * 1.5f + 1e-3f)
            fails++;
    }
    /* 3 */
    for (k = 0; k < NPRESETS; k++) {
        float peak;
        int bad, n = PRESETS[k].mode == RVM_MONO || PRESETS[k].mode == RVM_LEGATO ? 1 : 3;
        play_codec(k, n, CHORD, &peak, &bad);
        printf("3 %-8s %-14s peak %.3f  voices %d/%d  arena %3d KB  unloaded %02x%s\n", PRESETS[k].cat,
               PRESETS[k].name, peak, rk_fm1_status(3), rk_fm1_status(2), rk_fm1_status(0) / 1024, rk_fm1_status(4),
               bad ? "  NAN" : peak < 1e-4f ? "  SILENT" : "");
        if (bad || peak < 1e-4f)
            fails++;
    }
    /* 4 */
    for (k = 0; k < NPRESETS; k++)
        if (PRESETS[k].mode >= RVM_UNI2) {
            float n[4];
            rc_pause();
            rc_load(k);
            rc_resume();
            printf("4 %-14s mode %d: %d notes per key, detune %.3f\n", PRESETS[k].name, PRESETS[k].mode,
                   rc_unison(60.0f, n), rc_patch.detune);
        }
    /* 5: the overload guard. Sheds one voice per 20 blocks down to one; at one voice, 16 overloads close together
     * mute it (silence, not the engine) until the next load; spaced-out overloads don't */
    {
        int shed_to, heavy_close, heavy_spaced, quiet = 1, back;
        rc_pause(); rc_load(0); rc_resume();
        rc_do(RCE_VOICES, 6, 0.0f, 0.0f);
        rc_note_on(60, 0.8f);
        for (i = 0; i < 200; i++) { rc_render(lr, 256, 8388607.0f); rc_overload(); }
        shed_to = rc_patch.voices_now;
        for (i = 0; i < 40; i++) { rc_render(lr, 256, 8388607.0f); rc_overload(); }
        heavy_close = rc_heavy;
        rc_render(lr, 256, 8388607.0f);
        for (k = 0; k < 512; k++) if (lr[k]) quiet = 0;
        rc_pause(); rc_load(0); rc_resume();
        back = !rc_heavy;
        rc_do(RCE_VOICES, 1, 0.0f, 0.0f);
        for (i = 0; i < 200; i++) { rc_render(lr, 256, 8388607.0f); if (i % 8 == 0) rc_overload(); }
        heavy_spaced = rc_heavy;
        printf("5 overload: shed to %d voice, muted after close overloads %d (silent %d), cleared by a load %d, "
               "muted by spaced ones %d\n", shed_to, heavy_close, quiet, back, heavy_spaced);
        if (shed_to != 1 || !heavy_close || !quiet || !back || heavy_spaced)
            fails++;
        rc_pause(); rc_load(0); rc_resume();
    }
    /* 6: CLOUDS (clouds_check.c) */
    fails += check_clouds();
    /* 7: CLOUDS on the second core (fx_core2_check.c; with RACK_FX_CORE2 off the arrangement is off and these check nothing) */
#ifdef RACK_FX_CORE2
    fails += check_fx_core2(argv[0]);
#else
    printf("7 RACK_FX_CORE2 is off in this build: Clouds stays on the first core (nothing to check)\n");
#endif
    /* 8: the knob layer (update 18): the curve maths, then MACRO TEST (K1 on 8 targets, loaded from the packed ops as the
     * firmware loads a preset) gives each parameter the offset depth x curve(K1) x span, and a row target moves a row's depth */
    {
        static const struct { float k; int s, e, c; float want; } cv[] = {
            {0.0f, 0, 100, 0, 0.0f}, {0.5f, 0, 100, 0, 0.5f}, {1.0f, 0, 100, 0, 1.0f}, {0.25f, 0, 100, 1, 0.0625f}, {0.5f, 0, 100, 1, 0.25f},
            {0.25f, 0, 100, 2, 0.4375f}, {0.5f, 0, 100, 2, 0.75f}, {0.25f, 0, 100, 4, 0.15625f}, {0.5f, 0, 100, 4, 0.5f},
            {0.2f, 40, 100, 0, 0.0f}, {0.7f, 40, 100, 0, 0.5f}, {1.0f, 40, 100, 1, 1.0f}, {0.84f, 85, 100, 3, 0.0f},
            {0.85f, 85, 100, 3, 1.0f}, {1.0f, 85, 100, 3, 1.0f}, {0.5f, 0, 60, 0, 0.8333333f}, {0.9f, 0, 60, 2, 1.0f}, {0.5f, 70, 70, 0, 0.0f}};
        int bad_c = 0, bad_t = 0, nt = 0, i, kk;
        float worst = 0.0f;
        for (i = 0; i < (int)(sizeof cv / sizeof cv[0]); i++) {
            float g = rk_macro_curve(cv[i].k, cv[i].s, cv[i].e, cv[i].c);
            if (fabsf(g - cv[i].want) > 1e-5f) { bad_c++; printf("8 curve k=%g %d-%d type %d: %g, want %g\n", cv[i].k, cv[i].s, cv[i].e, cv[i].c, g, cv[i].want); }
        }
        static const float ks[] = {0.0f, 0.3f, 0.6f, 0.8f, 1.0f};
        for (kk = 0; kk < 5; kk++) {
            float e1[8], e0[8], tgt_k = ks[kk];
            int sl[8], pa[8], dp[8], st[8], en[8], cu[8], n = 0, j, out[5];
            rk_init();
            rc_run(PRESET_OPS + PRESETS[0].at, PRESETS[0].len);
            rc_run(MACRO_TEST_OPS, sizeof MACRO_TEST_OPS);
            for (j = 0; j < RS_N; j++)
                rc_meta(j, j >= RS_AMP ? 100 + j : rc_patch.mod[j]);
            rk_set_macro(0, tgt_k);
            rk_render(48);
            for (j = 0; j < 8; j++) {            /* (read without rendering between: the modulators stand still) */
                int d = rk_get_macro_target(0, j, out);
                if (kk == 0) nt += d != 0;
                sl[j] = out[0]; pa[j] = out[1]; dp[j] = d; st[j] = out[2]; en[j] = out[3]; cu[j] = out[4];
            }
            for (j = 0; j < 8; j++)
                if (sl[j] < 9) e1[j] = rk_eff_param(sl[j], pa[j]);
            rk_clear_macro_targets();
            for (j = 0; j < 8; j++)
                if (sl[j] < 9) e0[j] = rk_eff_param(sl[j], pa[j]);
            for (j = 0; j < 8; j++) {
                float lo = -1e9f, hi = 1e9f, span = 1.0f, want, key;
                int m;
                if (sl[j] >= 9) continue;
                key = (float)(sl[j] >= 6 ? 100 + sl[j] : rc_patch.mod[sl[j] < 0 ? 0 : sl[j]]);
                (void)key;
                for (m = 0; m < NMETA_OF; m++)
                    if (META_OF[m].key == (sl[j] >= 6 ? 100 + sl[j] : rc_patch.mod[sl[j]]))
                        for (i = 0; i < META_OF[m].n; i++)
                            if (META[META_OF[m].at + i].param == pa[j]) { span = META[META_OF[m].at + i].span; lo = META[META_OF[m].at + i].lo; hi = META[META_OF[m].at + i].hi; }
                want = (float)dp[j] * 0.01f * rk_macro_curve(tgt_k, st[j], en[j], cu[j]) * span;
                if (e0[j] > lo + 1e-4f && e0[j] + want < hi - 1e-4f && e0[j] + want > lo + 1e-4f) {
                    float err = fabsf((e1[j] - e0[j]) - want);
                    if (err > worst) worst = err;
                    if (err > 1e-4f) { bad_t++; printf("8 K1 %.2f target %d (slot %d param %d): moved %g, want %g (e0 %g lo %g hi %g span %g)\n", tgt_k, j, sl[j], pa[j], e1[j] - e0[j], want, e0[j], lo, hi, span); }
                }
            }
        }
        {   /* a row's depth: the row (M2 to SRC param 0, 0.2) plus the knob's 0.5 x 0.5 at K1 = 0.5 */
            float a, b;
            rk_init();
            rk_set_meta(0, 0, 0.0f, 10.0f, 1.0f);
            rk_set_route(0, 2, 0, 0, 0.2f, 0);
            rk_set_macro(1, 1.0f);
            rk_set_macro(0, 0.5f);
            rk_render(48);
            a = rk_eff_param(0, 0);
            rk_set_macro_target(0, 0, 9, 0, 50, 0, 100, 0);
            rk_render(48);
            b = rk_eff_param(0, 0);
            if (fabsf(a - 0.2f) > 1e-5f || fabsf(b - 0.45f) > 1e-5f) { bad_t++; printf("8 row depth: %g then %g, want 0.2 then 0.45\n", a, b); }
        }
        printf("8 knob layer: %d curve points, MACRO TEST %d targets on K1 at 5 knob positions, worst error %.2g, %d wrong\n",
               (int)(sizeof cv / sizeof cv[0]), nt, worst, bad_c + bad_t);
        if (bad_c || bad_t || nt != 8)
            fails++;
    }
    /* 9: notes end. Every preset, loaded as the firmware loads it, played in ways that stress the voices: more keys than
     * voices (a stolen voice fades first, stage 7), the new note let go during that fade, a key pressed again before its
     * release ends, two keys let go together, legato pairs let go in either order. Once the last key is up no voice may
     * still have its gate on, and (presets with sustain) all voices must be silent within their own release time as a
     * lone note's (x 1.5 + 50 ms) */
    {
        static const char *PN[] = {"steal", "steal, new note off at once", "steal, off after 1 block", "steal, off after 2 blocks",
                                   "steal, off after 3 blocks", "steal, off after 4 blocks", "same key again in its release",
                                   "all let go together", "legato pair A B", "legato pair B A"};
        int bad9 = 0, nsus = 0, np, pat, nn, j, ms;
        for (k = 0; k < NPRESETS; k++) {
            int V, base_ms, sus;
            float s_amp;
            rk_seed(0x21u);
            rc_pause();
            rc_load(k);
            rc_resume();
            s_amp = rk_eff_param(RS_AMP, 2);
            sus = s_amp > 0.01f;
            nsus += sus;
            V = rk_fm1_status(3);
            rk_note_on(60.0f, 0.8f);                   /* a lone note: its own release time */
            for (j = 0; j < 400; j++) rk_render(48);
            rk_note_off(60.0f);
            for (base_ms = 0; base_ms < 12000 && rk_fm1_status(8); base_ms++) rk_render(48);
            for (pat = 0; pat < 10; pat++) {
                int lim, gated, n_on = V + 2 > 8 ? 8 : V + 2;
                rc_pause();
                rc_load(k);
                rc_resume();
                np = V;
                if (pat <= 5) {
                    for (nn = 0; nn < np; nn++) { rk_note_on(48.0f + 3.0f * (float)nn, 0.8f); rk_render(48); }
                    for (j = 0; j < 100; j++) rk_render(48);
                    if (pat == 0) {
                        for (nn = np; nn < n_on; nn++) rk_note_on(48.0f + 3.0f * (float)nn, 0.8f);
                    } else {
                        rk_note_on(48.0f + 3.0f * (float)np, 0.8f);
                        for (j = 1; j < pat; j++) rk_render(48);
                        rk_note_off(48.0f + 3.0f * (float)np);
                    }
                    for (j = 0; j < 20; j++) rk_render(48);
                    for (nn = 0; nn < np; nn++) { rk_note_off(48.0f + 3.0f * (float)nn); rk_render(48); }
                    for (nn = np; nn < n_on; nn++) rk_note_off(48.0f + 3.0f * (float)nn);
                } else if (pat == 6) {
                    rk_note_on(60.0f, 0.8f);
                    for (j = 0; j < 50; j++) rk_render(48);
                    rk_note_off(60.0f);
                    for (j = 0; j < 3; j++) rk_render(48);
                    rk_note_on(60.0f, 0.8f);
                    for (j = 0; j < 20; j++) rk_render(48);
                    rk_note_off(60.0f);
                } else if (pat == 7) {
                    for (nn = 0; nn < np; nn++) rk_note_on(48.0f + 3.0f * (float)nn, 0.8f);
                    for (j = 0; j < 100; j++) rk_render(48);
                    for (nn = 0; nn < np; nn++) rk_note_off(48.0f + 3.0f * (float)nn);
                } else {
                    rk_note_on(60.0f, 0.8f);
                    for (j = 0; j < 30; j++) rk_render(48);
                    rk_note_on(64.0f, 0.8f);
                    for (j = 0; j < 30; j++) rk_render(48);
                    rk_note_off(pat == 8 ? 60.0f : 64.0f);
                    for (j = 0; j < 30; j++) rk_render(48);
                    rk_note_off(pat == 8 ? 64.0f : 60.0f);
                }
                gated = rk_fm1_status(9);
                lim = base_ms * 3 / 2 + 50;
                for (ms = 0; ms < 12000 && rk_fm1_status(8); ms++) rk_render(48);
                if (gated || (sus && base_ms < 12000 && ms > lim)) {
                    bad9++;
                    printf("9 %-14s %s: %d gate(s) on after every key up, silent after %d ms (a lone note: %d ms)\n", PRESETS[k].name, PN[pat], gated, ms, base_ms);
                }
            }
        }
        printf("9 notes end: %d presets (%d with sustain) x %d ways of playing, no gate left on, silent within the release time: %d wrong\n", NPRESETS, nsus, 10, bad9);
        if (bad9)
            fails++;
    }
    /* 10: the filter's SWEEP (update 26). The filter is always in the chain: a patch with it open is transparent (the build's render
     * check compares the 46 presets with the sweep centred, sample for sample, with the build before it), turned down the sound
     * gets darker (its spectral centroid falls), up brighter (rises), the open filter stays open, and a matrix LFO on the cutoff
     * still moves it, with the SWEEP at any place. A plain patch (INIT, its filter set) and three factory presets with their own
     * modulation taken out, so the centroid stands still in time */
    {
        static const char *const FN[] = {"INIT", "SAW LEAD", "MOOG BASS", "HORN SECT"};
        float buf[1152], opn[2][1152], c0, c1, e0, e1;
        int bad10 = 0, fi, j, v;
        const float SW[3] = {-0.4f, 0.0f, 0.4f};
        for (fi = 0; fi < 4; fi++) {
            int pk = -1;
            float cen[3];
            for (k = 0; k < NPRESETS; k++)
                if (!strcmp(PRESETS[k].name, FN[fi])) pk = k;
            if (pk < 0) { bad10++; printf("10 preset %s not found\n", FN[fi]); continue; }
            for (v = 0; v < 3; v++) {
                patch_run(pk, SW[v], 1, fi == 0, 0.0f, 1, buf, &c0, &c1, &e0, &e1);
                cen[v] = centroid(buf);
            }
            {
                int dn = cen[0] < cen[1] * 0.97f, up = cen[2] > cen[1] * 1.03f;
                printf("10 SWEEP %-10s centroid %.0f Hz at -0.4, %.0f at 0, %.0f at +0.4: %s, %s\n", FN[fi], cen[0], cen[1], cen[2],
                       dn ? "down darker" : "DOWN NOT DARKER", up ? "up brighter" : "UP NOT BRIGHTER");
                if (!dn || !up) bad10++;
            }
        }
        {   /* open: INIT's filter at its default (cutoff at the top): transparent; the sweep up changes nothing, down darkens */
            float cd[3], md = 0.0f;
            for (v = 0; v < 3; v++) {
                patch_run(0, v == 0 ? -0.7f : SW[v], 0, 0, 0.0f, 1, buf, &c0, &c1, &e0, &e1);
                if (v < 2) memcpy(opn[v], buf, sizeof buf);
                cd[v] = centroid(buf);
            }
            for (j = 0; j < 1024; j++) if (fabsf(buf[j] - opn[1][j]) > md) md = fabsf(buf[j] - opn[1][j]);
            printf("10 open filter (INIT): centroid %.0f Hz at -0.7, %.0f at 0, %.0f at +0.4; +0.4 against 0: largest sample difference %g\n", cd[0], cd[1], cd[2], md);
            if (md != 0.0f || !(cd[0] < cd[1] * 0.97f)) bad10++;
        }
        for (v = 0; v < 3; v++) {   /* an LFO on the cutoff (matrix row 11: LFO 1 -> the filter's CUTOFF), SWEEP at -0.3, 0 and +0.3 */
            float lo, hi, flo, fhi, slo, shi;
            patch_run(0, 0.3f * (float)(v - 1), 1, 1, 0.0f, 36, buf, &slo, &shi, &e0, &e1);
            patch_run(0, 0.3f * (float)(v - 1), 1, 1, 0.25f, 36, buf, &lo, &hi, &flo, &fhi);
            printf("10 LFO on CUTOFF, SWEEP %+.1f: cutoff %.2f..%.2f, centroid %.0f..%.0f Hz (x%.2f; without the LFO x%.2f)\n", 0.3 * (v - 1), flo, fhi, lo, hi, hi / (lo > 1.0f ? lo : 1.0f), shi / (slo > 1.0f ? slo : 1.0f));
            if (!(hi > lo * 1.5f) || !(shi < slo * 1.1f)) bad10++;
        }
        printf("10 filter SWEEP: darker down, brighter up, an open filter transparent, the matrix still moves the cutoff: %d wrong\n", bad10);
        if (bad10)
            fails++;
    }
    /* 11: JAM's moves (update 28). Three presets (chorus + reverb, Clouds + reverb, a delay): each of the 12 moves changes at least one
     * of the 32 targets while on, and, once let go (HOLD), pressed again (LATCH) or cleared, every target is exactly the preset's own
     * value again (the targets as the voices hear them: rk_eff_param; the pitch, which has no such reader, is not in it). Two moves
     * together add up; the stutters' gate follows the tempo (twice the tempo, twice the cycles; 1/8 half, 1/32 double a 1/16's);
     * the velocity scales a move. A block is 1 ms (48 frames at 48 kHz); a 16th at 120 bpm is 6000 frames */
    {
        int bad11 = 0, pi, mvi, nchk = 0;
        const char *PN11[3] = {"WARM PAD", "TIDE PAD", 0};
        int pk11[3] = {-1, -1, -1}, j, t;
        float base[JAM_NT], cur[JAM_NT];
#define J11_TARGETS(out) do { for (t = 0; t < JAM_NT; t++) { int s_ = JAM_TAB[2 * t], p_ = JAM_TAB[2 * t + 1], sl_ = s_ == 255 ? -1 : s_ >= 30 ? (rc_patch.mod[4] == s_ ? 4 : rc_patch.mod[5] == s_ ? 5 : -1) : s_; \
            (out)[t] = sl_ < 0 ? 0.0f : rk_eff_param(sl_, p_); } } while (0)
#define J11_BASE(out) do { for (t = 0; t < JAM_NT; t++) { int s_ = JAM_TAB[2 * t], p_ = JAM_TAB[2 * t + 1], sl_ = s_ == 255 ? -1 : s_ >= 30 ? (rc_patch.mod[4] == s_ ? 4 : rc_patch.mod[5] == s_ ? 5 : -1) : s_; \
            (out)[t] = sl_ < 0 ? 0.0f : rk_eff_param_nojam(sl_, p_); } } while (0)
#define J11_RUN(nb, f16) do { for (j = 0; j < (nb); j++) { jam_block(0.001f, (f16), 48.0f); rk_render(48); } } while (0)
        for (k = 0; k < NPRESETS; k++) {
            int a, d = 0;
            for (a = 0; a < 2; a++) if (!strcmp(PRESETS[k].name, PN11[a])) pk11[a] = k;
            if (pk11[2] < 0) {   /* the first preset with a delay */
                rk_seed(0x21u); rc_pause(); rc_load(k); rc_resume();
                d = rc_patch.mod[4] == 31 || rc_patch.mod[5] == 31;
                if (d) pk11[2] = k;
            }
        }
        for (pi = 0; pi < 3; pi++) {
            if (pk11[pi] < 0) { bad11++; printf("11 preset %d not found\n", pi); continue; }
            for (mvi = 0; mvi < JAM_NM; mvi++) {
                int latch, moved, ex = 0;
                for (latch = 0; latch < 3; latch++) {   /* 0 HOLD, 1 LATCH, 2 cleared while on */
                    float mx = 0.0f;
                    rk_seed(0x21u); rc_pause(); rc_load(pk11[pi]); rc_resume();
                    rc_note_on(60, 0.8f); rc_note_on(64, 0.8f); rc_note_on(67, 0.8f);
                    for (j = 0; j < 200; j++) rk_render(48);
                    jam_defaults(); jam_init(); jam_clear();
                    jam_v[JV_LAT(mvi)] = latch == 1 ? 1.0f : 0.0f;
                    J11_TARGETS(cur); J11_BASE(base);
                    jam_press(mvi, 100);
                    for (j = 0; j < 2000; j++) {
                        jam_block(0.001f, 6000.0f, 48.0f); rk_render(48);
                        J11_TARGETS(cur); J11_BASE(base);
                        for (t = 0; t < JAM_NT; t++) if (fabsf(cur[t] - base[t]) > mx) mx = fabsf(cur[t] - base[t]);
                    }
                    moved = mx > 1e-4f;
                    if (latch == 0) jam_release(mvi);
                    else if (latch == 1) { jam_release(mvi); J11_RUN(1000, 6000.0f); J11_TARGETS(cur); J11_BASE(base); mx = 0; for (t = 0; t < JAM_NT; t++) if (fabsf(cur[t] - base[t]) > mx) mx = fabsf(cur[t] - base[t]); if (!(mx > 1e-4f) && mvi != 1 && mvi != 2 && mvi != 3 && mvi != 8) { bad11++; printf("11 %s %s: LATCH let go of the key, the move ended\n", PRESETS[pk11[pi]].name, JAM_NAME[mvi]); } jam_press(mvi, 100); }
                    else jam_clear();
                    J11_RUN(5000, 6000.0f);
                    J11_TARGETS(cur); J11_BASE(base);
                    for (t = 0; t < JAM_NT; t++) if (cur[t] != base[t]) { ex++; if (ex < 3) printf("11 %s %s (%s): target %d is %g, was %g\n", PRESETS[pk11[pi]].name, JAM_NAME[mvi], latch == 0 ? "hold" : latch == 1 ? "latch" : "clear", t, cur[t], base[t]); }
                    if (!moved) { bad11++; printf("11 %s %s (%s): nothing changed while on\n", PRESETS[pk11[pi]].name, JAM_NAME[mvi], latch == 0 ? "hold" : latch == 1 ? "latch" : "clear"); }
                    nchk++;
                }
                if (ex) bad11++;
            }
        }
        {   /* stacking and velocity: WASH (6) and FILTER DIVE (9) on the SWEEP, WARM PAD */
            float d6, d9, d69, dlo, dhi, b0;
            (void)b0;
            rk_seed(0x21u); rc_pause(); rc_load(pk11[0]); rc_resume();
            rc_note_on(60, 0.8f);
            jam_defaults(); jam_init(); jam_clear();
            J11_TARGETS(cur); J11_BASE(base); b0 = 0;
            jam_press(6, 100); J11_RUN(5000, 6000.0f); J11_TARGETS(cur); J11_BASE(base); d6 = cur[T_SWEEP] - base[T_SWEEP]; jam_release(6); J11_RUN(5000, 6000.0f);
            jam_press(9, 100); J11_RUN(5000, 6000.0f); J11_TARGETS(cur); J11_BASE(base); d9 = cur[T_SWEEP] - base[T_SWEEP]; jam_release(9); J11_RUN(5000, 6000.0f);
            jam_press(6, 100); jam_press(9, 100); J11_RUN(5000, 6000.0f); J11_TARGETS(cur); J11_BASE(base); d69 = cur[T_SWEEP] - base[T_SWEEP];
            jam_release(6); jam_release(9); J11_RUN(5000, 6000.0f);
            jam_press(6, 20); J11_RUN(5000, 6000.0f); J11_TARGETS(cur); J11_BASE(base); dlo = cur[T_SWEEP] - base[T_SWEEP]; jam_release(6); J11_RUN(5000, 6000.0f);
            jam_press(6, 127); J11_RUN(5000, 6000.0f); J11_TARGETS(cur); J11_BASE(base); dhi = cur[T_SWEEP] - base[T_SWEEP]; jam_release(6); J11_RUN(5000, 6000.0f);
            J11_TARGETS(cur); J11_BASE(base);
            printf("11 stack: SWEEP offset WASH %.3f + DIVE %.3f = %.3f, together %.3f; velocity 20: %.3f, 127: %.3f\n", d6, d9, d6 + d9, d69, dlo, dhi);
            if (fabsf(d69 - (d6 + d9)) > 1e-3f || !(d6 < -0.05f) || !(d9 < -0.3f)) { bad11++; printf("11 stacking fails\n"); }
            if (!(dlo > dhi + 0.02f) || !(dlo < -0.01f)) { bad11++; printf("11 velocity does not scale the move\n"); }
            if (cur[T_SWEEP] != base[T_SWEEP]) { bad11++; printf("11 the SWEEP did not come back\n"); }
        }
        {   /* clocked: the stutters' LEVEL gate at 120, 60 and 240 bpm, 1/8, 1/16, 1/32 */
            static const int MV[3] = {1, 2, 3};
            float f16s[3] = {6000.0f, 12000.0f, 3000.0f};
            int cyc[3][3], a, b;
            for (a = 0; a < 3; a++)
                for (b = 0; b < 3; b++) {
                    int closed = 0, prev = 0, n = 0;
                    rk_seed(0x21u); rc_pause(); rc_load(pk11[0]); rc_resume();
                    rc_note_on(60, 0.8f);
                    jam_defaults(); jam_init(); jam_clear();
                    J11_TARGETS(cur); J11_BASE(base);
                    jam_press(MV[a], 127);
                    for (j = 0; j < 4000; j++) {
                        jam_block(0.001f, f16s[b], 48.0f); rk_render(48);
                        J11_TARGETS(cur); J11_BASE(base);
                        closed = cur[T_LEVEL] < base[T_LEVEL] - 0.2f;
                        if (j > 100 && closed && !prev) n++;
                        prev = closed;
                    }
                    jam_clear();
                    cyc[a][b] = n;
                }
            printf("11 stutter gate cycles in 4 s: 1/16 at 120 bpm %d, at 60 bpm %d, at 240 bpm %d; 1/8 at 120 bpm %d; 1/32 at 120 bpm %d\n", cyc[1][0], cyc[1][1], cyc[1][2], cyc[0][0], cyc[2][0]);
            if (!(cyc[1][0] >= 28 && cyc[1][0] <= 33) || !(cyc[1][1] >= 14 && cyc[1][1] <= 17) || !(cyc[1][2] >= 56 && cyc[1][2] <= 66) ||
                !(cyc[0][0] >= 14 && cyc[0][0] <= 17) || !(cyc[2][0] >= 56 && cyc[2][0] <= 66)) { bad11++; printf("11 the stutters do not follow the tempo\n"); }
        }
        printf("11 JAM: %d runs of 12 moves x 3 presets x (hold, latch, clear): each changes its targets, each gives back the preset's own values exactly; stacking, velocity, tempo: %d wrong\n", nchk, bad11);
        if (bad11)
            fails++;
    }
    drum_checks(argv[0], &fails);   /* 12 (FM1 Phase 2 step 1: the drum voices) */
    dkit_checks(&fails);            /* 12g (step 3: the factory kits through the engine) */
    dseq_checks(&fails);            /* 13 (step 2: the drum sequencer) */
    printf(fails ? "%d FAILED\n" : "all passed\n", fails);
    return fails != 0;
}
