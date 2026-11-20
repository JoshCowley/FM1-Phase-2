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
#define HR_NB 9                               /* update 14: HOME's ribbons: 9 frequency bands x 72 columns of history, one byte each (aliased on the ribbon arrays below) */
#define HR_NC 72
#define HR_ND 48                              /* HOME's grain dabs (aliased on the mote field: HOME has no motes) */
#define SC_CLOUDS 0                           /* update 13: the pictures (cp.scene) */
#define SC_BRAIDS 1
#define SC_RINGS 2
#define SC_FILTER 3
#define SC_TIDES 4
#define SC_PEAKS 5
#define SC_GRIDS 6
#define SC_FUNC 7
#define SC_DELAY 8
#define SC_REVERB 9
#define SC_CHORUS 10
#define SC_HOME 11
#define SC_LFO 12                             /* update 15: the LFO and AMP pages */
#define SC_AMP 13
#define FM_MIN_MS 80u                         /* update 15: the family's pictures at most 12.5 a second (no page sends more rows a second than Clouds did) */
#define CP_MIN_MS 50u                         /* at most 20 pictures a second (update 8; was 30 ms, ~33) */
#define CP_GAP 3                              /* changed rows this close (or closer) go in one transfer */
typedef struct {
    int32_t x, y, vx, vy;                     /* picture pixels x 256; pixels a second x 256 */
    uint16_t age, life;                       /* ms */
    uint8_t hue, sz, gain, flags;             /* HUE step 0-15; 0-2 size; brightness; 1 soft, 2 spark */
} cp_mote_t;
typedef struct {                              /* update 14: a grain dab on HOME (16 bytes) */
    uint16_t age, life, step0;                /* ms; ms; the history step it was born at */
    int16_t due;                              /* ms until it appears (a delay's repeat); 0: alive */
    int8_t xc;                                /* the column it sits in */
    uint8_t bf, g, sz, sm, fl, xj, pad;       /* band x 16, brightness, size 0-2, smear px, flags (1 soft, 2 repeat), pixel in the column */
} hdab_t;
static struct {
    union {                                   /* update 14: the ribbon arrays are the Clouds page's and the other pictures'; HOME's own history shares them */
        struct {
            uint8_t env[240];                 /* the ribbon: the recent output level, a column written as the head passes */
            uint8_t colh[240], colg[240];     /* this frame's half-height and brightness of each column */
        };
        uint8_t tr[720];                      /* update 15: the family's history (vis_fam.c): up to 16 slices of 45 points, on the same bytes */
        uint8_t hist[HR_NB * HR_NC];          /* HOME: the band energies of the last 7 seconds, 0-255, a ring of columns per band */
    };
    union {
        cp_mote_t m[CP_NM];
        hdab_t hd[HR_ND];                     /* HOME: the grain dabs */
    };
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
    int wob, lf, norefl, hsc, scene, cur, cy, H, nobirth, gm, hs;   /* update 13: the picture on screen (SC_*), the one last drawn, the ribbon's centre row, the picture's height, no grain births (HOME without Clouds), granular presence 0-256, hue shift of the motes */
} cp = {.rng = 2463534242u, .pkey = -1, .force = 1, .cy = CP_CY, .H = CP_H, .gm = 256};

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
    if ((uint32_t)x >= cv_w || (uint32_t)y >= cv_h)
        return;
    p = &cv_px[(uint32_t)y * cv_w + (uint32_t)x];
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

/* the smoothed output level (cp.lv) from a raw peak, and the brightness cp.gain from it (update 13: shared by every picture) */
static void cp_level(float raw, uint32_t dt)
{
    raw = raw * 4.0f > 1.0f ? 1.0f : raw * 4.0f;
    raw = raw * (2.0f - raw);                 /* compressed: a quiet chord still moves the picture */
    cp.lv += (raw - cp.lv) * (raw > cp.lv ? (dt > 40u ? 1.0f : (float)dt * 0.025f) : (float)dt * 0.0055f > 1.0f ? 1.0f : (float)dt * 0.0055f);
    cp.gain = 52 + (int32_t)(cp.lv * 2.2f > 1.0f ? 203.0f : cp.lv * 2.2f * 203.0f);
}
/* the motes move on dt ms (a frozen picture's hang), and the dead are cleared; cp.nlive counts the living */
static void cp_motes_advance(uint32_t dt, uint32_t lv8)
{
    int i;
    cp.nlive = 0;
    for (i = 0; i < CP_NM; i++) {
        cp_mote_t *o = &cp.m[i];
        uint32_t step;
        if (!o->life)
            continue;
        step = cp.freeze ? dt >> 2 : dt;
        if (cp.freeze) {
            o->vx -= o->vx * (int32_t)dt >> 8;
            o->vy -= o->vy * (int32_t)dt >> 8;
        }
        if (lv8 < 3u)
            step = dt << 1;
        o->x += o->vx * (int32_t)dt / 1000;
        o->y += o->vy * (int32_t)dt / 1000;
        o->age = (uint16_t)(o->age + step);
        if (o->age >= o->life || o->y < (4 << 8) || o->y > ((int32_t)cp.H << 8))
            o->life = 0;
        else
            cp.nlive++;
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
    cp_level(raw, dt);
    mode = mode < 0 ? 0 : mode > 2 ? 2 : mode;
    if (cp.nobirth)                           /* HOME without Clouds: no grains, the granular field is only fading */
        mode = 0, dens = 0.5f;
    cp.mode = mode;
    cp.freeze = !cp.nobirth && (int)(P(mon, 10) + 0.5f) == 1;
    if (mode == 2)
        qual |= 1;                            /* SPECTRL plays as the mono QUALITY (engine: CL_TRIMMED) */
    bufms = CP_BUF_MS[qual] * (rk_fm1_status(5) ? 3u : 2u) / 2u;   /* (Clouds at 32 kHz holds the same samples 1.5 x as long) */
    lv8 = (uint32_t)(cp.lv * 255.0f);
    pitch = pitch < -1.0f ? -1.0f : pitch > 1.0f ? 1.0f : pitch;
    pf = (int)((pitch + 1.0f) * 0.5f * 15.0f + 0.5f);
    {   /* the new notes: a burst of grains (NOTE TRG and a fresh chord both start grains) */
        uint8_t g = 0;
        for (i = 0; i < 6; i++)
            if (mon[28 + i] >= 1.0f)
                g |= (uint8_t)(1u << i);
        gates = (g & ~gate_was) != 0 && !cp.nobirth;
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
            lo = lvi * (236u + (cp_rnd() & 31u)) >> 8;
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
                y = cp.cy - 18 + (int32_t)(cp_rnd() % 36u);
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
    cp_motes_advance(dt, lv8);
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


/* update 13: what the other pictures (vis_pic.c) and HOME (vis_home.c) supply; every picture goes through this file's layout, sending and timing */
static void pic_step(int sc, const float *mon, uint32_t dt);
static void pic_draw(int sc, const float *mon);
static int pic_idle(int sc);
static const char *pic_sub(int sc, const float *mon);
static void pk_icon_draw(int ic, int32_t x, int32_t y, uint16_t c);
static void fam_reset(void);
static void fm_clouds_step(const float *mon, uint32_t dt);
static const float *home_frame(uint32_t dt);
static void home_draw(const float *mon, int pf);
static void home_text(void);
static void home_reset(void);
static void home_readouts(void);
static uint32_t home_sig(void);

/* one of the four read-outs: name, a small arc, the value, in the knob's colour; g (0-256) how lately it was turned: it grows */
static void cp_ro_cell(int k, const char *l, const char *t, float f, int bip, int list, uint16_t kcol, int32_t g)
{
    int32_t x0 = k * 60, cx = x0 + 13, cy = 27, r, w = 2, a;
    cv_text(x0 + 3, 2, &FONT_T, l, mix565(kcol, K_DIM, 110 + (uint32_t)g * 145u / 256u));
    if (list) {
        const felucca_font_t *fo = g > 40 && text_w(&FONT_B, t) <= 56 ? &FONT_B : &FONT_T;
        cv_text(x0 + 3, fo == &FONT_B ? 15 : 19, fo, t, g > 40 ? K_FG : mix565(K_FG, K_DIM, 150));
        if (g > 8)
            cv_rect(x0 + 3, 37, 14 * g / 256, 1, kcol);
        return;
    }
    r = 8 + (int32_t)(3 * g / 256);
    f = fm_clampf(f, 0.0f, 1.0f);
    for (a = 0; a < 28; a++) {            /* the arc, 270 degrees from the bottom left, round over the top */
        int32_t ang = 96 + a * 192 / 27, px = cx + (cp_cos(ang) * r >> 14), py = cy + (cp_sin(ang) * r >> 14);
        int on = bip ? (f >= 0.5f ? a >= 13 && (float)a / 27.0f <= f + 0.02f : a <= 14 && (float)a / 27.0f >= f - 0.02f) : (float)a / 27.0f <= f + 0.001f;
        cv_rect(px - (w - 1) / 2, py - (w - 1) / 2, w, w, on ? mix565(kcol, K_FG, 256u - (uint32_t)g / 3u) : mix565(kcol, K_BG, 50));
    }
    if (g > 40 && text_w(&FONT_B, t) <= 38)
        cv_text(x0 + 26, 15, &FONT_B, t, K_FG);
    else
        cv_text(x0 + 24, 19, &FONT_T, t, mix565(K_FG, K_DIM, 160 + (uint32_t)g / 4u));
}
static void cp_ro_begin(void)
{
    uint32_t tl = plat_ticks24();
    cv_begin(240, 40, K_BG);
    cp.a_lcd += plat_ticks24() - tl;          /* (it waits for the transfer before: LCD time) */
    cv_rect(0, 0, 240, 1, K_FAINT);
}
/* how lately knob k's value moved, 256 just now .. 0 at rest; key changes (a new page) start from rest */
static int32_t cp_ro_act(int k, float v, int key)
{
    uint32_t now = plat_ms();
    if (key != cp.pkey)
        cp.prev[k] = v;
    if (v != cp.prev[k]) {
        cp.act[k] = now;
        cp.prev[k] = v;
    }
    return now - cp.act[k] < 900u && cp.act[k] ? (int32_t)(256u - (now - cp.act[k]) * 256u / 900u) : 0;
}

static void cp_readouts(int pg, const ui_mod_t *m)
{
    int slot = page_slot(pg), off = page_first(m, mn.sub), k, key = pg << 4 | mn.sub;
    cp_ro_begin();
    for (k = 0; k < 4; k++) {
        int pi = off + k, np = m->pages[mn.sub], q = 0;
        const ui_param_t *p = k < np ? &m->p[pi] : 0;
        float v;
        char t[24], l[16];
        int32_t g;
        if (!p)
            continue;
        v = rk_get_param(slot, pofs(pg) + pi);
        g = cp_ro_act(k, v, key);
        while (p->l[q] && q < 15) { l[q] = p->l[q]; q++; }
        l[q] = 0;
        while (text_w(&FONT_T, l) > 56 && q > 1) l[--q] = 0;
        fmt(p, v, t);
        cp_ro_cell(k, l, t, frac(p, v), p->t == UT_BIP, p->t == UT_LIST || p->t == UT_INT, KNOB_C[k], g);
    }
    cp.pkey = key;
    cp_send(CP_RY, 40, cp.rh + CP_H);
}

static void cp_dbg_text(void)
{
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
            cv_text(6, cp.H - 52 + (int32_t)k * 16, &FONT_B, q, K_GOLD);
        }
    }
}
static int pic_icon_of(int sc)
{
    static const uint8_t IC[14] = {IC_CLOUDS, IC_BRAIDS, IC_RINGS, IC_FILTER, IC_TIDES, IC_PEAKS, IC_GRIDS, IC_FUNC, IC_DELAY, IC_REVERB, IC_CHORUS, IC_HOME, IC_LFO, IC_AMP};
    return IC[sc > 13 ? 0 : sc];
}
static void cp_text_layers(const float *mon, int pg, const ui_mod_t *m)
{
    int np = npages(pg), sb = mn.sub, i;
    if (cp.scene == SC_HOME) {
        home_text();
        return;
    }
    if (has_mod_page(pg)) { np -= MOD_ROWS - 1; if (sb > m->npages) sb = m->npages; }
    if (cp.by < 26) {
        pk_icon_draw(pic_icon_of(cp.scene), 3, 1, page_colour(pg));      /* update 13: the page's icon, alive with its process */
        cv_text(24, 2, &FONT_T, m->name, K_FG);
        cv_text(24, 12, &FONT_T, cp.scene == SC_CLOUDS ? CP_MODE[cp.mode] : pic_sub(cp.scene, mon), K_DIM);
        for (i = 0; i < np && np > 1; i++) {                        /* the page: small dots, top right */
            int32_t x = 234 - (np - 1 - i) * 7, r = i == sb ? 2 : 1;
            cv_rect(x - r, 5 - r, 2 * r + 1, 2 * r + 1, i == sb ? K_FG : mix565(K_DIM, K_BG, 140));
        }
        if (cp.freeze && cp.scene == SC_CLOUDS) {                  /* FROZEN: a small snow-flake and the word */
            cv_line(7, 29, 15, 29, 0xBF7Fu);
            cv_line(11, 25, 11, 33, 0xBF7Fu);
            cv_line(8, 26, 14, 32, 0xBF7Fu);
            cv_line(8, 32, 14, 26, 0xBF7Fu);
            cv_text(20, 24, &FONT_T, "FROZEN", 0xBF7Fu);
        }
    }
    cp_dbg_text();
}

/* which picture a page shows (SC_*), -1: none (update 13: every module page of a loaded module, but its MOD page, and HOME) */
static int pic_scene(int pg)
{
    const ui_mod_t *m;
    if (pg == PG_HOME)
        return SC_HOME;
    if (pg == PG_AMP || pg == PG_LFO) {
        m = page_mod(pg);
        if (mn.pick || is_mod_sub(pg) || (pg == PG_LFO && mn.sub >= m->npages))
            return -1;
        return pg == PG_AMP ? SC_AMP : SC_LFO;
    }
    if (pg == PG_FLT)                         /* the filter's own page (update 26): its picture, on GLO's parameters */
        return mn.pick || is_mod_sub(pg) ? -1 : SC_FILTER;
    if (pg < PG_SRC || pg > PG_FX2)
        return -1;
    m = page_mod(pg);
    if (!m || mn.pick || is_mod_sub(pg))
        return -1;
    switch (m->id) {
    case 2: return SC_BRAIDS;
    case 10: return SC_RINGS;
    case 12: return SC_FILTER;
    case 20: return SC_PEAKS;
    case 21: return SC_TIDES;
    case 23: return SC_GRIDS;
    case 24: return SC_FUNC;
    case 30: return SC_CLOUDS;
    case 31: return SC_DELAY;
    case 32: return SC_REVERB;
    case 33: return SC_CHORUS;
    default: return -1;
    }
}
static int clouds_page(int pg) { return pic_scene(pg) == SC_CLOUDS; }

/* the whole picture screen between the status strip and the bottom: the picture, then the read-outs */
static void help(const char *s);
static int cp_screen_do(int pg, const ui_mod_t *m, int force);
static void clouds_screen(int pg, const ui_mod_t *m, int force)
{
    uint32_t ta = plat_ticks24(), now = plat_ms();
    cp.scene = pic_scene(pg);
    if (cp.scene < 0)
        return;
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
        fprintf(stderr, "picture %d: a second: %u frames, %u.%u rows and %u.%u transfers a frame, draw %u us a frame (host)\n", cp.scene, cp.a_n, cp.sh_rows / 10u, cp.sh_rows % 10u, cp.sh_xf / 10u, cp.sh_xf % 10u, cp.a_draw / (24u * n));
#endif
        cp.t_sec = now; cp.a_draw = cp.a_lcd = cp.a_n = cp.a_rows = cp.a_xf = 0;
    }
}
static int cp_screen_do(int pg, const ui_mod_t *m, int force)
{
    uint32_t now = plat_ms(), dt, t0;
    const float *mon;
    int band, pf = 7, quiet, sc = cp.scene;
    if (cp.cur != sc) {                       /* a different picture: nothing of the last one's carries over */
        int i;
        for (i = 0; i < CP_NM; i++) cp.m[i].life = 0;
        for (i = 0; i < 720; i++) cp.tr[i] = 0;
        fam_reset();
        cp.lv = 0.0f; cp.nlive = 0; cp.head = 0; cp.ph = 0; cp.cur = sc;
        if (sc == SC_HOME) home_reset();      /* (update 14: HOME's history and dabs start empty) */
        force = 1;
        cp.rh_ok = 0;
    }
    if (cp.force) {
        force = 1;
        cp.force = 0;
        cp.rh_ok = 0;                         /* the screen was cleared or drawn over: every row is sent again */
    }
    if (!force && now - cp.draw_ms < (sc == SC_HOME ? CP_MIN_MS : FM_MIN_MS))
        return 0;
    dt = cp.t_ms ? now - cp.t_ms : 0u;
    dt = dt > 100u ? 100u : dt;
    cp.t_ms = now;
    cp.draw_ms = now;
    t0 = plat_ticks24();
    cp.wob = 3; cp.cy = CP_CY; cp.H = CP_H; cp.norefl = 0; cp.lf = 0; cp.hsc = 256; cp.hs = 0; cp.gm = 256; cp.nobirth = 0;
    if (sc == SC_HOME) {
        cp.H = 174;
        mon = home_frame(dt);                 /* (it stepped the granular field, and the morph) */
    } else {
        mon = rk_monitor(page_slot(pg));
        if (sc == SC_CLOUDS) {
            cp_step(mon, dt);
            fm_clouds_step(mon, dt);
        } else
            pic_step(sc, mon, dt);
    }
    if (sc == SC_CLOUDS) {
        float pitch = P(mon, 2);
        pitch = pitch < -1.0f ? -1.0f : pitch > 1.0f ? 1.0f : pitch;
        pf = (int)((pitch + 1.0f) * 7.5f + 0.5f);
    }
    quiet = cp.lv < 0.012f && !cp.nlive && (pic_idle(sc) || sc == SC_HOME);
    {   /* nothing moves (silent, no motes) and nothing was changed: the picture stays as it is */
        uint32_t sig = 0;
        int i;
        if (sc == SC_HOME)
            sig = home_sig();
        else
            for (i = 0; i < 16; i++)
                sig = sig * 31u + (uint32_t)(P(mon, i) * 255.0f + 300.0f);
        if (sc == SC_CLOUDS)
            sig ^= (uint32_t)((int)(P(mon, 9) + 0.5f) + 4 * cp.freeze + 16 * ((int)(P(mon, 11) + 0.5f) & 3)) * 97u;
        sig ^= (uint32_t)(8 * cp.dbg) * 97u + (uint32_t)sc * 7919u;
        if (!force && quiet && sig == cp.sig && !cp.dbg) {
            cp.still = 1;
            if (sc == SC_HOME) home_readouts(); else cp_readouts(pg, m);
            cp.rh_ok = 1;
            if (mn.learn) help("");
            return 1;
        }
        cp.sig = sig;
        cp.still = 0;
    }
    for (band = 0; band * 40 < cp.H; band++) {
        int32_t y0 = band * 40, hb = cp.H - y0 < 40 ? cp.H - y0 : 40;
        {
            uint32_t tl = plat_ticks24();
            cv_begin(240, (uint32_t)hb, RGB(2, 3, 7));
            cp.a_lcd += plat_ticks24() - tl;
        }
        cv_oy = -y0;
        cp.by = y0;
        cp.bh = hb;
        if (sc == SC_HOME)
            home_draw(mon, pf);
        else
            pic_draw(sc, mon);
        cp_text_layers(mon, pg, m);
        cv_oy = 0;
        cp_send((uint32_t)(CP_Y + y0), hb, cp.rh + y0);
    }
    cp.by = 0;
    if (sc == SC_HOME) home_readouts(); else cp_readouts(pg, m);
    cp.rh_ok = 1;                                 /* from now on the rows' hashes say what the screen holds */
    cp.us = (plat_ticks24() - t0) / PLAT_TICKS_PER_US;
    cp.us_max = cp.us > cp.us_max ? cp.us : cp.us_max;
    cp.us_sum += cp.us;
    cp.nd++;
    if (mn.learn)
        help("");
    return 1;
}
#include "vis_pic.c"
