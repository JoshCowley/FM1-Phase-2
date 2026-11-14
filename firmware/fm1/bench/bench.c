// SPDX-License-Identifier: GPL-3.0
// FM1 Phase 2: licensed GPL-3.0, see LICENSE; third-party code and its licences: NOTICE.md.
/* FM1 Rack benchmark: the cases and how each one is timed. Plain C, shared by the firmware (app.c includes it)
 * and the computer's check (bench_host.cc). The engine is engine/rack.cc built with -DRACK_FM1 -DRACK_LITE.
 *
 * A case: a factory preset (INIT, or INIT plus the preset's differences: cases.h), changes on top of it (a model,
 * a module taken out), a number of notes held. It is set up and played for 0.1 s untimed, then timed NRUN times over
 * 480 frames (10 ms at the engine's 48 kHz). The engine has to render 48,000 frames a second on the FM-1 too (the
 * codec runs at 44.1 kHz, so the port resamples or moves the engine to 44.1 kHz), so
 *     CPU % of one core = microseconds for 480 frames / 100.
 * The lowest of the runs is the figure (interrupts only ever add time); the highest is kept as well. */
#include <stdint.h>

/* the engine, through rk.cc (its C interface under names of the benchmark's own: the platform's unity build has
 * functions called init, render and so on of its own) */
void rk_init(void);
void rk_seed(uint32_t s);
void rk_set_module(int slot, int id);
void rk_set_param(int slot, int param, float v);
void rk_set_meta(int slot, int param, float lo, float hi, float span);
void rk_set_route(int i, int src, int slot, int param, float amt, int via);
void rk_set_macro(int i, float v);
void rk_set_master(float v);
void rk_set_master_fx(float lo, float mid, float hi, float comp);
void rk_set_legato(int on);
void rk_set_vintage(float v);
void rk_set_ext_slew(int i, float s);
void rk_set_tune(int note, float semis);
void rk_set_voices(int n);
void rk_clouds_opts(int k32, int skip_rev, int grains);   /* update 8: the Clouds changes as run-time switches (this benchmark's comparison cases only) */
void rk_note_on(float note, float vel);
void rk_all_off(void);
void rk_render(int n);
float *rk_out_l(void);
float *rk_out_r(void);
int rk_fm1_status(int what);
void rk_fm1_fit(void);
void rk_set_worker(int (*run)(void (*)(uint32_t), uint32_t), int (*wait)(void));
uint32_t rk_worker_blocks(void);
/* the second core, when the platform has it (app.c: plat_cpu2_*; bench_host.c: a worker that runs the job at once):
 * the 2 CORES cases hand it half the voices; every other case runs on one core */
static int (*bench_cpu2_run)(void (*)(uint32_t), uint32_t);
static int (*bench_cpu2_wait)(void);

typedef struct { uint8_t fn, a, b, c, d; float x, y, z, w; } bop_t;
typedef struct { const char *name; const bop_t *ops; uint16_t nops; uint8_t voices, mode; } bpreset_t;
#include "cases.h"

enum { P_INIT, P_FOLDED_BASS, P_KARPLUS, P_TIDAL_FM, P_GRID_GATE, P_CH_GLASS_PNO, P_BS_SUB_MONO, P_LD_SQUARE_CHIP,
       P_P2INIT, P_PLAYED };   /* cases.h order (make_cases.py PRESETS): the ones the cases are built on, then the ones played */
enum { S_SRC, S_SHP, S_MODA, S_MODB, S_FX1, S_FX2 };

static void run_ops(const bop_t *o, int n)
{
    int i;
    for (i = 0; i < n; i++, o++)
        switch (o->fn) {
        case 1: rk_set_module(o->a, o->b); break;
        case 2: rk_set_param(o->a, o->b, o->x); break;
        case 3: rk_set_meta(o->a, o->b, o->x, o->y, o->z); break;
        case 4: rk_set_route(o->a, o->b, o->c, o->d, o->x, (int)o->y); break;
        case 5: rk_set_macro(o->a, o->x); break;
        case 6: rk_set_master(o->x); break;
        case 7: rk_set_master_fx(o->x, o->y, o->z, o->w); break;
        case 8: rk_set_legato((int)o->x); break;
        case 9: rk_set_vintage(o->x); break;
        case 10: rk_set_ext_slew(o->a, o->x); break;
        case 11: rk_set_tune(o->a, o->x); break;
        }
}

/* ------------------------------------------------------------------ the cases --- */
/* GRAIN is shelved (FM1 Phase 2 step 4): its group, cases and setup exist only with -DRACK_WITH_GRAIN (the engine's flag) */
enum { G_CORE, G_BRAIDS, G_RINGS, G_SHP, G_MOD, G_FX, G_VOICES, G_CLOUDS, G_PRESET,
#ifdef RACK_WITH_GRAIN
       G_GRAIN,
#endif
       G_CORES2, NGROUP };
static const char *const GROUP[NGROUP] = {"CORE", "BRAIDS", "RINGS", "FILTER", "MOD", "FX", "VOICES",
                                           "CLOUDS", "PRESETS",
#ifdef RACK_WITH_GRAIN
                                           "GRAIN",
#endif
                                           "2 CORES"};
static const char *const BRAIDS_MODEL[47] = {"CSAW", "/\\-_", "//-_", "FOLD", "UUUU", "SUB-", "SUB/", "SYN-",
    "SYN/", "//x3", "-_x3", "/\\x3", "SIx3", "RING", "////", "//uu", "TOY*", "ZLPF", "ZPKF", "ZBPF", "ZHPF", "VOSM",
    "VOWL", "VFOF", "HARM", "FM", "FBFM", "WTFM", "PLUK", "BOWD", "BLOW", "FLUT", "BELL", "DRUM", "KICK", "CYMB",
    "SNAR", "WTBL", "WMAP", "WLIN", "WTx4", "NOIS", "TWLN", "CLKN", "CLOU", "PRTC", "QPSK"};
static const char *const RINGS_MODEL[6] = {"MODAL", "SYMPATH", "STRING", "FM VOICE", "SYM QNT", "STR+VERB"};

/* a case: the preset, up to four changes (fn 1 set_module, 2 set_param; 0 none), notes held, and the case it is
 * compared with (the module's own cost is the difference; -1 none) */
/* grains: a GRAIN case (FM1 Phase 2): the granular source with at most this many grains a voice (0: not one)
 * cl: a CLOUDS case (FM1 Phase 2): 1 + its row in CLC (the settings), 0 none */
typedef struct { uint8_t group, preset, notes, grains, cl; int8_t minus; uint8_t ch[4][3]; float chv[4]; const char *name; } bcase_t;

#define NCASE_MAX 200
static bcase_t bcases[NCASE_MAX];
static int bncases;

static int bc_add(int group, int preset, int notes, const char *name)
{
    bcase_t *c = &bcases[bncases];
    c->group = (uint8_t)group;
    c->preset = (uint8_t)preset;
    c->notes = (uint8_t)notes;
    c->grains = 0;
    c->cl = 0;
    c->minus = -1;
    c->name = name;
    return bncases++;
}
#ifdef RACK_WITH_GRAIN
/* a GRAIN case: INIT with GRAIN as the source, as many grains a voice as asked, held at the most (bench_setup) */
static int bc_grain(int group, int notes, int grains, const char *name)
{
    int k = bc_add(group, P_INIT, notes, name);
    bcases[k].grains = (uint8_t)grains;
    return k;
}
#endif
static void bc_change(int k, int i, int fn, int slot, int param, float v)
{
    bcases[k].ch[i][0] = (uint8_t)fn;
    bcases[k].ch[i][1] = (uint8_t)slot;
    bcases[k].ch[i][2] = (uint8_t)param;
    bcases[k].chv[i] = v;
}

/* CLOUDS (FM1 Phase 2, 15 Nov 2026): every CLOUDS case is the firmware's own INIT (P2 INIT: a Braids tone) with the notes held,
 * Clouds in FX 1 when on, and the row of CLC it names. Clouds' settings, for every row: POSITION 0.2, SIZE 0.5, PITCH 0,
 * TEXTURE 0.4, BLEND 1.0 (full wet), SPREAD 0.5, FEEDBACK 0.3, REVERB 0.3, the rest at the page's defaults; MODE, QUALITY, DENSITY,
 * FREEZE as the row says. fx2: what is in FX 2 (0 nothing, 1 the reverb of CH GLASS PNO, 2 the delay of LD SQUARE CHIP). */
/* update 8: no32 = 1 runs Clouds at 48 kHz (RACK_CLOUDS_32K off), noskip = 1 keeps Clouds' internal reverb running at REVERB 0, cap: the grain
 * ceiling (0 the build's own, 40; 1 none: Mutable's own count, 32 stereo 16-bit, 45 stereo 8-bit, 57 mono 8-bit; n the ceiling n), revz = 1 REVERB 0.
 * They are set per case (rk_clouds_opts in bench_setup); the firmware itself never changes them. */
typedef struct { uint8_t on, mode, qual, fx2, freeze; float dens; uint8_t no32, noskip, cap, revz; } clc_t;
enum { CM_GRAN, CM_LOOP, CM_SPEC };                       /* the firmware's MODE list (STRETCH is out) */
enum { CQ_16ST, CQ_16MO, CQ_8ST, CQ_8MO };                /* the page's QUALITY list */
static const clc_t CLC[] = {
    /* 0 */ {0, 0, 0, 0, 0, 0.0f},                         /* Braids only (the reference) */
    /* 1 */ {1, CM_GRAN, CQ_16ST, 0, 0, 0.9f},
    /* 2 */ {1, CM_GRAN, CQ_8ST, 0, 0, 0.9f},
    /* 3 */ {1, CM_GRAN, CQ_16MO, 0, 0, 0.9f},
    /* 4 */ {1, CM_GRAN, CQ_8MO, 0, 0, 0.9f},
    /* 5 */ {1, CM_LOOP, CQ_16ST, 0, 0, 0.9f},
    /* 6 */ {1, CM_LOOP, CQ_8ST, 0, 0, 0.9f},
    /* 7 */ {1, CM_LOOP, CQ_16MO, 0, 0, 0.9f},
    /* 8 */ {1, CM_LOOP, CQ_8MO, 0, 0, 0.9f},
    /* 9 */ {1, CM_SPEC, CQ_16ST, 0, 0, 0.9f},
    /* 10 */ {1, CM_SPEC, CQ_8ST, 0, 0, 0.9f},
    /* 11 */ {1, CM_SPEC, CQ_16MO, 0, 0, 0.9f},
    /* 12 */ {1, CM_SPEC, CQ_8MO, 0, 0, 0.9f},
    /* 13 */ {1, CM_GRAN, CQ_16ST, 0, 0, 0.7f},            /* the default DENSITY */
    /* 14 */ {1, CM_GRAN, CQ_16ST, 0, 0, 1.0f},            /* the most */
    /* 15 */ {0, 0, 0, 1, 0, 0.0f},                        /* the reverb alone */
    /* 16 */ {0, 0, 0, 2, 0, 0.0f},                        /* the delay alone */
    /* 17 */ {1, CM_GRAN, CQ_16ST, 1, 0, 0.9f},           /* Clouds + the reverb */
    /* 18 */ {1, CM_GRAN, CQ_16ST, 2, 0, 0.9f},           /* Clouds + the delay */
    /* 19 */ {1, CM_GRAN, CQ_16ST, 0, 1, 0.9f},           /* FREEZE */
    /* update 8 (16 Nov 2026): one run shows what each change is worth. Rows 13, 1 and 14 above are GRANULR 16 ST at 0.7, 0.9, 1.0 with all
     * three on (the build's setting); these are the same with 32K off (48 kHz), with all three off (update 7), the ceiling off, a lower one, and
     * the 8-bit mono quality where the ceiling acts; and the reverb-skip pair */
    /* 20 */ {1, CM_GRAN, CQ_16ST, 0, 0, 0.7f, 1, 0, 0, 0},  /* 48 kHz */
    /* 21 */ {1, CM_GRAN, CQ_16ST, 0, 0, 0.9f, 1, 0, 0, 0},
    /* 22 */ {1, CM_GRAN, CQ_16ST, 0, 0, 1.0f, 1, 0, 0, 0},
    /* 23 */ {1, CM_GRAN, CQ_16ST, 0, 0, 0.7f, 1, 1, 1, 0},  /* all off: update 7 */
    /* 24 */ {1, CM_GRAN, CQ_16ST, 0, 0, 0.9f, 1, 1, 1, 0},
    /* 25 */ {1, CM_GRAN, CQ_16ST, 0, 0, 1.0f, 1, 1, 1, 0},
    /* 26 */ {1, CM_GRAN, CQ_16ST, 0, 0, 1.0f, 0, 0, 1, 0},  /* no grain ceiling (Mutable's own 32 at 16 ST) */
    /* 27 */ {1, CM_GRAN, CQ_8MO, 0, 0, 1.0f, 0, 0, 0, 0},   /* 8 MONO, ceiling 40 (of Mutable's 57) */
    /* 28 */ {1, CM_GRAN, CQ_8MO, 0, 0, 1.0f, 0, 0, 1, 0},   /* 8 MONO, no ceiling */
    /* 29 */ {1, CM_GRAN, CQ_16ST, 0, 0, 1.0f, 0, 0, 24, 0}, /* 16 ST, a ceiling of 24 (what a lower one would buy) */
    /* 30 */ {1, CM_GRAN, CQ_16ST, 0, 0, 0.9f, 0, 0, 0, 1},  /* REVERB 0, skip on */
    /* 31 */ {1, CM_GRAN, CQ_16ST, 0, 0, 0.9f, 0, 1, 0, 1},  /* REVERB 0, skip off */
};
#define CL_REF 0
#define CL_REVERB 15
#define CL_DELAY 16
#define M_CLOUDS 30
/* the Clouds page's parameters (clouds_def.py KINDS): 0 0..1, -1 -1..1, k a list of k options */
static const int8_t CL_KIND[22] = {0, 0, -1, 0, 0, 0, 0, 0, 0, 3, 2, 4, -1, 2, 2, 5, 0, 0, 4, 0, -1, -1};
static const float CL_VAL[22] = {0.2f, 0.5f, 0.0f, 0.9f, 0.4f, 1.0f, 0.5f, 0.3f, 0.3f, 0, 0, 0, 0.0f, 0, 0, 0, 0.0f, 0.0f, 0, 0.0f, 0.0f, 0.0f};

/* a CLOUDS case: P2 INIT, the row, the notes held; minus: the same without Clouds (the case it is compared with) */
static int bc_cl(int group, int notes, int row, const char *name, int minus)
{
    int k = bc_add(group, P_P2INIT, notes, name);
    bcases[k].cl = (uint8_t)(row + 1);
    bcases[k].minus = (int8_t)minus;
    return k;
}

static void bench_cases(void)
{
    int i, k, base;
    bncases = 0;
    /* the engine with no source: envelopes, matrix, LFOs, master, for one voice */
    k = bc_add(G_CORE, P_INIT, 1, "NO SOURCE");
    bc_change(k, 0, 1, S_SRC, 0, 0);
    /* Braids, each shape, one voice (FOLDED BASS with its filter taken out) */
    for (i = 0; i < 47; i++) {
        k = bc_add(G_BRAIDS, P_FOLDED_BASS, 1, BRAIDS_MODEL[i]);
        bc_change(k, 0, 1, S_SHP, 0, 0);
        bc_change(k, 1, 2, S_SRC, 0, (float)i);
    }
    /* Rings, each model, one voice: KARPLUS (Braids PLUK into Rings) without its delay; minus the same without
     * Rings */
    base = bc_add(G_RINGS, P_KARPLUS, 1, "(BRAIDS ONLY)");
    bc_change(base, 0, 1, S_FX1, 0, 0);
    bc_change(base, 1, 1, S_SHP, 0, 0);
    for (i = 0; i < 6; i++) {
        k = bc_add(G_RINGS, P_KARPLUS, 1, RINGS_MODEL[i]);
        bc_change(k, 0, 1, S_FX1, 0, 0);
        bc_change(k, 1, 2, S_SHP, 0, (float)i);
        bcases[k].minus = (int8_t)base;
    }
    /* the filter: FOLDED BASS (Braids into the filter), minus the same without it */
    base = bc_add(G_SHP, P_FOLDED_BASS, 1, "(BRAIDS ONLY)");
    bc_change(base, 0, 1, S_SHP, 0, 0);
    k = bc_add(G_SHP, P_FOLDED_BASS, 1, "FILTER");
    bcases[k].minus = (int8_t)base;
    /* the modulation modules, once a block whatever the voices: TIDAL FM's Tides and Peaks, GRID GATE's Grids */
    base = bc_add(G_MOD, P_TIDAL_FM, 1, "(NO MOD)");
    bc_change(base, 0, 1, S_MODA, 0, 0);
    bc_change(base, 1, 1, S_MODB, 0, 0);
    bc_change(base, 2, 1, S_FX1, 0, 0);
    bc_change(base, 3, 1, S_FX2, 0, 0);
    k = bc_add(G_MOD, P_TIDAL_FM, 1, "TIDES");
    bc_change(k, 0, 1, S_MODB, 0, 0);
    bc_change(k, 1, 1, S_FX1, 0, 0);
    bc_change(k, 2, 1, S_FX2, 0, 0);
    bcases[k].minus = (int8_t)base;
    k = bc_add(G_MOD, P_TIDAL_FM, 1, "PEAKS");
    bc_change(k, 0, 1, S_MODA, 0, 0);
    bc_change(k, 1, 1, S_FX1, 0, 0);
    bc_change(k, 2, 1, S_FX2, 0, 0);
    bcases[k].minus = (int8_t)base;
    k = bc_add(G_MOD, P_TIDAL_FM, 1, "FUNC");   /* (13 Nov 2026) the firmware's own function generator in MOD A */
    bc_change(k, 0, 1, S_MODA, 24, 0);
    bc_change(k, 1, 1, S_MODB, 0, 0);
    bc_change(k, 2, 1, S_FX1, 0, 0);
    bc_change(k, 3, 1, S_FX2, 0, 0);
    bcases[k].minus = (int8_t)base;
    base = bc_add(G_MOD, P_GRID_GATE, 1, "(NO GRIDS)");
    bc_change(base, 0, 1, S_MODA, 0, 0);
    bc_change(base, 1, 1, S_FX1, 0, 0);
    bc_change(base, 2, 1, S_FX2, 0, 0);
    k = bc_add(G_MOD, P_GRID_GATE, 1, "GRIDS");
    bc_change(k, 0, 1, S_FX1, 0, 0);
    bc_change(k, 1, 1, S_FX2, 0, 0);
    bcases[k].minus = (int8_t)base;
    /* the effects, once a block: LD SQUARE CHIP's delay, CH GLASS PNO's reverb, BS SUB MONO's chorus, each minus
     * its preset without it (the Phase 2 build has no Plaits: the three presets' source plays Braids) */
    base = bc_add(G_FX, P_LD_SQUARE_CHIP, 1, "(NO DELAY)");
    bc_change(base, 0, 1, S_FX1, 0, 0);
    k = bc_add(G_FX, P_LD_SQUARE_CHIP, 1, "DELAY");
    bcases[k].minus = (int8_t)base;
    base = bc_add(G_FX, P_CH_GLASS_PNO, 1, "(NO REVERB)");
    bc_change(base, 0, 1, S_FX1, 0, 0);
    k = bc_add(G_FX, P_CH_GLASS_PNO, 1, "REVERB");
    bcases[k].minus = (int8_t)base;
    base = bc_add(G_FX, P_BS_SUB_MONO, 1, "(NO CHORUS)");
    bc_change(base, 0, 1, S_FX1, 0, 0);
    k = bc_add(G_FX, P_BS_SUB_MONO, 1, "CHORUS");
    bcases[k].minus = (int8_t)base;
    /* voices: INIT (its Plaits source plays Braids in this build, shape 0) with 1, 2, 4 and 6 notes held */
    bc_add(G_VOICES, P_INIT, 1, "INIT x1");
    bc_add(G_VOICES, P_INIT, 2, "INIT x2");
    bc_add(G_VOICES, P_INIT, 4, "INIT x4");
    bc_add(G_VOICES, P_INIT, 6, "INIT x6");
#ifdef RACK_WITH_GRAIN
    /* GRAIN (FM1 Phase 2, 14 Nov 2026): the granular source playing as many grains a voice as GRAINS allows (each
     * 316 ms, 1000 a second wanted: every grain slot full, the worst case for the CPU), from a test recording in its
     * 128 KB buffer; one voice at 4 to 32 grains, then 4 and 6 voices at 8, 16 and 32 */
    bc_grain(G_GRAIN, 1, 4, "4 GR x1");
    bc_grain(G_GRAIN, 1, 8, "8 GR x1");
    bc_grain(G_GRAIN, 1, 16, "16 GR x1");
    bc_grain(G_GRAIN, 1, 32, "32 GR x1");
    bc_grain(G_GRAIN, 4, 8, "8 GR x4");
    bc_grain(G_GRAIN, 4, 16, "16 GR x4");
    bc_grain(G_GRAIN, 4, 32, "32 GR x4");
    bc_grain(G_GRAIN, 6, 8, "8 GR x6");
    bc_grain(G_GRAIN, 6, 16, "16 GR x6");
    bc_grain(G_GRAIN, 6, 32, "32 GR x6");
#endif
    /* CLOUDS (FM1 Phase 2, 15 Nov 2026): on a base of Braids with four notes held; the first case is the reference every
     * other is compared with ("own" = the difference), settings as CLC says */
    {
        static const char *const BCL_NAME[12] = {"GRAN 16ST", "GRAN 8ST", "GRAN 16MO", "GRAN 8MO", "LOOP 16ST", "LOOP 8ST",
                                            "LOOP 16MO", "LOOP 8MO", "SPEC 16ST", "SPEC 8ST", "SPEC 16MO", "SPEC 8MO"};
        int i, r4, v;
        r4 = bc_cl(G_CLOUDS, 4, CL_REF, "BRAIDS x4", -1);
        for (i = 0; i < 12; i++)                  /* CLC rows 1 to 12: GRANULR, LOOPDLY, SPECTRL x 16 ST, 8 ST, 16 MONO, 8 MONO */
            bc_cl(G_CLOUDS, 4, 1 + i, BCL_NAME[i], r4);
        bc_cl(G_CLOUDS, 4, 13, "GRAN D0.7", r4);
        bc_cl(G_CLOUDS, 4, 14, "GRAN D1.0", r4);
        bc_cl(G_CLOUDS, 4, 19, "GRAN FREEZE", r4);
        bc_cl(G_CLOUDS, 4, 20, "G.7 48K", r4);                /* update 8: the comparison cases (CLC rows 20 to 31) */
        bc_cl(G_CLOUDS, 4, 21, "G.9 48K", r4);
        bc_cl(G_CLOUDS, 4, 22, "G1.0 48K", r4);
        bc_cl(G_CLOUDS, 4, 23, "G.7 OLD", r4);
        bc_cl(G_CLOUDS, 4, 24, "G.9 OLD", r4);
        bc_cl(G_CLOUDS, 4, 25, "G1.0 OLD", r4);
        bc_cl(G_CLOUDS, 4, 26, "G1.0 NOCAP", r4);
        bc_cl(G_CLOUDS, 4, 29, "G1.0 CAP24", r4);
        bc_cl(G_CLOUDS, 4, 27, "8MO D1.0", r4);
        bc_cl(G_CLOUDS, 4, 28, "8MO NOCAP", r4);
        bc_cl(G_CLOUDS, 4, 30, "CL REV0", r4);
        bc_cl(G_CLOUDS, 4, 31, "CL REV0 NOSKIP", r4);
        k = bc_cl(G_CLOUDS, 4, CL_REVERB, "REVERB x4", r4);
        bc_cl(G_CLOUDS, 4, 17, "CL+REVERB", k);
        k = bc_cl(G_CLOUDS, 4, CL_DELAY, "DELAY x4", r4);
        bc_cl(G_CLOUDS, 4, 18, "CL+DELAY", k);
        {
            static const uint8_t NV[4] = {1, 2, 4, 6};
            static const char *const BN[4] = {"BRAIDS x1", "BRAIDS x2", "BRAIDS x4", "BRAIDS x6"};
            static const char *const CN[4] = {"CL 16ST x1", "CL 16ST x2", "CL 16ST x4", "CL 16ST x6"};
            for (v = 0; v < 4; v++) {
                int b = NV[v] == 4 ? r4 : bc_cl(G_CLOUDS, NV[v], CL_REF, BN[v], -1);
                bc_cl(G_CLOUDS, NV[v], 1, CN[v], b);
            }
        }
    }
    /* the factory presets the Lite firmware can play, a chord of as many notes as the preset has voices (one for
     * the mono presets) */
    for (i = P_PLAYED; i < NPRESETS; i++) {
        const bpreset_t *p = &PRESETS[i];
        bc_add(G_PRESET, i, p->mode ? 1 : p->voices > 6 ? 6 : p->voices, p->name);
    }
    /* the same on two cores (the second core renders the upper half of the voices): INIT with 2, 4 and 6 notes,
     * and every played preset with more than one voice, at its voices (12 Nov 2026: the Rings presets, at 2-4, had
     * no 2-core figure); CPU % is the first core's time, waiting for the second included */
    if (!bench_cpu2_run)                     /* one core only (it didn't start): no 2 CORES cases */
        return;
    bc_add(G_CORES2, P_INIT, 2, "INIT x2");
    bc_add(G_CORES2, P_INIT, 4, "INIT x4");
    bc_add(G_CORES2, P_INIT, 6, "INIT x6");
#ifdef RACK_WITH_GRAIN
    bc_grain(G_CORES2, 4, 16, "16 GR x4");
    bc_grain(G_CORES2, 4, 32, "32 GR x4");
    bc_grain(G_CORES2, 6, 8, "8 GR x6");
    bc_grain(G_CORES2, 6, 16, "16 GR x6");
    bc_grain(G_CORES2, 6, 32, "32 GR x6");
#endif
    /* CLOUDS (15 Nov 2026): the three heaviest of its one-core cases (by total CPU, the highest in each of four runs on a computer, which
     * is a rough guide only): GRAN D1.0, CL+REVERB and CL+DELAY, on two cores: the second core takes two of the four Braids voices;
     * Clouds itself runs once, on the first */
    bc_cl(G_CORES2, 4, 14, "GRAN D1.0", -1);
    bc_cl(G_CORES2, 4, 17, "CL+REVERB", -1);
    bc_cl(G_CORES2, 4, 18, "CL+DELAY", -1);
    for (i = P_PLAYED; i < NPRESETS; i++)
        if (!PRESETS[i].mode && PRESETS[i].voices >= 2)
            bc_add(G_CORES2, i, PRESETS[i].voices > 6 ? 6 : PRESETS[i].voices, PRESETS[i].name);
}

/* ------------------------------------------------------------------ running one --- */
#define WARM_FRAMES 4800
#define RUN_FRAMES 480
#define NRUN 5
#define WARM_STEPS (WARM_FRAMES / RUN_FRAMES)   /* the untimed 0.1 s, in steps of RUN_FRAMES (the firmware does one a frame) */
/* a CLOUDS case warms for 1 s (Clouds listens to the notes, its grains read what it has recorded: after 0.1 s a granular cloud is
 * still silent), the others for 0.1 s */
#define CL_WARM_STEPS 200                      /* 2 s (update 8: was 1 s; at 32 kHz the 8-bit mono buffer fills 1.5 x slower) */
static int bench_warm_steps(int k);

typedef struct {
    uint32_t best, worst;        /* ticks (PLAT_TICKS_PER_US a microsecond) for RUN_FRAMES */
    float peak;                  /* the loudest sample in the timed runs */
    uint8_t voices_fit, voices_on, dropped, bad;   /* fm1_status 2, 3, 4; bad: a NaN or infinity came out */
    uint32_t arena;              /* bytes of the arena used */
} bres_t;
static bres_t bres[NCASE_MAX];

static const float CHORD[6] = {48, 52, 55, 59, 62, 65};   /* C3 E3 G3 B3 D4 F4 */

#ifdef RACK_WITH_GRAIN
/* GRAIN's settings for the cases: SIZE 316 ms, DENSITY at most (1000 a second), POSITION 0.3 travelling slowly
 * forward, some JITTER, DETUNE and PAN RND, a little REVERSE; GRAINS as the case asks */
#define M_GRAIN 4
static void grain_setup(int grains)
{
    static const float v[13] = {0.75f, 1.0f, 0.3f, 0.55f, 0.3f, 0.2f, 0.6f, 0.8f, 0.5f, 0.5f, 0.0f, 0.0f, 0.1f};
    int p;
    rk_set_module(S_SRC, M_GRAIN);
    for (p = 0; p < 13; p++) {
        rk_set_meta(S_SRC, p, 0.0f, 1.0f, 1.0f);
        rk_set_param(S_SRC, p, v[p]);
    }
    rk_set_param(S_SRC, 10, (float)(grains - 1) / 31.0f);
}
#endif

/* a CLOUDS case's modules (bench_cases, CLC): FX 2 first (the settings of a preset's FX 1 module: INIT's, then the preset's own
 * changes to slot FX 1, put in FX 2), then Clouds in FX 1 with its 22 parameters */
static void run_ops_to_fx2(const bop_t *o, int n)
{
    for (; n > 0; n--, o++)
        if (o->a == S_FX1 && (o->fn == 1 || o->fn == 2 || o->fn == 3)) {
            if (o->fn == 1) rk_set_module(S_FX2, o->b);
            else if (o->fn == 2) rk_set_param(S_FX2, o->b, o->x);
            else rk_set_meta(S_FX2, o->b, o->x, o->y, o->z);
        }
}
static void cl_setup(const clc_t *r)
{
    int p;
    rk_clouds_opts(!r->no32, !r->noskip, r->cap == 0 ? 40 : r->cap == 1 ? 0 : r->cap);
    if (r->fx2) {
        const bpreset_t *fx = &PRESETS[r->fx2 == 1 ? P_CH_GLASS_PNO : P_LD_SQUARE_CHIP];
        run_ops_to_fx2(PRESETS[P_INIT].ops, PRESETS[P_INIT].nops);
        run_ops_to_fx2(fx->ops, fx->nops);
    }
    if (!r->on)
        return;
    rk_set_module(S_FX1, M_CLOUDS);
    for (p = 0; p < 22; p++) {
        int8_t kd = CL_KIND[p];
        float v = CL_VAL[p];
        if (kd > 0) rk_set_meta(S_FX1, p, 0.0f, (float)(kd - 1), (float)(kd - 1));
        else if (kd < 0) rk_set_meta(S_FX1, p, -1.0f, 1.0f, 2.0f);
        else rk_set_meta(S_FX1, p, 0.0f, 1.0f, 1.0f);
        if (p == 3) v = r->dens;   /* FREEZE (10) is set half way through the warm-up: bench_warm */
        else if (p == 9) v = (float)r->mode;
        else if (p == 10) v = 0.0f;
        else if (p == 11) v = (float)r->qual;
        else if (p == 8 && r->revz) v = 0.0f;       /* REVERB 0 (Clouds' own) */
        rk_set_param(S_FX1, p, v);
    }
}

static void bench_setup(int k)
{
    const bcase_t *c = &bcases[k];
    const bpreset_t *p = &PRESETS[c->preset];
    int i;
    rk_seed(0x21u);
    rk_clouds_opts(1, 1, 40);                  /* (update 8: the build's own Clouds settings, unless a case's row changes them) */
    rk_set_worker(c->group == G_CORES2 ? bench_cpu2_run : 0, bench_cpu2_wait);
    rk_init();
    run_ops(PRESETS[P_INIT].ops, PRESETS[P_INIT].nops);
    if (c->preset != P_INIT)
        run_ops(p->ops, p->nops);
#ifdef RACK_WITH_GRAIN
    if (c->grains)
        grain_setup(c->grains);
#endif
    if (c->cl)
        cl_setup(&CLC[c->cl - 1]);
    for (i = 0; i < 4; i++) {
        if (c->ch[i][0] == 1)
            rk_set_module(c->ch[i][1], (int)c->chv[i]);
        else if (c->ch[i][0] == 2)
            rk_set_param(c->ch[i][1], c->ch[i][2], c->chv[i]);
    }
    if (c->group == G_PRESET || c->group == G_CORES2 || c->group == G_CLOUDS)
        rk_fm1_fit();          /* as the firmware loads a preset (rc_load): buffers sized to it, the room to voices
                                * (until 14 Nov 2026 not here, so the delay + reverb presets fit one voice) */
    rk_set_voices(c->notes);
    for (i = 0; i < c->notes; i++)
        rk_note_on(CHORD[i], 0.8f);
    bres[k].best = 0xFFFFFFFFu;
    bres[k].worst = 0;
    bres[k].peak = 0.0f;
    bres[k].bad = 0;
}

/* one untimed step (n: 1 to the number of steps); FREEZE is switched on half way, once the cloud has something to hold */
static void bench_warm(int k, int n)
{
    rk_render(RUN_FRAMES);
    if (bcases[k].cl && CLC[bcases[k].cl - 1].freeze && n == CL_WARM_STEPS / 2)
        rk_set_param(S_FX1, 10, 1.0f);
}
static int bench_warm_steps(int k) { return bcases[k].cl ? CL_WARM_STEPS : WARM_STEPS; }

/* one timed run (ticks: the platform's microsecond clock times PLAT_TICKS_PER_US) */
static void bench_run(int k, uint32_t (*ticks)(void))
{
    uint32_t t0, t;
    const float *l, *r;
    int i;
    t0 = ticks();
    rk_render(RUN_FRAMES);
    t = ticks() - t0;
    if (t < bres[k].best)
        bres[k].best = t;
    if (t > bres[k].worst)
        bres[k].worst = t;
    l = rk_out_l();
    r = rk_out_r();
    for (i = 0; i < RUN_FRAMES; i++) {
        float a = l[i] < 0 ? -l[i] : l[i], b = r[i] < 0 ? -r[i] : r[i];
        if (!(a <= 1e9f) || !(b <= 1e9f))   /* NaN or infinity */
            bres[k].bad = 1;
        else {
            if (a > bres[k].peak)
                bres[k].peak = a;
            if (b > bres[k].peak)
                bres[k].peak = b;
        }
    }
}

static void bench_done(int k)
{
    rk_all_off();
    bres[k].voices_fit = (uint8_t)rk_fm1_status(2);
    bres[k].voices_on = (uint8_t)rk_fm1_status(3);
    bres[k].dropped = (uint8_t)rk_fm1_status(4);
    bres[k].arena = (uint32_t)rk_fm1_status(0);
}

/* CPU % of one core, in tenths: microseconds for 480 frames / 100; minus its comparison case if it has one */
static int32_t bench_pct10(int k, int own, uint32_t ticks_per_us)
{
    int32_t us = (int32_t)(bres[k].best / ticks_per_us);
    if (!own && bcases[k].minus >= 0)
        us -= (int32_t)(bres[bcases[k].minus].best / ticks_per_us);
    return us / 10;
}

/* ------------------------------------------------------------------ the FPU check --- */
/* single-precision arithmetic as the compiler emits it with -mfprev1 (hardware float): a fixed sum whose exact
 * IEEE result is known (bench_host prints it; the same on any IEEE single-precision machine without fused
 * multiply-add) */
#define FPU_EXPECT 0x416F0A3Du   /* 14.94 */
static volatile float fpu_seed = 1.0001f;
static uint32_t fpu_check(uint32_t (*ticks)(void), uint32_t *took)
{
    union { float f; uint32_t u; } v;
    float x = 1.0f, m = fpu_seed;
    uint32_t t0 = ticks();
    int i;
    for (i = 1; i <= 10000; i++)
        x = x * m + 0.5f / (float)i;
    *took = ticks() - t0;
    v.f = x;
    return v.u;
}
