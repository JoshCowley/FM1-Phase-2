// SPDX-License-Identifier: GPL-3.0
// FM1 Phase 2: licensed GPL-3.0, see LICENSE; third-party code and its licences: NOTICE.md.
/* CLOUDS in the Lite firmware (FM1 Phase 2 step 5), checked on a computer; included by rack_host.c (section 6).
 *   6a  each mode the firmware offers (GRANULR, LOOPDLY, SPECTRL) at each QUALITY, in a patch with Braids held in a chord:
 *       Clouds' own output (BLEND all wet) is not silent and not clipping; a stressed patch (feedback, reverb, density all
 *       high) stays finite and inside full scale; FREEZE keeps a cloud sounding after the notes end, and the same patch
 *       unfrozen does not;
 *   6b  the arena: every pair of effects (CLOUDS included, twice too), light and heavy settings, with and without Rings,
 *       never uses more than the arena;
 *   6c  the give-way rule (engine/rack.cc fm1_layout): with CLOUDS loaded and the arena short the REVERB goes first (when
 *       the effects and RACK_CLOUDS_VOICES voices don't fit), the DELAY only if one voice would not fit even so; CLOUDS stays;
 *       in either slot; and with room for all, nothing is dropped; without CLOUDS the old order holds (FX 2, then FX 1). */
#include "ui_defs.h"

#define CL_FX1 RS_FX1
#define CL_FX2 RS_FX2
enum { CM_GRANULAR = 0, CM_LOOPDLY = 1, CM_SPECTRAL = 2 };   /* the firmware's MODE list */
enum { P_POS, P_SIZE, P_PITCH, P_DENS, P_TEX, P_BLEND, P_SPREAD, P_FB, P_REV, P_MODE, P_FREEZE, P_QUAL };
static const char *const CM_NAME[3] = {"GRANULR", "LOOPDLY", "SPECTRL"};

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
static void cl_set(int slot, int p, float v) { rk_set_param(slot, p, v); }

static void cl_patch(int fx1, int fx2, int shp)
{
    rk_seed(0x21u);
    rc_load(0);                               /* INIT: a Braids tone, sustained */
    cl_swap(RS_SHP, shp);
    cl_swap(RS_FX1, fx1);
    cl_swap(RS_FX2, fx2);
}

static int cl_clouds_slot(void) { return rc_patch.mod[RS_FX1] == 30 ? RS_FX1 : RS_FX2; }

/* the rms and peak of the left and right outputs over frames [from, to) of what render() gives, and a count of samples that
 * are not finite or reach full scale */
typedef struct { double rms; float peak; int bad; } cl_meas_t;
static cl_meas_t cl_run(int secs, int from_ms, int to_ms, void (*each)(int ms))
{
    cl_meas_t m = {0, 0, 0};
    double e = 0;
    long n = 0;
    int ms, i;
    for (ms = 0; ms < secs * 1000; ms += 10) {
        if (each)
            each(ms);
        rk_render(480);
        if (ms >= from_ms && ms < to_ms)
            for (i = 0; i < 480; i++) {
                float l = rk_out_l()[i], r = rk_out_r()[i];
                if (!(fabsf(l) < 1e6f) || !(fabsf(r) < 1e6f)) { m.bad++; continue; }
                if (fabsf(l) >= 1.0f || fabsf(r) >= 1.0f)
                    m.bad++;
                if (fabsf(l) > m.peak) m.peak = fabsf(l);
                if (fabsf(r) > m.peak) m.peak = fabsf(r);
                e += 0.5 * (l * l + r * r);
                n++;
            }
    }
    m.rms = n ? sqrt(e / (double)n) : 0;
    return m;
}
static void cl_chord(int ms) { if (ms == 0) { rk_note_on(48.0f, 0.8f); rk_note_on(52.0f, 0.8f); rk_note_on(55.0f, 0.8f); rk_note_on(59.0f, 0.8f); } }

static int check_clouds_audio(void)
{
    int fails = 0, mode, q;
    for (mode = 0; mode < 3; mode++)
        for (q = 0; q < 4; q++) {
            int s;
            cl_meas_t w, x;
            cl_patch(30, 0, 0);
            s = cl_clouds_slot();
            cl_set(s, P_POS, 0.3f); cl_set(s, P_SIZE, 0.5f); cl_set(s, P_DENS, 0.8f); cl_set(s, P_TEX, 0.4f);
            cl_set(s, P_BLEND, 1.0f);             /* all wet: Clouds' own sound, not the dry Braids */
            cl_set(s, P_MODE, (float)mode); cl_set(s, P_QUAL, (float)q);
            rk_set_voices(4);
            w = cl_run(3, 1000, 3000, cl_chord);  /* the last two of three seconds */
            cl_patch(30, 0, 0);
            s = cl_clouds_slot();                 /* the stress: feedback, reverb, density, size, spread at their top, loud input */
            cl_set(s, P_FB, 0.95f); cl_set(s, P_REV, 1.0f); cl_set(s, P_DENS, 1.0f); cl_set(s, P_SIZE, 1.0f);
            cl_set(s, P_SPREAD, 1.0f); cl_set(s, P_BLEND, 0.6f); cl_set(s, P_MODE, (float)mode); cl_set(s, P_QUAL, (float)q);
            cl_set(s, P_PITCH, 1.0f);             /* +24 semitones */
            rk_set_voices(6);
            x = cl_run(4, 1000, 4000, cl_chord);
            printf("6a %-8s quality %d: wet rms %.4f peak %.3f | stress peak %.3f%s\n", CM_NAME[mode], q, w.rms, w.peak, x.peak,
                   w.rms >= 0.005 && !w.bad && !x.bad && x.rms > 0.0f ? "" : "   FAIL");
            if (w.rms < 0.005 || w.bad || x.bad || x.rms <= 0.0f)
                fails++;
        }
    {   /* freeze: a cloud that goes on sounding after the notes have ended (GRANULR, wet) */
        int k;
        double rms[2] = {0, 0};
        for (k = 0; k < 2; k++) {
            int s, ms;
            double e = 0;
            long n = 0;
            cl_patch(30, 0, 0);
            s = cl_clouds_slot();
            cl_set(s, P_POS, 0.3f); cl_set(s, P_SIZE, 0.5f); cl_set(s, P_DENS, 0.8f); cl_set(s, P_BLEND, 1.0f);
            rk_set_voices(4);
            for (ms = 0; ms < 6000; ms += 10) {
                int i;
                if (ms == 0) cl_chord(0);
                if (ms == 1500 && k) cl_set(s, P_FREEZE, 1.0f);       /* freeze with the notes still sounding */
                if (ms == 1600) rk_all_off();
                rk_render(480);
                if (ms >= 5000)
                    for (i = 0; i < 480; i++) { e += rk_out_l()[i] * rk_out_l()[i]; n++; }
            }
            rms[k] = sqrt(e / (double)n);
        }
        printf("6a freeze: rms of the 6th second, notes ended at 1.6 s: not frozen %.5f, frozen %.4f%s\n", rms[0], rms[1],
               rms[1] >= 0.005 && rms[1] > 5.0 * rms[0] ? "" : "   FAIL");
        if (rms[1] < 0.005 || rms[1] <= 5.0 * rms[0])
            fails++;
    }
    return fails;
}

/* ---- arena ---------------------------------------------------------------------------------------------------- */
/* effect settings: 0 the page's defaults, 1 the most memory-hungry (a 1 s delay line; the reverb's SHIMMER and longest PRE),
 * 2 the least (DELAY TIME 1/16: 6,429 samples a side at 112 BPM) */
static void cl_heavy(int slot, int on)
{
    int id = rc_patch.mod[slot];
    if (id == 31 && on == 2) cl_set(slot, 0, 0.0f);
    else if (id == 31) { cl_set(slot, 9, on ? 1.0f : 0.0f); cl_set(slot, 10, on ? 1.0f : 0.3f); }          /* SYNC FREE, FREE MS: 1 s line */
    if (id == 32) { cl_set(slot, 4, on ? 2.0f : 0.0f); cl_set(slot, 5, on ? 1.0f : 0.0f); }           /* SHIMMER, PRE: longest */
    rk_fm1_fit();
}

static int check_clouds_arena(void)
{
    static const int FX[5] = {0, 30, 31, 32, 33};
    int fails = 0, a, b, heavy, shp, cases = 0, worst = 0, arena = 0, nodrop_other = 0;
    for (shp = 0; shp < 2; shp++)
        for (heavy = 0; heavy < 2; heavy++)
            for (a = 0; a < 5; a++)
                for (b = 0; b < 5; b++) {
                    int used, voices, dropped;
                    cl_patch(FX[a], FX[b], shp ? 10 : 0);
                    cl_heavy(RS_FX1, heavy);
                    cl_heavy(RS_FX2, heavy);
                    used = rk_fm1_status(0); arena = rk_fm1_status(1); voices = rk_fm1_status(2); dropped = rk_fm1_status(4);
                    cases++;
                    if (used > worst) worst = used;
                    if (used > arena || voices < 1 || (dropped >> RS_SRC & 1)) {
                        printf("6b FAIL fx %d %d %s %s: used %d of %d, voices %d, dropped %02x\n", FX[a], FX[b], shp ? "RINGS" : "no shaper",
                               heavy ? "heavy" : "light", used, arena, voices, dropped);
                        fails++;
                    }
                    if (FX[a] == 30 && FX[b] == 30 && !(dropped >> RS_FX2 & 1)) { printf("6b FAIL two CLOUDS: the second was not refused\n"); fails++; }
                    if (FX[a] != 30 && FX[b] != 30 && (dropped >> RS_SRC & 1)) nodrop_other++;
                }
    printf("6b arena: %d patches (every pair of effects, light and heavy, with and without Rings): largest use %d KB of %d KB%s\n",
           cases, worst / 1024, arena / 1024, fails ? "   FAIL" : "");
    return fails;
}

/* ---- the give-way rule ---------------------------------------------------------------------------------------- */
static const char *cl_dropped_text(int d, char *t)
{
    t[0] = 0;
    if (d >> RS_FX1 & 1) strcat(t, "FX1 ");
    if (d >> RS_FX2 & 1) strcat(t, "FX2 ");
    if (d >> RS_SHP & 1) strcat(t, "SHP ");
    if (d >> RS_SRC & 1) strcat(t, "SRC ");
    if (!t[0]) strcpy(t, "none");
    return t;
}
static int cl_expect(const char *what, int fx1, int fx2, int shp, int h1, int h2, int want_drop, int min_voices)
{
    char t[32];
    int d, v, u;
    cl_patch(fx1, fx2, shp);
    cl_heavy(RS_FX1, h1);
    cl_heavy(RS_FX2, h2);
    d = rk_fm1_status(4); v = rk_fm1_status(2); u = rk_fm1_status(0);
    printf("6c %-52s dropped %-9s used %3d KB voices %d%s\n", what, cl_dropped_text(d, t), u / 1024, v,
           (d == want_drop && v >= min_voices && u <= rk_fm1_status(1)) ? "" : "   FAIL");
    return !(d == want_drop && v >= min_voices && u <= rk_fm1_status(1));
}

static int check_clouds_giveway(void)
{
    int f = 0;
    const int R = 1 << RS_FX1, R2 = 1 << RS_FX2;
    /* room for all: nothing is dropped, and the rule does not act on its own */
    f += cl_expect("CLOUDS + REVERB (plate)", 30, 32, 0, 0, 0, 0, 4);
    f += cl_expect("CLOUDS + REVERB at its longest (SHIMMER, PRE 150 ms)", 30, 32, 0, 0, 1, 0, 4);
    f += cl_expect("CLOUDS + DELAY (the page's 1/8D) + 4 voices", 30, 31, 0, 0, 0, 0, 4);
    f += cl_expect("CLOUDS + CHORUS", 30, 33, 0, 0, 0, 0, 4);
    /* the REVERB gives way first: with Rings the effects and 4 voices do not fit, so the reverb goes, either slot */
    f += cl_expect("CLOUDS + REVERB + Rings: the reverb goes (FX 2)", 30, 32, 10, 0, 0, R2, 4);
    f += cl_expect("REVERB + CLOUDS + Rings: the reverb goes (FX 1)", 32, 30, 10, 0, 0, R, 4);
    /* the DELAY only if absolutely necessary: the same Rings, a short DELAY (1/16) stays while one voice fits ... */
    f += cl_expect("CLOUDS + DELAY 1/16 + Rings: the delay stays", 30, 31, 10, 0, 2, 0, 1);
    /* ... and goes when it does not: the page's DELAY (1/8D) with Rings, or a 1 s line without; CLOUDS stays either way */
    f += cl_expect("CLOUDS + DELAY 1/8D + Rings: one voice does not fit", 30, 31, 10, 0, 0, R2, 1);
    f += cl_expect("DELAY 1/8D + CLOUDS + Rings: (FX 1)", 31, 30, 10, 0, 0, R, 1);
    f += cl_expect("CLOUDS + DELAY 1 s: it does not fit with Clouds at all", 30, 31, 0, 0, 1, R2, 1);
    /* a second CLOUDS is refused: one engine instance */
    f += cl_expect("CLOUDS in both slots: the second is refused", 30, 30, 0, 0, 0, R2, 4);
    /* without CLOUDS nothing changes: the long delay and the long reverb fit together, nothing is dropped */
    f += cl_expect("no CLOUDS: DELAY 1 s + REVERB at its longest", 31, 32, 0, 1, 1, 0, 1);
    return f;
}

static int check_clouds(void)
{
    int fails = check_clouds_audio();
    fails += check_clouds_arena();
    fails += check_clouds_giveway();
    return fails;
}
