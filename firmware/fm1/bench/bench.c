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
void rk_set_macro_glide(int i, float v);
void rk_set_macro_target(int k, int idx, int slot, int param, int depth, int start, int end, int curve);
void rk_modsum_precompute(int on);   /* 23 Nov 2026: the engine's mod_sum sums ahead (on) or scans as 2.32 did (off): the MODSUM cases' A/B (RACK_BENCH_AB) */
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
int rk_list_map(int module, int i);        /* update 61: a Braids shape (2) or Rings model (10) as it plays */
void rk_fm1_fit(void);
void rk_set_worker(int (*run)(void (*)(uint32_t), uint32_t), int (*wait)(void));
uint32_t rk_worker_blocks(void);
void rk_set_worker_ex(void (*sync)(void), int (*on)(void));   /* update 9: the platform's barrier and "taking jobs" calls */
void rk_set_ticks(uint32_t (*ticks)(void));
void rk_fx_core2(int on);                  /* 0: Clouds on the first core, the voices split (update 8's arrangement); the benchmark's comparison cases */
void rk_fx_core2_voices(int on);           /* update 10: 0 keeps the voices above the fourth on the first core (update 9's arrangement); the comparison cases */
int rk_fx_core2_active(void);              /* 1: Clouds runs on the second core in this patch */
void rk_core_stats(uint32_t *job, uint32_t *wait, uint32_t *inl);
void rk_core_stats_reset(void);
void rk_drum_hit(int voice, int model, float note, float p1, float p2, float decay, float level, float pan, float send);   /* FM1 Phase 2 step 1: the drum voices */
void rk_drum_run(int on);
void rk_drum_busy(int mode);
void rk_drum_off(void);
int rk_drum_status(int what);
/* the second core, when the platform has it (app.c: plat_cpu2_*; bench_host.c: a worker that runs the job at once):
 * the 2 CORES cases hand it half the voices; every other case runs on one core */
static int (*bench_cpu2_run)(void (*)(uint32_t), uint32_t);
static int (*bench_cpu2_wait)(void);
static void (*bench_cpu2_sync)(void);       /* update 9: plat_cpu2_sync and plat_cpu2_on (the engine's Clouds job on the second core) */
static int (*bench_cpu2_on)(void);

typedef struct { float x; int16_t y, z, w; uint8_t fn, a, b, c, d; } bop_t;   /* 16 bytes (update 59; was 24 with four floats): y z w are whole numbers in every preset */
typedef struct { const char *name; const bop_t *ops; uint16_t nops; uint8_t voices, mode; } bpreset_t;
#include "cases.h"
#include "modsum_cases.h"   /* the MODSUM group's three presets, whole, with the knob layer */

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
        case 12: rk_set_macro_target(o->a, o->b, (int8_t)o->c, o->d, (int)o->x, (int)o->y, (int)o->z, (int)o->w); break;   /* the knob layer (modsum_cases.h) */
        }
}

/* ------------------------------------------------------------------ the cases --- */
/* GRAIN is shelved (FM1 Phase 2 step 4): its group, cases and setup exist only with -DRACK_WITH_GRAIN (the engine's flag) */
enum { G_CORE, G_BRAIDS, G_RINGS, G_SHP, G_MOD, G_FX, G_VOICES, G_CLOUDS, G_PRESET,
#ifdef RACK_WITH_GRAIN
       G_GRAIN,
#endif
       G_DRUMS,
       G_MODSUM,
       G_CORES2, NGROUP };
static const char *const GROUP[NGROUP] = {"CORE", "BRAIDS", "RINGS", "FILTER", "MOD", "FX", "VOICES",
                                           "CLOUDS", "PRESETS",
#ifdef RACK_WITH_GRAIN
                                           "GRAIN",
#endif
                                           "DRUMS", "MODSUM", "2 CORES"};
static const char *const BRAIDS_MODEL[47] = {"CSAW", "/\\-_", "//-_", "FOLD", "UUUU", "SUB-", "SUB/", "SYN-",
    "SYN/", "//x3", "-_x3", "/\\x3", "SIx3", "RING", "////", "//uu", "TOY*", "ZLPF", "ZPKF", "ZBPF", "ZHPF", "VOSM",
    "VOWL", "VFOF", "HARM", "FM", "FBFM", "WTFM", "PLUK", "BOWD", "BLOW", "FLUT", "BELL", "DRUM", "KICK", "CYMB",
    "SNAR", "WTBL", "WMAP", "WLIN", "WTx4", "NOIS", "TWLN", "CLKN", "CLOU", "PRTC", "QPSK"};
static const char *const RINGS_MODEL[6] = {"MODAL", "SYMPATH", "STRING", "FM VOICE", "SYM QNT", "STR+VERB"};

/* a case: the preset, up to four changes (fn 1 set_module, 2 set_param; 0 none), notes held, and the case it is
 * compared with (the module's own cost is the difference; -1 none) */
/* grains: a GRAIN case (FM1 Phase 2): the granular source with at most this many grains a voice (0: not one)
 * cl: a CLOUDS case (FM1 Phase 2): 1 + its row in CLC (the settings), 0 none */
/* c2off: a 2 CORES case with the Clouds-on-the-second-core arrangement switched off at run time (rk_fx_core2(0)): update 8's, the voices split, Clouds on the first core */
/* v1 (update 10): a 2 CORES case with the voices above the fourth kept on the first core (rk_fx_core2_voices(0)): update 9's arrangement, for comparison */
/* kit, dv, ease (step 1 of the drum track): a DRUMS case: the kit struck every step (1 cheap, 2 rich), its voices (3 or 4), IF BUSY = CLOUD on (the guard: Clouds' DENSITY held at 0.9) */
typedef struct { uint8_t group, preset, notes, grains, cl, c2off, v1, kit, dv, ease, ab, mov; int8_t minus; uint8_t ch[4][3]; float chv[4]; const char *name; } bcase_t;

#define NCASE_MAX 320
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
    c->c2off = 0;
    c->v1 = 0;
    c->kit = c->dv = c->ease = 0;
    c->ab = c->mov = 0;
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
    /* update 59 (21 Nov 2026): the grain ceiling 24 (2.35's, RACK_CLOUDS_MAX_GRAINS=24) at 32 kHz and at 48 kHz (RACK_CLOUDS_32K off: no resampler), to see which fits the
     * two cores' worst case. Rows 13, 1 and 14 are GRANULR 16 ST at DENSITY 0.7, 0.9, 1.0 with the 2.34 build's settings (32 kHz, ceiling 40, which at 16 ST changes
     * nothing: 32 grains); row 29 is 1.0 at 32 kHz with 24; these complete the set: 32 kHz with 24 at 0.7 and 0.9, and 48 kHz with 24 at 0.7, 0.9 and 1.0 */
    /* 32 */ {1, CM_GRAN, CQ_16ST, 0, 0, 0.7f, 0, 0, 24, 0},  /* 32 kHz, ceiling 24 */
    /* 33 */ {1, CM_GRAN, CQ_16ST, 0, 0, 0.9f, 0, 0, 24, 0},
    /* 34 */ {1, CM_GRAN, CQ_16ST, 0, 0, 0.7f, 1, 0, 24, 0},  /* 48 kHz, ceiling 24 */
    /* 35 */ {1, CM_GRAN, CQ_16ST, 0, 0, 0.9f, 1, 0, 24, 0},
    /* 36 */ {1, CM_GRAN, CQ_16ST, 0, 0, 1.0f, 1, 0, 24, 0},
};
#define CL_REF 0
#define CL_ON(c) ((c)->cl && CLC[(c)->cl - 1].on)       /* the case has Clouds in FX 1 (cl is its CLC row + 1: row 0 is Braids alone) */
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

/* the DRUMS cases (see bench_cases): kit 1 CHEAP, 2 RICH; 3 or 4 voices; refs = the four cases they are measured against (BRAIDS x4 and GRAN at DENSITY 0.7, 0.9, 1.0) */
static void bench_drum_cases(int group, int ref0, int ref7, int ref9, int ref10)
{
    static const char *const NAME[2][2][4] = {
        {{"CHEAP x3", "CHEAP x3 D.7", "CHEAP x3 D.9", "CHEAP x3 D1.0"}, {"CHEAP x4", "CHEAP x4 D.7", "CHEAP x4 D.9", "CHEAP x4 D1.0"}},
        {{"RICH x3", "RICH x3 D.7", "RICH x3 D.9", "RICH x3 D1.0"}, {"RICH x4", "RICH x4 D.7", "RICH x4 D.9", "RICH x4 D1.0"}}};
    static const char *const EASED[2] = {"CHEAP x4 D1.0 EASED", "RICH x4 D1.0 EASED"};
    static const uint8_t ROW[4] = {0, 13, 1, 14};   /* CLC rows: no Clouds, GRANULR 16 ST at DENSITY 0.7, 0.9, 1.0 */
    int refs[4], kit, nv, c, k;
    refs[0] = ref0; refs[1] = ref7; refs[2] = ref9; refs[3] = ref10;
    for (kit = 0; kit < 2; kit++)
        for (nv = 0; nv < 2; nv++)
            for (c = 0; c < 4; c++) {
                k = bc_cl(group, 4, ROW[c], NAME[kit][nv][c], refs[c]);
                bcases[k].kit = (uint8_t)(kit + 1);
                bcases[k].dv = (uint8_t)(3 + nv);
            }
    for (kit = 0; kit < 2; kit++) {
        k = bc_cl(group, 4, 14, EASED[kit], refs[3]);
        bcases[k].kit = (uint8_t)(kit + 1);
        bcases[k].dv = 4;
        bcases[k].ease = 1;
    }
}

static void bench_cases(void)
{
    int i, k, base;
    int r4s = -1, d7s = -1, d9s = -1, d10s = -1, g10c = -1, g10c48 = -1;      /* the CLOUDS group's BRAIDS x4, GRAN D0.7, GRAN 16ST (D0.9), GRAN D1.0: the DRUMS cases are measured against them */
    bncases = 0;
    /* the engine with no source: envelopes, matrix, LFOs, master, for one voice */
    k = bc_add(G_CORE, P_INIT, 1, "NO SOURCE");
    bc_change(k, 0, 1, S_SRC, 0, 0);
    /* Braids, each shape, one voice (FOLDED BASS with its filter taken out) */
    for (i = 0; i < 47; i++) {
        if (rk_list_map(2, i) != i) continue;      /* update 61: the withdrawn shapes (FBFM, PLUK, BOWD, BLOW, FLUT, the nine drum shapes) play as the nearest kept: 33 cases */
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
        if (rk_list_map(10, i) != i) continue;     /* update 61: MODAL, SYMPATH and SYM QNT are withdrawn (they played STRING): 3 cases */
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
        r4s = r4;
        for (i = 0; i < 12; i++)                  /* CLC rows 1 to 12: GRANULR, LOOPDLY, SPECTRL x 16 ST, 8 ST, 16 MONO, 8 MONO */
            { int q = bc_cl(G_CLOUDS, 4, 1 + i, BCL_NAME[i], r4); if (i == 0) d9s = q; }
        d7s = bc_cl(G_CLOUDS, 4, 13, "GRAN D0.7", r4);
        d10s = bc_cl(G_CLOUDS, 4, 14, "GRAN D1.0", r4);
        bc_cl(G_CLOUDS, 4, 19, "GRAN FREEZE", r4);
        bc_cl(G_CLOUDS, 4, 20, "G.7 48K", r4);                /* update 8: the comparison cases (CLC rows 20 to 31) */
        bc_cl(G_CLOUDS, 4, 21, "G.9 48K", r4);
        bc_cl(G_CLOUDS, 4, 22, "G1.0 48K", r4);
        bc_cl(G_CLOUDS, 4, 23, "G.7 OLD", r4);
        bc_cl(G_CLOUDS, 4, 24, "G.9 OLD", r4);
        bc_cl(G_CLOUDS, 4, 25, "G1.0 OLD", r4);
        bc_cl(G_CLOUDS, 4, 26, "G1.0 NOCAP", r4);
        g10c = bc_cl(G_CLOUDS, 4, 29, "G1.0 CAP24", r4);
        bc_cl(G_CLOUDS, 4, 32, "G.7 CAP24", r4);               /* update 59: the ceiling 24 at 32 kHz (G1.0 CAP24 above), and at 48 kHz */
        bc_cl(G_CLOUDS, 4, 33, "G.9 CAP24", r4);
        bc_cl(G_CLOUDS, 4, 34, "G.7 48K CAP24", r4);
        bc_cl(G_CLOUDS, 4, 35, "G.9 48K CAP24", r4);
        g10c48 = bc_cl(G_CLOUDS, 4, 36, "G1.0 48K CAP24", r4);
        bc_cl(G_CLOUDS, 4, 27, "8MO D1.0", r4);
        bc_cl(G_CLOUDS, 4, 28, "8MO NOCAP", r4);
        bc_cl(G_CLOUDS, 4, 30, "CL REV0", r4);
        bc_cl(G_CLOUDS, 4, 31, "CL REV0 NOSKIP", r4);
        k = bc_cl(G_CLOUDS, 4, CL_REVERB, "REVERB x4", r4);
        bc_cl(G_CLOUDS, 4, 17, "CL+REVERB", k);
        k = bc_cl(G_CLOUDS, 4, CL_DELAY, "DELAY x4", r4);
        bc_cl(G_CLOUDS, 4, 18, "CL+DELAY", k);
        bc_cl(G_CLOUDS, 6, 17, "CL+REVERB x6", -1);           /* update 9: the one-core twin of the 2 CORES case "CL+REV x6" */
        bc_cl(G_CLOUDS, 6, 1, "GRAN D0.9 x6", -1);             /* update 10: the one-core twins of the 2 CORES cases "GRAN D0.9 x6" and "CL+DELAY x6" */
        bc_cl(G_CLOUDS, 6, 18, "CL+DELAY x6", -1);
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
    /* DRUMS (FM1 Phase 2 step 1, 21 Nov 2026): the drum voices (engine/rack.cc "DRUMS") on the same base as CLOUDS: the firmware's P2 INIT with four notes
     * held, Clouds in FX 1 (GRANULR 16 ST, the CLOUDS group's settings) at DENSITY 0.7 / 0.9 / 1.0 or not at all. Every step (10 ms) every voice of the kit is struck, so
     * all of them sound all the time: the worst case, not the average (rk_drum_hit; the sequencer is not built yet). Four kits: CHEAP (KICK, SNARE, a CLOCKED NOISE hat, and a
     * FILTERED NOISE clap) and RICH (KICK, SNARE, the CYMBAL hat, a STRUCK DRUM perc), each with 3 voices (kick, snare, hat) or 4. "alone" = no Clouds: own = minus BRAIDS x4;
     * "D0.7" etc: own = minus the same Clouds case without drums (GRAN D0.7, GRAN 16ST, GRAN D1.0). IF BUSY is NONE in them (the engine's guard, which holds Clouds'
     * DENSITY at 0.9 while the drums run, is off), so D1.0 is a real 1.0; the cases ending "EASED" have it on (CLOUD: D1.0 becomes 0.9) at 4 voices. The same on two cores below ("2 CORES"). */
    bench_drum_cases(G_DRUMS, r4s, d7s, d9s, d10s);
    /* update 59: one-core twins of the two-core cases with the ceiling 24 (the "2 CORES" cases of the same names; the host check compares them) */
    {
        static const struct { const char *c, *r; uint8_t row; } V[2] = {{"CHEAP x4 D1.0 CAP24", "RICH x4 D1.0 CAP24", 29}, {"CHEAP x4 D1.0 48K CAP24", "RICH x4 D1.0 48K CAP24", 36}};
        int v;
        for (v = 0; v < 2; v++) {
            int ref = v ? g10c48 : g10c;                       /* the CLOUDS group's two rows: the drums' reference */
            k = bc_cl(G_DRUMS, 4, V[v].row, V[v].c, ref); bcases[k].kit = 1; bcases[k].dv = 4;
            k = bc_cl(G_DRUMS, 4, V[v].row, V[v].r, ref); bcases[k].kit = 2; bcases[k].dv = 4;
        }
    }
    /* MODSUM (23 Nov 2026): the engine's mod_sum precompute against the scan it replaced, on one core. Three whole factory presets (ROTARY ORGAN: the most LFO
     * routes, TIDE PAD: the heaviest, SHIMMER PAD) with four notes held, each with the knobs still ("still") and all four moving ("moving": a glide every
     * step, so the knob layer's sums are rebuilt each block), each twice: "NEW" (rk_modsum_precompute(1): the firmware as it is) and "OLD" (0: every call scans the 18 rows and
     * the knob targets, as 2.32 did). The output is the same either way (bench_host checks it). OLD's own figure = OLD minus NEW = what the precompute saves. */
    {
        static const char *const MSN[NMSPRESETS][2][2] = {
            {{"ROTARY ORGAN still NEW", "ROTARY ORGAN still OLD"}, {"ROTARY ORGAN moving NEW", "ROTARY ORGAN moving OLD"}},
            {{"TIDE PAD still NEW", "TIDE PAD still OLD"}, {"TIDE PAD moving NEW", "TIDE PAD moving OLD"}},
            {{"SHIMMER PAD still NEW", "SHIMMER PAD still OLD"}, {"SHIMMER PAD moving NEW", "SHIMMER PAD moving OLD"}}};
        int m, mv;
        for (m = 0; m < NMSPRESETS; m++)
            for (mv = 0; mv < 2; mv++) {
                int kn = bc_add(G_MODSUM, m, 4, MSN[m][mv][0]), ko = bc_add(G_MODSUM, m, 4, MSN[m][mv][1]);
                bcases[kn].ab = 1; bcases[ko].ab = 2;
                bcases[kn].mov = bcases[ko].mov = (uint8_t)mv;
                bcases[ko].minus = (int8_t)kn;
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
    {
    int r2c[4] = {-1, -1, -1, -1};
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
    r2c[0] = bc_cl(G_CORES2, 4, CL_REF, "BRAIDS x4", -1);   /* (step 1, drums: the DRUMS cases on two cores are measured against these four) */
    r2c[3] = bc_cl(G_CORES2, 4, 14, "GRAN D1.0", -1);
    bc_cl(G_CORES2, 4, 17, "CL+REVERB", -1);
    bc_cl(G_CORES2, 4, 18, "CL+DELAY", -1);
    /* update 9 (16 Nov 2026): Clouds on the second core (RACK_FX_CORE2): every voice and the other effect on the first, Clouds alone on the second, a block
     * behind (engine/rack.cc cl_pipe_). The three cases above now run that way. Each case below is reported as three lines: the block (what the CPU %
     * strip shows: the slower core, as the block takes as long as it), and its two cores' own loads "C1" (the first core's work: the block less the time it
     * waited for the second) and "C2" (the second core's job), each as % of the block. The second set is the same cases with the arrangement
     * switched off at run time ("OFF": the voices split, Clouds on the first core, as update 8), for comparison. */
    r2c[1] = bc_cl(G_CORES2, 4, 13, "GRAN D0.7", -1);
    r2c[2] = bc_cl(G_CORES2, 4, 1, "GRAN D0.9", -1);
    bc_cl(G_CORES2, 6, 17, "CL+REV x6", -1);
    bc_cl(G_CORES2, 4, 5, "LOOPDLY", -1);
    bc_cl(G_CORES2, 4, 9, "SPECTRL", -1);
    {
        static const char *const OFFN[8] = {"GRAN D0.7 OFF", "GRAN D0.9 OFF", "GRAN D1.0 OFF", "CL+DELAY OFF", "CL+REVERB OFF", "CL+REV x6 OFF", "LOOPDLY OFF", "SPECTRL OFF"};
        static const uint8_t ROW[8] = {13, 1, 14, 18, 17, 17, 5, 9}, NOTES[8] = {4, 4, 4, 4, 4, 6, 4, 4};
        for (i = 0; i < 8; i++)
            bcases[bc_cl(G_CORES2, NOTES[i], ROW[i], OFFN[i], -1)].c2off = 1;
    }
    /* update 10: six voices, the fifth and sixth in the second core's job (with Clouds loaded and no effect before it): GRANULR DENSITY 0.9 and Clouds + delay;
     * and the same two with the reverb case (CL+REV x6 above) once more with the two voices kept on the first core ("V1": update 9's arrangement) */
    bc_cl(G_CORES2, 6, 1, "GRAN D0.9 x6", -1);
    bc_cl(G_CORES2, 6, 18, "CL+DELAY x6", -1);
    bcases[bc_cl(G_CORES2, 6, 1, "GRAN D0.9 x6 V1", -1)].v1 = 1;
    bcases[bc_cl(G_CORES2, 6, 17, "CL+REV x6 V1", -1)].v1 = 1;
    bench_drum_cases(G_CORES2, r2c[0], r2c[1], r2c[2], r2c[3]);   /* step 1, drums: the same cases on two cores (the second core renders the drums) */
    /* update 59: the worst case on two cores (D1.0, four voices, no IF BUSY easing) with the ceiling 24 at 32 kHz ("CAP24") and at 48 kHz ("48K CAP24"): the Clouds case
     * alone (the reference of the two below it), then CHEAP x4 and RICH x4 with it. Compare "CHEAP x4 D1.0" and "RICH x4 D1.0" (32 kHz, no ceiling at 16 ST: 2.34's) */
    {
        static const struct { const char *g, *c, *r; uint8_t row; } V[2] = {
            {"GRAN D1.0 CAP24", "CHEAP x4 D1.0 CAP24", "RICH x4 D1.0 CAP24", 29},
            {"GRAN D1.0 48K CAP24", "CHEAP x4 D1.0 48K CAP24", "RICH x4 D1.0 48K CAP24", 36}};
        int v, g;
        for (v = 0; v < 2; v++) {
            g = bc_cl(G_CORES2, 4, V[v].row, V[v].g, -1);
            k = bc_cl(G_CORES2, 4, V[v].row, V[v].c, g); bcases[k].kit = 1; bcases[k].dv = 4;
            k = bc_cl(G_CORES2, 4, V[v].row, V[v].r, g); bcases[k].kit = 2; bcases[k].dv = 4;
        }
    }
    for (i = P_PLAYED; i < NPRESETS; i++)
        if (!PRESETS[i].mode && PRESETS[i].voices >= 2)
            bc_add(G_CORES2, i, PRESETS[i].voices > 6 ? 6 : PRESETS[i].voices, PRESETS[i].name);
    }
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
    uint8_t drum_on;             /* step 1, drums: voices still sounding when the case ended (a DRUMS case: all of its kit) */
    uint32_t c1best, c1worst, c2best, c2worst;   /* update 9, the 2 CORES cases: the first core's work and the second's job, ticks for RUN_FRAMES (lowest and highest run) */
} bres_t;
static bres_t bres[NCASE_MAX];
static int bench_knob_step;                    /* MODSUM "moving" cases: the step the knobs are at */

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
    rk_clouds_opts(!r->no32, !r->noskip, r->cap == 0 ? 40 : r->cap == 1 ? 0 : r->cap);   /* cap 0: 2.34's ceiling, 40 (at 16 ST it changes nothing: 32 grains), so the older rows stay comparable; the firmware itself is 24 from 2.35 */
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
    const bpreset_t *p = c->group == G_MODSUM ? &MSPRESETS[c->preset] : &PRESETS[c->preset];
    int i;
    rk_seed(0x21u);
    rk_clouds_opts(1, 1, 40);                  /* (update 8: the build's own Clouds settings, unless a case's row changes them) */
    rk_set_worker(c->group == G_CORES2 ? bench_cpu2_run : 0, bench_cpu2_wait);
    rk_set_worker_ex(c->group == G_CORES2 ? bench_cpu2_sync : 0, c->group == G_CORES2 ? bench_cpu2_on : 0);
    rk_fx_core2(!c->c2off);
    rk_fx_core2_voices(!c->v1);
    rk_drum_run(0);                            /* (step 1, drums: nothing carried over from the case before) */
    rk_drum_busy(c->ease ? 1 : 0);             /* IF BUSY: NONE, except in the EASED cases */
    rk_init();
    run_ops(PRESETS[P_INIT].ops, PRESETS[P_INIT].nops);
    if (c->preset != P_INIT || c->group == G_MODSUM)
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
    if (c->group == G_PRESET || c->group == G_CORES2 || c->group == G_CLOUDS || c->group == G_DRUMS || c->group == G_MODSUM)
        rk_fm1_fit();          /* as the firmware loads a preset (rc_load): buffers sized to it, the room to voices
                                * (until 14 Nov 2026 not here, so the delay + reverb presets fit one voice) */
    rk_set_voices(c->notes);
    for (i = 0; i < c->notes; i++)
        rk_note_on(CHORD[i], 0.8f);
    rk_drum_run(c->kit != 0);
    rk_modsum_precompute(c->ab != 2);          /* (MODSUM: OLD scans; every other case runs the engine as it is) */
    bench_knob_step = 0;
    bres[k].best = 0xFFFFFFFFu;
    bres[k].worst = 0;
    bres[k].c1best = bres[k].c2best = 0xFFFFFFFFu;
    bres[k].c1worst = bres[k].c2worst = 0;
    bres[k].peak = 0.0f;
    bres[k].bad = 0;
}

/* a DRUMS case: every voice of the kit struck (a decay of 0.3 s: they never go idle between steps), before the step's render. The kit: voice 0 KICK, 1 SNARE, 2 the hat
 * (CHEAP: CLOCKED NOISE, RICH: CYMBAL), 3 the clap or perc (CHEAP: FILTERED NOISE, RICH: STRUCK DRUM). The engine's models: 0 KICK, 1 SNARE, 2 CYMBAL, 3 STRUCK DRUM, 5 FILTERED NOISE, 6 CLOCKED NOISE */
/* a MODSUM "moving" case: the four knobs each travel up and down (a triangle, a different period each), their targets set before every step's render; the glide
 * (rk_set_macro_glide) moves each one a part of the way every engine block, so the knob layer's offsets change in every block */
static void bench_knob(int k)
{
    int j;
    if (!bcases[k].mov)
        return;
    for (j = 0; j < 4; j++) {
        int per = 20 + 7 * j, ph = (bench_knob_step + 5 * j) % per;
        float t = (float)(ph < per / 2 ? ph : per - ph) / (float)(per / 2);
        rk_set_macro_glide(j, t);
    }
    bench_knob_step++;
}
static void bench_hit(int k)
{
    bench_knob(k);
    static const uint8_t MODEL[2][4] = {{0, 1, 6, 5}, {0, 1, 2, 3}};
    static const float NOTE[4] = {36.0f, 50.0f, 70.0f, 62.0f};
    const bcase_t *c = &bcases[k];
    int v;
    if (!c->kit)
        return;
    for (v = 0; v < c->dv; v++)
        rk_drum_hit(v, MODEL[c->kit - 1][v], NOTE[v], 0.5f, 0.5f, 0.3f, 0.8f, 0.0f, 0.3f);
}

/* one untimed step (n: 1 to the number of steps); FREEZE is switched on half way, once the cloud has something to hold */
static void bench_warm(int k, int n)
{
    bench_hit(k);
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
    bench_hit(k);
    rk_set_ticks(ticks);
    rk_core_stats_reset();
    t0 = ticks();
    rk_render(RUN_FRAMES);
    t = ticks() - t0;
    if (t < bres[k].best)
        bres[k].best = t;
    if (t > bres[k].worst)
        bres[k].worst = t;
    if (bcases[k].group == G_CORES2) {   /* the cores' own loads: the first core's work is the block less what it spent waiting for the second */
        uint32_t job, wait, inl, c1;
        rk_core_stats(&job, &wait, &inl);
        c1 = t > wait ? t - wait : 0;
        if (c1 < bres[k].c1best) bres[k].c1best = c1;
        if (c1 > bres[k].c1worst) bres[k].c1worst = c1;
        if (job < bres[k].c2best) bres[k].c2best = job;
        if (job > bres[k].c2worst) bres[k].c2worst = job;
    }
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
    bres[k].drum_on = (uint8_t)rk_drum_status(0);
    rk_all_off();
    rk_drum_off();
    rk_drum_run(0);
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

/* the 2 CORES cases' own loads, in tenths of a percent of the block: core = 1 the first core's work, 2 the second core's job */
static int32_t bench_core_pct10(int k, int core, uint32_t ticks_per_us)
{
    uint32_t t = core == 1 ? bres[k].c1best : bres[k].c2best;
    return (int32_t)(t / ticks_per_us) / 10;
}
static int32_t bench_core_worst_us(int k, int core, uint32_t ticks_per_us) { return (int32_t)((core == 1 ? bres[k].c1worst : bres[k].c2worst) / ticks_per_us); }

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
