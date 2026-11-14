/* SPDX-License-Identifier: MPL-2.0 */
/* FM1 Phase 2 firmware: the CLOUDS page's picture (update 7), part of app.c after vis.c and vis3.c.
 *
 * The page is "picture first" with the picture all the way down to a slim band of four read-outs: the picture is 240 x
 * 190 (y 10 to 200), drawn in five bands of at most 40 rows (the canvas is 240 x 40), the read-outs 240 x 40 under it.
 * Drawing is in the main loop (app_loop: draw() and draw_well_only()), never in the audio interrupt or on the second
 * core, so a slow frame delays the screen and the buttons, not the sound.
 *
 * What is real and what is illustrative:
 *   real     the parameters as the engine hears them (rk_monitor: POSITION, SIZE, PITCH, DENSITY, TEXTURE, SPRAY, MODE,
 *            FREEZE, QUALITY, with the matrix's modulation), the output peak (the ribbon and every brightness), the
 *            voices' gates (a burst of grains on a new note), the buffer's real length for the QUALITY chosen (the
 *            comet's speed), the freeze (the ribbon stops being written, as the buffer does).
 *   modelled grain births: their rate from DENSITY (none at 0.47 to 0.53, where Clouds plays none), their place from
 *            POSITION and SPRAY, their size, hue, softness and speed from SIZE, PITCH and TEXTURE. Clouds' real grain
 *            positions are private to Mutable's code and are NOT read.
 *   illustrative  SPECTRL's 24 curtains: Clouds' FFT is not read; they follow TEXTURE, DENSITY, SIZE and the level.
 *            The ribbon's small column-to-column texture is cosmetic (it only ever scales the real level).
 * Cost: no floating point and no maths functions per pixel: glows are const sprites (clouds_sprites.h, in flash) added
 * with saturation into RGB565; at most CP_NM motes; the picture is drawn at most 30 times a second and not at all
 * while the sound is silent and nothing on it moves.
 * Update 8: at most 20 pictures a second, and only the ROWS THAT CHANGED are sent to the LCD (a hash of each row of the picture and of the
 * read-outs, kept from the last frame: rows alike are not sent; runs of changed rows within CP_GAP of each other go in one transfer). The
 * picture is composed in full as before (the compose is cheap, 130-160 us on the host); what is saved is the SPI transfer, which is what the
 * FM-1's 25-30 ms a frame (update 7's debug read-out) is made of, and the wait for it at the next canvas clear. */
#include "clouds_sprites.h"
#ifdef PLAT_HOST
#include <stdio.h>
#endif

#define CP_Y 10                               /* the picture: from under the status strip ... */
#define CP_H 190                              /* ... 190 rows, the read-outs from y 200 */
#define CP_RY 200
#define CP_CY 146                             /* the ribbon's centre line (picture rows), in the lower third */
#define CP_NM 48
#define CP_MIN_MS 50u                         /* at most 20 pictures a second (update 8; was 30 ms, ~33) */
#define CP_GAP 3                              /* changed rows this close (or closer) go in one transfer */
typedef struct {
    int32_t x, y, vx, vy;                     /* picture pixels x 256; pixels a second x 256 */
    uint16_t age, life;                       /* ms */
    uint8_t hue, sz, gain, flags;             /* HUE step 0-15; 0-2 size; brightness; 1 soft, 2 spark */
} cp_mote_t;
static struct {
    uint8_t env[240];                         /* the ribbon: the recent output level, a column written as the head passes */
    uint8_t colh[240], colg[240];             /* this frame's half-height and brightness of each column */
    cp_mote_t m[CP_NM];
    uint32_t lv8_prev;
    uint32_t head;                            /* the write head, in columns x 256 */
    uint32_t ph;                              /* the loop's playhead, in points x 256 */
    uint32_t t_ms, draw_ms, acc, rng, sig;
    int32_t gain;                             /* overall brightness 0-255 from the level (silence: dim) */
    float lv;                                 /* the smoothed output level 0-1 */
    uint32_t act[4];                          /* when each read-out's value last moved */
    float prev[4];
    int pkey, force, dbg, oct_both, still, nlive;
    uint32_t us, us_max, nd, us_sum;          /* the picture's draw time (ticks24 / 24): last, worst, count, sum */
    uint32_t rh[CP_H + 40];                   /* a hash of each row as last sent: the picture's 190 rows, then the read-outs' 40 */
    int rh_ok;                                /* 0: the screen's own content is not what rh says (a page change, a toast): send all */
    uint32_t t_lcd, t_sec, a_draw, a_lcd, a_n, a_rows, a_xf;   /* update 8: one second's sums: the LCD's ticks, when it began, draw ticks, LCD ticks, frames, rows and transfers sent */
    uint32_t sh_draw10, sh_lcd10, sh_fps, sh_rows, sh_xf;      /* the last second's figures: ms x 10 of a frame, frames, rows and transfers a frame x 10 */
    int by, bh, freeze, mode;                 /* the band being drawn */
} cp = {.rng = 2463534242u, .pkey = -1, .force = 1};

static uint32_t cp_rnd(void)
{
    cp.rng ^= cp.rng << 13;
    cp.rng ^= cp.rng >> 17;
    cp.rng ^= cp.rng << 5;
    return cp.rng;
}
static int32_t cp_sin(int32_t a) { return SIN256[a & 255]; }          /* a: 256 steps a turn, result +-16383 */
static int32_t cp_cos(int32_t a) { return SIN256[(a + 64) & 255]; }

/* c (r, g, b, 0-255) at a (0-255) added over the pixel (x, y of the picture), each channel saturating */
static inline void cp_add(int32_t x, int32_t y, uint32_t r, uint32_t g, uint32_t b, uint32_t a)
{
    uint32_t d, dr, dg, db;
    uint16_t *p;
    y -= cp.by;
    if ((uint32_t)x >= 240u || (uint32_t)y >= (uint32_t)cp.bh)
        return;
    p = &cv_px[(uint32_t)y * 240u + (uint32_t)x];
    d = swap16(*p);
    dr = (d >> 11) + ((r * a + 1024u) >> 11);
    dg = ((d >> 5) & 63u) + ((g * a + 512u) >> 10);
    db = (d & 31u) + ((b * a + 1024u) >> 11);
    *p = swap16((dr > 31u ? 31u : dr) << 11 | (dg > 63u ? 63u : dg) << 5 | (db > 31u ? 31u : db));
}
/* a sprite's alpha mask (w x h) with its top left at (x, y), colour c = 0xRRGGBB, brightness g (0-255) */
static void cp_spr(const uint8_t *s, int32_t w, int32_t h, int32_t x, int32_t y, uint32_t c, uint32_t g)
{
    int32_t i, j, y0 = y - cp.by < 0 ? cp.by - y : 0, y1 = y - cp.by + h > cp.bh ? cp.bh - (y - cp.by) : h;
    uint32_t r = c >> 16, gg = (c >> 8) & 255u, b = c & 255u;
    if (g == 0 || x >= 240 || x + w <= 0)
        return;
    for (j = y0; j < y1; j++)
        for (i = 0; i < w; i++) {
            uint32_t a = s[j * w + i];
            if (a)
                cp_add(x + i, y + j, r, gg, b, (a * g) >> 8);
        }
}
static uint32_t cp_hue(int32_t k)             /* HUE step k (0-15) as 0xRRGGBB */
{
    k = k < 0 ? 0 : k > 15 ? 15 : k;
    return (uint32_t)HUE[k * 3] << 16 | (uint32_t)HUE[k * 3 + 1] << 8 | HUE[k * 3 + 2];
}
static const char *const CP_MODE[3] = {"GRANULR", "LOOPDLY", "SPECTRL"};
static const uint16_t CP_BUF_MS[4] = {510, 930, 1020, 1850};   /* the buffer's length at 48 kHz for QUALITY 16 ST, 16 MONO, 8 ST, 8 MONO (1.5 x as long at 32 kHz, update 8) */

static int clouds_page(int pg)
{
    const ui_mod_t *m;
    if (pg != PG_FX1 && pg != PG_FX2)
        return 0;
    m = page_mod(pg);
    return m && m->id == 30 && !mn.pick && !is_mod_sub(pg);
}

static void cp_birth(int32_t x, int32_t y, int32_t vx, int32_t vy, int hue, int sz, int soft, int spark, uint32_t life)
{
    int i;
    for (i = 0; i < CP_NM; i++)
        if (!cp.m[i].life) {
            cp_mote_t *o = &cp.m[i];
            o->x = x << 8; o->y = y << 8; o->vx = vx; o->vy = vy;
            o->age = 0; o->life = (uint16_t)life;
            o->hue = (uint8_t)(hue < 0 ? 0 : hue > 15 ? 15 : hue);
            o->sz = (uint8_t)sz;
            o->gain = (uint8_t)(150u + cp_rnd() % 105u);
            o->flags = (uint8_t)((soft ? 1 : 0) | (spark ? 2 : 0));
            return;
        }
}

/* the picture's state moves on dt ms: level, write head, ribbon, grain births, motes */
static void cp_step(const float *mon, uint32_t dt)
{
    float raw = mon[48] > mon[49] ? mon[48] : mon[49], pos = P(mon, 0), size = P(mon, 1), pitch = P(mon, 2), dens = P(mon, 3);
    float tex = P(mon, 4), spray = P(mon, 16);
    int qual = (int)(P(mon, 11) + 0.5f) & 3, i, n, mode = (int)(P(mon, 9) + 0.5f), pf, hue, gates = 0;
    uint32_t bufms, col0, col1, lv8, lo;
    static uint8_t gate_was;
    raw = raw * 4.0f > 1.0f ? 1.0f : raw * 4.0f;
    raw = raw * (2.0f - raw);                 /* compressed: a quiet chord still moves the picture */
    cp.lv += (raw - cp.lv) * (raw > cp.lv ? (dt > 40u ? 1.0f : (float)dt * 0.025f) : (float)dt * 0.0055f > 1.0f ? 1.0f : (float)dt * 0.0055f);
    mode = mode < 0 ? 0 : mode > 2 ? 2 : mode;
    cp.mode = mode;
    cp.freeze = (int)(P(mon, 10) + 0.5f) == 1;
    if (mode == 2)
        qual |= 1;                            /* SPECTRL plays as the mono QUALITY (engine: CL_TRIMMED) */
    bufms = CP_BUF_MS[qual] * (rk_fm1_status(5) ? 3u : 2u) / 2u;   /* (Clouds at 32 kHz holds the same samples 1.5 x as long) */
    lv8 = (uint32_t)(cp.lv * 255.0f);
    cp.gain = 52 + (int32_t)(cp.lv * 2.2f > 1.0f ? 203.0f : cp.lv * 2.2f * 203.0f);
    pitch = pitch < -1.0f ? -1.0f : pitch > 1.0f ? 1.0f : pitch;
    pf = (int)((pitch + 1.0f) * 0.5f * 15.0f + 0.5f);
    {   /* the new notes: a burst of grains (NOTE TRG and a fresh chord both start grains) */
        uint8_t g = 0;
        for (i = 0; i < 6; i++)
            if (mon[28 + i] >= 1.0f)
                g |= (uint8_t)(1u << i);
        gates = (g & ~gate_was) != 0;
        gate_was = g;
    }
    /* the ribbon: written as the head passes, unless frozen; silent, the head parks and the ribbon settles flat */
    if (lv8 < 3u) {
        if (!cp.freeze)
            for (i = 0; i < 240; i++)
                cp.env[i] = (uint8_t)(cp.env[i] - (cp.env[i] >> 3) - (cp.env[i] ? 1 : 0));
    } else if (!cp.freeze) {
        col0 = cp.head >> 8;
        cp.head += dt * (240u * 256u) / bufms;
        if (cp.head >= 240u * 256u)
            cp.head -= 240u * 256u;
        col1 = cp.head >> 8;
        {
            int tot = (int)(col1 >= col0 ? col1 - col0 : col1 + 240u - col0);
            tot = tot ? tot : 1;
            for (n = 0, i = (int)col0; i != (int)col1 && n < 240; n++) {
            uint32_t lvi = (cp.lv8_prev * (uint32_t)(tot - n) + lv8 * (uint32_t)n) / (uint32_t)tot;
            lo = lvi * (224u + (cp_rnd() & 63u)) >> 8;
            cp.env[i] = (uint8_t)(lo > 255u ? 255u : lo);
            i = i + 1 >= 240 ? 0 : i + 1;
            }
        }
    }
    cp.lv8_prev = lv8;
    if (mode == 1 && lv8 >= 3u) {   /* LOOPDLY: the playhead goes round the loop */
        uint32_t nl = (uint32_t)((0.10f + 0.90f * size) * 256.0f) + 1u, loop_ms = (uint32_t)((float)bufms * (0.3f + 0.7f * size)) + 1u;
        cp.ph += dt * nl * 256u / loop_ms;
        cp.ph %= nl * 256u;
    }
    /* grain births */
    {
        float d = dens - 0.5f, rate = 0.0f, lvg = cp.lv * 3.0f > 1.0f ? 1.0f : cp.lv * 3.0f;
        d = d < 0.0f ? -d : d;
        if (mode == 0 && d > 0.03f && lv8 >= 3u)
            rate = (d - 0.03f) * (1.0f / 0.47f) * 70.0f * lvg;
        else if (mode == 1 && lv8 >= 3u)
            rate = 14.0f * lvg;
        else if (mode == 2 && lv8 >= 3u)
            rate = 12.0f * lvg;
        cp.acc += (uint32_t)(rate * 10.0f) * dt;
        n = 0;
        while (cp.acc >= 10000u || (gates && n < 5 && mode == 0 && d > 0.03f)) {
            int32_t x, y, vx, vy, sz, mult, ang;
            int soft = (float)(cp_rnd() & 255u) < tex * 255.0f;
            uint32_t r = cp_rnd();
            if (cp.acc >= 10000u)
                cp.acc -= 10000u;
            n++;
            if (n > 24)
                break;
            hue = pf + (int)(r & 3u) - 1;
            sz = size < 0.33f ? 0 : size < 0.66f ? 1 : 2;
            if ((r >> 4 & 3u) == 0u && sz < 2) sz++;
            if (pf < 5 && sz < 2) sz++;       /* down: larger */
            else if (pf > 11 && sz > 0 && (r >> 6 & 1u)) sz--;
            mult = -170 + 680 * pf / 15;       /* x 1/256: sinking a little at the bottom of PITCH, rising fast at the top */
            if (mode == 0) {
                int32_t j = 14 + (int32_t)(spray * 80.0f), px = (int32_t)(pos * 239.0f);
                x = px + (int32_t)((cp_rnd() & 255u) + (cp_rnd() & 255u) - 255u) * j / 255;
                x = x < 2 ? 2 : x > 237 ? 237 : x;
                y = CP_CY - 18 + (int32_t)(cp_rnd() % 36u);
                vx = (int32_t)(cp_rnd() % 16000u) - 8000;
                vy = -(int32_t)((50u + cp_rnd() % 70u) * 256u) * mult / 256;
                cp_birth(x, y, vx, vy, hue, sz, soft, 0, 800u + (cp_rnd() % 900u) + (uint32_t)sz * 150u);
            } else if (mode == 1) {           /* sparks off the lit arc of the orbit */
                int32_t nl = (int32_t)((0.10f + 0.90f * size) * 256.0f) + 1, s0 = (int32_t)(pos * 256.0f);
                int32_t pidx = (s0 + (int32_t)(cp_rnd() % (uint32_t)nl)) & 255, phi = (int32_t)(pitch * 28.0f);
                int32_t ex = 100 * cp_cos(pidx) >> 14, ey = 70 * cp_sin(pidx) >> 14;
                x = 120 + ((ex * cp_cos(phi) - ey * cp_sin(phi)) >> 14);
                y = 88 + ((ex * cp_sin(phi) + ey * cp_cos(phi)) >> 14);
                ang = pidx;
                vx = cp_cos(ang) * 40 >> 14 << 8;
                vy = cp_sin(ang) * 40 >> 14 << 8;
                cp_birth(x, y, vx, vy, hue, 0, 0, 1, 700u + cp_rnd() % 700u);
            } else {                          /* SPECTRL: sparks lift off the curtains' tops */
                int32_t k = (int32_t)(cp_rnd() % 24u);
                x = 10 + k * 9;
                y = 168 - (int32_t)cp.colh[x] - 4;
                cp_birth(x, y < 8 ? 8 : y, (int32_t)(cp_rnd() % 600u) - 300, -(int32_t)(26u + cp_rnd() % 30u) * 256, 4 + (int)(cp_rnd() & 3u), 0, 0, 1, 700u + cp_rnd() % 900u);
            }
            gates = 0;
            
        }
        if (cp.acc > 30000u)
            cp.acc = 30000u;
    }
    /* the motes */
    cp.nlive = 0;
    for (i = 0; i < CP_NM; i++) {
        cp_mote_t *o = &cp.m[i];
        uint32_t step;
        if (!o->life)
            continue;
        step = cp.freeze ? dt >> 2 : dt;      /* frozen: they hang, and age a quarter as fast */
        if (cp.freeze) {
            o->vx -= o->vx * (int32_t)dt >> 8;
            o->vy -= o->vy * (int32_t)dt >> 8;
        }
        if (lv8 < 3u)
            step = dt << 1;                   /* silent: what is left fades out */
        o->x += o->vx * (int32_t)dt / 1000;
        o->y += o->vy * (int32_t)dt / 1000;
        o->age = (uint16_t)(o->age + step);
        if (o->age >= o->life || o->y < (4 << 8) || o->y > (CP_H << 8))
            o->life = 0;
        else
            cp.nlive++;
    }
}

/* ------------------------------------------------------------------------------------------------ the layers */
static void cp_ribbon(int pf)
{
    int32_t x, cy = CP_CY, r0, r1, dy, h, lo = cp.by, hi = cp.by + cp.bh;
    uint32_t col = cp.freeze ? 0xA8DCFFu : cp_hue(pf - 3), r, g, b;
    r = ((col >> 16) * 2u + 255u) / 3u;       /* the ribbon: its hue, washed toward white */
    g = (((col >> 8) & 255u) * 2u + 255u) / 3u;
    b = ((col & 255u) * 2u + 255u) / 3u;
    if (cp.freeze) { r = 175; g = 225; b = 255; }
    for (x = 0; x < 240; x++) {
        uint32_t gn = cp.colg[x], ih = 10240u / (uint32_t)cp.colh[x];
        h = cp.colh[x];
        r0 = cy - h - 2;
        r1 = cy + h + 3;
        if (r0 < lo) r0 = lo;
        if (r1 > hi) r1 = hi;
        for (dy = r0; dy < r1; dy++) {
            uint32_t t = (uint32_t)((dy > cy ? dy - cy : cy - dy)) * ih >> 8, a;
            if (t > 63u)
                continue;
            a = PROF[t] * gn >> 8;
            cp_add(x, dy, r, g, b, a);
        }
        r0 = cy + h + 3;                      /* the reflection: a faint streak under the ribbon */
        r1 = r0 + 30;
        if (r0 < lo) r0 = lo;
        if (r1 > hi) r1 = hi;
        for (dy = r0; dy < r1; dy++) {
            uint32_t a = FADE[dy - (cy + h + 3)] * gn * (uint32_t)h >> 14;
            if (a > 0u)
                cp_add(x, dy, r, g, b, a > 90u ? 90u : a);
        }
    }
}
static void cp_granular(const float *mon, int pf)
{
    float pos = P(mon, 0), spray = P(mon, 16);
    int32_t xt = (int32_t)(pos * 239.0f), hw = 3 + (int32_t)(spray * 70.0f), x, y, i, hc = (int32_t)(cp.head >> 8);
    uint32_t col = cp.freeze ? 0xB8E4FFu : 0xDDE6FFu, g = (uint32_t)cp.gain;
    /* SPRAY: a soft halo round the thread, rising from the ribbon */
    if (spray > 0.02f)
        for (y = CP_CY - 70; y < CP_CY - 3; y++) {
            uint32_t va;
            if (y < cp.by || y >= cp.by + cp.bh)
                continue;
            va = FADE[(CP_CY - 3 - y) >> 1 < 31 ? (CP_CY - 3 - y) >> 1 : 31] * g >> 11;
            for (x = xt - hw; x <= xt + hw; x++) {
                uint32_t d = (uint32_t)(x > xt ? x - xt : xt - x) * 63u / (uint32_t)(hw + 1), a = GAUSS[d > 63u ? 63u : d] * va >> 8;
                if (a)
                    cp_add(x, y, col >> 16, (col >> 8) & 255u, col & 255u, a > 44u ? 44u : a);
            }
        }
    cp_ribbon(pf);
    /* POSITION: a thin thread from a marker to the ribbon */
    for (y = 12; y < CP_CY - 2; y++)
        if (y >= cp.by && y < cp.by + cp.bh)
            cp_add(xt, y, col >> 16, (col >> 8) & 255u, col & 255u, (60u + (uint32_t)(y - 12) * 100u / 130u) * g >> 8);
    for (i = 0; i < 3; i++)
        for (x = xt - 2 + i; x <= xt + 2 - i; x++)
            cp_add(x, 8 + i, 255, 150, 120, 200);
    /* the motes */
    for (i = 0; i < CP_NM; i++) {
        cp_mote_t *o = &cp.m[i];
        static const uint8_t SW[3] = {9, 13, 19};
        uint32_t f, gg;
        int32_t w;
        if (!o->life)
            continue;
        f = o->age * 5u < o->life ? o->age * 5u * 255u / o->life : (o->life - o->age) * 255u / (o->life - o->life / 5u);
        f = f > 255u ? 255u : f;
        gg = f * o->gain >> 8;
        gg = gg * (uint32_t)cp.gain >> 8;
        gg = gg * (300u - (uint32_t)o->sz * 70u) >> 8;   /* (the large ones less, so a crowd of them does not burn out) */
        gg = gg > 255u ? 255u : gg;
        if (cp.freeze)
            gg = gg * (176u + (uint32_t)(cp_sin((int32_t)o->age / 3 + i * 37) + 16384) / 256u) >> 8;   /* shimmer */
        w = SW[o->sz];
        {
            int32_t mx = (o->x >> 8) - w / 2, my = (o->y >> 8) - w / 2;
            if (my + w <= cp.by || my >= cp.by + cp.bh)
                continue;
            cp_spr(o->sz == 0 ? (o->flags & 1 ? SPR_S9 : SPR_H9) : o->sz == 1 ? (o->flags & 1 ? SPR_S13 : SPR_H13) : (o->flags & 1 ? SPR_S19 : SPR_H19), w, w, mx, my,
                   cp_hue(o->hue), gg);
        }
    }
    /* the write head: a comet at the real buffer rate; frozen, a hollow ring; silent, parked and dim */
    if (cp.freeze)
        cp_spr(SPR_RING, 15, 15, hc - 7, CP_CY - 7, 0xCFEFFFu, 255);
    else
        cp_spr(SPR_COMET, 40, 9, hc - 39, CP_CY - 4, 0xFFF4E4u, (uint32_t)cp.gain);
}
static void cp_sparks(uint32_t g)                      /* LOOPDLY's and SPECTRL's sparks */
{
    int i;
    for (i = 0; i < CP_NM; i++) {
        cp_mote_t *o = &cp.m[i];
        uint32_t f;
        if (!o->life)
            continue;
        f = o->age * 4u < o->life ? o->age * 4u * 255u / o->life : (o->life - o->age) * 255u / (o->life - o->life / 4u);
        f = (f > 255u ? 255u : f) * o->gain >> 8;
        cp_spr(SPR_H5, 5, 5, (o->x >> 8) - 2, (o->y >> 8) - 2, cp_hue(o->hue), f * g >> 8);
    }
}
static void cp_loop(const float *mon, int pf)
{
    float size = P(mon, 1), pos = P(mon, 0), pitch = P(mon, 2);
    int32_t nl = (int32_t)((0.10f + 0.90f * size) * 256.0f) + 1, s0 = (int32_t)(pos * 256.0f), phi = (int32_t)(pitch * 28.0f), i, j;
    uint32_t col = cp.freeze ? 0xB0E0FFu : cp_hue(pf), g = (uint32_t)cp.gain, r = col >> 16, gg = (col >> 8) & 255u, b = col & 255u;
    int32_t head = (s0 + (int32_t)(cp.ph >> 8)) & 255;
    int32_t cphi = cp_cos(phi), sphi = cp_sin(phi);
    for (i = 0; i < 256; i++) {
        int32_t lit = ((i - s0) & 255) < nl, e = cp.env[i * 15 >> 4], rx = 100 + (e >> 4), ry = 70 + (e >> 5);
        int32_t ex = rx * cp_cos(i) >> 14, ey = ry * cp_sin(i) >> 14;
        int32_t x = 120 + ((ex * cphi - ey * sphi) >> 14), y = 88 + ((ex * sphi + ey * cphi) >> 14);
        if (y < cp.by - 8 || y > cp.by + cp.bh + 8)
            continue;
        if (lit) {
            if (i & 1)
                cp_spr(SPR_S5, 5, 5, x - 2, y - 2, col, g);
            cp_add(x, y, 255, 255, 255, 30u + (uint32_t)e / 5u);
            for (j = 1; j < 1 + (e >> 6); j++)          /* bristles: the level at that point of the loop */
                cp_add(x + (ex * j / 90), y + (ey * j / 90), r, gg, b, (uint32_t)(140 - j * 14) * g >> 8);
        } else {
            cp_add(x, y, r, gg, b, 40u * g >> 8);
        }
    }
    for (j = 0; j < 9; j++) {                           /* the playhead with a short trail */
        int32_t k = (head - j * 3) & 255, ex = 100 * cp_cos(k) >> 14, ey = 70 * cp_sin(k) >> 14;
        int32_t x = 120 + ((ex * cphi - ey * sphi) >> 14), y = 88 + ((ex * sphi + ey * cphi) >> 14);
        if (j == 0 && cp.freeze)
            cp_spr(SPR_RING, 15, 15, x - 7, y - 7, 0xCFEFFFu, 255);
        else
            cp_spr(j == 0 ? SPR_H9 : SPR_S9, 9, 9, x - 4, y - 4, 0xFFF4E4u, (uint32_t)(255 - j * 26) * g >> 8);
    }
    cp_sparks(g);
}
static void cp_spectral(void)
{
    int32_t y, x, base = 168;
    uint32_t g = (uint32_t)cp.gain;
    for (x = 0; x < 240; x++) {                         /* each column its interpolated curtain height, silk-striped */
        int32_t h = cp.colh[x], d = ((x - 10) % 9 + 9) % 9;
        uint32_t stripe;
        d = d > 4 ? 9 - d : d;
        stripe = 150u + (uint32_t)(4 - d) * 26u;
        if (base - h >= cp.by + cp.bh || base <= cp.by)
            continue;
        for (y = base - h; y < base; y++) {
            uint32_t fr, a;
            if (y < cp.by || y >= cp.by + cp.bh)
                continue;
            fr = (uint32_t)(base - y) * 255u / (uint32_t)h;       /* 0 at the base, 255 at the top */
            a = (((((255u - fr) * (255u - fr) >> 8) * 153u + (255u - fr) * 102u) >> 8) * stripe) >> 8;   /* bright at the base, a soft top */
            a = (a * g >> 8) * 230u >> 8;
            cp_add(x, y, 30u + (fr * 130u >> 8), 235u + (fr >> 4), 120u + (fr * 110u >> 8), a);
        }
    }
    for (y = base; y < base + 8; y++)                   /* the glow where the curtains stand */
        for (x = 0; x < 240; x++)
            cp_add(x, y, 40, 230, 140, (FADE[(y - base) * 3] * g >> 8) >> 2);
    cp_sparks(g);
}

/* per-frame values every layer reads: each ribbon column's half height and brightness, SPECTRL's curtains */
static void cp_prepare(const float *mon)
{
    int32_t x, hc = (int32_t)(cp.head >> 8);
    if (cp.mode == 2) {
        float tex = P(mon, 4), dens = P(mon, 3), size = P(mon, 1);
        int32_t cen = (int32_t)(tex * 23.0f), wid = 2 + (int32_t)(size * 8.0f), k, hk[24];
        uint32_t rip = (uint32_t)((dens > 0.5f ? dens - 0.5f : 0.5f - dens) * 2.0f * 255.0f), lv = (uint32_t)(cp.lv * 255.0f);
        for (k = 0; k < 24; k++) {
            int32_t d = k > cen ? k - cen : cen - k, a = GAUSS[d * 63 / (wid * 3 + 1) > 63 ? 63 : d * 63 / (wid * 3 + 1)];
            int32_t sh = (cp_sin((int32_t)(cp.t_ms >> 3) + k * 41) + 16384) >> 7;           /* 0-255 */
            int32_t h = (int32_t)(14u + (uint32_t)(a * (int32_t)(40u + lv * 3u / 4u)) * 130u / 65025u * 1u) + ((cp_sin((int32_t)(cp.t_ms >> 4) + k * 29) * 7) >> 14);
            h = h * (256 - (int32_t)(rip * (uint32_t)sh >> 9)) >> 8;
            if (lv < 8u) h = 4 + h / 10;
            h = h < 2 ? 2 : h > 150 ? 150 : h;
            hk[k] = h;
        }
        for (x = 0; x < 240; x++) {              /* between the curtains' centres (x = 10 + 9k) the heights run straight */
            int32_t u = x - 10, kk = u < 0 ? 0 : u / 9, fr = u < 0 ? 0 : u % 9;
            int32_t h0 = hk[kk > 23 ? 23 : kk], h1 = hk[kk + 1 > 23 ? 23 : kk + 1];
            cp.colh[x] = (uint8_t)(h0 + (h1 - h0) * fr / 9);
        }
    } else {
        for (x = 0; x < 240; x++) {
            int32_t age = hc - x;
            uint32_t rec;
            age = age < 0 ? age + 240 : age;
            rec = 255u - (uint32_t)age * 70u / 240u;
            cp.colh[x] = (uint8_t)(1 + (cp.env[x] * 22u >> 8));
            cp.colg[x] = (uint8_t)(rec * (uint32_t)cp.gain >> 8);
        }
    }
}

/* update 8: the canvas (hb rows, drawn for screen row y) to the LCD, only the rows that are not what was last sent there (rh: the hash of
 * each); runs of changed rows no more than CP_GAP rows apart are one transfer. rh_ok = 0 (the screen was cleared or drawn over): all. */
static void cp_send(uint32_t y, int hb, uint32_t *rh)
{
    int r, a = -1, e = -1;
    uint32_t t0 = plat_ticks24(), keep = cv_h;
    for (r = 0; r <= hb; r++) {
        int changed = 0;
        if (r < hb) {
            const uint16_t *q = cv_px + (uint32_t)r * 240u;
            uint32_t h = 2166136261u, i;
            for (i = 0; i < 240u; i += 2u)
                h = (h ^ ((uint32_t)q[i] | (uint32_t)q[i + 1u] << 16)) * 16777619u;
            changed = !cp.rh_ok || h != rh[r];
            rh[r] = h;
        }
        if (changed) {
            if (a < 0) a = r;
            e = r;
        } else if (a >= 0 && (r - e > CP_GAP || r == hb)) {      /* a run ends: rows a .. e */
            cv_h = (uint32_t)e + 1u;
            cv_blit_from(0, y, (uint32_t)a);
            cp.a_rows += (uint32_t)(e + 1 - a);
            cp.a_xf++;
            a = -1;
        }
    }
    cv_h = keep;
    cp.a_lcd += plat_ticks24() - t0;
}

/* the four read-outs under the picture: name, a small arc, the value, in the slider colours; the one being turned grows */
static void cp_readouts(int pg, const ui_mod_t *m)
{
    uint32_t now = plat_ms();
    int slot = page_slot(pg), off = page_first(m, mn.sub), k, key = pg << 4 | mn.sub, a;
    {
        uint32_t tl = plat_ticks24();
        cv_begin(240, 40, K_BG);
        cp.a_lcd += plat_ticks24() - tl;      /* (it waits for the transfer before: LCD time) */
    }
    cv_rect(0, 0, 240, 1, K_FAINT);
    for (k = 0; k < 4; k++) {
        int pi = off + k, np = m->pages[mn.sub];
        const ui_param_t *p = k < np ? &m->p[pi] : 0;
        int32_t x0 = k * 60, cx = x0 + 13, cy = 27, g, r, w;
        uint16_t kcol = KNOB_C[k];
        float v;
        char t[24], l[16];
        if (!p)
            continue;
        v = rk_get_param(slot, pofs(pg) + pi);
        if (key != cp.pkey)
            cp.prev[k] = v;
        if (v != cp.prev[k]) {
            cp.act[k] = now;
            cp.prev[k] = v;
        }
        g = now - cp.act[k] < 900u && cp.act[k] ? (int32_t)(256u - (now - cp.act[k]) * 256u / 900u) : 0;   /* 256 just turned .. 0 at rest */
        {
            int q = 0;
            while (p->l[q] && q < 15) { l[q] = p->l[q]; q++; }
            l[q] = 0;
            while (text_w(&FONT_T, l) > 56 && q > 1) l[--q] = 0;
        }
        cv_text(x0 + 3, 2, &FONT_T, l, mix565(kcol, K_DIM, 110 + (uint32_t)g * 145u / 256u));
        fmt(p, v, t);
        if (p->t == UT_LIST || p->t == UT_INT) {
            const felucca_font_t *f = g > 40 && text_w(&FONT_B, t) <= 56 ? &FONT_B : &FONT_T;
            cv_text(x0 + 3, f == &FONT_B ? 15 : 19, f, t, g > 40 ? K_FG : mix565(K_FG, K_DIM, 150));
            if (g > 8)
                cv_rect(x0 + 3, 37, 14 * g / 256, 1, kcol);
            continue;
        }
        r = 8 + (int32_t)(3 * g / 256);
        w = 2;
        for (a = 0; a < 28; a++) {            /* the arc, 270 degrees from the bottom left, round over the top */
            int32_t ang = 96 + a * 192 / 27, px = cx + (cp_cos(ang) * r >> 14), py = cy + (cp_sin(ang) * r >> 14);
            float f = fm_clampf(frac(p, v), 0.0f, 1.0f);
            int on = p->t == UT_BIP ? (f >= 0.5f ? a >= 13 && (float)a / 27.0f <= f + 0.02f : a <= 14 && (float)a / 27.0f >= f - 0.02f) : (float)a / 27.0f <= f + 0.001f;
            cv_rect(px - (w - 1) / 2, py - (w - 1) / 2, w, w, on ? mix565(kcol, K_FG, 256u - (uint32_t)g / 3u) : mix565(kcol, K_BG, 50));
        }
        if (g > 40 && text_w(&FONT_B, t) <= 38) {
            cv_text(x0 + 26, 15, &FONT_B, t, K_FG);
        } else {
            cv_text(x0 + 24, 19, &FONT_T, t, mix565(K_FG, K_DIM, 160 + (uint32_t)g / 4u));
        }
    }
    cp.pkey = key;
    cp_send(CP_RY, 40, cp.rh + CP_H);
}

static void cp_text_layers(const float *mon, int pg, const ui_mod_t *m)
{
    int np = npages(pg), sb = mn.sub, i;
    (void)mon; (void)m;
    if (has_mod_page(pg)) { np -= MOD_ROWS - 1; if (sb > m->npages) sb = m->npages; }
    if (cp.by < 26) {
        cv_text(6, 2, &FONT_T, CP_MODE[cp.mode], K_DIM);
        for (i = 0; i < np && np > 1; i++) {                        /* the page: small dots, top right */
            int32_t x = 234 - (np - 1 - i) * 7, r = i == sb ? 2 : 1;
            cv_rect(x - r, 5 - r, 2 * r + 1, 2 * r + 1, i == sb ? K_FG : mix565(K_DIM, K_BG, 140));
        }
        if (cp.freeze) {                                           /* FROZEN: a small snow-flake and the word */
            cv_line(7, 17, 15, 17, 0xBF7Fu);
            cv_line(11, 13, 11, 21, 0xBF7Fu);
            cv_line(8, 14, 14, 20, 0xBF7Fu);
            cv_line(8, 20, 14, 14, 0xBF7Fu);
            cv_text(20, 12, &FONT_T, "FROZEN", 0xBF7Fu);
        }
    }
    if (cp.dbg) {   /* update 8: three lines in the page's big number font, a one-second average, refreshed once a second */
        static const char *const NAME[3] = {"DRAW ", "LCD ", "FPS "};
        uint32_t v[3], k;
        v[0] = cp.sh_draw10; v[1] = cp.sh_lcd10; v[2] = cp.sh_fps;
        for (k = 0; k < 3; k++) {
            char n[12], q[24];
            q[0] = 0;
            cat(q, NAME[k]);
            if (k < 2) {
                int_text((int32_t)(v[k] / 10u), n);
                cat(q, n);
                cat(q, ".");
                int_text((int32_t)(v[k] % 10u), n);
                cat(q, n);
                cat(q, " MS");
            } else {
                int_text((int32_t)v[k], n);
                cat(q, n);
            }
            cv_text(6, CP_H - 52 + (int32_t)k * 16, &FONT_B, q, K_GOLD);
        }
    }
}

/* the whole CLOUDS screen between the status strip and the bottom: the picture, then the read-outs */
static void help(const char *s);
static int cp_screen_do(int pg, const ui_mod_t *m, int force);
static void clouds_screen(int pg, const ui_mod_t *m, int force)
{
    uint32_t ta = plat_ticks24(), now = plat_ms();
    if (!cp_screen_do(pg, m, force))
        return;                               /* (not due yet: no frame) */
    cp.a_draw += plat_ticks24() - ta;
    cp.a_n++;
    if (!cp.t_sec)
        cp.t_sec = now;
    if (now - cp.t_sec >= 1000u) {            /* the last second's averages, for the read-out (and, on the host, stderr) */
        uint32_t n = cp.a_n ? cp.a_n : 1u;        /* ticks (24 a microsecond) -> ms x 10: x 10 / 24000, rounded */
        cp.sh_draw10 = (cp.a_draw * 10u + 12000u * n) / (24000u * n);      /* (a second of ticks, 24 M, x 10 fits 32 bits) */
        cp.sh_lcd10 = (cp.a_lcd * 10u + 12000u * n) / (24000u * n);
        cp.sh_fps = (cp.a_n * 1000u + (now - cp.t_sec) / 2u) / (now - cp.t_sec);
        cp.sh_rows = cp.a_rows * 10u / n; cp.sh_xf = cp.a_xf * 10u / n;
#ifdef PLAT_HOST
        fprintf(stderr, "clouds picture: a second: %u frames, %u.%u rows and %u.%u transfers a frame (before update 8: 230 rows, 6 transfers)\n", cp.a_n, cp.sh_rows / 10u, cp.sh_rows % 10u, cp.sh_xf / 10u, cp.sh_xf % 10u);
#endif
        cp.t_sec = now; cp.a_draw = cp.a_lcd = cp.a_n = cp.a_rows = cp.a_xf = 0;
    }
}
static int cp_screen_do(int pg, const ui_mod_t *m, int force)
{
    uint32_t now = plat_ms(), dt, t0;
    const float *mon = rk_monitor(page_slot(pg));
    int band, pf, quiet;
    float pitch = P(mon, 2);
    if (cp.force) {
        force = 1;
        cp.force = 0;
        cp.rh_ok = 0;                         /* the screen was cleared or drawn over: every row is sent again */
    }
    if (!force && now - cp.draw_ms < CP_MIN_MS)
        return 0;
    dt = cp.t_ms ? now - cp.t_ms : 0u;
    dt = dt > 100u ? 100u : dt;
    cp.t_ms = now;
    cp.draw_ms = now;
    t0 = plat_ticks24();
    cp_step(mon, dt);
    pitch = pitch < -1.0f ? -1.0f : pitch > 1.0f ? 1.0f : pitch;
    pf = (int)((pitch + 1.0f) * 7.5f + 0.5f);
    quiet = cp.lv < 0.012f && !cp.nlive;
    {   /* nothing moves (silent, no motes) and nothing was changed: the picture stays as it is */
        uint32_t sig = (uint32_t)(P(mon, 0) * 255.0f) | (uint32_t)(P(mon, 1) * 255.0f) << 8 | (uint32_t)((P(mon, 2) + 1.0f) * 120.0f) << 16 | (uint32_t)(P(mon, 3) * 255.0f) << 24;
        sig ^= (uint32_t)(P(mon, 4) * 255.0f) * 2654435761u ^ (uint32_t)(P(mon, 16) * 255.0f) * 40503u ^ (uint32_t)((int)(P(mon, 9) + 0.5f) + 4 * cp.freeze + 8 * cp.dbg + 16 * ((int)(P(mon, 11) + 0.5f) & 3)) * 97u;
        if (!force && quiet && sig == cp.sig && !cp.dbg) {
            cp.still = 1;
            cp_readouts(pg, m);
            cp.rh_ok = 1;
            if (mn.learn) help("");
            return 1;
        }
        cp.sig = sig;
        cp.still = 0;
    }
    cp_prepare(mon);
    for (band = 0; band * 40 < CP_H; band++) {
        int32_t y0 = band * 40, hb = CP_H - y0 < 40 ? CP_H - y0 : 40;
        {
            uint32_t tl = plat_ticks24();
            cv_begin(240, (uint32_t)hb, RGB(2, 3, 7));
            cp.a_lcd += plat_ticks24() - tl;
        }
        cv_oy = -y0;
        cp.by = y0;
        cp.bh = hb;
        if (cp.mode == 0)
            cp_granular(mon, pf);
        else if (cp.mode == 1)
            cp_loop(mon, pf);
        else
            cp_spectral();
        cp_text_layers(mon, pg, m);
        cv_oy = 0;
        cp_send((uint32_t)(CP_Y + y0), hb, cp.rh + y0);
    }
    cp_readouts(pg, m);
    cp.rh_ok = 1;                                 /* from now on the rows' hashes say what the screen holds */
    cp.us = (plat_ticks24() - t0) / PLAT_TICKS_PER_US;
    cp.us_max = cp.us > cp.us_max ? cp.us : cp.us_max;
    cp.us_sum += cp.us;
    cp.nd++;
    if (mn.learn)
        help("");
    return 1;
}
