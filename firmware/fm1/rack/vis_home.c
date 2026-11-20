/* SPDX-License-Identifier: MPL-2.0 */
/* FM1 Phase 2 firmware: update 14, HOME's picture (the "spectral ribbon"), part of app.c (included before draw_home).
 *
 * A time-frequency picture. Nine ribbons, one a frequency band (low at the bottom, high at the top, log spaced, 170 Hz to 13 kHz), each a filled
 * profile of that band's energy over the last seven seconds, in one hue ramp (teal with Clouds in the patch, amber without), dimmer with age, the front
 * (low) ones brighter. At the right a bright vertical "now" edge with a dot at each band's newest level. Grains are small bright dabs at their pitch's
 * band near the now edge; a delay is a fainter repeat of each dab one delay time later; the reverb lengthens each dab's trail; the filter shades the
 * bands above its cutoff and draws a dashed glowing edge at the cutoff's height; FREEZE puts an ice-tinted box round the held stretch (the Clouds
 * buffer's length before the moment it went on, and everything since) in which the ribbons are pale and still; modulation makes the ribbons breathe
 * at the real LFO's pace. A preset change sweeps a seam across, the new hue ramp entering from the now edge, in about half a second.
 *
 * Real: the band energies (from the actual output: the last 1024 samples of the output scope, 23 ms at 44.1 kHz, see hm_spectrum), FREEZE, the filter's
 *       cutoff as heard, the delay's time (the division at the tempo, or FREE MS), FEEDBACK and MIX, the reverb's amount and SIZE, the LFO and modulator
 *       values, the note events (a bar at the band of each held note), the output level.
 * Modelled: the grain dabs. Clouds' own grains are private: births follow DENSITY, SIZE, PITCH, TEXTURE, POSITION and the level as on the Clouds page,
 *       a dab sits at a band drawn in proportion to the live band energies (a grain plays what the buffer holds) and moved by PITCH (a few more are an octave up),
 *       and in a frozen box it sits in the held stretch.
 *       The delay's repeats are the dab again, fainter by FEEDBACK, one delay time later. Not every grain is drawn.
 * Drawing: the history moves on one 3 px column every 100 ms, not every frame, so between those steps only what moves is redrawn differently (the dabs, the now
 * edge's dots, the dashed edge, the freeze outline) and the changed-rows-only transfer sends few rows. The picture's own state is the ribbon arrays and the
 * mote field of the picture kit (nothing else is on screen when HOME is). */

#define HR_X0 8                                   /* the first column's left pixel; column j covers x = 8 + 3j .. 10 + 3j */
#define HR_XN (HR_X0 + HR_NC * 3)                 /* 224: the now edge */
#define HR_YB0 152                                /* the lowest band's baseline (picture rows); the next is 14 higher */
#define HR_DY 14
#define HR_STEP 100u                              /* ms a column */
#define HR_TOP 22                                 /* the picture's top (the name sits above) */
#define HR_BOT 158

typedef struct { uint16_t s0, s1; uint8_t bf, open; } hm_note_t;
static struct {
    float acc[HR_NB];                             /* the band levels of this step's snapshots, summed */
    float flt, ycut, dly, dfb, dms, rev, rsz;   /* eased: filter present, the cutoff's row, delay amount, feedback, time (ms), reverb amount, SIZE */
    float pkl, breath;
    uint32_t ids, gp, bacc;
#ifdef PLAT_HOST
    uint32_t sp_us, sp_n;                         /* the spectrum step's cost, for the host's log */
#endif
    uint16_t step, hz, stepms, nacc, frz_step, mt;   /* steps so far; steps since a non-zero column; ms into this step; snapshots; the step FREEZE went on; ms into a morph (0: none) */
    uint8_t cur[HR_NB];                           /* each band's level now (0-255), for the now edge's dots */
    uint8_t hh, pa, pb, frz, frz_cols, init, cl_on, gone;   /* the newest column's index; the old and new hue ramps; frozen; the held stretch in columns; ... */
    int16_t hmax[HR_NB];                          /* the tallest each ribbon grows (px), with the breathing of this step */
    int8_t vb[6], nbi;
    hm_note_t nb[8];
} hm;

/* hue ramps: 5 stops at 0, 64, 140, 204, 255 (RGB), and the accent for "now" */
static const uint8_t HR_RAMP[5][5][3] = {
    {{4, 16, 26}, {10, 70, 100}, {40, 168, 190}, {120, 224, 228}, {222, 250, 248}},       /* teal: with Clouds */
    {{24, 10, 4}, {100, 46, 16}, {212, 118, 38}, {250, 184, 96}, {255, 238, 200}},        /* amber: without */
    {{10, 16, 30}, {60, 100, 160}, {160, 200, 236}, {226, 240, 252}, {255, 255, 255}},    /* ice: held */
    {{6, 8, 26}, {28, 44, 120}, {80, 112, 222}, {160, 184, 255}, {230, 236, 255}},         /* indigo: modulators (update 15) */
    {{16, 6, 24}, {80, 36, 120}, {170, 90, 210}, {226, 160, 244}, {252, 226, 255}},        /* violet: rhythm and structure */
};
static const uint32_t HR_ACC[5] = {0xFFBE5Au, 0x6EEEFFu, 0xFFFFFFu, 0xFFB850u, 0xFFE070u};
static const uint8_t HR_BB[HR_NB] = {255, 244, 232, 220, 208, 196, 184, 172, 160};   /* the front ribbons brighter (of 256) */
static const uint16_t HR_RECIP[48] = {0, 65535, 32768, 21845, 16384, 13107, 10923, 9362, 8192, 7282, 6554, 5958, 5461, 5041, 4681, 4369, 4096, 3855, 3641, 3449, 3277, 3121, 2979, 2849, 2731, 2621, 2521, 2427, 2341, 2260, 2185, 2114, 2048, 1986, 1928, 1872, 1820, 1771, 1725, 1680, 1638, 1598, 1560, 1524, 1489, 1456, 1425, 1394};

static uint32_t hr_ramp(int pal, int tv)
{
    static const uint8_t T[5] = {0, 64, 140, 204, 255};
    static const uint16_t IV[4] = {1024, 862, 1024, 1285};   /* 65536 / the stop spacing >> 4 ... (spacing 64, 76, 64, 51 -> 256 * 256 / spacing) */
    const uint8_t (*s)[3] = HR_RAMP[pal];
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

static void hm_hmax(float br)                     /* how tall each ribbon may grow (px), breathing with the modulation (br -1..1) */
{
    int b;
    for (b = 0; b < HR_NB; b++) {
        int yb = HR_YB0 - b * HR_DY, h0 = yb - HR_TOP - 3 < 42 ? yb - HR_TOP - 3 : 42;
        hm.hmax[b] = (int16_t)((float)h0 * (1.0f + 0.22f * br * ((b & 1) ? -0.7f : 1.0f)));
    }
}
static void home_reset(void)
{
    int i;
    for (i = 0; i < HR_NB * HR_NC; i++) cp.hist[i] = 0;
    for (i = 0; i < HR_ND; i++) { cp.hd[i].life = 0; cp.hd[i].due = 0; }
    for (i = 0; i < HR_NB; i++) { hm.acc[i] = 0.0f; hm.cur[i] = 0; }
    hm.init = 0; hm.hh = 0; hm.step = 0; hm.hz = 0; hm.stepms = 0; hm.nacc = 0; hm.frz = 0; hm.mt = 0; hm.gp = 0;
    for (i = 0; i < 6; i++) hm.vb[i] = -1;
    hm_hmax(0.0f);
    for (i = 0; i < 8; i++) hm.nb[i].open = 0, hm.nb[i].s1 = 0, hm.nb[i].s0 = 0, hm.nb[i].bf = 255;
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

static float hm_band_p(float f)                   /* a frequency's place on the ribbons' axis: 0 (170 Hz) to 9 (13 kHz) */
{
    float p = fm_log2f(f / 170.0f) * (9.0f / 6.2597f);
    return p < 0.0f ? 0.0f : p > 8.95f ? 8.95f : p;
}
static float hm_note_p(float note, float semis) { return hm_band_p(440.0f * fm_exp2f((note - 69.0f + semis) / 12.0f)); }

static void hm_dab(int xc, float p, int g, int sz, int soft, int sm, uint32_t life, int due, int fl)
{
    int i;
    for (i = 0; i < HR_ND; i++)
        if (!cp.hd[i].life) {
            hdab_t *o = &cp.hd[i];
            o->age = 0; o->life = (uint16_t)life; o->step0 = hm.step; o->due = (int16_t)due;
            o->xc = (int8_t)(xc < 0 ? 0 : xc > HR_NC - 1 ? HR_NC - 1 : xc);
            o->bf = (uint8_t)(p * 16.0f); o->g = (uint8_t)(g > 255 ? 255 : g); o->sz = (uint8_t)sz; o->sm = (uint8_t)sm;
            o->fl = (uint8_t)((soft ? 1 : 0) | fl); o->xj = (uint8_t)(cp_rnd() % 3u);
            return;
        }
}

/* the arrangement and parameters, the engine's live values: one monitor() call a slot (the 1st gives the peaks, the clouds' one is the last) */
static const float *home_frame(uint32_t dt)
{
    static const uint8_t FXS[2] = {RS_FX1, RS_FX2};
    const float *mon = rk_monitor(RS_AMP);
    float k = dt >= 150u ? 1.0f : (float)dt / 150.0f, pk_a = mon[48], pk_b = mon[49], raw = pk_a > pk_b ? pk_a : pk_b;
    int i, cl = -1;
    uint32_t ids = 0, gates = 0, gnew;
    float env[6], note[6], flt = 0.0f, ycut = 26.0f, dly = 0.0f, rev = 0.0f, rsz = 0.5f, dms = 300.0f, dfb = 0.3f, fb;
    int frz = 0;
    for (i = 0; i < 6; i++) {
        env[i] = mon[16 + i];
        note[i] = mon[34 + i];
        gates |= mon[28 + i] >= 1.0f ? 1u << i : 0u;
    }
    hm.breath = (mon[44] * 0.5f + mon[40] * 0.35f + mon[41] * 0.25f);
    hm.pkl = raw;
    for (i = 0; i < RS_N; i++)
        ids = ids * 37u + (uint32_t)rc_patch.mod[i] + ((i == RS_FX1 || i == RS_FX2) && rk_fx_bypass(i) ? 101u : 0u);
    ids = ids * 37u + (uint32_t)cur_entry;
    for (i = 0; i < 2; i++) {                     /* Clouds, the delay, the reverb */
        int s = FXS[i], id = rc_patch.mod[s];
        if (id == 30) { if (!rk_fx_bypass(s)) cl = s; continue; }
        if (id < 31 || id > 33 || rk_fx_bypass(s))
            continue;
        mon = rk_monitor(s);
        if (id == 31) {
            dms = pk_delay_s(mon) * 1000.0f;
            dfb = P(mon, 1);
            dly = 0.35f + 0.65f * P(mon, 3);
        } else if (id == 32) {
            rev = 0.35f + 0.65f * P(mon, 3);
            rsz = P(mon, 0);
        }
    }
    {                                             /* the filter's cutoff as heard (always in the chain; update 26) */
        float e = 0.0f, fc;
        mon = rk_monitor(RS_GLO);
        for (i = 0; i < 6; i++) if (env[i] > e) e = env[i];
        fc = 440.0f * fm_exp2f((P(mon, FLT_P0 + 1) * 120.0f - 30.0f + P(mon, FLT_P0 + 4) * e * 60.0f + P(mon, FLT_SWEEP) * 60.0f - 9.0f) / 12.0f);
        fc = fc < 20.0f ? 20.0f : fc > 20000.0f ? 20000.0f : fc;
        flt = fc < 19000.0f ? 1.0f : 0.0f;        /* open: no line */
        ycut = (float)HR_YB0 - hm_band_p(fc) * (float)HR_DY;
        if (fc < 170.0f) ycut = (float)HR_YB0;
    }
    if (cl >= 0) {
        mon = rk_monitor(cl);
        frz = (int)(P(mon, 10) + 0.5f) == 1;
    }
    hm.cl_on = cl >= 0;
    if (!hm.init) {                               /* the first frame: everything is where it is going */
        hm.flt = flt; hm.ycut = ycut; hm.dly = dly; hm.dfb = dfb; hm.dms = dms; hm.rev = rev; hm.rsz = rsz;
        hm.pa = hm.pb = cl >= 0 ? 0 : 1;
        hm.ids = ids; hm.init = 1; hm.mt = 0;
    } else {
        hm.flt += (flt - hm.flt) * k; hm.ycut += (ycut - hm.ycut) * k; hm.dly += (dly - hm.dly) * k; hm.dfb += (dfb - hm.dfb) * k;
        hm.dms += (dms - hm.dms) * k; hm.rev += (rev - hm.rev) * k; hm.rsz += (rsz - hm.rsz) * k;
        if (ids != hm.ids) {                      /* a preset or a module in or out: the seam sweeps */
            hm.ids = ids;
            hm.pa = hm.pb;
            hm.pb = cl >= 0 ? 0 : 1;
            hm.mt = 1;
        }
    }
    if (hm.mt) {
        hm.mt = (uint16_t)(hm.mt + dt);
        if (hm.mt >= 520u) { hm.mt = 0; hm.pa = hm.pb; }
    }
    /* FREEZE: the box covers the held stretch (the buffer's length before the moment it went on) and what has happened since */
    if (frz && !hm.frz) {
        static const uint16_t BUF_MS[4] = {510, 930, 1020, 1850};
        int qual = (int)(P(mon, 11) + 0.5f) & 3, md = (int)(P(mon, 9) + 0.5f);
        uint32_t ms = BUF_MS[qual] * (rk_fm1_status(5) ? 3u : 2u) / 2u;
        if (md == 2) ms = BUF_MS[qual | 1] * (rk_fm1_status(5) ? 3u : 2u) / 2u;
        hm.frz_cols = (uint8_t)(ms / HR_STEP < 4u ? 4u : ms / HR_STEP > 40u ? 40u : ms / HR_STEP);   /* (as the births' bcols) */
        hm.frz_step = hm.step;
    }
    hm.frz = (uint8_t)frz;
    cp_level(raw, dt);
    cp.freeze = frz;
    /* note events: a bar at the held note's band, from its start to its end (or now) */
    gnew = gates & ~hm.gp;
    for (i = 0; i < 6; i++) {
        if (gnew >> i & 1u) {
            hm_note_t *n = &hm.nb[hm.nbi & 7];
            n->s0 = hm.step; n->s1 = 0; n->open = 1;
            n->bf = (uint8_t)(hm_note_p(note[i], 0.0f) * 16.0f);
            hm.vb[i] = (int8_t)(hm.nbi & 7);
            hm.nbi = (int8_t)((hm.nbi + 1) & 7);
        } else if ((hm.gp >> i & 1u) && !(gates >> i & 1u) && hm.vb[i] >= 0) {
            hm.nb[hm.vb[i]].open = 0;
            hm.nb[hm.vb[i]].s1 = hm.step;
            hm.vb[i] = -1;
        }
    }
    hm.gp = gates;
    /* the spectrum: a snapshot a frame, averaged over the 100 ms step */
    {
        uint8_t lv[HR_NB];
#ifdef PLAT_HOST
        uint32_t t0 = plat_ticks24();
#endif
        if (raw < 0.0004f && cp.lv < 0.003f) {
            for (i = 0; i < HR_NB; i++) lv[i] = 0;
        } else {
            hm_spectrum(lv);
#ifdef PLAT_HOST
            hm.sp_us += (plat_ticks24() - t0) / PLAT_TICKS_PER_US;
            hm.sp_n++;
#endif
        }
        for (i = 0; i < HR_NB; i++) {
            hm.acc[i] += (float)lv[i];
            hm.cur[i] = (uint8_t)((int)hm.cur[i] + (((int)lv[i] - (int)hm.cur[i]) * (lv[i] > hm.cur[i] ? 3 : 1) >> 2));
        }
        hm.nacc++;
    }
    hm.stepms = (uint16_t)(hm.stepms + dt);
    while (hm.stepms >= HR_STEP) {                /* a column */
        int b, nz = 0, ph = (hm.hh + HR_NC) % HR_NC;
        hm.stepms = (uint16_t)(hm.stepms - HR_STEP);
        hm.hh = (uint8_t)((hm.hh + 1) % HR_NC);
        for (b = 0; b < HR_NB; b++) {
            int v = hm.nacc ? (int)(hm.acc[b] / (float)hm.nacc + 0.5f) : 0;
            v = (2 * v * v / 255 + v) / 3;                /* a little convex: quiet bands stay low, strong ones stand up */
            v = v * 6 / 5;
            if (hm.frz)                           /* held: the ribbons settle and stay put */
                v = (3 * (int)cp.hist[b * HR_NC + ph] + v + 2) >> 2;
            v = v > 255 ? 255 : v;
            cp.hist[b * HR_NC + hm.hh] = (uint8_t)v;
            nz |= v;
            hm.acc[b] = 0.0f;
        }
        hm.nacc = 0;
        hm.step++;
        hm.hz = nz ? 0u : (uint16_t)(hm.hz < 1000u ? hm.hz + 1u : hm.hz);
        hm_hmax(fm_clampf(hm.breath, -1.0f, 1.0f) * (rc_patch.mod[RS_MODA] || rc_patch.mod[RS_MODB] ? 1.0f : 0.35f));   /* breathing, as the modulation is at this step */
    }
    /* the grains: births (Clouds' rates, shown at a fifth), their pitch band, the delay's repeats */
    cp.nlive = 0;
    {
        int n = 0;
        for (i = 0; i < HR_ND; i++) {
            hdab_t *o = &cp.hd[i];
            if (!o->life)
                continue;
            if (o->due > 0) {
                o->due = (int16_t)(o->due - (int)dt);
                if (o->due <= 0) { o->due = 0; o->age = 0; o->step0 = hm.step; o->xc = (int8_t)(HR_NC - 1 - (int)(cp_rnd() % 2u)); }
                cp.nlive++;
                continue;
            }
            o->age = (uint16_t)(o->age + dt);
            if (o->age >= o->life || (int)o->xc - (int)(uint16_t)(hm.step - o->step0) < 0)
                o->life = 0;
            else
                cp.nlive++;
            n++;
        }
        (void)n;
    }
    if (cl >= 0) {
        float pos = P(mon, 0), size = P(mon, 1), pitch = fm_clampf(P(mon, 2), -1.0f, 1.0f), dens = P(mon, 3), tex = P(mon, 4), spray = P(mon, 16);
        int md = (int)(P(mon, 9) + 0.5f), nb = 0, cnt, v, bcols;
        float d = dens - 0.5f, rate = 0.0f, lvg = cp.lv * 3.0f > 1.0f ? 1.0f : cp.lv * 3.0f;
        uint32_t lv8 = (uint32_t)(cp.lv * 255.0f);
        d = d < 0.0f ? -d : d;
        md = md < 0 ? 0 : md > 2 ? 2 : md;
        {
            static const uint16_t BM[4] = {510, 930, 1020, 1850};
            uint32_t ms = BM[((int)(P(mon, 11) + 0.5f) & 3) | (md == 2 ? 1 : 0)] * (rk_fm1_status(5) ? 3u : 2u) / 2u;
            bcols = (int)(ms / HR_STEP);
            bcols = bcols < 4 ? 4 : bcols > 40 ? 40 : bcols;
        }
        if (md == 0 && d > 0.03f && lv8 >= 3u)
            rate = (d - 0.03f) * (1.0f / 0.47f) * 70.0f * lvg;
        else if (md == 1 && lv8 >= 3u)
            rate = 14.0f * lvg;
        else if (md == 2 && lv8 >= 3u)
            rate = 12.0f * lvg;
        rate *= 0.3f;
        rate = rate > 14.0f ? 14.0f : rate;
        hm.bacc += (uint32_t)(rate * 10.0f) * dt;
        cnt = 0;
        for (v = 0; v < 6; v++) if (gnew >> v & 1u) nb = 1;
        while ((hm.bacc >= 10000u || (nb && cnt < 3 && md == 0 && d > 0.03f)) && cnt < 8) {
            uint32_t r = cp_rnd();
            int vo = 0, j, g, sz, sm, xc;
            float p;
            if (hm.bacc >= 10000u) hm.bacc -= 10000u;
            cnt++;
            nb = 0;
            {   /* the grain plays what the buffer holds: a band drawn at random in proportion to the live band energies (squared), moved by PITCH */
                uint32_t tot = 0, pick, acc2 = 0;
                for (j = 0; j < HR_NB; j++) tot += (uint32_t)hm.cur[j] * hm.cur[j] >> 6;
                pick = tot ? cp_rnd() % tot : 0u;
                vo = 0;
                for (j = 0; j < HR_NB; j++) {
                    acc2 += (uint32_t)hm.cur[j] * hm.cur[j] >> 6;
                    if (pick < acc2) { vo = j; break; }
                }
            }
            p = (float)vo + 0.5f + pitch * (2.0f / 0.696f);
            if ((r >> 4 & 3u) == 0u) p += 1.44f;  /* some are an octave up */
            p += ((float)(cp_rnd() & 255u) - 127.5f) * (0.5f + spray) / 255.0f;
            p = p < 0.0f ? 0.0f : p > 8.95f ? 8.95f : p;
            sz = size < 0.33f ? 0 : size < 0.66f ? 1 : 2;
            g = 150 + (int)(cp_rnd() % 105u);
            sm = 6 + (int)(size * 10.0f) + (int)(hm.rev * (0.4f + hm.rsz) * 22.0f);
            if (hm.frz)
                xc = (HR_NC - 1) - (hm.step - hm.frz_step) - (int)hm.frz_cols + (int)((pos * 0.7f + 0.15f) * (float)hm.frz_cols) + (int)(cp_rnd() % (uint32_t)(2 + hm.frz_cols / 3)) - (int)(hm.frz_cols / 6);
            else
                xc = HR_NC - 1 - (int)(pos * (float)bcols) - (int)(cp_rnd() % 3u);
            if (xc < 0 || xc > HR_NC - 1)
                continue;
            hm_dab(xc, p, g, sz, (float)(cp_rnd() & 255u) < tex * 255.0f, sm, 1000u + (cp_rnd() % 800u) + (uint32_t)sz * 150u, 0, 0);
            if (hm.dly > 0.05f && (r & 1u)) {                 /* the delay: the dab again, one delay time later, fainter by FEEDBACK, twice (three times when it feeds back a lot) */
                int kk, gk = g;
                fb = 0.25f + hm.dfb * 0.7f;
                for (kk = 1; kk <= (hm.dfb > 0.55f ? 3 : 2); kk++) {
                    gk = (int)((float)gk * fb * (0.5f + 0.5f * hm.dly));
                    if (gk < 24) break;
                    hm_dab(HR_NC - 1, p, gk, sz, 0, sm, 1000u, (int)(hm.dms * (float)kk), 2);
                }
            }
        }
        if (hm.bacc > 30000u)
            hm.bacc = 30000u;
    }
#ifdef PLAT_HOST
    if (hm.sp_n >= 100u) {
        fprintf(stderr, "home spectrum: %u snapshots, %u us each on average (host)\n", hm.sp_n, hm.sp_us / hm.sp_n);
        hm.sp_n = hm.sp_us = 0;
    }
#endif
    (void)ycut;
    return mon;
}

static uint32_t home_sig(void)
{
    uint32_t s = hm.ids * 2654435761u ^ (uint32_t)ui.shown * 40503u ^ (uint32_t)(ui.shown != cur_entry) * 977u ^ (uint32_t)mn.learn * 7u ^ (uint32_t)mn.learn_m * 31u;
    int i;
    s = s * 31u + (uint32_t)(hm.hz <= HR_NC + 1 ? hm.step : 0u);
    s = s * 31u + (uint32_t)hm.mt / 40u + (uint32_t)hm.frz * 3u + (uint32_t)(hm.flt * 4.0f) + (uint32_t)(hm.ycut * 2.0f) + (uint32_t)hm.pa * 5u + (uint32_t)hm.pb * 7u;
    for (i = 0; i < 4; i++)
        s = s * 131u + (uint32_t)(rc_patch.macro[i] * 1000.0f);
    return s;
}

/* ------------------------------------------------------------------------------------------------ drawing */
static inline void hr_px(uint16_t *row, int32_t x, uint16_t c) { row[x] = swap16(c); }
static inline uint16_t hr_565(uint32_t r, uint32_t g, uint32_t b) { return (uint16_t)((r & 0xF800u) | ((g >> 5) & 0x07E0u) | (b >> 11)); }   /* 8.8 fixed in, 565 out */

/* the nine ribbons, back (high) to front (low), each a filled profile: its crest line bright, the body a gradient down to the baseline */
static void hr_ribbons(int ycut, int xseam, int fx0)
{
    int b, j, f, by = cp.by, be = cp.by + cp.bh, pa = hm.pa, pb = hm.pb;
    for (b = HR_NB - 1; b >= 0; b--) {
        int yb = HR_YB0 - b * HR_DY, hmax = hm.hmax[b], idx = (hm.hh + 1) % HR_NC;
        const uint8_t *hb = cp.hist + b * HR_NC;
        if (yb - hmax - 1 >= be || yb + 2 < by)
            continue;
        {   /* the baseline, faint */
            int y = yb + 1;
            if (y >= by && y < be) {
                uint16_t *row = cv_px + (uint32_t)(y - by) * cv_w, c;
                uint32_t bc = hr_ramp(pb, 24);
                int x;
                c = (uint16_t)(((bc >> 19) & 31u) << 11 | ((bc >> 10) & 63u) << 5 | ((bc >> 3) & 31u));
                for (x = HR_X0; x < HR_XN; x++) hr_px(row, x, c);
            }
        }
        for (j = 0; j < HR_NC; j++, idx = idx + 1 >= HR_NC ? 0 : idx + 1) {
            int e0 = hb[idx], e1 = j < HR_NC - 1 ? hb[idx + 1 >= HR_NC ? 0 : idx + 1] : e0, xc = HR_X0 + j * 3, em = e0 > e1 ? e0 : e1, ag, tv;
            uint32_t cc, ch;
            if (em == 0)
                continue;
            ag = 80 + j * 176 / (HR_NC - 1);
            tv = 56 + ((em * (765 - em) >> 9) * 199 >> 8);
            tv = (tv * HR_BB[b] >> 8) * ag >> 8;
            {   /* the colour: the hue ramp (the old one left of the seam, the new one right of it, blended across 12 px), pale in the held box */
                uint32_t ca, cb2;
                int xm = xc + 1;
                if (xseam < 0 || xm >= xseam + 6) cc = hr_ramp(pb, tv);
                else if (xm <= xseam - 6) cc = hr_ramp(pa, tv);
                else { ca = hr_ramp(pa, tv); cb2 = hr_ramp(pb, tv); cc = pk_mix(ca, cb2, (uint32_t)((xm - xseam + 6) * 256 / 12)); }
                if (fx0 >= 0 && xm >= fx0) {
                    uint32_t ic = hr_ramp(2, tv * 3 / 2 > 255 ? 255 : tv * 3 / 2);
                    cc = pk_mix(cc, ic, 200u);
                }
                ch = pk_mix(cc, 0xFFFFFFu, 70u);
            }
            for (f = 0; f < 3; f++) {
                int x = xc + f, hp = ((e0 * (3 - f) + e1 * f) * hmax * 86) >> 16, ytop, y0, y1, y;
                uint32_t r, g, bl, dr, dg, db, c0, c1;
                uint16_t *p;
                if (hp < 1)
                    continue;
                ytop = yb - hp;
                y0 = ytop < by ? by : ytop;
                y1 = yb + 1 > be ? be : yb + 1;       /* rows ytop .. yb */
                if (y0 >= y1)
                    continue;
                c0 = hr_scale(cc, 185u);              /* just under the crest */
                c1 = hr_scale(cc, 40u);               /* at the baseline */
                {
                    int len = hp < 1 ? 1 : hp > 47 ? 47 : hp;
                    int32_t k = HR_RECIP[len];
                    r = (c0 >> 16) << 8; g = ((c0 >> 8) & 255u) << 8; bl = (c0 & 255u) << 8;
                    dr = (uint32_t)(((int32_t)(c1 >> 16) - (int32_t)(c0 >> 16)) * k >> 8);
                    dg = (uint32_t)(((int32_t)((c1 >> 8) & 255u) - (int32_t)((c0 >> 8) & 255u)) * k >> 8);
                    db = (uint32_t)(((int32_t)(c1 & 255u) - (int32_t)(c0 & 255u)) * k >> 8);
                    if (y0 > ytop + 1) {              /* the rows above this pass are skipped: step the gradient to the first one */
                        int skip = y0 - ytop - 1;
                        r += dr * (uint32_t)skip; g += dg * (uint32_t)skip; bl += db * (uint32_t)skip;
                    }
                }
                p = cv_px + (uint32_t)(y0 - by) * cv_w + x;
                for (y = y0; y < y1; y++, p += cv_w) {
                    uint16_t c;
                    if (y == ytop) {
                        c = hr_565((ch >> 16) << 8, ((ch >> 8) & 255u) << 8, (ch & 255u) << 8);
                    } else {
                        c = hr_565(r, g, bl);
                        r += dr; g += dg; bl += db;
                    }
                    if (y < ycut) {                    /* above the filter's cutoff: shaded */
                        c = (uint16_t)((c >> 1) & 0x7BEFu);
                        if (y % 3 == 0) c = (uint16_t)((c >> 1) & 0x7BEFu);
                    }
                    *p = swap16(c);
                }
            }
        }
    }
}

static void hr_chain(void);

static void home_draw(const float *mon, int pf)
{
    int i, ycut = hm.flt > 0.5f ? (int)(hm.ycut + 0.5f) : -1, by = cp.by, be = cp.by + cp.bh;
    int xseam = hm.mt ? HR_XN - (int)((uint32_t)hm.mt * (HR_XN - HR_X0 + 8) / 500u) : -1, fx0 = -1, x, y, pal = hm.mt ? hm.pb : hm.pb;
    uint32_t acc = HR_ACC[pal], now = plat_ms();
    (void)mon; (void)pf;
    if (hm.mt) xseam = xseam < HR_X0 - 6 ? HR_X0 - 6 : xseam;
    if (hm.frz) {                                 /* the held stretch begins the buffer's length before the moment FREEZE went on */
        int c = (HR_NC - 1) - (int)(uint16_t)(hm.step - hm.frz_step) - (int)hm.frz_cols;
        fx0 = HR_X0 + (c < 0 ? 0 : c) * 3;
        acc = HR_ACC[2];
    }
    hr_ribbons(ycut, xseam, fx0);
    /* the note events: a thin bright bar at the band of each held note, from its start to its end (or now) */
    for (i = 0; i < 8; i++) {
        hm_note_t *n = &hm.nb[i];
        int c0, c1, yy, xa, xb;
        if (n->bf == 255)
            continue;
        c0 = (HR_NC - 1) - (int)(uint16_t)(hm.step - n->s0);
        c1 = n->open ? HR_NC - 1 : (HR_NC - 1) - (int)(uint16_t)(hm.step - n->s1);
        if (c1 < 0 || c0 > HR_NC)
            continue;
        c0 = c0 < 0 ? 0 : c0;
        yy = HR_YB0 - (int)n->bf * HR_DY / 16 - 4;
        if (yy < by - 1 || yy > be)
            continue;
        xa = HR_X0 + c0 * 3; xb = HR_X0 + c1 * 3 + 2;
        for (x = xa; x <= xb; x++)
            pk_px(x, yy, acc, hm.cl_on ? 55u : 110u);
        if (n->open)
            pk_blob(0, 0, HR_XN, yy, acc, hm.cl_on ? 120u : 230u);
    }
    /* the grains: dabs at their band, riding the ribbon's crest, a trail to the older side */
    for (i = 0; i < HR_ND; i++) {
        hdab_t *o = &cp.hd[i];
        int colq, b, hp, xd, yd, q, sm;
        uint32_t life, gg, dc;
        if (!o->life || o->due > 0)
            continue;
        colq = (int)o->xc - (int)(uint16_t)(hm.step - o->step0);
        if (colq < 0)
            continue;
        b = o->bf >> 4;
        b = b > HR_NB - 1 ? HR_NB - 1 : b;
        hp = cp.hist[b * HR_NC + (hm.hh + 1 + colq) % HR_NC] * hm.hmax[b] >> 8;
        xd = HR_X0 + colq * 3 + o->xj;
        yd = HR_YB0 - (int)o->bf * HR_DY / 16 - hp - 2;
        life = o->life;
        gg = (uint32_t)o->g * (life - o->age) / life;
        if (o->age < 90u) gg = gg * (o->age + 40u) / 130u;   /* rises in a moment */
        if (yd < by - 8 || yd > be + 8)
            continue;
        dc = fx0 >= 0 && xd >= fx0 ? pk_mix(0xDCEBFFu, 0xFFFFFFu, 120u) : pk_mix(hr_ramp(pal, 235), 0xFFFFFFu, 110u);
        if (o->fl & 2u)
            dc = pk_mix(dc, acc, 90u);
        sm = o->sm;
        for (q = 1; q <= sm; q++)
            pk_px(xd - q, yd, dc, gg * (uint32_t)(sm - q + 1) / (uint32_t)(sm + 1) * ((o->fl & 2u) ? 6u : 11u) >> 4);
        pk_blob(o->sz == 0 ? 0 : 1, o->sz == 2 ? 0 : o->fl & 1u && o->sz == 0, xd, yd, dc, (o->fl & 2u) ? gg * 3u / 4u : gg);   /* small and crisp: 5 or 9 px, hard unless the smallest and soft */
        if (!(o->fl & 2u))
            pk_blob(0, 0, xd, yd, 0xFFFFFFu, gg / 2u);   /* a white core */
    }
    /* the filter: the dashed glowing edge at the cutoff's height */
    if (ycut >= 0 && ycut >= by - 3 && ycut < be + 3) {
        uint32_t ph = (now / 70u) & 7u, la = (uint32_t)(hm.flt * 230.0f);
        for (x = HR_X0 - 2; x < HR_XN + 6; x++) {
            if (((uint32_t)x + ph) % 8u < 5u) {
                pk_px(x, ycut, acc, la);
                pk_px(x, ycut - 1, acc, la / 5u);
                pk_px(x, ycut + 1, acc, la / 5u);
            }
        }
        pk_blob(1, 1, HR_X0 - 2, ycut, acc, (uint32_t)(hm.flt * 150.0f));
        pk_blob(1, 1, HR_XN + 4, ycut, acc, (uint32_t)(hm.flt * 150.0f));
    }
    /* FREEZE: an outlined, ice-tinted box round the held stretch */
    if (hm.frz) {
        int x1 = HR_XN + 3, y0 = HR_TOP + 2, y1 = HR_BOT - 2, w = x1 - fx0;
        uint32_t ic = 0xDCEBFFu, ph = (now / 14u) % (uint32_t)(2 * (w + (y1 - y0)));
        for (x = fx0; x <= x1; x++) {
            pk_px(x, y0, ic, 190u);
            pk_px(x, y1, ic, 190u);
        }
        for (y = y0; y <= y1; y++) {
            pk_px(fx0, y, ic, 190u);
            pk_px(x1, y, ic, 190u);
        }
        for (i = 1; i <= 7; i++) {                 /* the corners */
            pk_px(fx0 + i, y0 + 1, ic, 150u); pk_px(fx0 + 1, y0 + i, ic, 150u);
            pk_px(fx0 + i, y1 - 1, ic, 150u); pk_px(fx0 + 1, y1 - i, ic, 150u);
        }
        /* a glint runs round the outline */
        if ((int)ph < w) pk_blob(1, 1, fx0 + (int)ph, y0, 0xFFFFFFu, 220u);
        else if ((int)ph < w + (y1 - y0)) pk_blob(1, 1, x1, y0 + ((int)ph - w), 0xFFFFFFu, 220u);
        else if ((int)ph < 2 * w + (y1 - y0)) pk_blob(1, 1, x1 - ((int)ph - w - (y1 - y0)), y1, 0xFFFFFFu, 220u);
        else pk_blob(1, 1, fx0, y1 - ((int)ph - 2 * w - (y1 - y0)), 0xFFFFFFu, 220u);
        cv_rect(fx0 + 2, y0 + 2, 24, 10, RGB(6, 12, 24));
        cv_text(fx0 + 4, y0 + 2, &FONT_T, "FRZ", 0xBF7Fu);
    }
    /* the now edge: a bright vertical, a dot at each band's newest level, a tick on the right */
    for (y = HR_TOP + 2; y < HR_BOT - 1; y++) {
        pk_px(HR_XN, y, acc, 200u);
        pk_px(HR_XN + 1, y, acc, 70u);
    }
    for (i = 0; i < HR_NB; i++) {
        int yb = HR_YB0 - i * HR_DY, lv = cp.hist[i * HR_NC + hm.hh], yy = yb - (lv * hm.hmax[i] >> 8) - 1;   /* the newest column's level (it moves with the steps, not between them) */
        uint32_t a = 90u + (uint32_t)lv * 165u / 255u;
        pk_blob(0, 0, HR_XN + 1, yy, acc, a);
        pk_px(HR_XN + 5, yb, acc, 40u + (uint32_t)lv / 2u);
        pk_px(HR_XN + 6, yb, acc, 40u + (uint32_t)lv / 2u);
    }
    /* a preset change: the seam */
    if (xseam >= HR_X0 - 4 && xseam <= HR_XN)
        for (y = HR_TOP; y < HR_BOT; y++) {
            pk_px(xseam, y, HR_ACC[hm.pb], 230u);
            pk_px(xseam - 1, y, HR_ACC[hm.pb], 90u);
            pk_px(xseam + 1, y, HR_ACC[hm.pb], 90u);
        }
    hr_chain();
}

/* the chain: each active process's icon in signal order, joined, a spark running along at the output's level (small, under the ribbons) */
static void hr_chain(void)
{
    static const uint8_t SL[6] = {RS_SRC, RS_SHP, RS_FX1, RS_FX2, RS_MODA, RS_MODB};
    int ic[6], nn = 0, kk, x0;
    uint16_t cc[6];
    for (kk = 0; kk < 6; kk++) {
        int id = rc_patch.mod[SL[kk]];
        if (!id)
            continue;
        ic[nn] = module_icon(id);
        cc[nn] = (rk_fm1_status(4) >> SL[kk] & 1) ? K_WARN : ((SL[kk] == RS_FX1 || SL[kk] == RS_FX2) && rk_fx_bypass(SL[kk])) ? K_FAINT : kk == 0 ? K_SRC : kk == 1 ? K_SHP : kk < 4 ? K_FX : K_MOD;
        nn++;
    }
    x0 = 120 - (nn - 1) * 15;
    for (kk = 0; kk < nn; kk++) {
        int32_t cx = x0 + kk * 30;
        if (kk) {
            uint32_t t = (plat_ms() / 4u + (uint32_t)kk * 40u) % 30u;
            pk_line(cx - 30 + 11, 166, cx - 11, 166, 0x9098B8u, 40u);
            pk_blob(0, 0, cx - 30 + 11 + (int32_t)t * 8 / 30, 166, 0xE8ECFFu, (uint32_t)(50 + hm.pkl * 4.0f * 195.0f > 255.0f ? 255.0f : 50 + hm.pkl * 4.0f * 195.0f));
        }
        pk_icon_draw(ic[kk], cx - 6, 160, cc[kk]);
    }
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

/* the four macros under the picture, in the Clouds page's read-outs: each by the first thing it moves; LEARN's chosen one blinks gold */
static void home_readouts(void)
{
    uint32_t now = plat_ms();
    int k, key = PG_HOME << 4;
    cp_ro_begin();
    for (k = 0; k < 4; k++) {
        float v = rc_patch.macro[k], amt;
        int r, nt = 0, src = 0, via, sel = mn.learn && !mn.learn_src && mn.learn_m == k, q;
        uint16_t first = 0, kc = KNOB_C[k];
        char t[24], l[24];
        int32_t g = cp_ro_act(k, v, key);
        for (r = 0; r < 18; r++) {
            uint16_t d = route_dst(r, &src, &via, &amt);
            if (d && src == 1 + k) {
                if (!nt) first = d;
                nt++;
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
        while (text_w(&FONT_T, l) > 56 && q > 1) l[--q] = 0;
        if (sel && (now / 250u) & 1u)
            kc = K_GOLD;
        int_text((int32_t)(v * 100.0f + 0.5f), t);
        cp_ro_cell(k, l, t, v, 0, 0, kc, g);
    }
    cp.pkey = key;
    cp_send(CP_RY, 40, cp.rh + CP_H);
}
#include "vis_fam.c"
