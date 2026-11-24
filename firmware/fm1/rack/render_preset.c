// SPDX-License-Identifier: GPL-3.0
// FM1 Phase 2: licensed GPL-3.0, see LICENSE; third-party code and its licences: NOTICE.md.
/* Any factory preset played through the firmware's own sound path (rc_load, rc_render at 44.1 kHz) from a script, to a
 * 32-bit float stereo WAV (peak and level measured on the float, nothing clipped). Update 11, for the granular presets' demos.
 *   render_preset PRESET SCRIPT OUT.wav        PRESET: its name (underscores for spaces) or its number
 * SCRIPT lines (times in seconds; '#' starts a comment):
 *   secs S                    length (default 16)
 *   voices N                  the voices to play (default: the preset's own)
 *   on T DUR NOTE VEL         a note (MIDI number, velocity 0..1)
 *   ramp K T0 T1 V0 V1        macro K (1-4) moves linearly from V0 to V1 between T0 and T1 (holds V1 after, V0 before)
 *   par SLOT PARAM V          a parameter's value, set after the load (SLOT 0-5 as the page: SRC SHP MOD-A MOD-B FX-1 FX-2; for trying values)
 *   set K T V                 macro K steps to V at T
 * Macros start where the preset leaves them. Built by build_host.sh beside rack_host. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "rack_core.c"

#define SR 44100
#define MAXEV 512
typedef struct { float t, d; int n; float v; int on; } ev_t;
typedef struct { int k; float t0, t1, v0, v1; } mv_t;
static ev_t ev[MAXEV]; static int nev;
static mv_t mv[MAXEV]; static int nmv;
static int pars[64][2]; static float parv[64]; static int npar;
static float wavbuf[2 * SR * 64];

static void wavf(const char *path, const float *x, int frames)
{
    FILE *f = fopen(path, "wb");
    uint32_t n = (uint32_t)frames * 8u, v;
    if (!f) return;
    fwrite("RIFF", 1, 4, f); v = 36u + n; fwrite(&v, 4, 1, f); fwrite("WAVEfmt ", 1, 8, f);
    v = 16; fwrite(&v, 4, 1, f); v = 3u | 2u << 16; fwrite(&v, 4, 1, f); v = SR; fwrite(&v, 4, 1, f);
    v = SR * 8u; fwrite(&v, 4, 1, f); v = 8u | 32u << 16; fwrite(&v, 4, 1, f); fwrite("data", 1, 4, f); fwrite(&n, 4, 1, f);
    fwrite(x, 4, (size_t)frames * 2, f);
    fclose(f);
}

int main(int argc, char **argv)
{
    int k = -1, voices = 0, secs = 16, b, i;
    char line[256], nm[64];
    FILE *s;
    float mac[4];
    if (argc < 4) { fprintf(stderr, "render_preset PRESET SCRIPT OUT.wav\n"); return 1; }
    for (i = 0; argv[1][i] && i < 63; i++) nm[i] = argv[1][i] == '_' ? ' ' : argv[1][i];
    nm[i] = 0;
    for (i = 0; i < NPRESETS; i++) if (!strcmp(PRESETS[i].name, nm)) k = i;
    if (k < 0 && argv[1][0] >= '0' && argv[1][0] <= '9') k = atoi(argv[1]);
    if (k < 0 || k >= NPRESETS) { fprintf(stderr, "no preset %s\n", argv[1]); return 1; }
    s = fopen(argv[2], "r");
    if (!s) { fprintf(stderr, "no script\n"); return 1; }
    while (fgets(line, sizeof line, s)) {
        float a, bb, c, d, e; int kk, i;
        if (sscanf(line, "secs %f", &a) == 1) secs = (int)a;
        else if (sscanf(line, "voices %f", &a) == 1) voices = (int)a;
        else if (nev < MAXEV && sscanf(line, "on %f %f %f %f", &a, &bb, &c, &d) == 4) { ev[nev].t = a; ev[nev].d = bb; ev[nev].n = (int)c; ev[nev].v = d; nev++; }
        else if (nmv < MAXEV && sscanf(line, "ramp %d %f %f %f %f", &kk, &a, &bb, &c, &d) == 5) { mv[nmv++] = (mv_t){kk - 1, a, bb, c, d}; }
        else if (npar < 64 && sscanf(line, "par %d %d %f", &kk, &i, &a) == 3) { pars[npar][0] = kk; pars[npar][1] = i; parv[npar++] = a; }
        else if (nmv < MAXEV && sscanf(line, "set %d %f %f", &kk, &a, &c) == 3) { mv[nmv++] = (mv_t){kk - 1, a, a, c, c}; }
        (void)e;
    }
    fclose(s);
    if (secs > 64) secs = 64;
#ifdef RACK_BENCH_AB
    if (getenv("RP_Q")) set_quality((unsigned)atoi(getenv("RP_Q")));   /* 2.38: the quality trims as a mask (build with RACK_FLAGS=-DRACK_BENCH_AB): 0 = the 2.37 sound, 1 reverb sum, 2 short halfband, 4 light reverb diffusion */
#endif
    if (getenv("RP_CLQ")) rk_clouds_mask((unsigned)strtoul(getenv("RP_CLQ"), 0, 0));   /* update 42: the Clouds quality bits (shim/clouds/dsp/rack_cl_q.h): 0 = 2.40's Clouds, 31 NORMAL, 191 LITE */
    if (getenv("RP_CAP")) rk_clouds_opts(getenv("RP_K32") ? atoi(getenv("RP_K32")) : 1, 1, atoi(getenv("RP_CAP")));   /* RP_CAP=n RP_K32=0|1: the Clouds grain ceiling / 48 kHz switch, for A/B renders */
    rk_seed(0x21u);
    rc_pause();
    rc_load(k);
    rc_resume();
    if (voices) rc_voices(voices);
    for (i = 0; i < npar; i++) rc_set(pars[i][0], pars[i][1], parv[i]);
    for (i = 0; i < 4; i++) mac[i] = rc_patch.macro[i];
    for (i = 0; i < nmv; i++) if (mv[i].k >= 0 && mv[i].k < 4 && mv[i].t0 == 0.0f) { /* first value applies at once */ }
    {
        int32_t blk[2 * 256];
        float peak = 0;
        for (b = 0; b + 256 <= SR * secs; b += 256) {
            float t0 = (float)b / SR, t1 = (float)(b + 256) / SR;
            int j;
            for (i = 0; i < nev; i++) {
                if (!ev[i].on && ev[i].t >= t0 - 1e-6f && ev[i].t < t1) { rc_note_on(ev[i].n, ev[i].v); ev[i].on = 1; }
                if (ev[i].on == 1 && ev[i].t + ev[i].d < t1) { rc_note_off(ev[i].n); ev[i].on = 2; }
            }
            for (i = 0; i < nmv; i++) {          /* the latest move whose time has begun sets its macro */
                const mv_t *m = &mv[i];
                float v;
                if (t0 < m->t0) continue;
                v = t0 >= m->t1 || m->t1 <= m->t0 ? m->v1 : m->v0 + (m->v1 - m->v0) * (t0 - m->t0) / (m->t1 - m->t0);
                mac[m->k] = v;          /* later lines win: the script lists moves in time order per macro */
            }
            for (j = 0; j < 4; j++) if (fabsf(mac[j] - rc_patch.macro[j]) > 1e-4f) rc_macro(j, mac[j]);
            rc_render(blk, 256, 8388607.0f * 0.7f);
            for (j = 0; j < 512; j++) { float v = (float)blk[j] / 8388607.0f; wavbuf[2 * b + j] = v; if (fabsf(v) > peak) peak = fabsf(v); }
        }
        wavf(argv[3], wavbuf, SR * secs);
        printf("%s voices %d peak %.3f\n", PRESETS[k].name, rc_patch.voices_now, peak);
    }
    return 0;
}
