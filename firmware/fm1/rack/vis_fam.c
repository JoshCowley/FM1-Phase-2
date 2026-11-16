/* SPDX-License-Identifier: MPL-2.0 */
/* FM1 Phase 2 firmware: update 15, the picture family ("option C"), part of app.c at the end of vis_home.c.
 *
 * One visual language for every page that has a picture, after the HOME ribbons the owner approved and the page mock-ups drawn beside them:
 * ONE hero object per page on near-black, depth (stacked slices drawn back to front, each with a dark underside that hides the lines behind it:
 * hidden-line terrain), the real signal as the material in fine 1 px lines or filled gradient profiles, one hue ramp per process family and one
 * bright accent for "now", glow used only for the accent, no text in the window beyond the page's header and its four read-outs.
 *
 * Hue ramps (hr_ramp, vis_home.c): TEAL = effects and processing (Filter, Delay, Reverb, Chorus, Clouds; HOME with Clouds); AMBER = sound
 * sources and the voice (Braids, Rings, AMP); INDIGO = modulators (LFO, Tides, Peaks, Func); VIOLET = rhythm and structure (Grids, the
 * sequencer, arp and chord pages); ICE = held (a frozen Clouds) and links (the matrix). The accent is each ramp's own (HR_ACC).
 *
 * The kit (this file's top): fm_terrain (slices with perspective, occlusion and an accent slice), fm_curve (a filled gradient profile under
 * a curve), fm_seg (a fine line), fm_dash (a dashed guide), fm_node (the "now" dab). History is stepped at a low rate (a slice every 100 to 150
 * ms) and lives in the one shared state block (cp.tr, aliased on the ribbon arrays: no new RAM); every picture is composed in the same five
 * 40-row bands as before, with plain writes (two pixels at a time for the fills), no floating point and no maths functions in a pixel loop, and
 * is shown at most 12.5 times a second (so that no page sends more rows a second than the Clouds page did).
 *
 * What is real, modelled and illustrative is listed per page in README.md. */

#define FM_NS 16                                  /* at most this many slices of at most FM_NP points in cp.tr (720 bytes) */
#define FM_NP 45
#define FM_MIN_MS 80u                             /* the family's frame interval: 12.5 pictures a second */
#define FR_TEAL 0
#define FR_AMBER 1
#define FR_ICE 2
#define FR_INDIGO 3
#define FR_VIOLET 4

static struct {
    uint16_t acc, acc2;                           /* ms since the last history step (two clocks) */
    uint8_t hot;                                  /* history steps still to go before the picture has settled (0: nothing moves) */
    uint8_t fl[3], vprev[6], ph;                  /* Grids' flares per lane; the voices' last envelope (AMP); a phase */
    uint8_t me[12];                               /* Rings: each mode's energy */
    uint16_t mp[12];                              /* ... and phase */
    int16_t a, b;
} fm;
#define FM_TR (cp.tr)

static inline uint16_t fm_c565(uint32_t c) { return (uint16_t)(((c >> 8) & 0xF800u) | ((c >> 5) & 0x07E0u) | ((c >> 3) & 0x1Fu)); }
static inline uint16_t fm_sw(uint32_t c) { return swap16(fm_c565(c)); }     /* as the canvas holds it */
static uint32_t fm_ramp(int ramp, int tv) { return hr_ramp(ramp, tv); }
static uint32_t fm_acc(int ramp) { return HR_ACC[ramp]; }
static inline void fm_put(int32_t x, int32_t y, uint16_t sw)
{
    y -= cp.by;
    if ((uint32_t)x < 240u && (uint32_t)y < (uint32_t)cp.bh)
        cv_px[(uint32_t)y * cv_w + (uint32_t)x] = sw;
}
/* a fine 1 px line, written plainly (sw: the colour as the canvas holds it) */
static void fm_seg(int32_t x0, int32_t y0, int32_t x1, int32_t y1, uint16_t sw)
{
    int32_t dx = pk_abs(x1 - x0), dy = -pk_abs(y1 - y0), sx = x0 < x1 ? 1 : -1, sy = y0 < y1 ? 1 : -1, err = dx + dy, g = 300;
    if ((y0 < cp.by && y1 < cp.by) || (y0 >= cp.by + cp.bh && y1 >= cp.by + cp.bh))
        return;
    while (g--) {
        int32_t e2 = 2 * err;
        fm_put(x0, y0, sw);
        if (x0 == x1 && y0 == y1)
            break;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}
/* a dashed guide line (the filter's cutoff, a frozen box): every third pixel, laid over what is there */
static void fm_dash(int32_t x0, int32_t y0, int32_t x1, int32_t y1, uint32_t c, uint32_t a)
{
    int32_t n = pk_abs(x1 - x0) > pk_abs(y1 - y0) ? pk_abs(x1 - x0) : pk_abs(y1 - y0), i;
    for (i = 0; i <= n; i += 3)
        pk_px(x0 + (x1 - x0) * i / (n ? n : 1), y0 + (y1 - y0) * i / (n ? n : 1), c, a);
}
static void fm_node(int32_t x, int32_t y, int ramp, uint32_t a)    /* the "now" dab: the ramp's accent, a white heart */
{
    pk_blob(2, 1, x, y, fm_acc(ramp), a * 3u / 4u);
    pk_blob(1, 0, x, y, 0xFFFFFFu, a);
}

/* ---- terrain: slices stacked in depth. d: ns slices of np heights (0-255), slice 0 the farthest; the geometry is a trapezoid (a baseline row,
 * a left and a right edge and a height for 255 at the far and the near slice, the slices between spaced closer at the back). Each slice is drawn
 * back to front: its crest as a fine line, a short gradient body under it, then the dark underside down to its baseline, which hides what is
 * behind. bt: each slice's brightness (0-255) or 0 for a fall-off with depth; bright (0-256) scales all; sel: the accent slice (-1 none). */
typedef struct { int16_t y0, y1, xl0, xr0, xl1, xr1, h0, h1; } fm_geo_t;
static void fm_sg(const fm_geo_t *g, int ns, int s, int32_t *yb, int32_t *xl, int32_t *xr, int32_t *hm)
{
    int32_t t = ns > 1 ? s * 256 / (ns - 1) : 256, e = (t + (t * t >> 8)) >> 1;
    *yb = g->y0 + ((g->y1 - g->y0) * e >> 8);
    *xl = g->xl0 + ((g->xl1 - g->xl0) * e >> 8);
    *xr = g->xr0 + ((g->xr1 - g->xr0) * e >> 8);
    *hm = g->h0 + ((g->h1 - g->h0) * e >> 8);
}
static void fm_terrain(const uint8_t *d, int np, int ns, const fm_geo_t *g, int ramp, uint32_t bright, const uint8_t *bt, int sel)
{
    int s, by = cp.by, be = cp.by + cp.bh;
    int16_t ys[124];
    uint32_t dark = (uint32_t)fm_sw(0x020307u);
    for (s = 0; s < ns; s++) {
        int32_t yb, xl, xr, hm, w, n, j, step, t = ns > 1 ? s * 256 / (ns - 1) : 256;
        uint32_t br, c, cs[6], fillw[12], k;
        const uint8_t *q = d + s * np;
        fm_sg(g, ns, s, &yb, &xl, &xr, &hm);
        w = xr - xl;
        br = bt ? bt[s] : 52u + (uint32_t)(203 * t >> 8);
        br = br * bright >> 8;
        if (w < 8 || br < 8u || yb + 1 < by || yb - hm - 2 >= be)
            continue;
        n = (w >> 1) + 1;
        step = ((np - 1) << 16) / w;
        for (j = 0; j < n; j++) {
            int32_t uu = j * 2 * step, i = uu >> 16, f = (uu >> 8) & 255, i1 = i + 1 < np ? i + 1 : np - 1, h = (q[i] * (256 - f) + q[i1] * f) >> 8;
            ys[j] = (int16_t)(yb - (h * hm >> 8));
        }
        c = fm_ramp(ramp, (int)br);
        cs[0] = c;
        for (k = 0; k < 12; k++)                   /* the body: a gradient from the crest to dark */
            fillw[k] = (uint32_t)fm_sw(hr_scale(fm_ramp(ramp, (int)(br * 150u >> 8)), 8u + 220u * (11u - k) * (11u - k) / 121u));
        for (j = 0; j < n; j++) {
            int32_t xe = (xl + 2 * j) & ~1, ya = ys[j], y, yz = yb < be - 1 ? yb : be - 1;
            uint32_t *p;
            if (xe < 0 || xe > 238)
                continue;
            if (ya < by) ya = by;
            if (ya > yz)
                continue;
            p = (uint32_t *)(void *)(cv_px + (uint32_t)(ya - by) * cv_w + (uint32_t)xe);
            for (y = ya; y <= yz; y++, p += cv_w / 2u) {
                int32_t r = y - ys[j];
                uint32_t cc = r < 1 ? 0u : r < 13 ? fillw[r - 1] : dark;
                *p = r < 1 ? (uint32_t)fm_sw(c) * 0x10001u : cc | cc << 16;
            }
        }
        {   /* the crest: a fine line (the accent slice in the accent, with a faint line above it) */
            uint16_t cr = fm_sw(hr_scale(fm_ramp(ramp, (int)(br + 40u > 255u ? 255u : br + 40u)), 256u)), ca = fm_sw(fm_acc(ramp)), ch = fm_sw(hr_scale(fm_acc(ramp), 90u));
            for (j = 1; j < n; j++) {
                int32_t x0 = xl + 2 * (j - 1), x1 = xl + 2 * j;
                if (s == sel) {
                    fm_seg(x0, ys[j - 1] - 1, x1, ys[j] - 1, ch);
                    fm_seg(x0, ys[j - 1], x1, ys[j], ca);
                } else {
                    fm_seg(x0, ys[j - 1], x1, ys[j], cr);
                }
            }
        }
    }
}
static void fm_push(uint8_t *d, int np, int ns, const uint8_t *nw)    /* a new nearest slice; the others one step back */
{
    int i;
    for (i = 0; i < (ns - 1) * np; i++)
        d[i] = d[i + np];
    for (i = 0; i < np; i++)
        d[(ns - 1) * np + i] = nw[i];
}
/* a filled gradient profile: h (240 heights, 0-255) as a curve over base, hm px for 255; light under the line falling away to dark; the line fine and bright */
static void fm_curve(const uint8_t *h, int32_t base, int32_t hm, int ramp, uint32_t bright, int32_t x0, int32_t x1, int32_t depth)
{
    int32_t x, by = cp.by, be = cp.by + cp.bh, py = 0;
    uint32_t gl[40], ln = (uint32_t)fm_sw(fm_ramp(ramp, (int)(bright * 230u >> 8) + 25)), k, c = fm_ramp(ramp, (int)(bright * 200u >> 8));
    for (k = 0; k < 40u; k++) {
        uint32_t sc = 150u * (40u - k) * (40u - k) / 1600u + 6u;
        gl[k] = (uint32_t)fm_sw(hr_scale(c, sc));
    }
    for (x = x0; x < x1; x++) {
        int32_t yt = base - (h[x] * hm >> 8), ya = yt < by ? by : yt, yz = base < be - 1 ? base : be - 1, y;
        uint16_t *p;
        if (ya <= yz) {
            p = cv_px + (uint32_t)(ya - by) * cv_w + (uint32_t)x;
            for (y = ya; y <= yz; y++, p += cv_w) {
                int32_t r = (y - yt) * 40 / (depth > 0 ? depth : 1);
                *p = (uint16_t)(r < 0 ? 0 : r >= 40 ? gl[39] : gl[r]);
            }
        }
        if (x > x0)
            fm_seg(x - 1, py, x, yt, (uint16_t)ln);
        py = yt;
    }
}
static int32_t fm_slice_end(const fm_geo_t *g, int ns, int s, int tail, const uint8_t *d, int np)   /* the row of a slice's last point */
{
    int32_t yb, xl, xr, hm;
    fm_sg(g, ns, s, &yb, &xl, &xr, &hm);
    (void)tail;
    return yb - (d[s * np + np - 1] * hm >> 8);
}

/* ------------------------------------------------------------------------------------------------ the pictures */
static uint32_t fm_bright(void) { return 160u + (uint32_t)cp.gain * 96u / 255u; }      /* silent: dim, not dark */
static float fm_envmax(const float *mon)
{
    float e = 0.0f;
    int i;
    for (i = 0; i < 6; i++)
        if (mon[16 + i] > e) e = mon[16 + i];
    return e;
}
static int fm_hist_h(int sc)                      /* which modulator history a modulation picture shows (vz.hist) */
{
    if (sc == SC_LFO)
        return 4 + mn.sub % 3;
    return page_slot(mn.page) == RS_MODB ? 2 : 0;
}

/* ---- BRAIDS (SOURCE): the waveform as a terrain over TIMBRE: the nearest slice TIMBRE 1, the farthest 0, the current TIMBRE the accent slice */
static const fm_geo_t G_SRC = {58, 172, 54, 186, 14, 226, 30, 78};
static void fm_braids_step(const float *mon, uint32_t dt)
{
    static const uint8_t SH[6] = {2, 3, 1, 5, 4, 0};
    int fam = pk_fam((int)(P(mon, 0) + 0.5f)), sh = SH[fam], s, i, last = (int)mon[62], note = last >= 0 && last < 6 ? (int)mon[34 + last] : 0;
    int cyc = 3 + (note > 0 ? (note - 40) / 14 : 0), p2 = (int)(P(mon, 2) * 255.0f);
    float e = fm_envmax(mon);
    uint32_t amp8 = 90u + (uint32_t)(e * 165.0f);
    cyc = cyc < 2 ? 2 : cyc > 6 ? 6 : cyc;
    if (e > 0.02f)
        pk.sp[0] = (uint16_t)(pk.sp[0] + dt * (uint32_t)(30.0f + e * 90.0f));
    for (s = 0; s < 14; s++) {
        int32_t ts = s * 255 / 13;
        for (i = 0; i < FM_NP; i++) {
            int32_t u = i * 255 / 44, p = u * cyc + (pk.sp[0] >> 8) + s * 5, w1 = pk_wave(sh, p), w2 = pk_wave(sh, 2 * p + p2 + ts / 3), kb = ts * 3 / 4, w = (w1 * (256 - kb) + w2 * kb) >> 8;
            int32_t win = cp_sin(u >> 1), h = 100 + (((w >> 6) * 90 >> 8) * (int32_t)amp8 >> 8);
            h = h * win >> 14;
            FM_TR[s * FM_NP + i] = (uint8_t)(h < 0 ? 0 : h > 255 ? 255 : h);
        }
    }
    fm.a = (int16_t)(P(mon, 1) * 13.0f + 0.5f);       /* the accent slice: TIMBRE */
    fm.b = (int16_t)(e * 255.0f);
}
static void fm_braids_draw(void)
{
    fm_terrain(FM_TR, FM_NP, 14, &G_SRC, FR_AMBER, fm_bright(), 0, fm.a);
}

/* ---- RINGS: the resonator's modes as a stack of standing waves, the fundamental nearest; each mode's energy from the strike, how long it rings from DAMPING, how many from BRIGHTNESS */
static const fm_geo_t G_RING = {52, 172, 60, 180, 14, 226, 24, 70};
static void fm_rings_step(const float *mon, uint32_t dt, uint32_t edge)
{
    int k, s, i;
    float life = 0.5f + P(mon, 3) * 4.5f, stc = P(mon, 1), br = P(mon, 2);
    for (k = 0; k < 12; k++) {
        uint32_t tau = (uint32_t)(life * 330.0f / (1.0f + (float)k * 0.45f)) + 40u;
        if (edge)
            fm.me[k] = (uint8_t)(256.0f - (float)k * 256.0f * (1.0f - br) / 11.0f * 0.9f - (float)k * 4.0f);
        fm.me[k] = (uint8_t)(fm.me[k] - (fm.me[k] * dt / tau) - (fm.me[k] && dt > 8u ? 1 : 0));
        fm.mp[k] = (uint16_t)(fm.mp[k] + dt * (uint32_t)(14.0f + (float)k * 10.0f + stc * (float)k * 16.0f));
        if (fm.me[k] > 2) fm.hot = 3;
    }
    for (s = 0; s < 12; s++) {
        k = 11 - s;
        for (i = 0; i < FM_NP; i++) {
            int32_t u = i * 255 / 44, v = cp_sin((u * (k + 1)) >> 1) * cp_cos(fm.mp[k] >> 8) >> 14, h = 14 + ((int32_t)fm.me[k] * (v + 16384) >> 15);
            FM_TR[s * FM_NP + i] = (uint8_t)(h > 255 ? 255 : h < 0 ? 0 : h);
        }
    }
    fm.a = (int16_t)(P(mon, 4) * 44.0f);              /* POSITION: where it is struck, a dab on the nearest mode */
}
static void fm_rings_draw(void)
{
    uint8_t bt[12];
    int s;
    for (s = 0; s < 12; s++)
        bt[s] = (uint8_t)(60 + fm.me[11 - s] * 150 / 256 + s * 4);
    fm_terrain(FM_TR, FM_NP, 12, &G_RING, FR_AMBER, fm_bright(), bt, 11);
    {   /* where it is struck */
        int32_t yb, xl, xr, hm, x;
        fm_sg(&G_RING, 12, 11, &yb, &xl, &xr, &hm);
        x = xl + (xr - xl) * fm.a / 44;
        fm_node(x, yb - (FM_TR[11 * FM_NP + fm.a] * hm >> 8), FR_AMBER, 160u + (uint32_t)fm.me[0] * 95u / 256u);
    }
}

/* ---- FILTER: the response as a filled gradient curve with the real spectrum receding behind it; the cutoff a dashed guide, a bloom on the resonance */
static const fm_geo_t G_FSP = {38, 98, 80, 206, 70, 228, 8, 34};
#define FF_BASE 176
#define FF_H 96
static void fm_filter_step(const float *mon, uint32_t dt)
{
    int type = (int)(P(mon, 0) + 0.5f), x;
    float cut = P(mon, 1), reso = P(mon, 2), q = 0.5f + reso * reso * 25.0f, fc, e = fm_envmax(mon);
    type = type < 0 ? 0 : type > 5 ? 5 : type;
    fc = 440.0f * fm_exp2f((cut * 120.0f - 30.0f + P(mon, 4) * e * 60.0f - 9.0f) / 12.0f);
    fc = fc < 20.0f ? 20.0f : fc > 20000.0f ? 20000.0f : fc;
    for (x = 0; x < 240; x += 2) {                /* the curve's height 0-255 every second column (-36 to +18 dB), the others between */
        float f = 20.0f * fm_exp2f((float)x / 239.0f * 9.9658f), m = fmag(type, f / fc, q, reso), db = 6.0206f * fm_log2f(m + 1e-6f);
        int h;
        db = db < -36.0f ? -36.0f : db > 18.0f ? 18.0f : db;
        h = (int)((db + 36.0f) * (255.0f / 54.0f));
        cp.colh[x] = (uint8_t)h;
        if (x)
            cp.colh[x - 1] = (uint8_t)((cp.colh[x - 2] + h) >> 1);
    }
    cp.colh[239] = cp.colh[238];
    fm.a = (int16_t)(fm_log2f(fc / 20.0f) / 9.9658f * 239.0f);                   /* the cutoff's column */
    fm.b = (int16_t)(reso * 255.0f);
    fm.acc = (uint16_t)(fm.acc + dt);
    if (fm.acc >= 100u) {                         /* the real spectrum of the output, 9 bands, a slice every 100 ms */
        uint8_t sp[9];
        fm.acc = 0;
        hm_spectrum(sp);
        fm_push(cp.colg, 9, 10, sp);
    }
}
static void fm_filter_draw(void)
{
    int32_t cx = fm.a, y, yp;
    fm_terrain(cp.colg, 9, 10, &G_FSP, FR_TEAL, 190u, 0, -1);
    fm_curve(cp.colh, FF_BASE, FF_H, FR_TEAL, fm_bright(), 0, 240, 62);
    fm_dash(cx, 28, cx, FF_BASE, 0xFFF0D0u, 90u);
    yp = FF_BASE - (cp.colh[cx < 0 ? 0 : cx > 239 ? 239 : cx] * FF_H >> 8);
    y = (int32_t)fm.b;
    pk_blob(3, 1, cx, yp, fm_acc(FR_TEAL), 50u + (uint32_t)y * 2u / 3u);
    pk_blob(1, 0, cx, yp, 0xFFFFFFu, 120u + (uint32_t)y / 2u);
}

/* ---- the modulators (LFO, TIDES, PEAKS, FUNC): the real output as a curve with its history receding; the node at the newest sample, a bright trail along the nearest edge */
static const fm_geo_t G_MOD = {50, 168, 70, 214, 18, 222, 44, 76};
#define FM_MS 12
static void fm_mod_step(int sc, const float *mon, uint32_t dt)
{
    int h = fm_hist_h(sc), s, i;
    float v = (float)vz.hist[h][(vz.hist_w + 113u) % 114u] * 0.01f;
    (void)dt;
    for (s = 0; s < FM_MS; s++) {
        int k = FM_MS - 1 - s, st = 69 - 5 * k;
        for (i = 0; i < FM_NP; i++) {
            int32_t val = vz.hist[h][(vz.hist_w + (uint32_t)(st + i)) % 114u], hh = 128 + val * 124 / 100;
            FM_TR[s * FM_NP + i] = (uint8_t)(hh < 4 ? 4 : hh > 255 ? 255 : hh);
        }
    }
    if (sc != SC_TIDES && sc != SC_LFO && v - pk.prev > 0.22f) {      /* a trigger (Peaks, Func): a ring and sparks leave the node */
        int k;
        int32_t yb, xl, xr, hm, ny;
        fm_sg(&G_MOD, FM_MS, FM_MS - 1, &yb, &xl, &xr, &hm);
        ny = yb - (FM_TR[(FM_MS - 1) * FM_NP + FM_NP - 1] * hm >> 8);
        pk_spark(xr, ny, 12, 4, 0, 0, 800u);
        for (k = 0; k < 4; k++)
            pk_spark(xr, ny, 13, 2, -pk_vel(8000, 22000), pk_vel(-9000, 18000), 500u + cp_rnd() % 400u);
    }
    pk.prev = v;
}
static void fm_mod_draw(int sc)
{
    uint8_t bt[FM_MS];
    int s;
    int32_t yb, xl, xr, hm, px = 0, py = 0;
    (void)sc;
    for (s = 0; s < FM_MS; s++)
        bt[s] = (uint8_t)(46 + s * 200 / (FM_MS - 1));
    fm_terrain(FM_TR, FM_NP, FM_MS, &G_MOD, FR_INDIGO, fm_bright(), bt, -1);
    for (s = 0; s < FM_MS; s++) {                /* the trail: each slice's newest point, joined */
        int32_t x, y;
        fm_sg(&G_MOD, FM_MS, s, &yb, &xl, &xr, &hm);
        x = xr;
        y = yb - (FM_TR[s * FM_NP + FM_NP - 1] * hm >> 8);
        if (s)
            fm_seg(px, py, x, y, fm_sw(hr_scale(fm_acc(FR_INDIGO), 60u + (uint32_t)s * 14u)));
        px = x;
        py = y;
    }
    fm_node(px, py, FR_INDIGO, 255u);
    pk_motes(256);
}

/* ---- GRIDS: the 96 step levels as pegs on three lanes receding into depth, the real clock step a bright scan line, a flare where a hit triggers */
static void fm_grids_step(const float *mon)
{
    int st = (int)mon[47] & 31, lane;
    for (lane = 0; lane < 3; lane++)
        fm.fl[lane] = (uint8_t)(fm.fl[lane] - (fm.fl[lane] >> 2) - (fm.fl[lane] ? 1 : 0));
    if (st != pk.step) {
        pk.step = (uint8_t)st;
        for (lane = 0; lane < 3; lane++)
            if (mon[64 + lane * 32 + st] > 0.3f)
                fm.fl[lane] = 255;
    }
}
static void fm_grids_draw(const float *mon)
{
    static const int16_t LY[3] = {82, 120, 168}, LX0[3] = {50, 34, 14}, LX1[3] = {190, 206, 226}, LH[3] = {22, 32, 44};
    int st = (int)mon[47] & 31, lane, s, ramp = FR_VIOLET;
    for (lane = 0; lane < 3; lane++) {
        int32_t y = LY[lane], x0 = LX0[lane], x1 = LX1[lane], hh = LH[lane], bl = 70 + lane * 50;
        fm_seg(x0 - 6, y, x1 + 6, y, fm_sw(hr_ramp(ramp, 52 + lane * 20)));
        for (s = 0; s < 32; s++) {
            float lv = mon[64 + lane * 32 + s];
            int32_t x = x0 + (x1 - x0) * s / 31, h = 2 + (int32_t)(lv * (float)hh);
            uint32_t tv = (uint32_t)(bl + lv * 150.0f);
            if (s == st)
                tv = 255u;
            if (lv < 0.06f) {
                fm_put(x, y - 1, fm_sw(hr_ramp(ramp, 90)));
                continue;
            }
            fm_seg(x, y, x, y - h, fm_sw(hr_ramp(ramp, (int)tv > 255 ? 255 : (int)tv)));
            fm_put(x, y - h - 1, fm_sw(hr_scale(0xFFFFFFu, (uint32_t)(80 + lv * 170.0f))));
            if (s % 4 == 0)
                fm_put(x, y + 3, fm_sw(hr_ramp(ramp, 70)));
        }
        if (fm.fl[lane])
            pk_blob(2, 1, x0 + (x1 - x0) * st / 31, y - 4, fm_acc(ramp), (uint32_t)fm.fl[lane]);
    }
    {   /* the clock step: a scan line down through the lanes in perspective */
        int32_t xa = LX0[0] + (LX1[0] - LX0[0]) * st / 31, xb = LX0[2] + (LX1[2] - LX0[2]) * st / 31;
        fm_dash(xa, 56, 82, 82, 0xFFFFFFu, 120u);
        fm_dash(xa, 82, xb, 168, 0xFFFFFFu, 90u);
        pk_blob(0, 0, xa, 82, 0xFFFFFFu, 255u);
    }
    pk_motes(256);
}

/* ---- DELAY: receding terraces, each repeat a fainter, further copy of the profile of the sound (the real level, a slice every 40 ms, delayed by the repeat's number of delay times) */
#define FD_RING 480                               /* the level history lives in cp.tr[480..607] (cp.colg) */
static void fm_delay_step(const float *mon, uint32_t dt, uint8_t *bt, int32_t *y0)
{
    float fb = P(mon, 1), mix = P(mon, 3), a = 1.0f, ak;
    uint32_t tms = (uint32_t)(pk_delay_s(mon) * 1000.0f);
    int k, s, i, n = 1 + (int)(fb * 8.0f + 0.5f);
    uint8_t *ring = FM_TR + FD_RING;
    fm.acc2 = (uint16_t)(fm.acc2 + dt);
    if (fm.acc2 >= 40u) {
        fm.acc2 = 0;
        for (i = 0; i < 127; i++)
            ring[i] = ring[i + 1];
        ring[127] = (uint8_t)(cp.lv * 255.0f);
        if (ring[127] > 3) fm.hot = 20;
        else if (fm.hot) fm.hot--;
    }
    for (s = 0; s < 10; s++) {
        k = 9 - s;
        ak = 0.0f;
        if (k < n) {
            a = 1.0f;
            for (i = 0; i < k; i++) a *= fb;
            ak = a;
        }
        bt[s] = k < n ? (uint8_t)(40.0f + 215.0f * ak * (0.35f + 0.65f * mix) > 255.0f ? 255.0f : 40.0f + 215.0f * ak * (0.35f + 0.65f * mix)) : 0;
        {
            int o = (int)(k * tms / 40u);
            int st;
            o = o > 82 ? 82 : o;
            st = 128 - FM_NP - o;
            for (i = 0; i < FM_NP; i++) {
                float v = (float)ring[st + i] * (ak > 0.0f ? ak : 0.0f);
                FM_TR[s * FM_NP + i] = (uint8_t)(v > 255.0f ? 255.0f : v);
            }
        }
    }
    {
        int32_t sp = 70 + (int32_t)(tms / 8u);
        *y0 = 176 - (sp > 140 ? 140 : sp);
    }
}
static uint8_t fd_bt[10];
static int32_t fd_y0;
static void fm_delay_draw(void)
{
    fm_geo_t g = {0, 176, 66, 174, 14, 226, 18, 70};
    g.y0 = (int16_t)fd_y0;
    fm_terrain(FM_TR, FM_NP, 10, &g, FR_TEAL, 256u, fd_bt, 9);
}

/* ---- REVERB: a depth volume, nested arches receding into fog; SIZE sets how deep it goes, DAMP how fast the fog thickens, DIFFUSE how the lines waver; the held tail (modelled from the output level) brightens it, each note's wavefront runs back through it */
static void fm_reverb_step(const float *mon, uint32_t dt)
{
    float size = P(mon, 0), tau = 380.0f + size * 2600.0f;
    cp_level(mon[48] > mon[49] ? mon[48] : mon[49], 0u);
    pk.tail -= pk.tail * (float)dt / tau;
    if (cp.lv > pk.tail) pk.tail = cp.lv;
    if (pk.tail < 0.004f) pk.tail = 0.0f;
    fm.ph = (uint8_t)(fm.ph + dt / 40u);
}
static void fm_reverb_draw(const float *mon)
{
    float size = P(mon, 0), damp = P(mon, 1), dif = P(mon, 2);
    int k, i, N = 18, hot = -1;
    int32_t y1 = 176, yh = 56, w1 = 112, wh = 58, H1 = 78, dep = (int32_t)(80.0f + size * 176.0f), now = (int32_t)plat_ms(), j;
    uint32_t tail = (uint32_t)(pk.tail * 255.0f), fog = (uint32_t)(150.0f + damp * 380.0f), life = 600u + (uint32_t)(size * 2000u);
    int32_t pk_k = -1;
    for (j = 0; j < 8; j++) {                     /* a note's wavefront: the arch it has reached */
        uint32_t age = vz.pulse_t[j] ? (uint32_t)(now - (int32_t)vz.pulse_t[j]) : 99999u;
        if (age < life && (pk_k < 0 || (int32_t)(age * (uint32_t)N / life) < pk_k))
            pk_k = (int32_t)(age * (uint32_t)(N * 256 / 256) / life), hot = (int)(255u - age * 255u / life);
    }
    for (k = N - 1; k >= 0; k--) {
        int32_t t = k * 256 / (N - 1) * dep >> 8, e = (t + (t * t >> 8)) >> 1, yb = y1 - ((y1 - yh) * e >> 8), w = w1 - ((w1 - wh) * e >> 8), H = H1 - (H1 * 80 / 100 * e >> 8), px = 0, py = 0;
        uint32_t br = (uint32_t)(256 - (int32_t)((uint32_t)t * fog >> 8 > 230u ? 230u : (uint32_t)t * fog >> 8));
        uint16_t c;
        br = (br * (90u + tail * 165u / 255u)) >> 8;
        if (k == 0)
            br = br < 255u ? br + (255u - br) / 3u : br;
        if (k == pk_k && hot > 0)
            c = fm_sw(fm_acc(FR_TEAL));
        else
            c = fm_sw(fm_ramp(FR_TEAL, (int)br));
        for (i = 0; i <= 28; i++) {
            int32_t u = i * 255 / 28, x = 120 - w + (2 * w * u >> 8), y = yb - (H * cp_sin(u >> 1) >> 14);
            y += (cp_sin(u * 3 + k * 41 + fm.ph) * (int32_t)(dif * 5.0f) >> 14) * cp_sin(u >> 1) >> 14;
            if (i)
                fm_seg(px, py, x, y, c);
            px = x; py = y;
        }
        fm_seg(120 - w, yb, 120 + w, yb, fm_sw(fm_ramp(FR_TEAL, (int)(br / 3u))));    /* the floor under it */
    }
    {   /* the walls running back */
        int32_t t = dep, e = (t + (t * t >> 8)) >> 1, yb = y1 - ((y1 - yh) * e >> 8), w = w1 - ((w1 - wh) * e >> 8);
        fm_seg(120 - w1, y1, 120 - w, yb, fm_sw(fm_ramp(FR_TEAL, 70)));
        fm_seg(120 + w1, y1, 120 + w, yb, fm_sw(fm_ramp(FR_TEAL, 70)));
    }
    (void)mon;
}

/* ---- CHORUS: woven fine lines; the voices' spread follows DEPTH (and RATE moves them), WIDTH spreads them, MIX sets how much of the wet shows */
static void fm_chorus_step(const float *mon, uint32_t dt)
{
    pk.sp[0] = (uint16_t)(pk.sp[0] + dt * (uint32_t)(4.0f + (0.05f + P(mon, 0) * 4.0f) * 65.0f));
    pk.sp[1] = (uint16_t)(pk.sp[1] + dt * 30u);
}
static void fm_chorus_draw(const float *mon)
{
    float depth = P(mon, 1), wid = P(mon, 2), mix = P(mon, 3);
    int j, i;
    int32_t lfo = cp_sin((int32_t)(pk.sp[0] >> 8)), amp = 14 + (int32_t)(cp.lv * 18.0f), spread = 5 + (int32_t)(depth * 38.0f), gap = 4 + (int32_t)(wid * 8.0f);
    for (j = 0; j < 11; j++) {
        int32_t d = j - 5, px = 0, py = 0, cy = 98 + d * gap, ph = (d * spread) + (lfo * (int32_t)(depth * 30.0f) >> 14) * d / 2 + (int32_t)(pk.sp[1] >> 8);
        uint32_t tv = (uint32_t)(d == 0 ? 250 : (60 + 150 * mix) * (6 - pk_abs(d)) / 6.0f);
        uint16_t c = fm_sw(fm_ramp(FR_TEAL, (int)tv));
        for (i = 0; i <= 60; i++) {
            int32_t u = i * 255 / 60, x = 8 + (224 * u >> 8), y = cy + ((amp * cp_sin(u >> 1) >> 14) * cp_sin(u * 2 + ph) >> 14);
            if (i)
                fm_seg(px, py, x, y, c);
            px = x; py = y;
        }
    }
    {   /* "now": the dry line's centre */
        int32_t y = 98 + ((amp * cp_sin(60) >> 14) * cp_sin(120 + (int32_t)(pk.sp[1] >> 8)) >> 14);
        fm_node(120, y, FR_TEAL, 220u);
    }
}

/* ---- CLOUDS: the buffer as a grain terrain (a slice of its envelope every 130 ms, newest nearest), the POSITION a scan line through it with SPRAY a band, grains lifting off as sparks; FROZEN: the terrain stops, goes ice-pale, a pale box round it */
#define FC_NP 40
#define FC_NS 12
static const fm_geo_t G_CL = {52, 170, 62, 178, 12, 228, 24, 82};
static void fm_clouds_step(const float *mon, uint32_t dt)
{
    int mode = cp.mode;
    uint32_t lv8 = (uint32_t)(cp.lv * 255.0f);
    fm.acc = (uint16_t)(fm.acc + dt);
    if (fm.acc >= 130u) {
        uint8_t sl[FC_NP];
        int i, j;
        fm.acc = 0;
        if (!cp.freeze) {
            if (mode == 2) {                       /* SPECTRL: the curtains (illustrative: they follow TEXTURE, DENSITY, SIZE and the level) */
                float tex = P(mon, 4), size = P(mon, 1);
                int32_t cen = (int32_t)(tex * (float)(FC_NP - 1)), wid = 2 + (int32_t)(size * 6.0f);
                for (i = 0; i < FC_NP; i++) {
                    int32_t dd = i > cen ? i - cen : cen - i, a = GAUSS[dd * 63 / (wid * 3 + 1) > 63 ? 63 : dd * 63 / (wid * 3 + 1)], sh = (cp_sin((int32_t)(cp.t_ms >> 3) + i * 41) + 16384) >> 8;
                    int32_t h = (30 + (a * (int32_t)(60u + lv8) >> 8) * 150 / 256) * (192 + (sh >> 2)) >> 8;
                    sl[i] = (uint8_t)(lv8 < 8u ? 4 : h > 255 ? 255 : h);
                }
            } else {                               /* the buffer's real recent output level, from the oldest column to the newest */
                int32_t hc = (int32_t)(cp.head >> 8);
                for (i = 0; i < FC_NP; i++) {
                    uint32_t m = 0;
                    for (j = 0; j < 6; j++) {
                        uint32_t v = cp.env[(hc + 1 + i * 6 + j) % 240];
                        m = v > m ? v : m;
                    }
                    m = m * 6u / 5u;
                    sl[i] = (uint8_t)(m > 255u ? 255u : m);
                }
                {   /* the grains' texture (illustrative: peaks at random places, more of them away from DENSITY's middle, wider for a bigger SIZE) */
                    float dn = P(mon, 3) - 0.5f;
                    uint32_t g = 70u + (uint32_t)((dn < 0.0f ? -dn : dn) * 2.0f * 150.0f), r0 = cp_rnd() & 255u, r1 = r0, r2;
                    int wsm = 1 + (int)(P(mon, 1) * 2.0f);
                    uint8_t nz[FC_NP];
                    for (i = 0; i < FC_NP; i++) {
                        r2 = cp_rnd() & 255u;
                        nz[i] = (uint8_t)(r2 * r2 >> 8);
                    }
                    for (i = 0; i < FC_NP; i++) {
                        uint32_t a = 0, n = 0;
                        for (j = -wsm; j <= wsm; j++)
                            if (i + j >= 0 && i + j < FC_NP) { a += nz[i + j]; n++; }
                        a = a / n * 3u / 2u;
                        a = a > 255u ? 255u : a;
                        sl[i] = (uint8_t)((uint32_t)sl[i] * (256u - g + g * a / 256u) >> 8);
                    }
                    (void)r1;
                }
            }
            fm_push(cp.colh, FC_NP, FC_NS, sl);
            fm.hot = FC_NS + 2;
        }
        if (lv8 < 3u && fm.hot)
            fm.hot--;
    }
}
static void fm_clouds_draw(const float *mon)
{
    float pos = P(mon, 0), spray = P(mon, 16), size = P(mon, 1);
    int ramp = cp.freeze ? FR_ICE : FR_TEAL, s, i;
    int32_t y0 = G_CL.y0, y1 = G_CL.y1, xa, xb, xc, wd = 2 + (int32_t)(spray * 18.0f);
    uint32_t br = cp.freeze ? 256u : fm_bright();
    uint8_t bt[FC_NS];
    for (s = 0; s < FC_NS; s++)
        bt[s] = (uint8_t)(cp.freeze ? 80 + s * 14 : 50 + s * 17);
    fm_terrain(cp.colh, FC_NP, FC_NS, &G_CL, ramp, br, bt, cp.freeze ? FC_NS - 1 : -1);
    if (cp.mode == 1) {                           /* LOOPDLY: the loop's start and end, the playhead between */
        int32_t s0 = (int32_t)(pos * 256.0f), nl = (int32_t)((0.10f + 0.90f * size) * 256.0f) + 1, ph = (s0 + (int32_t)(cp.ph >> 8)) & 255;
        s0 = s0 * 220 / 256;
        xa = 12 + s0;
        xb = 12 + (s0 + nl * 220 / 256 > 216 ? 216 : s0 + nl * 220 / 256);
        fm_dash(xa, y1, xa + 20, y0, 0xFFC878u, 130u);
        fm_dash(xb, y1, xb + 10, y0, 0xFFC878u, 130u);
        xc = 12 + ph * 216 / 256;
        fm_seg(xc, y1 + 2, xc + 14, y0, fm_sw(fm_acc(ramp)));
        fm_node(xc, y1 + 2, ramp, 230u);
    } else if (cp.mode == 0) {                    /* the scan line: POSITION, with SPRAY a band either side */
        int32_t xf = 20 + (int32_t)(pos * 200.0f), xbk = 70 + (int32_t)(pos * 100.0f);
        if (wd > 3) {
            fm_dash(xf - wd, y1, xbk - wd / 2, y0, 0xFFC878u, 80u);
            fm_dash(xf + wd, y1, xbk + wd / 2, y0, 0xFFC878u, 80u);
        }
        fm_seg(xf, y1 + 3, xbk, y0, fm_sw(fm_acc(ramp)));
        fm_node(xf, y1 + 3, ramp, 240u);
    }
    for (i = 0; i < CP_NM; i++) {                 /* grains lifting off */
        cp_mote_t *o = &cp.m[i];
        uint32_t f;
        if (!o->life)
            continue;
        f = o->age * 4u < o->life ? o->age * 4u * 255u / o->life : (o->life - o->age) * 255u / (o->life - o->life / 4u);
        f = (f > 255u ? 255u : f) * o->gain >> 8;
        pk_blob(0, 0, o->x >> 8, o->y >> 8, o->flags & 1 ? hr_ramp(ramp, 200) : 0xFFFFFFu, f);
    }
    if (cp.freeze) {                              /* unmistakable: a pale outlined box round the terrain */
        fm_dash(8, 44, 232, 44, 0xBFE0FFu, 200u);
        fm_dash(8, 180, 232, 180, 0xBFE0FFu, 200u);
        fm_dash(8, 44, 8, 180, 0xBFE0FFu, 200u);
        fm_dash(232, 44, 232, 180, 0xBFE0FFu, 200u);
    }
}

/* ---- AMP: the envelope as a filled gradient profile (ATTACK, DECAY, SUSTAIN, RELEASE as set), each sounding voice's real envelope a node travelling along it */
#define FA_BASE 172
#define FA_H 112
static int fa_x[5];
static void fm_amp_step(const float *mon, uint32_t dt)
{
    float a = P(mon, 0), d = P(mon, 1), sus = P(mon, 2), r = P(mon, 3), wa = 10.0f + a * 56.0f, wd = 12.0f + d * 62.0f, ws = 36.0f, wr = 12.0f + r * 62.0f, tot = wa + wd + ws + wr, sc = 208.0f / tot;
    int x, i, x0 = 16;
    (void)dt;
    fa_x[0] = x0;
    fa_x[1] = x0 + (int)(wa * sc);
    fa_x[2] = fa_x[1] + (int)(wd * sc);
    fa_x[3] = fa_x[2] + (int)(ws * sc);
    fa_x[4] = fa_x[3] + (int)(wr * sc);
    for (x = 0; x < 240; x++) {
        float v = 0.0f;
        if (x >= fa_x[0] && x < fa_x[1]) v = (float)(x - fa_x[0]) / (float)(fa_x[1] - fa_x[0] + 1);
        else if (x >= fa_x[1] && x < fa_x[2]) v = 1.0f - (1.0f - sus) * (float)(x - fa_x[1]) / (float)(fa_x[2] - fa_x[1] + 1);
        else if (x >= fa_x[2] && x < fa_x[3]) v = sus;
        else if (x >= fa_x[3] && x <= fa_x[4]) v = sus * (1.0f - (float)(x - fa_x[3]) / (float)(fa_x[4] - fa_x[3] + 1));
        cp.colh[x] = (uint8_t)(v * 255.0f);
    }
    for (i = 0; i < 6; i++)
        if (mon[16 + i] > 0.02f) fm.hot = 3;
}
static void fm_amp_draw(const float *mon)
{
    int i;
    float sus = P(mon, 2);
    fm_curve(cp.colh, FA_BASE, FA_H, FR_AMBER, fm_bright(), fa_x[0], fa_x[4] + 1, 70);
    for (i = 1; i < 4; i++)                       /* the stage boundaries, dashed */
        fm_dash(fa_x[i], FA_BASE - (cp.colh[fa_x[i] < 240 ? fa_x[i] : 239] * FA_H >> 8), fa_x[i], FA_BASE, 0xFFFFFFu, 40u);
    for (i = 0; i < 6; i++) {                     /* each voice's envelope: its node on the curve */
        float e = mon[16 + i], g = mon[28 + i];
        int32_t x, y;
        if (g < 0.0f || e < 0.01f) { fm.vprev[i] = (uint8_t)(e * 255.0f); continue; }
        if (g >= 1.0f) {
            if (e * 255.0f > (float)fm.vprev[i] + 0.5f && e < 0.995f)
                x = fa_x[0] + (int32_t)((float)(fa_x[1] - fa_x[0]) * e);
            else if (e > sus + 0.02f)
                x = fa_x[1] + (int32_t)((float)(fa_x[2] - fa_x[1]) * (1.0f - e) / (1.0f - sus + 0.001f));
            else
                x = fa_x[2] + (int32_t)((float)(fa_x[3] - fa_x[2]) * 0.5f);
        } else {
            x = fa_x[3] + (int32_t)((float)(fa_x[4] - fa_x[3]) * (1.0f - e / (sus + 0.001f > 1.0f ? 1.0f : sus + 0.001f)));
        }
        fm.vprev[i] = (uint8_t)(e * 255.0f);
        y = FA_BASE - (int32_t)(e * (float)FA_H);
        fm_node(x, y, FR_AMBER, 255u);
    }
}

/* ------------------------------------------------------------------------------------------------ the dispatch (vis_pic.c calls these) */
static void fam_step(int sc, const float *mon, uint32_t dt, uint32_t edge)
{
    switch (sc) {
    case SC_BRAIDS: fm_braids_step(mon, dt); break;
    case SC_RINGS: fm_rings_step(mon, dt, edge); break;
    case SC_FILTER: fm_filter_step(mon, dt); break;
    case SC_TIDES: case SC_PEAKS: case SC_FUNC: case SC_LFO: fm_mod_step(sc, mon, dt); break;
    case SC_GRIDS: fm_grids_step(mon); break;
    case SC_DELAY: fm_delay_step(mon, dt, fd_bt, &fd_y0); break;
    case SC_REVERB: fm_reverb_step(mon, dt); break;
    case SC_CHORUS: fm_chorus_step(mon, dt); break;
    case SC_AMP: fm_amp_step(mon, dt); break;
    default: break;
    }
}
static void fam_draw(int sc, const float *mon)
{
    switch (sc) {
    case SC_BRAIDS: fm_braids_draw(); break;
    case SC_RINGS: fm_rings_draw(); break;
    case SC_FILTER: fm_filter_draw(); break;
    case SC_TIDES: case SC_PEAKS: case SC_FUNC: case SC_LFO: fm_mod_draw(sc); break;
    case SC_GRIDS: fm_grids_draw(mon); break;
    case SC_DELAY: fm_delay_draw(); break;
    case SC_REVERB: fm_reverb_draw(mon); break;
    case SC_CHORUS: fm_chorus_draw(mon); break;
    case SC_CLOUDS: fm_clouds_draw(mon); break;
    case SC_AMP: fm_amp_draw(mon); break;
    default: break;
    }
}

/* per frame: the picture's own state moves on dt ms (strikes, held tails, history steps, sparks) */
static void fam_reset(void)
{
    uint8_t *q = (uint8_t *)(void *)&fm;
    uint32_t i;
    for (i = 0; i < sizeof fm; i++)
        q[i] = 0;
    pk.prev = 0.0f; pk.tail = 0.0f; pk.e = 0.0f; pk.step = 0; pk.gprev = 0;
    fm.hot = 4;
}
static void pic_step(int sc, const float *mon, uint32_t dt)
{
    float raw = mon[48] > mon[49] ? mon[48] : mon[49];
    uint32_t lv8, gt = pk_gates(mon), edge;
    cp_level(raw, dt);
    cp.freeze = 0;
    lv8 = (uint32_t)(cp.lv * 255.0f);
    edge = gt & ~(uint32_t)pk.gprev;
    pk.gprev = (uint8_t)gt;
    if ((sc == SC_RINGS || sc == SC_AMP) && fm.hot)
        fm.hot--;
    fam_step(sc, mon, dt, edge);
    cp_motes_advance(dt, lv8 > 2u ? lv8 : 3u);
}
static void pic_draw(int sc, const float *mon) { fam_draw(sc, mon); }
static int pic_idle(int sc)
{
    int v;
    switch (sc) {
    case SC_TIDES: case SC_PEAKS: case SC_FUNC: case SC_LFO: case SC_GRIDS: return 0;
    case SC_CLOUDS: case SC_DELAY: case SC_RINGS: case SC_AMP: return fm.hot == 0;
    case SC_REVERB: return pk.tail < 0.004f && pk_recent(&v) > 3000u;
    default: return 1;
    }
}
