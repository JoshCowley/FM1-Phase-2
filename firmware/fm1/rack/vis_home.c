/* SPDX-License-Identifier: MPL-2.0 */
/* FM1 Phase 2 firmware: update 40, HOME as the signal path again (part of app.c, included before draw_home).
 *
 * The look of the FM1 Rack (MODFM1) HOME before Phase 2's update 13 (git 6f949b9^, app.c home_tiles / home_macros): Teenage Engineering's mixer
 * drawn as the sound's path, finer lines with a touch of halo, each knob's colour on the element it moves. What is drawn, top to bottom:
 *   - the mods row: a tile for each modulation source in use (the loaded modulator slots: PEAKS, TIDES, GRIDS or FUNC; the LFOs, MSEQ, the GATE
 *     PATTERN and the knob layer when something is routed from them; JAM while it is on) and the DRUMS while the pattern plays; a line runs from
 *     each down to every stage it moves, with a pulse running along it as brightly as the source is moving (the drums join the path between the
 *     last stage before the effects and the first effect: their wet part goes through the effects);
 *   - the path: the stages in signal order, only those in use, closed up and centred (the source, Rings when it is the shaper, the filter (always),
 *     the effect slots as they sit, Clouds by its mode), each a tile with its icon (home_icons.h) and name; dots run along the path at the output's level;
 *   - the scope of the output, tinted by the colours of the routes that are moving, white when loud, with the reverb's halo, the delay's echo and
 *     the chorus's second line;
 *   - the four macros as sliders (home_readouts), in their knobs' colours, with the movement a via source gives them.
 * The tiles and lines are drawn in the picture family's 40-row bands (vis_clouds.c: only the changed rows are sent), the stage list and the
 * levels are made once a frame (home_frame). The spectral ribbon of update 14 is gone; its band spectrum (hm_spectrum) and hue ramps are
 * kept below, the other pictures use them. */

static const uint8_t HR_RAMP[5][5][3] = {
    {{4, 16, 26}, {10, 70, 100}, {40, 168, 190}, {120, 224, 228}, {222, 250, 248}},       /* teal: with Clouds */
    {{24, 10, 4}, {100, 46, 16}, {212, 118, 38}, {250, 184, 96}, {255, 238, 200}},        /* amber: without */
    {{10, 16, 30}, {60, 100, 160}, {160, 200, 236}, {226, 240, 252}, {255, 255, 255}},    /* ice: held */
    {{6, 8, 26}, {28, 44, 120}, {80, 112, 222}, {160, 184, 255}, {230, 236, 255}},         /* indigo: modulators (update 15) */
    {{16, 6, 24}, {80, 36, 120}, {170, 90, 210}, {226, 160, 244}, {252, 226, 255}},        /* violet: rhythm and structure */
};
static const uint32_t HR_ACC[5] = {0xFFBE5Au, 0x6EEEFFu, 0xFFFFFFu, 0xFFB850u, 0xFFE070u};

/* update 40: a picture is drawn in its page's colour (theme.h K_C_*, the colour of the icon on HOME): hx_set makes a fifth ramp from it (dark, deep, the colour, a lighter
 * tint, near white) that hr_ramp and hr_acc give in place of the family's ramps (the ice of a held Clouds keeps its own); hx_set(-1): the family's ramps */
static uint8_t HRX[5][3];
static uint32_t HXACC;
static int hx_on;
static void hx_set(int sc)
{
    uint16_t c = 0;
    uint32_t r, g, b, k;
    static const uint8_t TW[5] = {13, 72, 184, 90, 205};   /* the stops' weights of the colour (of 256), then white for the last two */
    switch (sc) {
    case SC_CLOUDS: c = K_C_CLOUDS; break;
    case SC_BRAIDS: c = K_C_BRAIDS; break;
    case SC_RINGS: c = K_C_RINGS; break;
    case SC_FILTER: c = K_C_FILTER; break;
    case SC_TIDES: c = K_C_TIDES; break;
    case SC_PEAKS: c = K_C_PEAKS; break;
    case SC_GRIDS: c = K_C_GRIDS; break;
    case SC_FUNC: c = K_C_FUNC; break;
    case SC_DELAY: c = K_C_DELAY; break;
    case SC_REVERB: c = K_C_REVERB; break;
    case SC_CHORUS: c = K_C_CHORUS; break;
    case SC_LFO: c = K_C_LFO; break;
    default: break;
    }
    hx_on = c != 0;
    if (!c)
        return;
    r = (uint32_t)(c >> 11) * 255u / 31u; g = (uint32_t)((c >> 5) & 63u) * 255u / 63u; b = (uint32_t)(c & 31u) * 255u / 31u;
    for (k = 0; k < 5; k++) {
        uint32_t w = k < 3 ? TW[k] : 256u;           /* stops 3, 4: the colour toward white by TW */
        uint32_t t = k < 3 ? 0u : TW[k] ;
        HRX[k][0] = (uint8_t)(k < 3 ? r * w >> 8 : r + ((255u - r) * t >> 8));
        HRX[k][1] = (uint8_t)(k < 3 ? g * w >> 8 : g + ((255u - g) * t >> 8));
        HRX[k][2] = (uint8_t)(k < 3 ? b * w >> 8 : b + ((255u - b) * t >> 8));
    }
    HXACC = (r + ((255u - r) * 110u >> 8)) << 16 | (g + ((255u - g) * 110u >> 8)) << 8 | (b + ((255u - b) * 110u >> 8));
}
static uint32_t hr_acc(int pal) { return hx_on && pal != 2 ? HXACC : HR_ACC[pal]; }
static uint32_t hr_ramp(int pal, int tv)
{
    static const uint8_t T[5] = {0, 64, 140, 204, 255};
    static const uint16_t IV[4] = {1024, 862, 1024, 1285};   /* 65536 / the stop spacing >> 4 ... (spacing 64, 76, 64, 51 -> 256 * 256 / spacing) */
    const uint8_t (*s)[3] = hx_on && pal != 2 ? HRX : HR_RAMP[pal];
    int i;
    uint32_t t;
    tv = tv < 0 ? 0 : tv > 255 ? 255 : tv;
    i = tv < 64 ? 0 : tv < 140 ? 1 : tv < 204 ? 2 : 3;
    t = (uint32_t)(tv - T[i]) * IV[i] >> 8;           /* 0-256 */
    return (((uint32_t)s[i][0] * (256u - t) + (uint32_t)s[i + 1][0] * t) >> 8) << 16 | (((uint32_t)s[i][1] * (256u - t) + (uint32_t)s[i + 1][1] * t) >> 8) << 8 | (((uint32_t)s[i][2] * (256u - t) + (uint32_t)s[i + 1][2] * t) >> 8);
}
static uint32_t hr_scale(uint32_t c, uint32_t k)    /* c x k / 256 */
{
    return ((c >> 16) * k >> 8) << 16 | (((c >> 8) & 255u) * k >> 8) << 8 | ((c & 255u) * k >> 8);
}


/* ------------------------------------------------------------------------------------------------ the spectrum
 * From the output scope (rc_scope: the last 1024 samples of the output, mono, at the codec's 44.1 kHz = 23 ms, refilled continuously). 23 ms resolves
 * 43 Hz at best, a Hann window about twice that, so the bands start at 170 Hz and the lowest two are blurred together a little; nothing below 170 Hz is shown.
 * Bands 0-4 (170 Hz to 1.9 kHz): the 1024 samples summed 8 at a time (128 at 5.5 kHz; the box filter takes about -13 dB or more off what would alias),
 * Hann window, a Goertzel at two frequencies in each band. Bands 5-8 (1.9 to 13 kHz): the newest 256 samples (5.8 ms, 172 Hz a bin), Hann window, three
 * Goertzel frequencies a band. A band's power is its largest probe's. Level = dB re full scale, -84 to 0 mapped to 0-255, with a tilt
 * (-11 dB at the lowest band to +2 dB at the highest) to even out a typical sound's slope. About 5,000 multiply-adds, in the main loop, once a picture frame; none in the audio interrupt. */
static const float HR_TILT[HR_NB] = {-11.0f, -9.0f, -7.0f, -5.0f, -3.0f, -1.0f, 0.0f, 1.0f, 2.0f};   /* dB: a typical sound is loudest low; this evens the ribbons' heights out */
static const float HC_LO[10] = {1.948484f, 1.929125f, 1.865904f, 1.815692f, 1.654166f, 1.528569f, 1.136185f, 0.843609f, 0.009354f, -0.530416f};
static const float HC_HI[12] = {1.912237f, 1.883063f, 1.844321f, 1.772472f, 1.698186f, 1.600525f, 1.422102f, 1.241351f, 1.009894f, 0.605751f, 0.221640f, -0.231942f};
static void hm_spectrum(uint8_t *lv)
{
    int16_t d[128];
    uint32_t end = rc_scope_w, j, k, b, q;
    for (j = 0; j < 128u; j++) {
        int32_t s = 0;
        for (k = 0; k < 8u; k++)
            s += rc_scope[(end - RC_SCOPE_N + j * 8u + k) & (RC_SCOPE_N - 1u)];
        d[j] = (int16_t)(s >> 3);
    }
    for (b = 0; b < HR_NB; b++) {
        float best = 0.0f, db, kk;
        int n = b < 5u ? 2 : 3;
        for (q = 0; q < (uint32_t)n; q++) {
            float c = b < 5u ? HC_LO[b * 2u + q] : HC_HI[(b - 5u) * 3u + q], s1 = 0.0f, s2 = 0.0f, p;
            if (b < 5u) {
                for (j = 0; j < 128u; j++) {
                    float s0 = (float)(d[j] * (16384 - cp_cos((int32_t)(2u * j)))) + c * s1 - s2;
                    s2 = s1; s1 = s0;
                }
            } else {
                for (j = 0; j < 256u; j++) {
                    float s0 = (float)((int32_t)rc_scope[(end - 256u + j) & (RC_SCOPE_N - 1u)] * (16384 - cp_cos((int32_t)j))) + c * s1 - s2;
                    s2 = s1; s1 = s0;
                }
            }
            p = s1 * s1 + s2 * s2 - c * s1 * s2;
            best = p > best ? p : best;
        }
        /* P x 16 / (N x 32768 x 32766)^2 is the probe's amplitude squared, as a fraction of full scale */
        kk = b < 5u ? 16.0f / (128.0f * 128.0f * 32768.0f * 32768.0f * 32766.0f * 32766.0f) : 16.0f / (256.0f * 256.0f * 32768.0f * 32768.0f * 32766.0f * 32766.0f);
        db = 3.0103f * fm_log2f(best * kk + 1e-12f) + HR_TILT[b];
        db = (db + 84.0f) * (255.0f / 84.0f);
        lv[b] = (uint8_t)(db < 0.0f ? 0.0f : db > 255.0f ? 255.0f : db);
    }
}


#include "home_icons.h"
/* HOME's rows (picture rows: screen row - 10) */
#define H_MY 16                               /* the mods row's tiles ... */
#define H_MH 42
#define H_PH 44                               /* the path's tiles */
#define H_TW 38                               /* a path tile's width, and the gap between */
#define H_TG 11
#define H_SBOT 148                            /* the scope's bottom (update 41 step 4: it was 174; the notes band takes the rows under it) */
#define H_NB 150                              /* the notes band: the chord and notes at the left, a keyboard at the right, to the picture's end (174) */
typedef struct { int16_t x; uint16_t col; uint8_t hi, act, feed, flag, slot, p, q; int8_t v; } h2_t;   /* (update 41 step 3) v, p, q: what the icon shows moving: the source's value, a stage's own settings; see h2_icon_live */   /* flag: 1 lost (no room), 2 off (bypassed); slot: a stage's engine slot, a mods tile's kind */
static struct {
    h2_t st[5], md[8];                        /* the path's stages, the mods row's tiles (the drums last) */
    uint8_t ns, nm, drum, ea[HI_N];           /* counts; the drum tile's index (255: none); each icon's eased activity */
    int16_t py, sy, jx, mw, echo;             /* the path's top row, the scope's top row, the drums' joining column, the mods' tile width, the delay echo's shift (px) */
    float lev, ph, ph2;                       /* the output's level; the dots' phase (px), the pulses' phase */
    uint16_t tint, rv, dl, ch;                /* the scope: its tint; the reverb's, the delay's echo (px) and the chorus' amounts */
    uint32_t hits;                            /* the drum hits seen, and when the last was */
    uint32_t hit_at;
    uint8_t jam;
    int8_t sweep;                             /* the filter's SWEEP x 100 */
    uint32_t ms;                              /* the frame's time (the icons' animation) */
    uint32_t nk, nt_at;                       /* the notes band: the keys lit (bit k: key k), and when something last sounded */
    char nname[20], ntext[40];                /* the chord's name and the notes' names, made again when the notes change */
    uint32_t nsig;
    uint8_t nt[PL_MAXN], nn, ntot, nbass, narp;   /* the notes shown (lowest first), how many sound, the bass's and the arp's note now (0 none) */
} h2;
float rk_eff_param(int slot, int param);
static void h2_notes(void);
static uint32_t h2_nfade(void);
static const char *const HI_NAME[HI_N] = {"BRAIDS", "RINGS", "FILTER", "CLOUDS", "LOOP", "SPECTRL", "REVERB", "DELAY", "CHORUS", "LFO", "TIDES", "PEAKS", "FUNC",
                                          "GRIDS", "MSEQ", "GATE", "KNOB", "DRUMS", "JAM"};
/* each icon's colour: the colour of its page (theme.h K_C_*; the table is in the manual) */
static uint16_t h2_col(int hi)
{
    switch (hi) {
    case HI_BRAIDS: return K_C_BRAIDS;
    case HI_RINGS: return K_C_RINGS;
    case HI_FILTER: return K_C_FILTER;
    case HI_CLOUDS: case HI_CLOUDS_LOOP: case HI_CLOUDS_SPEC: return K_C_CLOUDS;
    case HI_REVERB: return K_C_REVERB;
    case HI_DELAY: return K_C_DELAY;
    case HI_CHORUS: return K_C_CHORUS;
    case HI_LFO: return K_C_LFO;
    case HI_TIDES: return K_C_TIDES;
    case HI_PEAKS: return K_C_PEAKS;
    case HI_FUNC: return K_C_FUNC;
    case HI_GRIDS: return K_C_GRIDS;
    case HI_MSEQ: return K_C_MSEQ;
    case HI_GATE: return K_C_GATE;
    case HI_KNOB: return K_C_KNOB;
    case HI_DRUMS: return K_C_DRUMS;
    default: return K_C_JAM;
    }
}
static void home_reset(void)
{
    int i;
    for (i = 0; i < HI_N; i++) h2.ea[i] = 0;
    h2.lev = 0.0f;
    cp.nlive = 0;
}

static float h2_level(void)                   /* the output's level lately, 0..1 (the scope's last 256 samples) */
{
    uint32_t end = rc_scope_w, k;
    float s = 0.0f;
    for (k = 0; k < 256u; k++) {
        float v = (float)rc_scope[(end - 1u - k) & (RC_SCOPE_N - 1u)] * (1.0f / 16384.0f);
        s += v * v;
    }
    s = fm_sqrtf(s * (1.0f / 256.0f)) * 3.0f;
    return s > 1.0f ? 1.0f : s;
}
static void h2_ease(int hi, float a)          /* a stage's activity: quick up, slow down */
{
    int t = (int)(fm_clampf(a, 0.0f, 1.0f) * 255.0f), e = h2.ea[hi];
    e += (t - e) / (t > e ? 2 : 6);
    h2.ea[hi] = (uint8_t)e;
}
static int h2_stage_of(int slot, int param)   /* the stage a (slot, parameter) is on (the SRC stage is 0): -1 none */
{
    int i;
    if (slot == RS_AMP) slot = RS_SRC;
    if (slot == RS_GLO) {
        if (param < FLT_P0 || param > FLT_SWEEP)
            return -1;
        for (i = 0; i < h2.ns; i++) if (h2.st[i].hi == HI_FILTER) return i;
        return -1;
    }
    for (i = 0; i < h2.ns; i++)
        if (h2.st[i].slot == slot)
            return i;
    return -1;
}
static void h2_feed(int tile, int stage) { if (tile >= 0 && stage >= 0) h2.md[tile].feed |= (uint8_t)(1u << stage); }

/* the stage list, the mods row and every level, once a frame */
static const float *home_frame(uint32_t dt)
{
    const float *mon = rk_monitor(RS_AMP);
    float raw = mon[48] > mon[49] ? mon[48] : mon[49], env = 0.0f, pkl3 = raw * 3.0f, tr = 0.0f, tg = 0.0f, tb = 0.0f, tw = 0.0f;
    int i, k, ti[8], jam = jam_v[JV_ON] > 0.5f, lfo = 0, mseq = 0, gate = 0, pump = 0, knob = 0, ma, mb;
    cp_level(raw, dt);
    cp.nlive = 0;
    h2.ms = plat_ms();
    h2_notes();
    h2.lev += (h2_level() - h2.lev) * (dt > 60u ? 0.8f : 0.4f);
    h2.ph += (float)dt * (0.012f + 0.09f * h2.lev);
    h2.ph2 += (float)dt * 0.0011f;
    for (i = 0; i < 6; i++)
        if (mon[16 + i] > env) env = mon[16 + i];
    /* the path */
    h2.ns = 0;
    for (i = 0; i < 5; i++) {
        h2_t *s;
        int slot = i == 0 ? RS_SRC : i == 1 ? RS_SHP : i == 2 ? RS_GLO : i == 3 ? RS_FX1 : RS_FX2, id = i == 2 ? 12 : rc_patch.mod[slot], hi;
        if (!id || (i == 1 && id != 10))
            continue;
        hi = i == 0 ? HI_BRAIDS : i == 1 ? HI_RINGS : i == 2 ? HI_FILTER : id == 30 ? HI_CLOUDS : id == 31 ? HI_DELAY : id == 32 ? HI_REVERB : HI_CHORUS;
        s = &h2.st[h2.ns++];
        s->hi = (uint8_t)hi;
        s->feed = 0;
        s->slot = (uint8_t)slot;
        s->v = 0; s->p = 0; s->q = 0;
        if (hi == HI_FILTER) {                /* the filter's curve: cutoff and resonance as they are now (the matrix and the envelope in), the type */
            s->v = (int8_t)(fm_clampf(rk_eff_param(RS_GLO, FLT_P0 + 1), 0.0f, 1.0f) * 127.0f);
            s->p = (uint8_t)(fm_clampf(rk_eff_param(RS_GLO, FLT_P0 + 2), 0.0f, 1.0f) * 255.0f);
            s->q = (uint8_t)(rk_get_param(RS_GLO, FLT_P0) + 0.5f);
        }
        s->flag = (uint8_t)((slot < RS_GLO && (rk_fm1_status(4) >> slot & 1)) ? 1 : 0) | (uint8_t)((slot == RS_FX1 || slot == RS_FX2) && rk_fx_bypass(slot) ? 2 : 0);
        if (hi == HI_CLOUDS) {
            int md;
            mon = rk_monitor(slot);
            md = (int)(P(mon, 9) + 0.5f);
            s->hi = (uint8_t)(md == 1 ? HI_CLOUDS_LOOP : md == 2 ? HI_CLOUDS_SPEC : HI_CLOUDS);
            s->v = (int8_t)(fm_clampf(P(mon, 3), 0.0f, 1.0f) * 127.0f);      /* DENSITY, SIZE */
            s->p = (uint8_t)(fm_clampf(P(mon, 1), 0.0f, 1.0f) * 255.0f);
            mon = rk_monitor(RS_AMP);
        } else if (hi == HI_REVERB || hi == HI_DELAY || hi == HI_CHORUS) {
            int bp = s->flag & 2;
            float mix;
            mon = rk_monitor(slot);
            if (hi == HI_REVERB) { s->v = (int8_t)(fm_clampf(P(mon, 0), 0.0f, 1.0f) * 127.0f); s->p = (uint8_t)(fm_clampf(P(mon, 3), 0.0f, 1.0f) * 255.0f); }                       /* SIZE, MIX */
            else if (hi == HI_DELAY) { s->v = (int8_t)(fm_clampf(P(mon, 1), 0.0f, 1.0f) * 127.0f); s->p = (uint8_t)(fm_clampf(P(mon, 3), 0.0f, 1.0f) * 255.0f); s->q = (uint8_t)(fm_clampf(pk_delay_s(mon), 0.0f, 1.0f) * 250.0f); }   /* FEEDBACK, MIX, the time (4 ms) */
            else { s->v = (int8_t)(fm_clampf(P(mon, 0), 0.0f, 1.0f) * 127.0f); s->p = (uint8_t)(fm_clampf(P(mon, 1), 0.0f, 1.0f) * 255.0f); }                                                  /* RATE, DEPTH */
            mix = hi == HI_DELAY ? 0.35f + 0.65f * P(mon, 3) : hi == HI_REVERB ? 0.35f + 0.65f * P(mon, 3) : 0.35f + 0.65f * P(mon, 2);
            if (!bp) {
                if (hi == HI_REVERB) h2.rv = (uint16_t)(mix * 255.0f);
                else if (hi == HI_CHORUS) h2.ch = (uint16_t)(mix * 255.0f);
                else { h2.dl = (uint16_t)(mix * 255.0f); h2.echo = (int16_t)(14.0f + fm_clampf(pk_delay_s(mon), 0.02f, 0.5f) * 110.0f); }
            }
            mon = rk_monitor(RS_AMP);
        }
        s->col = h2_col(s->hi);
        h2_ease(s->hi, i < 3 ? env * 1.3f : pkl3);
        s->act = h2.ea[s->hi];
    }
    {                                         /* (the amounts are the ones of effects in use) */   int sr = 0, sd = 0, sc = 0;
        for (i = 0; i < h2.ns; i++) {
            if (h2.st[i].flag & 2) continue;
            sr |= h2.st[i].hi == HI_REVERB; sd |= h2.st[i].hi == HI_DELAY; sc |= h2.st[i].hi == HI_CHORUS;
        }
        if (!sr) h2.rv = 0;
        if (!sd) h2.dl = 0;
        if (!sc) h2.ch = 0;
    }
    /* the mods row */
    for (i = 0; i < 8; i++) ti[i] = -1;
    for (i = 0; i < 18; i++) {
        int src, via;
        float amt;
        uint16_t d = route_dst(i, &src, &via, &amt);
        if (!d || fm_fabsf(amt) < 0.001f) continue;
        lfo |= src >= 13 && src <= 15;
        mseq |= src >= 17 && src <= 20;
        gate |= src >= 23 && src <= 24;
        pump |= src == 25;
        knob |= src >= 1 && src <= 4;
    }
    h2.nm = 0;
    ma = rc_patch.mod[RS_MODA]; mb = rc_patch.mod[RS_MODB];
    for (i = 0; i < 8; i++) {
        int hi = -1, id = 0;
        float a = 0.0f;
        switch (i) {
        case 0: id = ma; break;
        case 1: id = mb; break;
        case 2: hi = lfo ? HI_LFO : -1; for (k = 0; k < 3; k++) a = fm_fabsf(mon[44 + k]) > a ? fm_fabsf(mon[44 + k]) : a; break;
        case 3: hi = mseq ? HI_MSEQ : -1; break;
        case 4: hi = gate ? HI_GATE : -1; for (k = 0; k < 2; k++) a = gp[k].on ? 1.0f : a; break;
        case 5: hi = knob ? HI_KNOB : -1; break;
        case 6: hi = jam ? HI_JAM : -1; for (k = 0; k < JAM_NM; k++) a = jam_m[k].lvl > a ? jam_m[k].lvl : a; break;
        default: hi = dr.run || pump ? HI_DRUMS : -1; break;
        }
        if (i < 2) {
            hi = id == 20 ? HI_PEAKS : id == 21 ? HI_TIDES : id == 23 ? HI_GRIDS : id == 24 ? HI_FUNC : -1;
            a = fm_fabsf(mon[40 + 2 * i]) > fm_fabsf(mon[41 + 2 * i]) ? fm_fabsf(mon[40 + 2 * i]) : fm_fabsf(mon[41 + 2 * i]);
        }
        if (hi < 0) continue;
        ti[i] = h2.nm;
        h2.md[h2.nm].hi = (uint8_t)hi;
        h2.md[h2.nm].feed = 0;
        h2.md[h2.nm].flag = 0;
        h2.md[h2.nm].slot = (uint8_t)i;     /* which kind of source (the order above) */
        h2.md[h2.nm].v = 0; h2.md[h2.nm].p = 0; h2.md[h2.nm].q = 0;
        if (i == 2) { float bv = 0.0f; for (k = 0; k < 3; k++) if (fm_fabsf(mon[44 + k]) > fm_fabsf(bv)) bv = mon[44 + k]; h2.md[h2.nm].v = (int8_t)(fm_clampf(bv, -1.0f, 1.0f) * 127.0f); }   /* the LFO moving most */
        else if (i < 2) h2.md[h2.nm].v = (int8_t)(fm_clampf(mon[40 + 2 * i], -1.0f, 1.0f) * 127.0f);     /* the module's first output */
        else if (i == 4) h2.md[h2.nm].v = (int8_t)(gp[0].on || gp[1].on ? 127 : -127);
        h2.md[h2.nm].col = h2_col(hi);
        h2_ease(hi, a);
        h2.md[h2.nm].act = h2.ea[hi];
        h2.nm++;
    }
    h2.drum = 255;
    if (dr.run) {
        if (dr.hits != h2.hits) { h2.hits = dr.hits; h2.hit_at = plat_ms(); }
        h2.drum = (uint8_t)(h2.nm - 1);
        k = (int)(plat_ms() - h2.hit_at);
        h2_ease(HI_DRUMS, k < 260 ? 1.0f - (float)k / 260.0f : 0.0f);
        h2.md[h2.drum].act = h2.ea[HI_DRUMS];
    }
    h2.jam = (uint8_t)jam;
    h2.sweep = (int8_t)(fm_clampf(rk_get_param(RS_GLO, FLT_SWEEP), -1.0f, 1.0f) * 100.0f);
    /* what each source moves */
    for (i = 0; i < 18; i++) {
        int src, via, tl = -1, st;
        float amt;
        uint16_t d = route_dst(i, &src, &via, &amt);
        if (!d || fm_fabsf(amt) < 0.001f) continue;
        st = h2_stage_of((int)((d >> 8) & 0x7Fu) - 1, d & 0xFF);
        tl = src == 9 || src == 10 ? ti[0] : src == 11 || src == 12 ? ti[1] : src >= 13 && src <= 15 ? ti[2] : src >= 17 && src <= 20 ? ti[3] : src >= 23 && src <= 24 ? ti[4] : src == 25 ? ti[7] : src >= 1 && src <= 4 ? ti[5] : -1;
        h2_feed(tl, st);
        if (tl >= 0 && (src >= 17 && src <= 20)) h2.md[tl].v = (int8_t)(fm_clampf(mon[50 + i] / (fm_fabsf(amt) > 0.05f ? amt : 1.0f), -1.0f, 1.0f) * 127.0f);   /* MSEQ: the route's value, the source's own */
        if (i < 12 && fm_fabsf(mon[50 + i]) > 0.03f) {
            float w = fm_clampf(fm_fabsf(mon[50 + i]) * 2.0f, 0.0f, 1.0f);
            uint16_t c = src_colour(src);
            tr += w * (float)(c >> 11); tg += w * (float)(c >> 5 & 63); tb += w * (float)(c & 31); tw += w;
            if (tl >= 0 && w > 0.0f) {
                int a = (int)(w * 255.0f);
                if (h2.md[tl].act < a) h2.md[tl].act = (uint8_t)a;
            }
        }
    }
    for (k = 0; k < 4 && ti[5] >= 0; k++)
        for (i = 0; i < 8; i++) {
            int o[5];
            if (rk_get_macro_target(k, i, o))
                h2_feed(ti[5], h2_stage_of(o[0], o[1]));
        }
    if (ti[6] >= 0)
        for (i = 0; i < h2.ns; i++)
            if (h2.st[i].hi == HI_FILTER || h2.st[i].hi == HI_BRAIDS || h2.st[i].slot == RS_FX1 || h2.st[i].slot == RS_FX2)
                h2_feed(ti[6], i);
    /* the knob layer's tile moves when a knob was turned lately (cp_ro_act) */
    if (ti[5] >= 0) {
        uint32_t now = plat_ms(), best = 0;
        for (k = 0; k < 4; k++) if (cp.act[k] >= best) { best = cp.act[k]; h2.md[ti[5]].v = (int8_t)((rc_patch.macro[k] * 2.0f - 1.0f) * 127.0f); }   /* the macro turned last */
        for (k = 0; k < 4; k++)
            if (cp.act[k] && now - cp.act[k] < 600u && h2.md[ti[5]].act < 220)
                h2.md[ti[5]].act = (uint8_t)(255u - (now - cp.act[k]) * 255u / 600u);
    }
    /* where the drums join: between the stage before the first effect and that effect (the end of the path with no effect) */
    h2.jx = -1;
    h2.py = h2.nm ? H_MY + H_MH + 14 : 34;
    h2.sy = h2.py + H_PH + 10;
    {
        int x0 = (240 - (h2.ns * H_TW + (h2.ns > 1 ? h2.ns - 1 : 0) * H_TG)) / 2, j = h2.ns;
        for (i = 0; i < h2.ns; i++) {
            h2.st[i].x = (int16_t)(x0 + i * (H_TW + H_TG));
            if (j == h2.ns && (h2.st[i].slot == RS_FX1 || h2.st[i].slot == RS_FX2)) j = i;
        }
        h2.jx = (int16_t)(j < h2.ns ? h2.st[j].x - H_TG / 2 : h2.ns ? h2.st[h2.ns - 1].x + H_TW + H_TG / 2 : 120);
    }
    {
        int n = h2.nm, w = n <= 6 ? H_TW : (232 - (n - 1) * 3) / (n ? n : 1), g = n <= 5 ? H_TG : n == 6 ? 4 : 3, x0 = (240 - (n * w + (n > 1 ? n - 1 : 0) * g)) / 2;
        for (i = 0; i < n; i++)
            h2.md[i].x = (int16_t)(x0 + i * (w + g));
        h2.mw = (int16_t)w;                  /* (the delay's echo is read in h2_scope from h2.dl and the tempo; mw is the mods' tile width) */
    }
    if (tw > 0.0f) {
        uint32_t k8 = tw > 1.0f ? 256u : (uint32_t)(tw * 256.0f);
        h2.tint = mix565((uint16_t)((uint32_t)(tr / tw + 0.5f) << 11 | (uint32_t)(tg / tw + 0.5f) << 5 | (uint32_t)(tb / tw + 0.5f)), K_SRC, k8);
    } else {
        h2.tint = K_SRC;
    }
    return mon;
}

static uint32_t home_sig(void)
{
    uint32_t s = (uint32_t)ui.shown * 40503u ^ (uint32_t)(ui.shown != cur_entry) * 977u ^ (uint32_t)mn.learn * 7u ^ (uint32_t)mn.learn_m * 31u ^ (uint32_t)h2.jam * 131u;
    int i;
    for (i = 0; i < h2.ns; i++) s = s * 31u + h2.st[i].hi * 7u + (h2.st[i].act >> 4) + (uint32_t)h2.st[i].flag * 3u;
    for (i = 0; i < h2.nm; i++) s = s * 31u + h2.md[i].hi * 7u + (h2.md[i].act >> 4) + h2.md[i].feed * 5u;
    for (i = 0; i < 4; i++) s = s * 131u + (uint32_t)(rc_patch.macro[i] * 1000.0f);
    s = s * 131u + h2.nk + (uint32_t)h2.nn * 7u + h2.nt[0] * 3u + h2.nbass * 11u + h2.narp * 13u + (h2_nfade() >> 4) * 5u + (uint32_t)(ui.zkeys & 127u) * 17u;
    for (i = 0; i < h2.ns; i++) s = s * 17u + (uint32_t)(uint8_t)h2.st[i].v + h2.st[i].p;
    for (i = 0; i < h2.nm; i++) s = s * 17u + (uint32_t)(uint8_t)h2.md[i].v;
    return s;
}

/* ------------------------------------------------------------------------------------------------ drawing (band by band) */
static void h2_icon_rows(int hi, int32_t x, int32_t y, uint16_t c, uint32_t br, int32_t rmax)   /* 24 x 24 coverage, in the colour c at br/256; the first rmax rows */
{
    uint16_t lut[16];
    int32_t r0 = cp.by - y, r1 = cp.by + cp.bh - y, r, q;
    if (r0 < 0) r0 = 0;
    if (r1 > rmax) r1 = rmax;
    if (r0 >= r1)
        return;
    lut[0] = 0;
    for (q = 1; q < 16; q++)
        lut[q] = mix565(c, K_BG, (uint32_t)q * br / 15u);
    for (r = r0; r < r1; r++) {
        const uint8_t *p = &HICON[hi][r * 12];
        for (q = 0; q < 12; q++)
            if (p[q]) {
                if (p[q] >> 4) cv_pset(x + 2 * q, y + r, lut[p[q] >> 4]);
                if (p[q] & 15) cv_pset(x + 2 * q + 1, y + r, lut[p[q] & 15]);
            }
    }
}
static void h2_icon(int hi, int32_t x, int32_t y, uint16_t c, uint32_t br) { h2_icon_rows(hi, x, y, c, br, 24); }

/* ---- update 41, step 3: the icons move with what their module does to the sound. Real data only (the engine's current parameters with the matrix in,
 * the modules' outputs, the output's level, the hits); the plain icon, dim, when a module is idle (its eased activity under 14). One frame's drawing is a few
 * dozen pixels a tile; nothing is stored but each tile's three bytes (v, p, q). */
static uint32_t h2_hash(uint32_t x) { x ^= x >> 15; x *= 0x2c1b3c6du; x ^= x >> 12; x *= 0x297a2d39u; x ^= x >> 15; return x; }
static void h2_px(int32_t x, int32_t y, uint16_t c, uint32_t a) { cv_pset(x, y, mix565(c, K_BG, a > 256u ? 256u : a)); }
static void h2_dot3(int32_t x, int32_t y, uint16_t c, uint32_t a)       /* 3 x 3, the centre brightest */
{
    int32_t i, j;
    for (j = -1; j <= 1; j++)
        for (i = -1; i <= 1; i++)
            h2_px(x + i, y + j, c, i || j ? a / 2u : a);
}
static const int8_t H2_SIN[32] = {0,25,49,71,90,106,118,125,127,125,118,106,90,71,49,25,0,-25,-49,-71,-90,-106,-118,-125,-127,-125,-118,-106,-90,-71,-49,-25};   /* a sine x 127, 32 steps a turn */
static int32_t h2_sin(float u)               /* sin(2 pi u) x 127, u in turns, any sign: the table, linearly between its steps */
{
    int32_t i = (int32_t)((u - fm_floorf(u)) * 512.0f), j = i >> 4, f = i & 15;
    return (H2_SIN[j & 31] * (16 - f) + H2_SIN[(j + 1) & 31] * f) >> 4;
}
static const int8_t H2_CIRC[28][2] = {{0,64},{14,62},{28,57},{40,50},{51,39},{58,28},{62,14},{64,0},{62,-14},{58,-28},{51,-39},{40,-50},{28,-57},{14,-62},{0,-64},{-14,-62},{-28,-57},{-40,-50},{-51,-39},{-58,-28},{-62,-14},{-64,0},{-62,14},{-58,28},{-51,39},{-40,50},{-28,57},{-14,62}};   /* sin, cos x 64 */
static void h2_filter(int32_t x, int32_t y, uint16_t c, uint32_t br, const h2_t *t)   /* the filter's response: cutoff, resonance and type as they are now */
{
    float cut = (float)t->v * (1.0f / 127.0f), res = (float)t->p * (1.0f / 255.0f), fc = 0.14f + cut * 0.72f, prev = 0.0f;
    int col, ty = t->q;
    cv_rect(x, y + 22, 24, 1, mix565(c, K_BG, br / 3u));
    for (col = 0; col < 24; col++) {
        float f = ((float)col + 0.5f) * (1.0f / 24.0f), r = f / fc, d = (f - fc) * 9.0f, g, pk = res * 0.85f / (1.0f + d * d * 1.6f);
        switch (ty) {
        case 1: g = 1.0f / (1.0f + d * d * 0.7f); pk *= 0.4f; break;                                             /* BP */
        case 2: r = fc / (f + 0.02f); g = 1.0f / (1.0f + r * r * 0.9f); break;                           /* HP */
        case 3: r = r * r; g = 1.0f / (1.0f + r * r * 0.9f); break;                                      /* LP24 */
        case 4: g = 1.0f - 0.95f / (1.0f + d * d * 2.0f); pk = 0.0f; break;                                      /* NOTCH */
        case 5: g = 0.35f + 0.9f / (1.0f + d * d * 1.0f); pk *= 0.5f; break;                                      /* PEAK */
        default: g = 1.0f / (1.0f + r * r * 0.9f); break;                                               /* LP */
        }
        g = (g + pk) * 0.62f;
        g = g > 1.0f ? 1.0f : g;
        g = g * 19.0f;
        if (col) {
            cv_line(x + col - 1, y + 21 - (int32_t)prev, x + col, y + 21 - (int32_t)g, c);
            cv_line(x + col - 1, y + 22 - (int32_t)prev, x + col, y + 22 - (int32_t)g, mix565(c, K_BG, br * 2u / 3u));
        }
        prev = g;
    }
    {                                         /* the cutoff, ticked on the floor */
        int32_t cx = x + (int32_t)(fc * 24.0f);
        cv_rect(cx, y + 22, 1, 2, K_FG);
    }
}
static void h2_reverb(int32_t x, int32_t y, uint16_t c, uint32_t br, const h2_t *t)   /* waves going out from a point, faster with SIZE, more of them with MIX, brighter with the level */
{
    float tt = (float)h2.ms * 0.001f * (0.35f + (float)t->v * (0.9f / 127.0f));
    int k, j, nr = 3 + (t->p > 128);
    h2_dot3(x + 3, y + 12, c, br);
    for (k = 0; k < nr; k++) {
        float u = tt + (float)k / (float)nr;
        int32_t r, px = 0, py = 0;
        uint16_t col;
        u -= fm_floorf(u);
        r = 2 + (int32_t)(u * 20.0f);
        col = mix565(c, K_BG, (uint32_t)((1.0f - u) * (float)br * (0.35f + 0.65f * (float)t->act * (1.0f / 255.0f))));
        for (j = 3; j <= 11; j++) {           /* the arc from 60 degrees above the horizontal to 60 below: nine points of the circle table, joined */
            int32_t ax = x + 3 + (H2_CIRC[j][0] * r + 32) / 64, ay = y + 12 - (H2_CIRC[j][1] * r + 32) / 64;
            if (j > 3 && ax < x + 26) cv_line(px, py, ax, ay, col);
            px = ax; py = ay;
        }
    }
}
static void h2_delay(int32_t x, int32_t y, uint16_t c, uint32_t br, const h2_t *t)    /* the echoes: bars falling with FEEDBACK, each lit in turn at the delay's time */
{
    int k, lit;
    float ratio = 0.3f + (float)t->v * (0.65f / 127.0f), hh = 19.0f;
    uint32_t tm = (uint32_t)t->q * 4u;
    tm = tm < 70u ? 70u : tm;
    lit = (int)((h2.ms / tm) % 7u);
    for (k = 0; k < 5; k++) {
        int32_t bh = (int32_t)hh + 1;
        uint32_t a = k == lit ? 256u : br * 2u / 5u + (k + 1 == lit ? br / 4u : 0u);
        cv_rect(x + 2 + 4 * k, y + 22 - bh, 3, bh, mix565(c, K_BG, a));
        hh *= ratio;
    }
    cv_rect(x + 1, y + 22, 22, 1, mix565(c, K_BG, br / 2u));
}
static void h2_chorus(int32_t x, int32_t y, uint16_t c, uint32_t br, const h2_t *t)   /* two lines swaying at RATE, as far apart as DEPTH */
{
    float ph = (float)h2.ms * 0.001f * (0.25f + (float)t->v * (4.0f / 127.0f)), off = 0.18f + (float)t->p * (0.5f / 255.0f);
    int32_t amp = 640 + (int32_t)t->p * 1536 / 255, col, py0 = 12, py1 = 12;      /* x 256 */
    uint16_t c0 = mix565(c, K_BG, br), c1 = mix565(c, K_BG, br * 3u / 5u);
    for (col = 0; col < 24; col++) {
        float u = (float)col * (1.0f / 16.0f) + ph;
        int32_t y0 = 12 + ((h2_sin(u) * amp) >> 15), y1 = 12 + ((h2_sin(u + off) * amp * 7 / 8) >> 15);
        if (col) {
            cv_line(x + col - 1, y + py1, x + col, y + y1, c1);
            cv_line(x + col - 1, y + py0, x + col, y + y0, c0);
        }
        py0 = y0; py1 = y1;
    }
}
static void h2_clouds(const h2_t *t, int32_t x, int32_t y, uint16_t c, uint32_t br)   /* the cloud and its grains: as many as DENSITY says, new ones as often */
{
    int hi = t->hi, k, n = 1 + (int)((float)t->v * (11.0f / 127.0f));
    uint32_t per = 50u + (uint32_t)(127 - t->v) * 2u, e = h2.ms / per;
    h2_icon_rows(hi, x, y, c, br, 14);
    if (hi == HI_CLOUDS) {                    /* grains scattering */
        for (k = 0; k < n; k++) {
            uint32_t hh = h2_hash(e * 7u + (uint32_t)k * 131u);
            int32_t gx = 2 + (int32_t)(hh % 20u), gy = 15 + (int32_t)((hh >> 8) % 8u);
            uint32_t a = 90u + (hh >> 16) % 166u;
            h2_px(x + gx, y + gy, c, a * br / 256u);
            h2_px(x + gx + 1, y + gy, c, a * br / 512u);
            h2_px(x + gx, y + gy + 1, c, a * br / 512u);
        }
    } else if (hi == HI_CLOUDS_LOOP) {        /* the loop: a grain going round a coil */
        float u = (float)h2.ms * (0.002f + (float)t->v * (0.006f / 127.0f));
        int j;
        for (j = 0; j < 4; j++) {
            float a = u - (float)j * 0.45f, r = 3.0f + 0.9f * fm_sinf(a * 0.5f) ;
            h2_dot3(x + 12 + (int32_t)(r * fm_cosf(a) * 1.8f), y + 19 + (int32_t)(r * fm_sinf(a)), c, br * (4u - (uint32_t)j) / 4u);
        }
    } else {                                  /* the spectrum: eight bars, their heights moving as the grains do */
        for (k = 0; k < 8; k++) {
            uint32_t hh = h2_hash(e * 13u + (uint32_t)k * 977u);
            int32_t bh = 2 + (int32_t)(hh % 7u) * (1 + t->v / 40) / 2;
            bh = bh > 9 ? 9 : bh;
            cv_rect(x + 2 + 3 * k, y + 23 - bh, 2, bh, mix565(c, K_BG, br * 4u / 5u));
        }
    }
}
static void h2_knob(int32_t x, int32_t y, uint16_t c, uint32_t br, const h2_t *t)     /* the knob layer: a dial and its pointer at the macro turned last */
{
    float m = ((float)t->v + 127.0f) * (1.0f / 254.0f), a = -2.356f + m * 4.712f;
    int k;
    float sa = fm_sinf(a), ca = fm_cosf(a);
    int32_t px = x + 12 + (int32_t)(8.0f * sa + (sa < 0 ? -0.5f : 0.5f)), py = y + 12 - (int32_t)(8.0f * ca + (ca < 0 ? -0.5f : 0.5f));
    uint16_t dc = mix565(c, K_BG, br / 2u);
    for (k = 0; k < 28; k++) {                /* the dial: 270 degrees of ring (the bottom quarter open) */
        if (k >= 11 && k <= 17) continue;
        cv_pset(x + 12 + (H2_CIRC[k][0] * 21 + 64) / 128, y + 12 - (H2_CIRC[k][1] * 21 + 64) / 128, dc);
    }
    cv_line(x + 12, y + 12, px, py, c);
    h2_dot3(px, py, c, br);
    h2_dot3(x + 12, y + 12, c, br / 2u);
}
static void h2_icon_live(const h2_t *t, int32_t x, int32_t y, uint16_t c, uint32_t br)
{
    int hi = t->hi;
    uint32_t a = t->act;
    int32_t dx = 0, dy = 0;
    float tt = (float)h2.ms * 0.001f;
    if (a < 14u) {                            /* idle: still and dim */
        h2_icon(hi, x, y, c, br);
        return;
    }
    switch (hi) {
    case HI_FILTER: h2_filter(x, y, c, br, t); return;
    case HI_REVERB: h2_reverb(x, y, c, br, t); return;
    case HI_DELAY: h2_delay(x, y, c, br, t); return;
    case HI_CHORUS: h2_chorus(x, y, c, br, t); return;
    case HI_CLOUDS: case HI_CLOUDS_LOOP: case HI_CLOUDS_SPEC: h2_clouds(t, x, y, c, br); return;
    case HI_KNOB: h2_knob(x, y, c, br, t); return;
    case HI_BRAIDS: dy = (int32_t)(fm_sinf(tt * 9.0f) * (float)a * (1.6f / 255.0f) + 0.5f); break;      /* wobbling with its level */
    case HI_RINGS: dx = (int32_t)(fm_sinf(tt * 7.0f) * (float)a * (1.6f / 255.0f) + 0.5f); dy = (int32_t)(fm_cosf(tt * 9.0f) * (float)a * (1.2f / 255.0f) + 0.5f); break;
    case HI_DRUMS: dy = (int32_t)(a * 3u / 255u); break;                                                   /* the drum's head gives under a hit */
    default: break;
    }
    if (hi == HI_GRIDS) {                     /* the grid: cells lit by the pattern, more of them as it works */
        uint32_t e = h2.ms / 90u;
        int i, j;
        for (j = 0; j < 4; j++)
            for (i = 0; i < 4; i++) {
                uint32_t hh = h2_hash(e * 31u + (uint32_t)(j * 4 + i));
                int lit = (int)(hh % 256u) < (int)a;
                cv_rect(x + 2 + 6 * i, y + 2 + 6 * j, 3, 3, mix565(c, K_BG, lit ? br : br / 4u));
            }
        return;
    }
    h2_icon(hi, x + dx, y + dy, c, br);
    if (hi == HI_JAM) {                       /* a move fired: a ring goes out */
        int32_t r = 3 + (int32_t)(255u - a) * 9 / 255;
        uint16_t col = mix565(c, K_BG, a * br / 256u);
        int k;
        for (k = 0; k < 28; k++)
            cv_pset(x + 12 + (H2_CIRC[k][0] * r + 32) / 64, y + 12 - (H2_CIRC[k][1] * r + 32) / 64, col);
    } else if (hi == HI_LFO || hi == HI_TIDES || hi == HI_PEAKS || hi == HI_FUNC || hi == HI_MSEQ || hi == HI_GATE) {   /* the value now: a dot riding the right edge */
        int32_t py = y + 21 - ((int32_t)t->v + 127) * 18 / 254;
        cv_rect(x + 21, y + 2, 1, 20, mix565(c, K_BG, br / 4u));
        h2_dot3(x + 21, py, K_FG, 256u);
    }
}
static int h2_vis(int32_t y, int32_t h) { return y + h > cp.by && y < cp.by + cp.bh; }
static void h2_tile(const h2_t *t, int32_t y, int32_t w, int32_t h)
{
    int lost = t->flag & 1, off = t->flag & 2;
    uint16_t c = lost ? K_WARN : off ? K_FAINT : t->col;
    uint32_t a = off ? 0u : t->act, br = off ? 90u : 118u + a * 138u / 255u;
    char n[10];
    int k = 0, ln;
    int32_t x = t->x, tw;
    if (!h2_vis(y - 1, h + 2))
        return;
    if (a > 40u)                              /* the halo: a touch, brighter as it works */
        rrect(x - 1, y - 1, w + 2, h + 2, mix565(c, K_BG, 30u + a / 5u), 4);
    rrect(x, y, w, h, mix565(c, K_BG, 90u + a / 3u), 3);
    rrect(x + 1, y + 1, w - 2, h - 2, K_BG, 2);
    if (off || lost) h2_icon(t->hi, x + (w - 24) / 2, y + 4, c, br);
    else h2_icon_live(t, x + (w - 24) / 2, y + 4, c, br);
    if (t->hi == HI_FILTER && h2.sweep && h2_vis(y + 27, 3)) {      /* the SWEEP: a short scale under the curve, a mark where it is, the centre ticked */
        int32_t bx = x + (w - 24) / 2, mx = bx + 12 + (int32_t)h2.sweep * 11 / 100;
        cv_rect(bx, y + 28, 24, 1, mix565(c, K_BG, 70));
        cv_rect(bx + 12, y + 26, 1, 5, mix565(c, K_BG, 110));
        cv_rect(mx - 1, y + 27, 3, 3, K_FG);
    }
    if (off) {
        cv_text(x + w / 2 - text_w(&FONT_T, "OFF") / 2, y + h - 13, &FONT_T, "OFF", K_DIM);
        return;
    }
    while (HI_NAME[t->hi][k] && k < 9) { n[k] = HI_NAME[t->hi][k]; k++; }
    n[k] = 0;
    ln = k;
    while (ln > 1 && text_w(&FONT_T, n) > w - 2) n[--ln] = 0;
    tw = text_w(&FONT_T, n);
    cv_text(x + (w - tw) / 2, y + h - 13, &FONT_T, n, lost ? K_WARN : mix565(c, K_DIM, 110u + a / 3u));
}
static void h2_dot(int32_t x, int32_t y, uint16_t c, uint32_t a)
{
    if (!h2_vis(y - 2, 5)) return;
    rrect(x - 2, y - 2, 5, 5, mix565(c, K_BG, a / 4u), 2);
    rrect(x - 1, y - 1, 3, 3, mix565(K_FG, c, a), 1);
}
static void h2_vline(int32_t x, int32_t y0, int32_t y1, uint16_t c)
{
    if (y1 < y0) { int32_t t = y0; y0 = y1; y1 = t; }
    if (h2_vis(y0, y1 - y0 + 1)) cv_rect(x, y0, 1, y1 - y0 + 1, c);
}
/* a feed: down from (x0, y0), across at the lane row, down to (x1, y1), a pulse running along it */
static void h2_feedline(int32_t x0, int32_t y0, int32_t ly, int32_t x1, int32_t y1, uint16_t c, uint32_t act, float u0)
{
    int32_t l1 = ly - y0, l2 = x1 > x0 ? x1 - x0 : x0 - x1, l3 = y1 - ly, tot = l1 + l2 + l3, p, d = x1 > x0 ? 1 : -1;
    uint16_t line = mix565(c, K_BG, 46u + act / 3u);
    h2_vline(x0, y0, ly, line);
    if (h2_vis(ly, 1) && l2) cv_rect(x1 > x0 ? x0 : x1, ly, l2 + 1, 1, line);
    h2_vline(x1, ly, y1, line);
    if (h2_vis(y1 - 3, 3)) {                  /* the arrow into the tile */
        cv_line(x1 - 2, y1 - 2, x1, y1, line);
        cv_line(x1 + 2, y1 - 2, x1, y1, line);
    }
    if (act > 12u && tot > 0) {
        float u = u0 - fm_floorf(u0);
        p = (int32_t)(u * (float)tot);
        if (p < l1) h2_dot(x0, y0 + p, c, act);
        else if (p < l1 + l2) h2_dot(x0 + d * (p - l1), ly, c, act);
        else h2_dot(x1, ly + (p - l1 - l2), c, act);
    }
}
static void h2_scope(void)
{
    uint32_t end = rc_scope_w, start = end - RC_SCOPE_N + 8u, k;
    int32_t top = h2.sy, h = H_SBOT - top, mid = top + h / 2, amp = h / 2 - 3, x, py[3], pp[3], by = cp.by, be = cp.by + cp.bh;
    uint16_t c, halo;
    uint32_t ld = (uint32_t)(h2.lev * 256.0f);
    if (top >= be || H_SBOT <= by)
        return;
    c = ld > 120u ? mix565(K_FG, h2.tint, (ld - 120u) * 256u / 136u) : h2.tint;     /* white when loud */
    halo = mix565(c, K_BG, 50);
    for (k = 0; k < 400u; k++) {              /* the first rise through zero in the oldest part */
        int16_t a = rc_scope[(start + k) & (RC_SCOPE_N - 1u)], b = rc_scope[(start + k + 1u) & (RC_SCOPE_N - 1u)];
        if (a < 0 && b >= 0)
            break;
    }
    if (k == 400u)
        k = 0;
    cv_rect(6, mid, 228, 1, mix565(K_FAINT, K_BG, 150));
    for (x = 0; x < 228; x++) {
        int32_t v = rc_scope[(start + k + 2u * (uint32_t)x) & (RC_SCOPE_N - 1u)], n;
        py[0] = mid - (v * amp) / 16384;
        py[0] = py[0] < top + 1 ? top + 1 : py[0] > H_SBOT - 2 ? H_SBOT - 2 : py[0];
        n = (int32_t)(h2.ch ? rc_scope[(start + k + 2u * (uint32_t)x + 9u) & (RC_SCOPE_N - 1u)] : 0);
        py[1] = mid - (n * amp) / 16384 * 3 / 4;
        py[1] = py[1] < top + 1 ? top + 1 : py[1] > H_SBOT - 2 ? H_SBOT - 2 : py[1];
        n = (int32_t)(h2.dl && x >= h2.echo ? rc_scope[(start + k + 2u * (uint32_t)(x - h2.echo)) & (RC_SCOPE_N - 1u)] : 0);
        py[2] = mid - (n * amp) / 16384 / 2;
        py[2] = py[2] < top + 1 ? top + 1 : py[2] > H_SBOT - 2 ? H_SBOT - 2 : py[2];
        if (x) {
            if (h2.rv) {                       /* the reverb's trail: a wider halo */
                cv_line(5 + x, pp[0] - 3, 6 + x, py[0] - 3, mix565(c, K_BG, 16u + h2.rv / 12u));
                cv_line(5 + x, pp[0] + 3, 6 + x, py[0] + 3, mix565(c, K_BG, 16u + h2.rv / 12u));
            }
            cv_line(5 + x, pp[0] - 1, 6 + x, py[0] - 1, halo);
            cv_line(5 + x, pp[0] + 1, 6 + x, py[0] + 1, halo);
            if (h2.dl) cv_line(5 + x, pp[2], 6 + x, py[2], mix565(c, K_BG, 50u + h2.dl / 5u));
            if (h2.ch) cv_line(5 + x, pp[1], 6 + x, py[1], mix565(K_FX, K_BG, 100u + h2.ch / 4u));
            cv_line(5 + x, pp[0], 6 + x, py[0], c);
        }
        pp[0] = py[0]; pp[1] = py[1]; pp[2] = py[2];
    }
}
/* ---- update 41 step 4: what is being played. The notes sounding (the chord layer's and the keys' own; the arp's and the bass's apart), or the chord held
 * while the arp plays it; kept, dimmed, for 1.1 s after the last one ends */
static void h2_notes(void)    /* (the layers: 0 chord, 1 bass, 2 arp: play.c's L_* names are colours here) */
{
    uint8_t s[PL_MAXN];
    int ns = 0, total = 0, i, k, held = 0, bass = 0, arp = 0, live;
    uint32_t nk = 0, now = h2.ms;
    for (i = 0; i < PL_NRAW; i++)
        if (pl_raw[i].active) held = 1;
    for (i = 0; i < 128; i++) {
        int d = pl_snd[i];
        for (k = 0; k < 3; k++) d -= lay_loc[k][i] > 0;
        if (d > 0 || lay_loc[0][i] || lay_out[0][i]) {
            total++;
            if (ns < PL_MAXN) s[ns++] = (uint8_t)i;
        }
        if (!bass && (lay_loc[1][i] || lay_out[1][i])) bass = i;
        if (!arp && (lay_loc[2][i] || lay_out[2][i])) arp = i;
    }
    if (held && pv.nchord) {                  /* the keys' chord (the arp may be the only thing sounding it) */
        ns = pv.nchord < PL_MAXN ? pv.nchord : PL_MAXN;
        total = pv.nchord;
        for (i = 0; i < ns; i++) s[i] = pv.chord[i];
    }
    live = ns || bass || arp;
    if (live) {
        for (i = 0; i < ns; i++) h2.nt[i] = s[i];
        h2.nn = (uint8_t)ns;
        h2.ntot = (uint8_t)(total > 255 ? 255 : total);
        h2.nbass = (uint8_t)bass;
        h2.narp = (uint8_t)arp;
        h2.nt_at = now;
        for (k = 0; k < NKEYS; k++) {
            int n = 53 + k + 12 * set.octave;
            for (i = 0; i < ns; i++)
                if (s[i] == n) nk |= 1u << k;
            if (arp == n) nk |= 1u << k;
        }
        h2.nk = nk | (ui.keys & ((1u << NKEYS) - 1u));
    } else if (now - h2.nt_at > 1100u) {
        h2.nn = h2.ntot = h2.nbass = h2.narp = 0;
        h2.nk = 0;
    }
}
static uint32_t h2_nfade(void)                /* 256 while it sounds, falling to 0 over the second after */
{
    uint32_t age = h2.ms - h2.nt_at;
    int sounding = 0, i;
    for (i = 0; i < PL_NRAW && !sounding; i++) sounding = pl_raw[i].active;
    if (sounding || age < 80u) return 256u;
    return age >= 1100u ? 0u : 256u - age * 256u / 1100u;
}
static void h2_noteband(void)
{
    static const uint8_t WHITE[12] = {1, 0, 1, 0, 1, 1, 0, 1, 0, 1, 0, 1};      /* by pitch class (C = 0) */
    int32_t kx = 122, ky = H_NB + 3, wi = 0;
    uint32_t fa = h2_nfade();
    int k, zone = zone_on();
    uint16_t on = K_CHD, zc = K_GOLD;
    if (!h2_vis(H_NB, 26)) return;
    for (k = 0; k < NKEYS; k++) {             /* the keys of the FM-1 (F3 up), whites first */
        int pc = (53 + k) % 12;
        if (WHITE[pc]) {
            int lit = h2.nk >> k & 1u, z = zone && k < 7, zh = ui.zkeys >> k & 1u;
            uint16_t col = lit ? mix565(on, GREY(64), fa) : GREY(64);
            if (zh) col = zc;
            cv_rect(kx + wi * 7, ky, 6, 18, col);
            if (z) cv_rect(kx + wi * 7, ky + 15, 6, 3, zh ? K_FG : zc);
            wi++;
        }
    }
    wi = 0;
    for (k = 0; k < NKEYS; k++) {             /* then the blacks, between */
        int pc = (53 + k) % 12;
        if (WHITE[pc]) { wi++; continue; }
        {
            int lit = h2.nk >> k & 1u, z = zone && k < 7, zh = ui.zkeys >> k & 1u;
            uint16_t col = lit ? mix565(on, GREY(8), fa * 3u / 4u) : GREY(8);
            if (zh) col = zc;
            cv_rect(kx + wi * 7 - 3, ky, 5, 11, col);
            if (z) cv_rect(kx + wi * 7 - 3, ky + 9, 5, 2, zh ? K_FG : zc);
        }
    }
    if (!h2.nn && !h2.nbass && !h2.narp) return;
    {                                         /* the chord's name, the notes, the arp's and the bass's note */
        char t[24], u[8];
        uint16_t nc = mix565(K_FG, K_BG, 60u + fa * 196u / 256u), tc = mix565(K_DIM, K_BG, 90u + fa * 166u / 256u);
        int i;
        uint32_t sg = h2.nn * 977u + h2.ntot;
        for (i = 0; i < h2.nn; i++) sg = sg * 131u + h2.nt[i];
        if (sg != h2.nsig || !h2.nname[0]) {   /* the names, only when the notes changed */
            char x[12];
            int shown = 0;
            h2.nsig = sg;
            h2.ntext[0] = 0;
            if (h2.nn) chord_name(h2.nt, h2.nn, h2.nname); else h2.nname[0] = 0;
            for (i = 0; i < h2.nn && h2.nn > 1; i++) {
                int sp = i ? 2 : 0;
                char *tx = h2.ntext;
                note_name(h2.nt[i], x);
                {   char keep[40];
                    int p = 0;
                    while (tx[p] && p < 39) { keep[p] = tx[p]; p++; }
                    keep[p] = 0;
                    if (sp) cat(tx, "  ");
                    cat(tx, x);
                    if (text_w_acc(&FONT_T, tx) > (h2.ntot > h2.nn || i + 1 < h2.nn ? 96 : 112)) {   /* no more room: this one stays out, the rest are +n */
                        tx[0] = 0; cat(tx, keep);
                        break;
                    }
                }
                shown++;
            }
            if (h2.nn > 1 && shown < h2.ntot) {
                cat(h2.ntext, "  +");
                int_text(h2.ntot - shown, u);
                cat(h2.ntext, u);
            }
        }
        if (h2.nn) {
            if (text_w_acc(&FONT_B, h2.nname) > 66) cv_text_acc(6, H_NB + 3, &FONT_XS, h2.nname, nc);
            else cv_text_acc(6, H_NB, &FONT_B, h2.nname, nc);
            if (h2.nn > 1) cv_text_acc(6, H_NB + 15, &FONT_T, h2.ntext, tc);
        }
        if (h2.narp) {
            t[0] = 0; cat(t, "ARP "); note_name(h2.narp, u); cat(t, u);
            cv_text_acc(76, H_NB, &FONT_XS, t, mix565(K_CHD, K_BG, 90u + fa * 166u / 256u));
        }
        if (h2.nbass) {
            t[0] = 0; cat(t, "BASS "); note_name(h2.nbass, u); cat(t, u);
            cv_text_acc(76, H_NB + 8, &FONT_XS, t, mix565(K_CHD, K_BG, 90u + fa * 166u / 256u));
        }
    }
}
static void home_draw(const float *mon, int pf)
{
    int i, j, n = h2.ns;
    int32_t pyc = h2.py + 22, x;
    (void)mon; (void)pf;
    /* the path's line and its dots (hidden under the tiles) */
    if (h2_vis(pyc - 3, 7) && n) {
        uint16_t line = mix565(K_SRC, K_BG, 46u + (uint32_t)(h2.lev * 70.0f));
        int32_t xe = h2.st[n - 1].x + H_TW;
        cv_rect(2, pyc, xe + H_TG - 2, 1, line);
        cv_rect(xe + H_TG - 3, pyc, 240 - xe - H_TG, 1, line);
        cv_line(230, pyc - 3, 234, pyc, line);
        cv_line(230, pyc + 3, 234, pyc, line);
        if (h2.lev > 0.02f)
            for (j = 0; j < 8; j++) {
                float u = h2.ph * 0.06f + (float)j * 0.125f;
                x = 4 + (int32_t)((u - fm_floorf(u)) * 226.0f);
                h2_dot(x, pyc, K_SRC, 60u + (uint32_t)(h2.lev * 195.0f));
            }
    }
    /* the mods row: the feeds first (under the tiles) */
    for (i = 0; i < h2.nm; i++) {
        const h2_t *m = &h2.md[i];
        int32_t mx = m->x + h2.mw / 2, ly = H_MY + H_MH + 3 + 3 * (i & 3);
        if (i == h2.drum) {
            h2_feedline(mx, H_MY + H_MH, ly, h2.jx, pyc - 2, m->col, m->act, h2.ph2 * 2.0f + 0.3f * (float)i);
            continue;
        }
        for (j = 0; j < n; j++)
            if (m->feed >> j & 1)
                h2_feedline(mx, H_MY + H_MH, ly, h2.st[j].x + H_TW / 2 + (i & 1) * 3 - 1, h2.py - 1, m->col, m->act, h2.ph2 * 3.0f + 0.31f * (float)i + 0.17f * (float)j);
    }
    for (i = 0; i < h2.nm; i++) {
        h2_t t = h2.md[i];
        h2_tile(&t, H_MY, h2.mw, H_MH);
    }
    for (i = 0; i < n; i++)
        h2_tile(&h2.st[i], h2.py, H_TW, H_PH);
    h2_scope();
    h2_noteband();
}

static void home_text(void)
{
    char t[32], n[12], name[24];
    int pending = ui.shown != cur_entry, c = entry_cat(ui.shown);
    const char *tag = ui.shown < NPRESETS ? PRESETS[ui.shown].cat : CAT_NAME[c];
    uint16_t cc = c ? CAT_COL[c] : K_SRC;
    if (cp.by < 26) {
        int32_t w;
        entry_name(ui.shown, name);
        cv_text(6, 1, &FONT_B, name, pending ? K_DIM : K_FG);
        if (pal_day) cv_text(7, 1, &FONT_B, name, pending ? K_DIM : K_FG);
        t[0] = 0;
        if (pending) {
            cat(t, "LOADING");
            w = text_w(&FONT_T, t);
            cv_text(234 - w, 3, &FONT_T, t, K_GOLD);
        } else if (ui.shown < NPRESETS) {
            int_text(ui.shown + 1, t);
            cat(t, "/");
            int_text(NPRESETS, n);
            cat(t, n);
            w = text_w(&FONT_T, t);
            cv_text(234 - w, 3, &FONT_T, t, K_DIM);
            cv_text(234 - w - 6 - text_w(&FONT_T, tag), 3, &FONT_T, tag, cc);
        } else {
            cv_text(234 - text_w(&FONT_T, "USER"), 3, &FONT_T, "USER", K_GOLD);
        }
    }
    cp_dbg_text();
}

/* the four macros, as sliders: a row each under the picture, in the knob's colour (K1 blue, K2 green, K3 white, K4 orange): its name (the preset's own or the first
 * thing it moves), the track with its fill and thumb, the value. A macro routed through a via source (an LFO, an envelope) gets a second mark where the via
 * source holds it and a band to it in the source's colour; the row grows bright while the knob is turned (cp_ro_act); LEARN's chosen one blinks gold */
#define HS_X 66
#define HS_W 124
static void home_readouts(void)
{
    uint32_t now = plat_ms();
    int k, key = PG_HOME << 4;
    const float *mon = rk_monitor(RS_AMP);
    cp_ro_begin();
    for (k = 0; k < 4; k++) {
        float v = rc_patch.macro[k], amt, live = -1.0f;
        int r, nt = 0, src = 0, via, sel = mn.learn && !mn.learn_src && mn.learn_m == k, q;
        uint16_t first = 0, kc = KNOB_C[k], mc = 0;
        char t[24], l[24];
        int32_t g = cp_ro_act(k, v, key), y = k * 10 + 1, tx, ty = y + 5;
        for (r = 0; r < 18; r++) {
            uint16_t d = route_dst(r, &src, &via, &amt);
            if (d && src == 1 + k) {
                if (!nt) first = d;
                nt++;
                if (via && r < 12 && fm_fabsf(amt) > 0.01f && live < 0.0f) {
                    live = fm_clampf(mon[50 + r] / amt, 0.0f, 1.0f);
                    mc = src_colour(via);
                }
            }
        }
        if (cur_entry >= 1 && cur_entry < NPRESETS && MNAMES[cur_entry][k][0]) {   /* (update 24) a factory preset names its own four knobs (presets.h MNAMES) */
            l[0] = 0;
            cat(l, MNAMES[cur_entry][k]);
        } else if (nt) {
            dst_name(first, l);
            if (nt > 1) {
                cat(l, "+");
                int_text(nt - 1, t);
                cat(l, t);
            }
        } else {
            l[0] = 0;
            cat(l, "UNSET");
            kc = mix565(kc, K_BG, 120);
        }
        q = 0;
        while (l[q]) q++;
        while (text_w(&FONT_T, l) > HS_X - 10 && q > 1) l[--q] = 0;
        if (sel && (now / 250u) & 1u)
            kc = K_GOLD;
        cv_text(5, y - 1, &FONT_T, l, mix565(kc, K_DIM, 150u + (uint32_t)g * 106u / 256u));
        v = fm_clampf(v, 0.0f, 1.0f);
        tx = HS_X + (int32_t)(v * (float)HS_W);
        cv_rect(HS_X, ty, HS_W + 1, 1, K_FAINT);                       /* the track, its quarters */
        for (q = 0; q < 5; q++)
            cv_rect(HS_X + q * HS_W / 4, ty - 2, 1, 5, mix565(K_FAINT, K_BG, 190));
        if (live >= 0.0f) {                                            /* the via source's hold on it */
            int32_t lx = HS_X + (int32_t)(live * (float)HS_W), a = lx < tx ? lx : tx, b = lx < tx ? tx : lx;
            rrect(a, ty - 2, b - a + 1, 5, mix565(mc, K_BG, 70), 2);
            cv_rect(lx, ty - 4, 1, 9, mc);
            cv_rect(lx - 1, ty - 3, 3, 1, mc);
        }
        if (tx > HS_X) {                                               /* the fill, a hairline with its halo */
            cv_rect(HS_X, ty, tx - HS_X, 1, kc);
            cv_rect(HS_X, ty - 1, tx - HS_X, 1, mix565(kc, K_BG, 70u + (uint32_t)g / 4u));
            cv_rect(HS_X, ty + 1, tx - HS_X, 1, mix565(kc, K_BG, 70u + (uint32_t)g / 4u));
        }
        rrect(tx - 2, ty - 4 - (int32_t)(g / 128), 5, 9 + (int32_t)(g / 64), mix565(kc, K_BG, 80), 2);   /* the thumb (larger while turned) */
        rrect(tx - 1, ty - 3 - (int32_t)(g / 128), 3, 7 + (int32_t)(g / 64), g > 20 ? K_FG : mix565(K_FG, kc, 150), 1);
        int_text((int32_t)(v * 100.0f + 0.5f), t);
        cv_text(236 - text_w(&FONT_T, t), y - 1, &FONT_T, t, g > 40 || nt ? K_FG : K_DIM);
    }
    cp.pkey = key;
    cp_send(CP_RY, 40, cp.rh + CP_H);
}
#include "vis_fam.c"
