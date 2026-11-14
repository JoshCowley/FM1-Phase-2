// SPDX-License-Identifier: GPL-3.0
// FM1 Phase 2: licensed GPL-3.0, see LICENSE; third-party code and its licences: NOTICE.md.
/* CLOUDS in the Lite firmware (FM1 Phase 2 step 5): demos as WAV files, and the host's cost of it.
 *   clouds_demo OUTDIR      three demos of about 8 s (44.1 kHz stereo, through the firmware's own sound path rc_render):
 *                             01_granular_on_pad.wav    GRANULR on a Braids pad: the pad and a cloud made from it
 *                             02_frozen_cloud.wav       a chord, FREEZE, the notes end and the cloud stays, its POSITION and TEXTURE moving
 *                             03_pitch_shifted_grains.wav  GRANULR with PITCH +12 then -12, the pad's own notes under it
 *   clouds_demo --cost      the x86 host's time to render (a PROXY: this PC's CPU, not the FM-1's), and the buffers
 *   clouds_demo --cost2     update 8: Clouds' own share of the render (over Braids x4) before (update 7: 48 kHz, no grain ceiling, the reverb always
 *                           running) and after, five runs each, for GRANULR 16 ST at DENSITY 0.7 / 0.9 / 1.0, LOOPDLY, SPECTRL; and the grain ceiling's
 *                           table (64 / 48 / 40 / 32, none) at DENSITY 1.0 for every QUALITY
 * Built by build_host.sh beside rack_host. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include "rack_core.c"
#include "clouds_check.c"

#define SR 44100
#define SECS 8
static int32_t lr[2 * SR * SECS];

static void wav(const char *path, const int32_t *x, int frames)
{
    FILE *f = fopen(path, "wb");
    uint32_t n = (uint32_t)frames * 4u, v;
    int i;
    if (!f)
        return;
    fwrite("RIFF", 1, 4, f); v = 36u + n; fwrite(&v, 4, 1, f); fwrite("WAVEfmt ", 1, 8, f);
    v = 16; fwrite(&v, 4, 1, f); v = 1u | 2u << 16; fwrite(&v, 4, 1, f); v = SR; fwrite(&v, 4, 1, f);
    v = SR * 4u; fwrite(&v, 4, 1, f); v = 4u | 16u << 16; fwrite(&v, 4, 1, f); fwrite("data", 1, 4, f); fwrite(&n, 4, 1, f);
    for (i = 0; i < frames * 2; i++) {
        int32_t s = x[i] >> 8;
        int16_t h = (int16_t)(s > 32767 ? 32767 : s < -32768 ? -32768 : s);
        fwrite(&h, 2, 1, f);
    }
    fclose(f);
}

static int preset_named(const char *name)
{
    int k;
    for (k = 0; k < NPRESETS; k++)
        if (!strcmp(PRESETS[k].name, name))
            return k;
    return 0;
}

typedef struct { float t, d; int n; float v; } ev_t;
/* every 256 frames: the notes due, then the demo's own automation (ms: the time) */
static void play(const ev_t *ev, int nev, void (*auto_)(int ms, int slot), int slot)
{
    int b, i, on[32] = {0};
    for (b = 0; b + 256 <= SR * SECS; b += 256) {
        float t0 = (float)b / SR, t1 = (float)(b + 256) / SR;
        for (i = 0; i < nev; i++) {
            if (!on[i] && ev[i].t >= t0 - 1e-6f && ev[i].t < t1) { rc_note_on(ev[i].n, ev[i].v); on[i] = 1; }
            if (on[i] == 1 && ev[i].t + ev[i].d < t1) { rc_note_off(ev[i].n); on[i] = 2; }
        }
        if (auto_)
            auto_((int)(t0 * 1000.0f), slot);
        rc_render(lr + 2 * b, 256, 8388607.0f * 0.7f);
    }
}

static float lerp(float a, float b, float x) { return a + (b - a) * (x < 0 ? 0 : x > 1 ? 1 : x); }

/* the pad: PD WARM STRINGS, then CLOUDS in FX 1 (the preset's own FX 1 is replaced; its FX 2 stays) */
static int demo_patch(void)
{
    int s;
    rk_seed(0x21u);
    rc_pause();
    rc_load(preset_named("PD WARM STRINGS"));
    cl_swap(RS_FX1, 30);
    rc_resume();
    s = RS_FX1;
    cl_set(s, P_MODE, 0.0f); cl_set(s, P_POS, 0.3f); cl_set(s, P_SIZE, 0.45f); cl_set(s, P_DENS, 0.75f); cl_set(s, P_TEX, 0.5f);
    cl_set(s, P_BLEND, 0.55f); cl_set(s, P_SPREAD, 0.8f); cl_set(s, P_FB, 0.15f); cl_set(s, P_REV, 0.3f);
    return s;
}

static const ev_t PADCH[] = {
    {0.2f, 3.0f, 48, 0.7f}, {0.2f, 3.0f, 55, 0.7f}, {0.2f, 3.0f, 59, 0.7f}, {0.2f, 3.0f, 64, 0.7f},
    {3.6f, 3.6f, 45, 0.7f}, {3.6f, 3.6f, 52, 0.7f}, {3.6f, 3.6f, 57, 0.7f}, {3.6f, 3.6f, 60, 0.7f},
};
static const ev_t FRZCH[] = {
    {0.2f, 2.0f, 48, 0.7f}, {0.2f, 2.0f, 55, 0.7f}, {0.2f, 2.0f, 59, 0.7f}, {0.2f, 2.0f, 64, 0.7f}, {0.2f, 2.0f, 67, 0.6f},
};

static void auto_freeze(int ms, int s)
{
    if (ms >= 1800) cl_set(s, P_FREEZE, 1.0f);                         /* the chord is still sounding */
    if (ms >= 1800) cl_set(s, P_BLEND, lerp(0.55f, 0.95f, (ms - 1800) / 1500.0f));
    if (ms >= 2400) {
        cl_set(s, P_POS, lerp(0.15f, 0.85f, (ms - 2400) / 5000.0f));    /* the frozen cloud is read from a moving place */
        cl_set(s, P_TEX, lerp(0.35f, 0.95f, (ms - 2400) / 5500.0f));
    }
}
static void auto_pitch(int ms, int s)
{
    cl_set(s, P_PITCH, ms < 4000 ? 0.5f : -0.5f);                       /* PITCH is bipolar, +-24 semitones: 0.5 is an octave */
    cl_set(s, P_BLEND, 0.75f);
}

/* ---- the host's cost ------------------------------------------------------------------------------------------ */
static double now_s(void)
{
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return (double)t.tv_sec + (double)t.tv_nsec * 1e-9;
}
/* seconds of this PC to render `audio` seconds of a patch (best of 3), as a share of real time */
static double cost(int notes, int audio)
{
    double best = 1e9;
    int rep, ms, i;
    for (rep = 0; rep < 3; rep++) {
        double t0;
        if (notes) { rk_all_off(); for (i = 0; i < notes; i++) rk_note_on(48.0f + 3.0f * (float)i, 0.8f); }
        for (ms = 0; ms < 1000; ms += 10) rk_render(480);      /* warm up: a second */
        t0 = now_s();
        for (ms = 0; ms < audio * 1000; ms += 10) rk_render(480);
        if (now_s() - t0 < best) best = now_s() - t0;
    }
    return best / (double)audio;
}

static void print_cost(void)
{
    static const char *const NAME[4] = {"GRANULR", "LOOPDLY", "SPECTRL", ""};
    double idle, one, four, base_fx;
    int mode, q, s;
    cl_patch(0, 0, 0); rk_set_voices(4);
    idle = cost(0, 4);
    cl_patch(0, 0, 0); rk_set_voices(1);
    one = cost(1, 4);
    cl_patch(0, 0, 0); rk_set_voices(4);
    four = cost(4, 4);
    printf("x86 host cost (a PROXY: this PC's CPU and compiler, not the FM-1's JieLi core; build -O2, 4 s of audio, best of 3)\n");
    printf("  share of real time on this PC; 'extra' is the share added to the 4-voice Braids patch\n");
    printf("  %-34s %7.2f %%\n", "idle (no notes, no effects)", idle * 100.0);
    printf("  %-34s %7.2f %%   one voice: %.2f %% over idle\n", "Braids x1", one * 100.0, (one - idle) * 100.0);
    printf("  %-34s %7.2f %%\n", "Braids x4, no effects", four * 100.0);
    cl_patch(32, 0, 0); rk_set_voices(4); base_fx = cost(4, 4);
    printf("  %-34s %7.2f %%   extra %.2f %%  (%.1f x one Braids voice)\n", "+ REVERB (plate)", base_fx * 100.0, (base_fx - four) * 100.0,
           (base_fx - four) / (one - idle));
    cl_patch(31, 0, 0); rk_set_voices(4); base_fx = cost(4, 4);
    printf("  %-34s %7.2f %%   extra %.2f %%  (%.1f x one Braids voice)\n", "+ DELAY", base_fx * 100.0, (base_fx - four) * 100.0,
           (base_fx - four) / (one - idle));
    for (mode = 0; mode < 3; mode++)
        for (q = 0; q < 4; q++) {
            char t[48];
            double c;
            if (mode == 2 && (q & 1) == 0) continue;               /* SPECTRL plays as its mono QUALITY: 16 MONO and 8 MONO only */
            cl_patch(30, 0, 0); s = cl_clouds_slot();
            cl_set(s, P_MODE, (float)mode); cl_set(s, P_QUAL, (float)q); cl_set(s, P_DENS, 0.8f); cl_set(s, P_BLEND, 0.6f);
            rk_set_voices(4);
            c = cost(4, 4);
            snprintf(t, sizeof t, "+ CLOUDS %s %s", NAME[mode], q == 0 ? "16 ST" : q == 1 ? "16 MONO" : q == 2 ? "8 ST" : "8 MONO");
            printf("  %-34s %7.2f %%   extra %.2f %%  (%.1f x one Braids voice)\n", t, c * 100.0, (c - four) * 100.0, (c - four) / (one - idle));
        }
    printf("buffers (engine/rack.cc, RACK_FM1 + RACK_CLOUDS): large %d B + small %d B = %d B (stock 118784 + 65408 = 184192 B), the\n"
           "  processor object (sizeof on x86-64: the device's 32-bit pointers make it a little smaller) lives in the arena too\n",
           2 * 49152, 49152, 3 * 49152);
}


/* ---- update 8: Clouds' own share, before and after (each change alone too) ------------------------------------------------------ */
typedef struct { int mode, qual; float dens, rev; } clcfg_t;
#define CAUDIO 16
static double braids4(void)
{
    rk_clouds_opts(1, 1, 40);
    cl_patch(0, 0, 0); rk_set_voices(4);
    return cost(4, CAUDIO);
}
static double own_share(const clcfg_t *c, int k32, int skip, int grains, double four)
{
    double with;
    int s;
    rk_clouds_opts(k32, skip, grains);
    cl_patch(30, 0, 0); s = cl_clouds_slot();
    cl_set(s, P_MODE, (float)c->mode); cl_set(s, P_QUAL, (float)c->qual); cl_set(s, P_DENS, c->dens); cl_set(s, P_BLEND, 1.0f);
    cl_set(s, P_POS, 0.2f); cl_set(s, P_SIZE, 0.5f); cl_set(s, P_TEX, 0.4f); cl_set(s, P_SPREAD, 0.5f); cl_set(s, P_FB, 0.3f); cl_set(s, P_REV, c->rev);
    rk_set_voices(4);
    with = cost(4, CAUDIO);
    rk_clouds_opts(1, 1, 40);
    return (with - four) * 100.0;
}
static void stats(const double *v, int n, double *mean, double *lo, double *hi)
{
    int i;
    *mean = 0; *lo = 1e9; *hi = -1e9;
    for (i = 0; i < n; i++) { *mean += v[i] / n; if (v[i] < *lo) *lo = v[i]; if (v[i] > *hi) *hi = v[i]; }
}
static void print_grains(void)
{
    int q, r, g;
    double mean, lo, hi, four;
    printf("grain ceiling (Mutable's own count: 32 stereo 16-bit, 45 stereo 8-bit, 40 mono 16-bit, 57 mono 8-bit), GRANULR at DENSITY 1.0, 32K on, 4 notes: own share %%\n");
    printf("    %-10s %8s %8s %8s %8s %8s %8s %8s\n", "QUALITY", "64", "48", "40", "32", "24", "16", "none");
    for (q = 0; q < 4; q++) {
        static const int CAP[7] = {64, 48, 40, 32, 24, 16, 0};
        clcfg_t cf = {0, 0, 1.0f, 0.3f};
        double a[7][3];
        cf.qual = q;
        for (r = 0; r < 3; r++) {
            four = braids4();
            for (g = 0; g < 7; g++) a[g][r] = own_share(&cf, 1, 1, CAP[g], four);
        }
        printf("    %-10s", q == 0 ? "16 ST" : q == 1 ? "16 MONO" : q == 2 ? "8 ST" : "8 MONO");
        for (g = 0; g < 7; g++) { stats(a[g], 3, &mean, &lo, &hi); printf(" %8.2f", mean); }
        printf("\n");
    }
}

/* update 8: what the grain ceiling does to the sound: Clouds' all-wet output (GRANULR, DENSITY 1.0, 4 notes), RMS and spectral centroid over 4 s */
static float snd[480 * 400];
static void print_grain_sound(void)
{
    static const int CAP[7] = {0, 64, 48, 40, 32, 24, 16};
    int q, g, s, ms, k, i;
    printf("grain ceiling and the sound (GRANULR, DENSITY 1.0, all wet, 4 s after 2 s): RMS / spectral centroid in Hz; 'none' = Mutable's own count\n    %-9s", "QUALITY");
    for (g = 0; g < 7; g++) printf("   %-13s", CAP[g] ? (CAP[g] == 64 ? "64" : CAP[g] == 48 ? "48" : CAP[g] == 40 ? "40" : CAP[g] == 32 ? "32" : CAP[g] == 24 ? "24" : "16") : "none");
    printf("\n");
    for (q = 0; q < 4; q++) {
        printf("    %-9s", q == 0 ? "16 ST" : q == 1 ? "16 MONO" : q == 2 ? "8 ST" : "8 MONO");
        for (g = 0; g < 7; g++) {
            double e = 0, num = 0, den = 0;
            int n = 0;
            rk_clouds_opts(1, 1, CAP[g]);
            cl_patch(30, 0, 0); s = cl_clouds_slot();
            cl_set(s, P_MODE, 0.0f); cl_set(s, P_QUAL, (float)q); cl_set(s, P_DENS, 1.0f); cl_set(s, P_BLEND, 1.0f);
            cl_set(s, P_POS, 0.2f); cl_set(s, P_SIZE, 0.5f); cl_set(s, P_TEX, 0.4f);
            rk_set_voices(4);
            for (i = 0; i < 4; i++) rk_note_on(48.0f + 3.0f * (float)i, 0.8f);
            for (ms = 0, k = 0; ms < 6000; ms += 10) {
                rk_render(480);
                if (ms >= 2000) { memcpy(snd + (size_t)k * 480, rk_out_l(), 480 * sizeof(float)); k++; }
            }
            n = k * 480;
            for (i = 0; i < n; i++) e += (double)snd[i] * snd[i];
            for (i = 100; i < 12000; i += 100) {            /* the centroid from a Goertzel scan, 100 Hz steps up to 12 kHz, a 1 s stretch */
                double w = 6.2831853 * i / 48000.0, c = 2.0 * cos(w), s1 = 0, s2 = 0, p2;
                int j;
                for (j = 0; j < 48000; j++) { double s0 = snd[j] + c * s1 - s2; s2 = s1; s1 = s0; }
                p2 = s1 * s1 + s2 * s2 - c * s1 * s2;
                num += i * sqrt(p2); den += sqrt(p2);
            }
            printf("   %.4f/%-5.0f ", sqrt(e / n), den > 0 ? num / den : 0.0);
        }
        printf("\n");
    }
    rk_clouds_opts(1, 1, 40);
}

static void print_cost2(void)
{
#define NRUN2 5
    static const clcfg_t CFG[] = {{0, 0, 0.7f, 0.3f}, {0, 0, 0.9f, 0.3f}, {0, 0, 1.0f, 0.3f}, {1, 0, 0.9f, 0.3f}, {2, 1, 0.9f, 0.3f}, {0, 0, 0.9f, 0.0f}};
    static const char *const NM[] = {"GRANULR 16 ST D0.7", "GRANULR 16 ST D0.9", "GRANULR 16 ST D1.0", "LOOPDLY 16 ST", "SPECTRL (16 MONO)", "GRANULR 16 ST D0.9 REVERB 0"};
    /* the variants: update 7 (all off), update 8 (all on), and each change alone */
    static const struct { const char *n; int k32, skip, grains; } V[] = {{"before (update 7)", 0, 0, 0}, {"after (update 8)", 1, 1, 40}, {"32K only", 1, 0, 0}, {"ceiling 40 only", 0, 0, 40}, {"skip only", 0, 1, 0}};
    int c, v, r, g, q;
    double m[5][NRUN2], mean, lo, hi, four = 0;
    printf("Clouds' own share (extra over Braids x4, in %% of this PC's real time: a PROXY, not the FM-1's core), REVERB 0.3, BLEND 1, 4 notes held;\n"
           "  mean (lowest-highest) of %d runs, runs of the variants interleaved\n", NRUN2);
    for (c = 0; c < (int)(sizeof CFG / sizeof *CFG); c++) {
        for (r = 0; r < NRUN2; r++) {
            four = braids4();
            for (v = 0; v < 5; v++) m[v][r] = own_share(&CFG[c], V[v].k32, V[v].skip, V[v].grains, four);
        }
        printf("%s   (Braids x4 alone %.2f %%)\n", NM[c], four * 100.0);
        for (v = 0; v < 5; v++) {
            stats(m[v], NRUN2, &mean, &lo, &hi);
            printf("    %-20s %6.2f (%5.2f-%5.2f)\n", V[v].n, mean, lo, hi);
        }
    }
    print_grains();
}

int main(int argc, char **argv)
{
    int s;
    char path[512];
    if (argc >= 2 && !strcmp(argv[1], "--cost")) {
        print_cost();
        return 0;
    }
    if (argc >= 2 && !strcmp(argv[1], "--cost2")) {
        if (argc >= 3 && !strcmp(argv[2], "grains")) { print_grains(); print_grain_sound(); } else print_cost2();
        return 0;
    }
    if (argc < 2) {
        fprintf(stderr, "clouds_demo OUTDIR | --cost | --cost2\n");
        return 1;
    }
    if (getenv("CL_K32"))                          /* update 8: CL_K32=0 renders with Clouds at 48 kHz (RACK_CLOUDS_32K off at run time), for the A/B */
        rk_clouds_opts(atoi(getenv("CL_K32")), 1, 40);
    s = demo_patch();
    play(PADCH, (int)(sizeof PADCH / sizeof *PADCH), 0, s);
    snprintf(path, sizeof path, "%s/01_granular_on_pad.wav", argv[1]);
    wav(path, lr, SR * SECS);
    s = demo_patch();
    cl_set(s, P_BLEND, 0.55f);
    play(FRZCH, (int)(sizeof FRZCH / sizeof *FRZCH), auto_freeze, s);
    snprintf(path, sizeof path, "%s/02_frozen_cloud.wav", argv[1]);
    wav(path, lr, SR * SECS);
    s = demo_patch();
    cl_set(s, P_SIZE, 0.35f);
    play(PADCH, (int)(sizeof PADCH / sizeof *PADCH), auto_pitch, s);
    snprintf(path, sizeof path, "%s/03_pitch_shifted_grains.wav", argv[1]);
    wav(path, lr, SR * SECS);
    return 0;
}
