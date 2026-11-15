/* SPDX-License-Identifier: MPL-2.0 */
/* FM1 Phase 2 firmware: update 13, the picture kit and the pictures of every module page, part of app.c (included at the end of vis_clouds.c).
 *
 * The CLOUDS page's machinery (vis_clouds.c: the layout of a picture over four slim read-outs, the glow sprites added with saturation into RGB565,
 * the mote field, the 20-frames-a-second cap, the changed-rows-only transfer, the DRAW / LCD / FPS read-out) is the one every picture uses; this
 * file adds the primitives (a glowing line, a string, a ring, a haze, the process icons) and one scene per module:
 *
 *   BRAIDS   real: the voices' envelopes, gates and notes, MODEL, TIMBRE, COLOR, BITS, RATE   modelled: the family a model belongs to (its string's shape and hue)
 *   RINGS    real: the gates (a strike), MODEL, STRUCTURE, BRIGHTNESS, DAMPING, POSITION      modelled: the ring's modes and how long they ring (from DAMPING)
 *   FILTER   real: TYPE, CUTOFF, RESONANCE, the envelope amount and the voices' envelopes (the cutoff as heard); the curve is the filter's own formula
 *            modelled: the energy motes (their rate follows the output level; their brightness follows the curve)
 *   TIDES / PEAKS / FUNC   real: the modulator's two outputs (vz.hist, 33 ms apart), as drawn on the old pages      modelled: the bursts at a Peaks trigger
 *   GRIDS    real: the 96 step levels and the clock step                                    DELAY   real: FEEDBACK, MIX, MODE, the notes played (vz.pulse), the tempo
 *            modelled: the delay time in ms (the division or FREE MS, as the sound's time is not exported)   REVERB   real: SIZE, DAMP, DIFFUSE, TYPE, SHIMMER, the output level
 *            modelled: the tail (the output level, held and let go at SIZE's rate; the reverb's own output is not exported)   CHORUS   real: RATE, DEPTH, WIDTH, MIX, the level
 * Every picture draws the same way: no floating point or maths functions in a pixel loop (const sprites and tables in flash), at most CP_NM motes, a
 * state of a few dozen bytes shared by all of them (pk, below; the mote field, ribbon history and row hashes are the Clouds page's own). */

static struct {
    uint8_t act[IC_N];                        /* how alive each process is, 0-255 (pk_tick, 30 times a second) */
    uint16_t iph[IC_N];                       /* each icon's phase, 4096 to the turn */
    uint16_t sp[6];                           /* string phases, 65536 to the turn */
    float e, tail, prev;                      /* a strike's energy, the reverb's held tail, a modulator's last value */
    uint8_t gprev, step;                      /* the voices' gates last frame, the Grids step last frame */
    uint32_t last_ms;
} pk;

/* ------------------------------------------------------------------------------------------------ small things */
static uint32_t pk_rgb(uint16_t c)            /* RGB565 -> 0xRRGGBB */
{
    uint32_t r = c >> 11, g = (c >> 5) & 63u, b = c & 31u;
    return (r << 3 | r >> 2) << 16 | (g << 2 | g >> 4) << 8 | (b << 3 | b >> 2);
}
static uint32_t pk_mix(uint32_t a, uint32_t b, uint32_t t)   /* a to b, t 0-256 */
{
    uint32_t r = ((a >> 16) * (256u - t) + (b >> 16) * t) >> 8, g = (((a >> 8) & 255u) * (256u - t) + ((b >> 8) & 255u) * t) >> 8, bl = ((a & 255u) * (256u - t) + (b & 255u) * t) >> 8;
    return r << 16 | g << 8 | bl;
}
static inline void pk_px(int32_t x, int32_t y, uint32_t c, uint32_t a) { cp_add(x, y, c >> 16, (c >> 8) & 255u, c & 255u, a > 255u ? 255u : a); }
static int32_t pk_wave(int sh, int32_t p)     /* one cycle in 256: sine, triangle, saw, soft square, pulse, rough (+-16383) */
{
    int32_t q = p & 255, v;
    switch (sh) {
    case 1: return q < 64 ? q * 256 : q < 192 ? (128 - q) * 256 : (q - 256) * 256;
    case 2: return (q - 128) * 128;
    case 3: v = cp_sin(q) * 3; return v > 16383 ? 16383 : v < -16383 ? -16383 : v;
    case 4: v = (cp_sin(q) - 6000) * 3; return v > 16383 ? 16383 : v < -16383 ? -16383 : v;
    case 5: return (cp_sin(q) + cp_sin(q * 3 + 40) / 2 + cp_sin(q * 7 + 90) / 4) * 4 / 7;
    default: return cp_sin(q);
    }
}
static int32_t pk_abs(int32_t v) { return v < 0 ? -v : v; }
/* an additive line with a faint halo to each side */
static void pk_line(int32_t x0, int32_t y0, int32_t x1, int32_t y1, uint32_t c, uint32_t a)
{
    int32_t dx = pk_abs(x1 - x0), dy = -pk_abs(y1 - y0), sx = x0 < x1 ? 1 : -1, sy = y0 < y1 ? 1 : -1, err = dx + dy, g = 400, lo = y0 < y1 ? y0 : y1, hi = y0 < y1 ? y1 : y0;
    uint32_t r = c >> 16, gg = (c >> 8) & 255u, b = c & 255u, h = a / 3u;
    if (hi + 2 < cp.by || lo - 2 >= cp.by + (int32_t)cv_h || (x0 < -2 && x1 < -2) || (x0 > 242 && x1 > 242))
        return;
    while (g--) {
        int32_t e2 = 2 * err;
        cp_add(x0, y0, r, gg, b, a > 255u ? 255u : a);
        if (dx >= -dy) { cp_add(x0, y0 - 1, r, gg, b, h); cp_add(x0, y0 + 1, r, gg, b, h); }
        else { cp_add(x0 - 1, y0, r, gg, b, h); cp_add(x0 + 1, y0, r, gg, b, h); }
        if (x0 == x1 && y0 == y1)
            break;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}
/* a string between two points, fixed at both ends, vibrating with amp pixels: shape sh (pk_wave), cyc256 cycles x 256 along it, phase ph (of 256),
 * harm (0-255) of the octave above, p2 its phase; hold: sample-and-hold of that many points (a lower RATE) */
static int pk_soft;                           /* 0-8: how much of a plain sine a string's shape keeps (HOME's strings are soft) */
static void pk_string(int32_t x0, int32_t y0, int32_t x1, int32_t y1, int32_t amp, int32_t cyc256, int32_t ph, int sh, int32_t harm, int32_t p2, int hold, uint32_t c, uint32_t a)
{
    int32_t n = (x1 - x0) / 2, i, px = x0, py = y0, ylo = y0 < y1 ? y0 : y1, yhi = y0 < y1 ? y1 : y0;
    if (n < 4) n = 4;
    if (yhi + amp + 3 < cp.by || ylo - amp - 3 >= cp.by + (int32_t)cv_h)
        return;
    for (i = 1; i <= n; i++) {
        int32_t j = hold > 1 ? i - i % hold : i, u = j * 255 / n, p = (u * cyc256 >> 8) + ph, w = pk_wave(sh, p), x = x0 + (x1 - x0) * i / n, y;
        if (pk_soft && sh)
            w = (w * (8 - pk_soft) + cp_sin(p & 255) * pk_soft) >> 3;
        if (harm)
            w = (w * (256 - (harm >> 1)) + pk_wave(sh, 2 * p + p2) * (harm >> 1)) >> 8;
        y = y0 + (y1 - y0) * i / n + (((amp * cp_sin(u >> 1)) >> 14) * w >> 14);
        pk_line(px, py, x, y, c, a);
        px = x;
        py = y;
    }
}
/* a ring (an ellipse) of dots joined by lines */
static void pk_ring(int32_t cx, int32_t cy, int32_t rx, int32_t ry, uint32_t c, uint32_t a)
{
    int32_t n = rx + ry, i, px = 0, py = 0;
    if (rx < 1 || a == 0 || cy + ry + 2 < cp.by || cy - ry - 2 >= cp.by + (int32_t)cv_h)
        return;
    n = n < 20 ? 20 : n > 96 ? 96 : n;
    for (i = 0; i <= n; i++) {
        int32_t ang = i * 256 / n, x = cx + (rx * cp_cos(ang) >> 14), y = cy + (ry * cp_sin(ang) >> 14);
        if (i)
            pk_line(px, py, x, y, c, a);
        px = x;
        py = y;
    }
}
/* a soft haze: a Gaussian ellipse; stp 2 lays it down on every other pixel (a fixed pattern) at twice the alpha: half the cost, a hint of grain */
static void pk_haze2(int32_t cx, int32_t cy, int32_t rx, int32_t ry, uint32_t c, uint32_t a, int32_t stp)
{
    int32_t y, x, y0 = cy - ry, y1 = cy + ry, kx;
    uint32_t r = c >> 16, g = (c >> 8) & 255u, b = c & 255u;
    if (rx < 2 || ry < 2 || a == 0)
        return;
    if (y0 < cp.by) y0 = cp.by;
    if (y1 > cp.by + (int32_t)cv_h) y1 = cp.by + (int32_t)cv_h;
    kx = 30 * 256 / rx;
    for (y = y0; y < y1; y++) {
        uint32_t ra = (uint32_t)GAUSS[(uint32_t)(pk_abs(y - cy) * 30 / ry)] * a >> 8;
        int32_t xs = cx - rx, xe = cx + rx;
        if (ra == 0)
            continue;
        if (xs < 0) xs = 0;
        if (xe > 239) xe = 239;
        for (x = xs + (stp > 1 ? ((xs + y) & 1) : 0); x <= xe; x += stp) {
            uint32_t al = (uint32_t)GAUSS[(uint32_t)(pk_abs(x - cx) * kx >> 8)] * ra >> 8;
            if (al)
                cp_add(x, y, r, g, b, al * (uint32_t)stp);
        }
    }
}
static void pk_haze(int32_t cx, int32_t cy, int32_t rx, int32_t ry, uint32_t c, uint32_t a) { pk_haze2(cx, cy, rx, ry, c, a, 1); }
static void pk_blob(int sz, int soft, int32_t x, int32_t y, uint32_t c, uint32_t g)   /* a glow sprite centred on (x, y): sz 0 5 px, 1 9, 2 13, 3 19 */
{
    switch (sz) {
    case 0: cp_spr(soft ? SPR_S5 : SPR_H5, 5, 5, x - 2, y - 2, c, g); break;
    case 1: cp_spr(soft ? SPR_S9 : SPR_H9, 9, 9, x - 4, y - 4, c, g); break;
    case 2: cp_spr(soft ? SPR_S13 : SPR_H13, 13, 13, x - 6, y - 6, c, g); break;
    default: cp_spr(soft ? SPR_S19 : SPR_H19, 19, 19, x - 9, y - 9, c, g); break;
    }
}
/* the motes of the pictures but CLOUDS and HOME: flags 1 soft, 2 a small spark, 4 an expanding ring (sz: its last radius / 8) */
static void pk_motes(uint32_t colmul)
{
    int i;
    for (i = 0; i < CP_NM; i++) {
        cp_mote_t *o = &cp.m[i];
        uint32_t f, gg;
        int32_t x, y;
        if (!o->life)
            continue;
        f = o->age * 4u < o->life ? o->age * 4u * 255u / o->life : (o->life - o->age) * 255u / (o->life - o->life / 4u);
        f = (f > 255u ? 255u : f) * o->gain >> 8;
        gg = (f * (uint32_t)cp.gain >> 8) * colmul >> 8;
        x = o->x >> 8;
        y = o->y >> 8;
        if (o->flags & 4) {
            uint32_t rr = 3u + (uint32_t)o->sz * 8u * o->age / o->life;
            pk_ring(x, y, (int32_t)rr, (int32_t)rr * 9 / 10, cp_hue(o->hue), (255u - 255u * o->age / o->life) * (uint32_t)o->gain / 255u * (uint32_t)cp.gain >> 8);
        } else if (o->flags & 2) {
            pk_blob(0, 0, x, y, cp_hue(o->hue), gg > 255u ? 255u : gg);
        } else {
            pk_blob(o->sz > 2 ? 2 : o->sz + 1, o->flags & 1, x, y, cp_hue(o->hue), gg > 255u ? 255u : gg);
        }
    }
}
/* a mote of the pictures' own kinds: 1 soft dust, 2 a spark, 4 an expanding ring; returns it (0: the field is full) */
static cp_mote_t *pk_spark(int32_t x, int32_t y, int hue, int kind, int32_t vx, int32_t vy, uint32_t life)
{
    int i;
    for (i = 0; i < CP_NM; i++)
        if (!cp.m[i].life) {
            cp_mote_t *o = &cp.m[i];
            o->x = x << 8; o->y = y << 8; o->vx = vx; o->vy = vy;
            o->age = 0; o->life = (uint16_t)life;
            o->hue = (uint8_t)(hue < 0 ? 0 : hue > 15 ? 15 : hue);
            o->sz = (uint8_t)(kind == 4 ? 2 : (cp_rnd() & 1u));
            o->gain = (uint8_t)(150u + cp_rnd() % 105u);
            o->flags = (uint8_t)kind;
            return o;
        }
    return 0;
}
static int32_t pk_vel(int32_t lo, int32_t span) { return lo + (int32_t)(cp_rnd() % (uint32_t)span); }

/* ------------------------------------------------------------------------------------------------ icons */
/* small animated pictures of the processes, 16 x 16 pixels, drawn with the same additive glow as the pictures; an icon is bright and quick while its
 * process is making or shaping sound (pk.act, from the engine's monitor in pk_tick) and dim and slow when it is idle */
static struct { uint32_t c, w; int32_t ox, oy; uint32_t br; } ik;
static void ik_p(int32_t x, int32_t y, uint32_t a) { pk_px(ik.ox + x, ik.oy + y, ik.c, a * ik.br >> 8); }
static void ik_w(int32_t x, int32_t y, uint32_t a) { pk_px(ik.ox + x, ik.oy + y, ik.w, a * ik.br >> 8); }
static void ik_l(int32_t x0, int32_t y0, int32_t x1, int32_t y1, uint32_t a)
{
    int32_t dx = pk_abs(x1 - x0), dy = -pk_abs(y1 - y0), sx = x0 < x1 ? 1 : -1, sy = y0 < y1 ? 1 : -1, err = dx + dy, g = 40;
    while (g--) {
        int32_t e2 = 2 * err;
        ik_p(x0, y0, a);
        if (x0 == x1 && y0 == y1)
            break;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}
static void ik_tab(const int8_t *t, int n, int dot, uint32_t a)   /* a polyline through n heights, 16 px wide, a dot at point dot */
{
    int i;
    for (i = 1; i < n; i++)
        ik_l((i - 1) * 15 / (n - 1), t[i - 1], i * 15 / (n - 1), t[i], a);
    ik_w(dot * 15 / (n - 1), t[dot], 255);
    ik_w(dot * 15 / (n - 1) + 1, t[dot], 90);
    ik_w(dot * 15 / (n - 1) - 1, t[dot], 90);
}
static int pk_icon_known(int ic)
{
    switch (ic) {
    case IC_BRAIDS: case IC_RINGS: case IC_FILTER: case IC_PEAKS: case IC_TIDES: case IC_GRIDS: case IC_DELAY: case IC_REVERB: case IC_CHORUS:
    case IC_AMP: case IC_LFO: case IC_MATRIX: case IC_SAVE: case IC_OUT: case IC_GLOBAL: case IC_ARP: case IC_CHORD: case IC_SEQ: case IC_FUNC: case IC_CLOUDS:
        return 1;
    default:
        return 0;
    }
}
static void pk_icon_draw(int ic, int32_t x, int32_t y, uint16_t col)
{
    int32_t ph = pk.iph[ic] >> 4, act = pk.act[ic], i, k;
    static const int8_t PEAKT[16] = {13, 12, 9, 5, 2, 4, 6, 8, 9, 10, 11, 12, 12, 13, 13, 13}, FUNCT[16] = {13, 9, 5, 3, 2, 3, 4, 5, 7, 8, 9, 10, 11, 12, 12, 13};
    static const int8_t LFOT[16] = {8, 5, 3, 2, 3, 5, 8, 11, 13, 14, 13, 11, 8, 5, 3, 2}, ADSRT[16] = {13, 9, 5, 2, 3, 5, 6, 6, 6, 6, 6, 6, 8, 10, 12, 13};
    if (!pk_icon_known(ic))
        return;
    ik.c = pk_rgb(col);
    ik.w = pk_mix(ik.c, 0xFFFFFFu, 170);
    ik.ox = x - 2;
    ik.oy = y - 2;
    ik.br = (uint32_t)(150 + act * 105 / 255);
    switch (ic) {
    case IC_BRAIDS: {                                   /* a string, fixed at both ends, with its octave above */
        int32_t amp = 2 + act * 4 / 255, py = 8;
        for (i = 0; i < 16; i++) {
            int32_t env = cp_sin(i * 8), y1 = 8 + ((amp * cp_sin(i * 32 + ph) >> 14) * env >> 14), y2 = 8 - ((amp * cp_sin(i * 64 - ph * 2) >> 14) * env >> 15);
            if (i) ik_l(i - 1, py, i, y1, 255);
            py = y1;
            ik_w(i, y2, 90);
        }
        ik_w(0, 8, 255); ik_w(15, 8, 255);
        break;
    }
    case IC_RINGS: {                                    /* a ring that wobbles in lobes */
        int32_t px = 0, py = 0, w = 1 + act * 2 / 255;
        for (i = 0; i <= 20; i++) {
            int32_t a = i * 256 / 20, r = 5 * 256 + w * (cp_sin(3 * a + ph * 2) >> 6), xx = 8 + (r * cp_cos(a) >> 22), yy = 8 + (r * cp_sin(a) >> 22);
            if (i) ik_l(px, py, xx, yy, 230);
            px = xx; py = yy;
        }
        ik_w(8, 8, 200);
        break;
    }
    case IC_FILTER: {                                   /* a low-pass curve whose cutoff sweeps and peaks */
        int32_t c0 = 8 + ((act > 20 ? cp_sin(ph) * 3 : 0) >> 14), py = 9;
        for (i = 0; i < 16; i++) {
            int32_t d = pk_abs(i - c0), yy = i <= c0 ? 9 : 9 + (i - c0) * 2;
            yy -= 4 * GAUSS[d * 14 > 63 ? 63 : d * 14] >> 8;
            yy = yy > 14 ? 14 : yy;
            if (i) ik_l(i - 1, py, i, yy, 255);
            py = yy;
        }
        ik_w(c0, 5, 140);
        break;
    }
    case IC_TIDES: {                                    /* a rolling wave */
        int32_t py = 8;
        for (i = 0; i < 16; i++) {
            int32_t yy = 8 + (4 * cp_sin(i * 24 + ph) >> 14), j;
            if (i) ik_l(i - 1, py, i, yy, 255);
            for (j = yy + 1; j < 15; j++) ik_p(i, j, (uint32_t)(60 - (j - yy) * 7));
            py = yy;
        }
        break;
    }
    case IC_PEAKS: ik_tab(PEAKT, 16, (ph >> 4) & 15, 255); break;
    case IC_FUNC: ik_tab(FUNCT, 16, (ph >> 4) & 15, 255); break;
    case IC_LFO: ik_tab(LFOT, 16, (ph >> 4) & 15, 255); break;
    case IC_AMP: ik_tab(ADSRT, 16, (ph >> 4) & 15, 255); break;
    case IC_GRIDS:                                      /* dots in a pattern, one step lit at a time */
        for (i = 0; i < 4; i++)
            for (k = 0; k < 4; k++) {
                int lit = ((i + 2 * k + (ph >> 6)) & 3) == 0;
                ik_p(2 + i * 4, 2 + k * 4, lit ? 255 : 60);
                ik_p(3 + i * 4, 2 + k * 4, lit ? 255 : 60);
                ik_p(2 + i * 4, 3 + k * 4, lit ? 255 : 60);
                ik_p(3 + i * 4, 3 + k * 4, lit ? 255 : 60);
                if (lit) ik_w(2 + i * 4, 2 + k * 4, 200);
            }
        break;
    case IC_DELAY:                                      /* repeating, fading bars, a pulse running along them */
        for (i = 0; i < 4; i++) {
            int32_t h = 11 - i * 3 - (i == 3 ? 0 : 0), t = (ph - i * 48) & 255, a = t < 128 ? 255 - t * 2 : 0, j;
            for (j = 0; j < h; j++) { ik_p(1 + i * 4, 13 - j, (uint32_t)(70 + a * 185 / 255)); ik_p(2 + i * 4, 13 - j, (uint32_t)(70 + a * 185 / 255)); }
            if (a > 120) ik_w(1 + i * 4, 13 - h, (uint32_t)a);
        }
        break;
    case IC_REVERB:                                     /* rings that spread and thin out */
        for (k = 0; k < 3; k++) {
            int32_t t = (ph + k * 85) & 255, r = 1 + t * 7 / 256, px = 0, py = 0;
            for (i = 0; i <= 16; i++) {
                int32_t a = i * 16, xx = 8 + (r * cp_cos(a) >> 14), yy = 8 + (r * cp_sin(a) >> 14);
                if (i) ik_l(px, py, xx, yy, (uint32_t)(255 - t));
                px = xx; py = yy;
            }
        }
        ik_w(8, 8, 220);
        break;
    case IC_CHORUS:                                     /* three strands woven */
        for (k = 0; k < 3; k++) {
            int32_t py = 8;
            for (i = 0; i < 16; i++) {
                int32_t yy = 8 + (4 * cp_sin(i * 24 + ph + k * 85) >> 14);
                if (i) ik_l(i - 1, py, i, yy, k == 1 ? 150 : 255);
                py = yy;
            }
        }
        break;
    case IC_CLOUDS: {                                   /* motes rising off a ribbon */
        int32_t py = 13;
        for (i = 0; i < 16; i++) {
            int32_t yy = 13 + (cp_sin(i * 30 + ph * 2) >> 14);
            if (i) ik_l(i - 1, py, i, yy, 230);
            ik_p(i, yy + 1, 70);
            py = yy;
        }
        for (k = 0; k < 4; k++) {
            int32_t t = (ph + k * 64) & 255, yy = 11 - t * 10 / 256, xx = 2 + k * 4 + (cp_sin(t + k * 70) >> 13);
            uint32_t a = (uint32_t)(t < 40 ? t * 6 : 255 - (t - 40) * 255 / 216);
            ik_w(xx, yy, a); ik_p(xx + 1, yy, a / 3); ik_p(xx - 1, yy, a / 3); ik_p(xx, yy + 1, a / 3);
        }
        break;
    }
    case IC_MATRIX:                                     /* links crossing between two columns, a pulse along each */
        for (k = 0; k < 3; k++) {
            int32_t ya = 3 + k * 5, yb = 3 + ((k + 1) % 3) * 5, yc = 3 + ((k + 2) % 3) * 5, t = ((ph >> 1) + k * 85) & 255;
            ik_l(2, ya, 13, yb, 90);
            ik_l(2, ya, 13, yc, 60);
            ik_w(2 + t * 11 / 256, ya + (yb - ya) * t / 256, 255);
            ik_w(2, ya, 255); ik_w(13, ya, 255);
        }
        break;
    case IC_ARP:                                        /* a rising stair, one step lit and hopping up */
        for (i = 0; i < 4; i++) {
            int32_t lit = ((ph >> 6) & 3) == i, h = 3 + i * 3, j;
            for (j = 0; j < h; j++) { ik_p(1 + i * 4, 13 - j, lit ? 255 : 90); ik_p(2 + i * 4, 13 - j, lit ? 255 : 90); ik_p(3 + i * 4, 13 - j, lit ? 255 : 90); }
            if (lit) ik_w(2 + i * 4, 13 - h, 255);
        }
        break;
    case IC_SEQ:                                        /* blocks at their own heights, the playing one lit */
        for (i = 0; i < 4; i++) {
            static const int8_t SY[4] = {8, 3, 10, 5};
            int32_t lit = ((ph >> 6) & 3) == i, j;
            for (j = 0; j < 3; j++) { ik_p(1 + i * 4, SY[i] + j, lit ? 255 : 90); ik_p(2 + i * 4, SY[i] + j, lit ? 255 : 90); ik_p(3 + i * 4, SY[i] + j, lit ? 255 : 90); }
            if (lit) ik_w(2 + i * 4, SY[i], 255);
        }
        break;
    case IC_CHORD:                                      /* three notes stacked, each swelling in turn */
        for (k = 0; k < 3; k++) {
            int32_t t = (ph - k * 60) & 255, a = 110 + (t < 128 ? (128 - t) : 0) * 145 / 128, j;
            for (j = 0; j < 5; j++) { ik_p(5 + j, 3 + k * 4, (uint32_t)a); ik_p(5 + j, 4 + k * 4, (uint32_t)a); }
            ik_w(7, 3 + k * 4, (uint32_t)a);
        }
        break;
    case IC_SAVE: {                                     /* a card with a label, a spark running round its edge */
        int32_t t = (ph * 56 / 256) % 48, sx, sy;
        for (i = 2; i <= 13; i++) { ik_p(i, 2, 190); ik_p(i, 13, 190); ik_p(2, i, 190); ik_p(13, i, 190); }
        for (i = 4; i <= 11; i++) { ik_p(i, 9, 90); ik_p(i, 11, 90); }
        t = t % 48;
        if (t < 12) { sx = 2 + t; sy = 2; } else if (t < 24) { sx = 13; sy = 2 + t - 12; } else if (t < 36) { sx = 13 - (t - 24); sy = 13; } else { sx = 2; sy = 13 - (t - 36); }
        ik_w(sx, sy, 255);
        break;
    }
    case IC_GLOBAL:                                     /* a cog turning */
        for (k = 0; k < 8; k++) {
            int32_t a = k * 32 + (ph >> 1), c = cp_cos(a), s = cp_sin(a);
            ik_l(8 + (3 * c >> 14), 8 + (3 * s >> 14), 8 + (6 * c >> 14), 8 + (6 * s >> 14), 230);
        }
        for (i = 0; i < 16; i++) ik_p(8 + (4 * cp_cos(i * 16) >> 14), 8 + (4 * cp_sin(i * 16) >> 14), 255);
        break;
    case IC_OUT:                                        /* a speaker, waves leaving it */
        for (i = 6; i <= 10; i++) { ik_p(2, i, 230); ik_p(3, i, 230); }
        ik_l(4, 6, 7, 3, 230); ik_l(4, 10, 7, 13, 230); ik_l(7, 3, 7, 13, 230);
        for (k = 0; k < 3; k++) {
            int32_t t = (ph + k * 85) & 255, r = 3 + k * 2 + t / 128, a = 255 - t;
            for (i = -2; i <= 2; i++) ik_p(8 + (r * cp_cos(i * 18) >> 14), 8 + (r * cp_sin(i * 18) >> 14), (uint32_t)a);
        }
        break;
    default:
        break;
    }
}
/* what the old pixel icon calls (app.c icon()) draw: this one at its place, in whatever canvas is open; 0: not an animated one, or the light palette */
static int pk_icon(int ic, int32_t x, int32_t y, uint16_t c)
{
    int by = cp.by;
    if (pal_day || !pk_icon_known(ic))
        return 0;
    cp.by = 0;
    pk_icon_draw(ic, x, y, c);
    cp.by = by;
    return 1;
}
/* 30 times a second (vis_tick): how alive each process is, from what the engine reports; the phases move faster the more alive */
static void pk_tick(const float *mon)
{
    float e = 0.0f, pkl = mon[48] > mon[49] ? mon[48] : mon[49], mo = 0.0f, lf = 0.0f, rt = 0.0f;
    uint8_t t[IC_N];
    int i;
    for (i = 0; i < 6; i++)
        if (mon[16 + i] > e) e = mon[16 + i];
    for (i = 0; i < 4; i++)
        if (fm_fabsf(mon[40 + i]) > mo) mo = fm_fabsf(mon[40 + i]);
    for (i = 0; i < 3; i++)
        if (fm_fabsf(mon[44 + i]) > lf) lf = fm_fabsf(mon[44 + i]);
    for (i = 0; i < 12; i++)
        if (fm_fabsf(mon[50 + i]) > rt) rt = fm_fabsf(mon[50 + i]);
#define Q8(x) ((x) >= 1.0f ? 255 : (x) <= 0.0f ? 0 : (int)((x) * 255.0f))
    for (i = 0; i < IC_N; i++) t[i] = 0;
    t[IC_BRAIDS] = t[IC_RINGS] = t[IC_AMP] = (uint8_t)Q8(e * 1.3f);
    t[IC_FILTER] = (uint8_t)Q8(e * 1.1f);
    t[IC_PEAKS] = t[IC_TIDES] = t[IC_FUNC] = (uint8_t)Q8(mo * 1.6f);
    t[IC_LFO] = (uint8_t)Q8(lf * 1.6f);
    t[IC_GRIDS] = t[IC_DELAY] = t[IC_REVERB] = t[IC_CHORUS] = t[IC_CLOUDS] = t[IC_OUT] = t[IC_HOME] = (uint8_t)Q8(pkl * 3.0f);
    t[IC_MATRIX] = (uint8_t)Q8(rt * 1.5f);
    t[IC_ARP] = AI(A_MODE) != AM_OFF ? (uint8_t)Q8(e * 1.3f + 0.25f) : 0;
    t[IC_CHORD] = CI(C_TYPE) != CT_OFF ? (uint8_t)Q8(e * 1.3f + 0.1f) : 0;
    t[IC_SEQ] = sq.run ? 230 : 0;
    t[IC_SAVE] = t[IC_GLOBAL] = 70;
#undef Q8
    for (i = 0; i < IC_N; i++) {
        pk.act[i] = (uint8_t)(pk.act[i] + ((int)t[i] - (int)pk.act[i]) / (t[i] > pk.act[i] ? 2 : 8));
        pk.iph[i] = (uint16_t)(pk.iph[i] + 40 + pk.act[i]);
    }
}

/* ------------------------------------------------------------------------------------------------ the pictures */
static const uint32_t PK_FAM[6] = {0xFFB347u, 0xFF7A66u, 0xFFE070u, 0xB6F06Bu, 0x6BD6FFu, 0xD78CFFu};   /* Braids' shape families */
#define PK_FILT 0x58F0D0u
#define PK_TIDE 0x58A8FFu
#define PK_PEAK 0xFF6C78u
#define PK_FUNC 0xF2D060u
#define PK_GRID 0xC8A0FFu
#define PK_DLY 0x70B4FFu
#define PK_REV 0xA090FFu
#define PK_CHO 0x62F0C0u
#define PK_RING 0xFFA866u

static int pk_fam(int model) { int f = model * 6 / 47; return f < 0 ? 0 : f > 5 ? 5 : f; }
static uint32_t pk_gates(const float *mon)        /* the voices' gates as bits */
{
    uint32_t g = 0;
    int i;
    for (i = 0; i < 6; i++)
        if (mon[28 + i] >= 1.0f)
            g |= 1u << i;
    return g;
}
static uint32_t pk_recent(int *vel)               /* ms since the last note played on the keys or MIDI (9999999 none) */
{
    uint32_t t = vz.pulse_t[(vz.pulse_w - 1u) & 7u];
    *vel = vz.pulse_v[(vz.pulse_w - 1u) & 7u];
    return t ? plat_ms() - t : 9999999u;
}
static float pk_delay_s(const float *mon)         /* the delay time in seconds, modelled: the division at the tempo, or FREE MS */
{
    static const float BEATS[9] = {0.25f, 0.5f, 0.75f, 1.0f, 2.0f, 0.333f, 0.667f, 1.5f, 3.0f};   /* 1/16 1/8 1/8D 1/4 1/2 1/8T 1/4T 1/4D 1/2D (quarter notes) */
    int i = (int)(P(mon, 0) + 0.5f);
    i = i < 0 ? 0 : i > 8 ? 8 : i;
    if (P(mon, 9) > 0.5f)
        return 0.06f + P(mon, 10) * P(mon, 10) * 0.9f;
    return BEATS[i] * 60.0f / tempo_bpm();
}

/* per frame: the picture's own state moves on dt ms (births, strikes, held tails) */
static void pic_step(int sc, const float *mon, uint32_t dt)
{
    float raw = mon[48] > mon[49] ? mon[48] : mon[49];
    uint32_t lv8, gt = pk_gates(mon), edge, i;
    cp_level(raw, dt);
    cp.freeze = 0;
    lv8 = (uint32_t)(cp.lv * 255.0f);
    edge = gt & ~(uint32_t)pk.gprev;
    pk.gprev = (uint8_t)gt;
    switch (sc) {
    case SC_BRAIDS: {
        int n = (int)(mon[63] + 0.5f), fam = pk_fam((int)(P(mon, 0) + 0.5f)), hue = (int)(P(mon, 2) * 3.0f);
        n = n < 1 ? 1 : n > 6 ? 6 : n;
        for (i = 0; i < 6; i++) {
            float e = mon[16 + i] > 0.0f ? mon[16 + i] : 0.0f;
            pk.sp[i] = (uint16_t)(pk.sp[i] + dt * (uint32_t)(40.0f + e * 70.0f + (float)i * 6.0f));
            if (i < (uint32_t)n && (((edge >> i) & 1u) || (e > 0.35f && (cp_rnd() & 63u) < 3u))) {
                pk_spark(30 + (int32_t)(cp_rnd() % 180u), 28 + ((int32_t)i * 2 + 1) * 66 / n, fam * 3 + hue, 2, pk_vel(-1500, 3000), -pk_vel(2000, 5000), 600u + cp_rnd() % 500u);
            }
        }
        break;
    }
    case SC_RINGS: {
        float life = 0.6f + P(mon, 3) * 4.0f;
        pk.e -= pk.e * (float)dt / (life * 380.0f);
        if (pk.e < 0.004f) pk.e = 0.0f;
        if (edge) {
            int k;
            pk.e = 1.0f;
            for (k = 0; k < 8; k++) {
                int32_t a = (int32_t)(cp_rnd() & 255u), r = 66;
                pk_spark(120 + (r * cp_cos(a) >> 14), 92 + (r * cp_sin(a) >> 14), 12 + (int)(cp_rnd() & 3u), 2, (cp_cos(a) * 60 >> 14) * 256, (cp_sin(a) * 60 >> 14) * 256, 700u + cp_rnd() % 600u);
            }
        }
        pk.sp[0] = (uint16_t)(pk.sp[0] + dt * (uint32_t)(30.0f + P(mon, 2) * 60.0f));
        pk.sp[1] = (uint16_t)(pk.sp[1] + dt * 7u);
        break;
    }
    case SC_FILTER: {
        int type = (int)(P(mon, 0) + 0.5f), x, k;
        float cut = P(mon, 1), reso = P(mon, 2), q = 0.5f + reso * reso * 25.0f, fc;
        float e = 0.0f;
        for (k = 0; k < 6; k++) if (mon[16 + k] > e) e = mon[16 + k];
        type = type < 0 ? 0 : type > 5 ? 5 : type;
        fc = 440.0f * fm_exp2f((cut * 120.0f - 30.0f + P(mon, 4) * e * 60.0f - 9.0f) / 12.0f);
        fc = fc < 20.0f ? 20.0f : fc > 20000.0f ? 20000.0f : fc;
        for (x = 0; x < 240; x += 2) {         /* the curve's height 0-255 every second column (-36 to +18 dB), the others between */
            float f = 20.0f * fm_exp2f((float)x / 239.0f * 9.9658f), m = fmag(type, f / fc, q, reso), db = 6.0206f * fm_log2f(m + 1e-6f);
            int h;
            db = db < -36.0f ? -36.0f : db > 18.0f ? 18.0f : db;
            h = (int)((db + 36.0f) * (255.0f / 54.0f));
            cp.colh[x] = (uint8_t)h;
            if (x)
                cp.colh[x - 1] = (uint8_t)((cp.colh[x - 2] + h) >> 1);
        }
        cp.colh[239] = cp.colh[238];
        pk.prev = (float)(int)(fm_log2f(fc / 20.0f) / 9.9658f * 239.0f);          /* the cutoff's column */
        pk.e = reso;
        {   /* energy motes: from the left at the level, riding the curve */
            uint32_t rate = (uint32_t)(cp.lv * 140.0f);
            cp.acc += rate * dt;
            while (cp.acc >= 10000u) {
                cp.acc -= 10000u;
                cp_mote_t *o = pk_spark(0, 100, 0, 2, 70 * 256 + (int32_t)(cp_rnd() % 40u) * 256, 0, 2600u);
                if (o)                       /* (the jitter rides in the flag's high nibble) */
                    o->flags = (uint8_t)(o->flags | (cp_rnd() & 15u) << 4);
            }
            if (cp.acc > 30000u) cp.acc = 30000u;
        }
        break;
    }
    case SC_TIDES: case SC_PEAKS: case SC_FUNC: {
        int h0 = page_slot(mn.page) == RS_MODB ? 2 : 0;
        float v = (float)vz.hist[h0][(vz.hist_w + 113u) % 114u] * 0.01f;
        if (sc != SC_TIDES && v - pk.prev > 0.22f) {                  /* a trigger: a ring and sparks leave the head */
            int k;
            pk_spark(236, 96 - (int32_t)(fm_clampf(v, -1.0f, 1.0f) * 60.0f), sc == SC_PEAKS ? 14 : 11, 4, 0, 0, 900u);
            for (k = 0; k < 4; k++)
                pk_spark(236, 96 - (int32_t)(fm_clampf(v, -1.0f, 1.0f) * 60.0f), sc == SC_PEAKS ? 13 : 10, 2, -pk_vel(8000, 22000), pk_vel(-9000, 18000), 500u + cp_rnd() % 400u);
        }
        pk.prev = v;
        break;
    }
    case SC_GRIDS: {
        int st = (int)mon[47] & 31, lane;
        if (st != pk.step) {
            pk.step = (uint8_t)st;
            for (lane = 0; lane < 3; lane++)
                if (mon[64 + lane * 32 + st] > 0.45f)
                    pk_spark(10 + st * 7, 48 + lane * 46, lane == 0 ? 13 : lane == 1 ? 11 : 3, 4, 0, 0, 700u);
        }
        break;
    }
    case SC_DELAY: {
        float t = pk_delay_s(mon), fb = P(mon, 1);
        uint32_t tms = (uint32_t)(t * 1000.0f), sp = tms / 10u;
        int j, k, n = 1 + (int)(fb * 9.0f);
        sp = sp < 14u ? 14u : sp > 50u ? 50u : sp;
        for (j = 0; j < 8; j++) {            /* a sparkle where each echo of a note lands */
            uint32_t age = plat_ms() - vz.pulse_t[j];
            if (!vz.pulse_t[j])
                continue;
            for (k = 1; k < n && k < 9; k++) {
                uint32_t at = (uint32_t)k * tms;
                if (age >= at && age < at + dt && 28u + sp * (uint32_t)k < 232u)
                    pk_spark(28 + (int32_t)(sp * (uint32_t)k), 96 + ((k & 1) && (int)(P(mon, 4) + 0.5f) == 0 ? -22 : 0), 2, 4, 0, 0, 800u);
            }
        }
        break;
    }
    case SC_REVERB: {
        float size = P(mon, 0), tau = 380.0f + size * 2600.0f;     /* ms: a bigger room lets go slower */
        pk.tail -= pk.tail * (float)dt / tau;
        if (cp.lv > pk.tail) pk.tail = cp.lv;
        if (pk.tail < 0.004f) pk.tail = 0.0f;
        {   /* dust in the room, turning slowly */
            uint32_t rate = (uint32_t)(pk.tail * (30.0f + P(mon, 2) * 70.0f + P(mon, 6) * 60.0f));
            cp.acc += rate * dt;
            while (cp.acc >= 10000u) {
                int32_t rx = 40 + (int32_t)(size * 70.0f), ry = 25 + (int32_t)(size * 40.0f), a = (int32_t)(cp_rnd() & 255u), r = (int32_t)(cp_rnd() % 100u);
                cp.acc -= 10000u;
                pk_spark(120 + (rx * r / 100 * cp_cos(a) >> 14), 92 + (ry * r / 100 * cp_sin(a) >> 14), 4 + (int)(P(mon, 6) * 6.0f), 1, pk_vel(-5000, 10000), -pk_vel(2000, 7000), 1500u + cp_rnd() % 1800u);
            }
            if (cp.acc > 30000u) cp.acc = 30000u;
        }
        break;
    }
    case SC_CHORUS:
        pk.sp[0] = (uint16_t)(pk.sp[0] + dt * (uint32_t)(4.0f + (0.05f + P(mon, 0) * 4.0f) * 65.0f * 1.0f * (1.0f)));
        pk.sp[1] = (uint16_t)(pk.sp[1] + dt * 40u);
        break;
    default:
        break;
    }
    cp_motes_advance(dt, lv8 > 2u ? lv8 : 3u);
    if (sc == SC_FILTER) {
        for (i = 0; i < CP_NM; i++) {            /* the energy motes follow the curve */
            cp_mote_t *o = &cp.m[i];
            int32_t x, jit;
            if (!o->life)
                continue;
            x = o->x >> 8;
            if (x > 238) { o->life = 0; continue; }
            jit = (int32_t)(o->flags >> 4) - 8;
            o->y = ((158 - (int32_t)cp.colh[x < 0 ? 0 : x] * 100 / 255 + jit) << 8);
        }
    }
}
static int pic_idle(int sc)
{
    int v;
    switch (sc) {
    case SC_TIDES: case SC_PEAKS: case SC_FUNC: case SC_GRIDS: return 0;
    case SC_DELAY: return pk_recent(&v) > 6000u;
    case SC_REVERB: return pk.tail < 0.004f && pk_recent(&v) > 3000u;
    case SC_RINGS: return pk.e < 0.004f;
    default: return 1;
    }
}
static const char *pic_sub(int sc, const float *mon)
{
    static const char *const RN[] = {"MODAL", "SYMPATHETIC", "STRING", "FM VOICE", "SYMP QUANT", "STRING+VERB"};
    static const char *const FN[] = {"LP", "BP", "HP", "LP24", "NOTCH", "PEAK"};
    int v = (int)(P(mon, 0) + 0.5f);
    const ui_mod_t *m = page_mod(mn.page);
    switch (sc) {
    case SC_BRAIDS: return v >= 0 && v < 47 ? UI_OPT[UP_BRAIDS[0].opt + v] : "";
    case SC_RINGS: return v >= 0 && v < 6 ? RN[v] : "";
    case SC_FILTER: return v >= 0 && v < 6 ? FN[v] : "";
    case SC_DELAY: return UI_OPT[UP_DELAY[0].opt + ((int)(P(mon, 0) + 0.5f) & 15) % 9];
    case SC_REVERB: return UI_OPT[UP_REVERB[4].opt + (((int)(P(mon, 4) + 0.5f)) % 3)];
    case SC_CHORUS: return UI_OPT[UP_CHORUS[4].opt + (((int)(P(mon, 4) + 0.5f)) % 3)];
    default: return m ? m->blurb : "";
    }
}

/* a modulator's trace as a glowing line with light under it, from vz.hist (114 values, 33 ms apart); the newest (right) is the brightest */
static void pk_trace(int h, int32_t cy, int32_t amp, int fill, uint32_t c, uint32_t a)
{
    int32_t x, py = 0, lo = cp.by, hi = cp.by + (int32_t)cv_h;
    uint32_t r = c >> 16, g = (c >> 8) & 255u, b = c & 255u;
    for (x = 0; x < 240; x += 2) {
        int32_t u = x * 113 * 16 / 239, i0 = u >> 4, fr = u & 15, i1 = i0 < 113 ? i0 + 1 : 113;
        int32_t v0 = vz.hist[h][(vz.hist_w + (uint32_t)i0) % 114u], v1 = vz.hist[h][(vz.hist_w + (uint32_t)i1) % 114u];
        int32_t v = (v0 * (16 - fr) + v1 * fr) >> 4, yc, y, ye;
        uint32_t aa = a * (uint32_t)(70 + x * 185 / 239) >> 8;
        v = v > 120 ? 120 : v < -120 ? -120 : v;
        yc = cy - v * amp / 100;
        if (x)
            pk_line(x - 2, py, x, yc, c, aa);
        py = yc;
        if (fill) {                           /* light under the line, falling away from it, towards the middle line */
            int32_t dir = yc < cy ? 1 : -1;
            ye = yc + dir * fill;
            for (y = yc; y != ye; y += dir) {
                uint32_t fa = FADE[(pk_abs(y - yc) >> 1) > 31 ? 31 : pk_abs(y - yc) >> 1] * aa / 4u * (uint32_t)(fill - pk_abs(y - yc)) / (uint32_t)fill >> 8;
                if (y >= lo && y < hi && fa) {
                    cp_add(x, y, r, g, b, fa);
                    cp_add(x + 1, y, r, g, b, fa);
                }
            }
        }
    }
}
static int32_t pk_trace_y(int h, int32_t cy, int32_t amp)   /* the newest value's row */
{
    int32_t v = vz.hist[h][(vz.hist_w + 113u) % 114u];
    v = v > 120 ? 120 : v < -120 ? -120 : v;
    return cy - v * amp / 100;
}

static void pic_draw(int sc, const float *mon)
{
    uint32_t g = (uint32_t)cp.gain;
    int i, k;
    switch (sc) {
    case SC_BRAIDS: {
        int n = (int)(mon[63] + 0.5f), fam = pk_fam((int)(P(mon, 0) + 0.5f)), ri = (int)(P(mon, 5) + 0.5f), hold;
        static const uint8_t HOLD[6] = {4, 2, 1, 1, 1, 1};
        float tim = P(mon, 1), col = P(mon, 2);
        int32_t harm = (int32_t)(tim * 200.0f), p2 = (int32_t)(col * 256.0f);
        uint32_t base = pk_mix(PK_FAM[fam], PK_FAM[(fam + 1) % 6], (uint32_t)(col * 150.0f));
        n = n < 1 ? 1 : n > 6 ? 6 : n;
        pk_soft = 3;
        ri = ri < 0 ? 0 : ri > 5 ? 5 : ri;
        hold = HOLD[ri];
        for (i = 0; i < n; i++) {
            float e = mon[16 + i] > 0.0f ? mon[16 + i] : 0.0f;
            int32_t ly = 28 + (i * 2 + 1) * 66 / n, amp = 2 + (int32_t)(e * (float)(60 / n < 20 ? 60 / n : 20)), note = (int32_t)mon[34 + i], cyc = (note > 0 ? 3 + (note - 36) / 8 : 4);
            uint32_t a = 38u + (uint32_t)(e * 217.0f), c = pk_mix(base, 0xFFFFFFu, (uint32_t)(e * 90.0f));
            cyc = cyc < 2 ? 2 : cyc > 14 ? 14 : cyc;
            pk_string(10, ly, 230, ly, amp + 2, cyc * 256 / 2 + 128, (int32_t)(pk.sp[i] >> 8), fam, harm, p2, hold, c, a * 7u / 10u);
            pk_string(10, ly, 230, ly, amp, cyc * 256 / 2 + 128, (int32_t)(pk.sp[i] >> 8), fam, harm, p2, hold, c, a);
            if (e > 0.05f)
                pk_string(10, ly + 1, 230, ly + 1, amp * 6 / 10, cyc * 512 / 2 + 256, (int32_t)(pk.sp[i] >> 8) * 2, fam, harm, p2, hold, PK_FAM[(fam + 2) % 6], a / 3u);
            pk_blob(1, 0, 10, ly, c, 60u + (uint32_t)(e * 195.0f));
            pk_blob(1, 0, 230, ly, c, 60u + (uint32_t)(e * 195.0f));
        }
        pk_soft = 0;
        pk_motes(256);
        break;
    }
    case SC_RINGS: {
        int rings = 1 + (int)(P(mon, 1) * 4.0f + 0.5f), model = (int)(P(mon, 0) + 0.5f);
        float bright = P(mon, 2), e = pk.e;
        int32_t cx = 120, cy = 92, R = 62, kk, n = 72, px = 0, py = 0, pos = (int32_t)(P(mon, 4) * 255.0f);
        uint32_t a = 70u + (uint32_t)(e * 185.0f), c = pk_mix(PK_RING, 0xFFFFFFu, (uint32_t)(e * 80.0f));
        for (kk = 0; kk < rings; kk++) {                  /* the ring and the quieter rings inside it */
            int32_t rr = R * (100 - kk * 17) / 100, m;
            uint32_t ak = a * (uint32_t)(100 - kk * 14) / 100u;
            for (i = 0; i <= n; i++) {
                int32_t ang = i * 256 / n, d = 0, x, y;
                for (m = 2; m <= 5; m++) {                /* four modes, each with its own phase and a share that falls with its number */
                    int32_t amp = (int32_t)(e * 13.0f * (bright * 0.9f + 0.45f)) / (m - 1) * (kk ? 1 : 2) / 2;
                    d += ((amp * cp_cos(m * ang + (int32_t)(pk.sp[1] >> 7) * m) >> 14) * cp_cos(((int32_t)(pk.sp[0] >> 8) * m) >> 1)) >> 14;
                }
                x = cx + ((rr + d) * cp_cos(ang) >> 14);
                y = cy + ((rr + d) * cp_sin(ang) >> 14) * 94 / 100;
                if (i)
                    pk_line(px, py, x, y, c, ak);
                px = x; py = y;
            }
        }
        for (i = 0; i < 8; i++) {                         /* spokes: the modes, standing */
            int32_t ang = i * 32 + (int32_t)(pk.sp[1] >> 9), ln = 10 + (int32_t)(e * 40.0f);
            pk_line(cx + (ln * cp_cos(ang) >> 14), cy + (ln * cp_sin(ang) >> 14), cx + ((R - 6) * cp_cos(ang) >> 14), cy + ((R - 6) * cp_sin(ang) >> 14), c, (uint32_t)(e * 90.0f) + 12u);
        }
        if (model == 2 || model == 5)                     /* STRING: one string across the middle */
            pk_string(cx - R, cy, cx + R, cy, 2 + (int32_t)(e * 10.0f), 512, (int32_t)(pk.sp[0] >> 7), 0, 0, 0, 1, c, a);
        pk_blob(3, 1, cx + (R * cp_cos(pos) >> 14), cy + (R * cp_sin(pos) >> 14) * 94 / 100, 0xFFE4C8u, 120u + (uint32_t)(e * 135.0f));   /* where it is struck */
        pk_blob(2, 1, cx, cy, c, 30u + (uint32_t)(e * 120.0f));
        pk_motes(256);
        break;
    }
    case SC_FILTER: {
        int32_t cxm = (int32_t)pk.prev, x, y, base = 158;
        float reso = pk.e;
        uint32_t r0, a = 90u + g * 150u / 255u, px = 0, pyy = 0;
        for (x = 0; x < 240; x++) {                       /* under the curve: the passband as light, thinning away from the line */
            int32_t h = cp.colh[x] * 100 / 255, yt = base - h;
            uint32_t hue_t = (uint32_t)(x * 256 / 239), c = pk_mix(0xFFA458u, 0x58F0D0u, hue_t > 256u ? 256u : hue_t);
            for (y = yt + 1; y < yt + 34 && y < base + 1; y++) {
                if (y < cp.by || y >= cp.by + (int32_t)cv_h)
                    continue;
                r0 = FADE[(y - yt) >> 1 > 31 ? 31 : (y - yt) >> 1] * (uint32_t)(40 + g / 3u) >> 9;
                cp_add(x, y, c >> 16, (c >> 8) & 255u, c & 255u, r0);
            }
        }
        for (x = 1; x < 240; x += 2) {                    /* the line */
            int32_t y0 = base - cp.colh[x - 1] * 100 / 255, y1 = base - cp.colh[x] * 100 / 255;
            uint32_t hue_t = (uint32_t)(x * 256 / 239), c = pk_mix(0xFFB468u, 0x70FFE0u, hue_t > 256u ? 256u : hue_t);
            pk_line(x - 1, y0, x + 1, y1, c, a);
            (void)px; (void)pyy;
        }
        for (y = 14; y < base; y += 5)                    /* the cutoff */
            if (y >= cp.by && y < cp.by + (int32_t)cv_h)
                cp_add(cxm, y, 255, 244, 220, 70);
        {
            int32_t yp = base - cp.colh[cxm < 0 ? 0 : cxm > 239 ? 239 : cxm] * 100 / 255;    /* resonance: a bloom on the peak */
            pk_blob(3, 1, cxm, yp, 0xFFFFE8u, 40u + (uint32_t)(reso * 215.0f * (0.4f + cp.lv)));
            pk_blob(1, 0, cxm, yp, 0xFFFFFFu, 120u + (uint32_t)(reso * 135.0f));
        }
        for (i = 0; i < CP_NM; i++) {                     /* the energy going through it */
            cp_mote_t *o = &cp.m[i];
            int32_t mx;
            uint32_t f, gg;
            if (!o->life)
                continue;
            mx = o->x >> 8;
            f = o->age < 300u ? o->age * 255u / 300u : (o->life - o->age) * 255u / (o->life - 300u);
            gg = (f > 255u ? 255u : f) * (uint32_t)(40 + cp.colh[mx < 0 ? 0 : mx > 239 ? 239 : mx] * 215 / 255) >> 8;
            pk_blob(1, 0, mx, o->y >> 8, pk_mix(0xFFB468u, 0x70FFE0u, (uint32_t)(mx * 256 / 239)), gg);
        }
        break;
    }
    case SC_TIDES: case SC_PEAKS: case SC_FUNC: {
        int h0 = page_slot(mn.page) == RS_MODB ? 2 : 0;
        uint32_t c1 = sc == SC_TIDES ? PK_TIDE : sc == SC_PEAKS ? PK_PEAK : PK_FUNC, c2 = pk_mix(c1, sc == SC_TIDES ? 0x9A70FFu : 0xFFFFFFu, 150);
        int th = sc == SC_TIDES ? 36 : sc == SC_PEAKS ? 50 : 30;
        int32_t cy = 96, hy, hx = 236;
        for (i = 14; i < 240; i += 6)                     /* the middle, dotted, and the extremes */
            pk_px(i, cy, 0xFFFFFFu, 26u);
        for (i = 20; i < 240; i += 12) { pk_px(i, cy - 60, 0xFFFFFFu, 14u); pk_px(i, cy + 60, 0xFFFFFFu, 14u); }
        pk_trace(h0 + 1, cy, 60, th / 2, c2, 150u);
        pk_trace(h0, cy, 60, th, c1, 255u);
        hy = pk_trace_y(h0, cy, 60);
        cp_spr(SPR_COMET, 40, 9, hx - 39, hy - 4, 0xFFF4E4u, 180u);
        pk_blob(3, 1, hx, hy, c1, 190u);
        pk_blob(1, 0, hx, hy, 0xFFFFFFu, 255u);
        pk_motes(256);
        break;
    }
    case SC_GRIDS: {
        int st = (int)mon[47] & 31, lane;
        static const uint32_t LC[3] = {0xFF8860u, 0xFFD870u, 0x70D8FFu};
        for (lane = 0; lane < 3; lane++) {
            int32_t ly = 48 + lane * 46, s;
            pk_line(8, ly, 232, ly, LC[lane], 10u);
            for (s = 0; s < 32; s++) {
                float lv = mon[64 + lane * 32 + s];
                int32_t x = 10 + s * 7;
                uint32_t a = 50u + (uint32_t)(lv * 205.0f);
                if (s == st) {                            /* the step now: a lit column */
                    pk_blob(2, 1, x, ly, 0xFFFFFFu, 60u + (uint32_t)(lv * 150.0f));
                }
                if (lv < 0.06f)
                    pk_px(x, ly, LC[lane], 60u);
                else if (lv < 0.5f)
                    pk_blob(1, 0, x, ly, LC[lane], a);
                else {
                    pk_blob(2, 1, x, ly, LC[lane], a / 2u);
                    pk_blob(1, 0, x, ly, pk_mix(LC[lane], 0xFFFFFFu, 90), a);
                }
            }
        }
        pk_haze(10 + st * 7, 95, 8, 80, 0xFFFFFFu, 22u + g / 12u);
        pk_motes(256);
        break;
    }
    case SC_DELAY: {
        float fb = P(mon, 1), mix = P(mon, 3);
        uint32_t tms = (uint32_t)(pk_delay_s(mon) * 1000.0f), sp = tms / 10u, now = plat_ms();
        int j, n = 1 + (int)(fb * 9.0f), ping = (int)(P(mon, 4) + 0.5f) == 0;
        int32_t px = 0, py = 0;
        float ak = 0.3f + 0.7f * mix;
        sp = sp < 14u ? 14u : sp > 50u ? 50u : sp;
        for (k = 0; k < 10; k++) {                        /* the lattice of repeats, fading by FEEDBACK: where each echo will land */
            int32_t x = 28 + (int32_t)(sp * (uint32_t)k), y = 96 + (ping && (k & 1) ? -22 : 0);
            uint32_t a = (uint32_t)(ak * 255.0f);
            if (x > 232 || k >= n + 1)
                break;
            for (j = 0; j < k; j++)
                a = (uint32_t)((float)a * fb);
            if (k)
                pk_line(px, py, x, y, PK_DLY, a / 2u);
            pk_blob(2, 1, x, y, PK_DLY, 60u + a / 2u);
            pk_blob(0, 0, x, y, 0xFFFFFFu, 70u + a / 2u);
            px = x; py = y;
        }
        for (j = 0; j < 8; j++) {                         /* each note's echoes, one after another, each ring dimmer than the last */
            uint32_t age = vz.pulse_t[j] ? now - vz.pulse_t[j] : 99999u;
            for (k = 0; k < 10 && k < n + 1; k++) {
                int32_t x = 28 + (int32_t)(sp * (uint32_t)k), y = 96 + (ping && (k & 1) ? -22 : 0);
                uint32_t at = (uint32_t)k * tms, ta, a, jj;
                if (x > 232 || age < at)
                    break;
                ta = age - at;
                if (ta > 900u)
                    continue;
                a = (uint32_t)(ak * (float)vz.pulse_v[j] * 2.0f);
                for (jj = 0; jj < (uint32_t)k; jj++)
                    a = (uint32_t)((float)a * fb);
                a = a * (900u - ta) / 900u;
                pk_ring(x, y, (int32_t)(3u + ta / 45u), (int32_t)(3u + ta / 50u), PK_DLY, a);
                pk_blob(2, 0, x, y, pk_mix(PK_DLY, 0xFFFFFFu, 110), a * (ta < 250u ? 250u - ta : 0u) / 250u);
            }
        }
        pk_motes(256);
        break;
    }
    case SC_REVERB: {
        float size = P(mon, 0), damp = P(mon, 1), tail = pk.tail;
        int32_t rx = 36 + (int32_t)(size * 50.0f), ry = 22 + (int32_t)(size * 32.0f), j, now = (int32_t)plat_ms();
        uint32_t col = pk_mix(0x8E80FFu, 0xFF9A70u, (uint32_t)(damp * 200.0f)), ha = 22u + (uint32_t)(tail * 120.0f);
        pk_haze(120, 92, rx, ry, col, ha);
        pk_haze2(120, 92, rx * 6 / 10, ry * 6 / 10, pk_mix(col, 0xFFFFFFu, 90), ha * 7u / 10u, 1);
        for (j = 0; j < 8; j++) {                         /* a note's wavefront crossing the room (after PRE) */
            uint32_t age = vz.pulse_t[j] ? (uint32_t)(now - (int32_t)vz.pulse_t[j]) : 99999u, life = 500u + (uint32_t)(size * 1800u);
            if (age < life) {
                uint32_t a = (255u - age * 255u / life) * vz.pulse_v[j] / 127u;
                pk_ring(120, 92, (int32_t)(age * (uint32_t)rx / life) + 3, (int32_t)(age * (uint32_t)ry / life) + 2, pk_mix(col, 0xFFFFFFu, 100), a / 3u);
            }
        }
        pk_motes(256);
        break;
    }
    case SC_CHORUS: {
        float depth = P(mon, 1), wid = P(mon, 2), mix = P(mon, 3);
        int32_t lfo = cp_sin((int32_t)(pk.sp[0] >> 8)), amp = 12 + (int32_t)(wid * 20.0f) + (int32_t)(cp.lv * 16.0f);
        static const uint32_t SC[5] = {0x62F0C0u, 0x62D8F0u, 0xA8A8FFu, 0x62D8F0u, 0x62F0C0u};
        for (k = 0; k < 5; k++) {
            int32_t spread = (k - 2) * (int32_t)(30.0f + depth * 60.0f), wob = (lfo * (int32_t)(depth * 40.0f) >> 14) * (k - 2) / 2, cy = 95 + (k - 2) * 10;
            uint32_t a = k == 2 ? 255u : (uint32_t)(90.0f + 140.0f * mix);
            pk_string(6, cy, 234, cy, amp, 2 * 256 + 128, (int32_t)(pk.sp[1] >> 8) + spread + wob, 0, 0, 0, 1, SC[k], a * (60u + g * 3u / 4u) >> 8);
        }
        pk_blob(2, 1, 120, 95, 0xFFFFFFu, 40u + g / 2u);
        pk_motes(256);
        break;
    }
    default:
        break;
    }
    (void)k;
}
