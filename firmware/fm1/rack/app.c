/* SPDX-License-Identifier: MPL-2.0
 * Modelled on fm1-base's template/app.c (Copyright (C) 2026 Charles Vestal, MPL-2.0), whose helpers it adapts. */
/* FM1 Rack (Lite) firmware: the screen and the controls (part of fm1-base's unity build, after the platform:
 * gfx.c and lib/touch.h are in scope). The sound is rack_core.c's; how keys and MIDI become notes (scales, chords, the
 * arpeggiator, tunings, MIDI in and out) is play.c's; the pages are menus.c's (editing), save.c's (SAVE, the library).
 *
 * On the home page:
 *   PRESET        the next / previous preset (loads when you stop turning)
 *   ALGORITHM     the next / previous category
 *   SELECT        voices, 1-6 (poly presets)
 *   K1-K4         macros 1-4
 *   OCT- / OCT+   octave          HOME   the rack          SAVE   name and store what is playing
 *   EDIT ENV LFO FX ARP SEL GLO   the pages (menus.c) */
#include "fastmath.h"
#define RC_IN_UNITY
#include "rack_core.c"

#define SET_MAGIC 0x524B4C41u                 /* "RKLA": the settings record; "RKL1" (stage 2) keeps its first three */
#define SET_MAGIC9 0x524B4C39u                /* "RKL9" (updates 55-62): ARP 40 values (no GATE PATTERN) */
#define SET_MAGIC8 0x524B4C38u                /* "RKL8" (updates 48-54): GLOBAL 23 values (no DISPLAY) */
#define SET_MAGIC7 0x524B4C37u                /* "RKL7" (updates 43-47): ARP 21 values (no RHYTHM) */
#define SET_MAGIC6 0x524B4C36u                /* "RKL6" (updates 40-42): ARP 20 values (no HUMAN) */
#define SET_MAGIC5 0x524B4C35u                /* "RKL5" (updates 38-39): GLOBAL 19 values (no CLICK) */
#define SET_MAGIC4 0x524B4C34u                /* "RKL4" (updates 36-37): the same; CHD FM-1 turned back on (update 38) */
#define SET_MAGIC3 0x524B4C33u                /* "RKL3" (updates 34-35): ARP 18 values, the layer switches in their old order */
#define SET_MAGIC1 0x524B4C31u
#define SET_MAGIC2 0x524B4C32u                /* "RKL2": ARP's 8 values only (before STYLE): all but the new ones kept */
typedef struct {
    uint32_t magic;
    uint8_t preset;
    int8_t octave;
    uint8_t voices;                           /* 0: the preset's own (GLOBAL > VOICES AUTO) */
    uint8_t rsv;
    float glo[24], sel[8], arp[53];           /* the GLOBAL, SEL and ARP pages (play.c G_*, C_*, A_*); GLOBAL 19-22: CLICK,
                                                 23: DISPLAY (update 55) */
} settings_t;
static settings_t set;
/* RKL8's record (updates 48-54), one GLOBAL value fewer: the older records are brought up to it as before, in it,
 * then into the new one (app_init) */
typedef struct {
    uint32_t magic;
    uint8_t preset;
    int8_t octave;
    uint8_t voices;
    uint8_t rsv;
    float glo[23], sel[8], arp[40];
} settings8_t;
#define SZ9 ((int)sizeof set - 13 * (int)sizeof(float))   /* RKL9's record: 13 ARP values fewer (the GATE PATTERN's) */
/* a moment as a button's "went down at" (0 means "not down"): now, or 1 at the clock's very start. Until update 38 it
 * was plat_ms() | 1, a millisecond in the future half the time: a hold test in the same frame then saw ~49 days held,
 * took the tap for a hold, and the tap did nothing (EDIT, SEL, ARP, SEQ, HOME, FX: "needs several tries") */
static uint32_t ms_stamp(void) { uint32_t t = plat_ms(); return t ? t : 1u; }
/* the platform's own screens (boot, crash, update...) with the app's faces: SYS_S in FONT_XS, the rest in FONT_B */
static const felucca_font_t *app_sys_face(int face) { return face ? &FONT_B : &FONT_XS; }
static int32_t app_sys_h(int face) { return app_sys_face(face)->h; }
static int32_t app_sys_w(int face, const char *s) { return text_w(app_sys_face(face), s); }
static void app_sys_text(int32_t x, int32_t y, int face, const char *s, uint16_t c) { cv_text(x, y, app_sys_face(face), s, c); }
static uint32_t in_btn;                       /* the buttons held this frame (the sequencer's SEL + K1 lasso) */
#include "ui_defs.h"
#include "play.c"
#include "seq.c"
#include "drumrun.c"                         /* the drum sequencer (drum track, step 2) */

static const char *module_name(int id)
{
    switch (id) {
    case 1: return "PLAITS";
    case 2: return "BRAIDS";
    case 10: return "RINGS";
    case 12: return "FILTER";
    case 20: return "PEAKS";
    case 21: return "TIDES";
    case 23: return "GRIDS";
    case 24: return "FUNC";
    case 30: return "CLOUDS";
    case 31: return "DELAY";
    case 32: return "REVERB";
    case 33: return "CHORUS";
    default: return 0;
    }
}
static const char *const MODE_NAME[] = {"POLY", "MONO", "LEGATO", "UNI 2", "UNI 4"};

/* ------------------------------------------------------------------ sound --- */
static float gain_now;                        /* glides to MASTER: no step at a block's edge */
static volatile uint32_t midi_count;

static void midi_in(void)                     /* audio interrupt: the packets that came in, played at once */
{
    uint32_t pkt;
    int port, want = GI(G_MIDICH);
    while (plat_midi_in(&pkt, &port)) {
        uint32_t st = (pkt >> 8) & 0xFFu, d1 = (pkt >> 16) & 0x7Fu, d2 = pkt >> 24;
        if (st == 0xF8u) {                    /* the clock: CLOCK MIDI steps the arp */
            pl_clock_tick();
            continue;
        }
        if (st == 0xFAu) {
            pl_clock_start();
            if (GI(G_CLOCK) == 1) sq.req = 1;
        }
        if (st == 0xFCu && GI(G_CLOCK) == 1)
            sq.req = 2;
        if (st >= 0xF0u)
            continue;
        if (((st & 0xF0u) == 0x90u || (st & 0xF0u) == 0x80u) && dr_midi_in((int)(st & 15u), (int)d1, (int)d2, (st & 0xF0u) == 0x90u && d2))
            continue;                         /* a drum lane's note (the drum track: its channel and note, IN on): it plays the lane, not the synth */
        if (want && (int)(st & 15u) + 1 != want)   /* GLOBAL > MIDI CH */
            continue;
        midi_count++;
        if ((st & 0xF0u) == 0x90u && d2)
            pl_key_on((int)d1, (int)d1, (float)d2 / 127.0f);
        else if ((st & 0xF0u) == 0x80u || (st & 0xF0u) == 0x90u)
            pl_key_off((int)d1);
        else if ((st & 0xF0u) == 0xE0u)       /* pitch bend: +- GLOBAL > BEND semitones */
            rc_do(RCE_BEND, 0, ((float)((int32_t)(d2 << 7 | d1) - 8192) / 8192.0f) * (float)GI(G_BEND), 0.0f);
        else if ((st & 0xF0u) == 0xC0u && GI(G_PROG))
            pl_prog_req = (int)d1 + 1;
        else if ((st & 0xF0u) == 0xB0u && d1 == 64u)
            pl_sustain(d2 >= 64u);
        else if ((st & 0xF0u) == 0xB0u && (d1 == 123u || d1 == 120u))
            rc_do(RCE_ALL_OFF, 0, 0.0f, 0.0f);
        else if ((st & 0xF0u) == 0xB0u && d1 >= 20u && d1 <= 23u)   /* CC 20-23: the macros */
            rc_do(RCE_MACRO, (int)d1 - 20, (float)d2 / 127.0f, 0.0f);
        else if ((st & 0xF0u) == 0xB0u && d1 == SWEEP_CC)   /* CC 74 (brightness): the filter's SWEEP, 64 the preset's own cutoff */
            rc_do(RCE_SWEEP, 0, d2 <= 64u ? ((float)d2 - 64.0f) / 64.0f : ((float)d2 - 64.0f) / 63.0f, 0.0f);
    }
}

/* the buttons' presses, counted by the render (every 5.8 ms; update 38): the main loop reads the buttons once a
 * screen frame, and a quick tap could fall between two frames and be lost. input() takes each press from here, once */
static volatile uint8_t btn_downs[32];
static volatile uint32_t btn_isr_at;          /* when the render last looked (no audio: the frames' own edges) */
static volatile int btn_isr_on;               /* the render has looked at least once */
static uint32_t btn_rprev;
static void btn_count(void)
{
    uint32_t b = plat_buttons(), d = b & ~btn_rprev, i;
    btn_rprev = b;
    for (i = 0; d; i++, d >>= 1)
        if (d & 1u)
            btn_downs[i]++;
    btn_isr_at = plat_ms();
    btn_isr_on = 1;
}

void app_render(int32_t *lr, uint32_t n, uint32_t master_q12)
{
    float gain = (float)master_q12 / 4096.0f * 8300000.0f;
    uint32_t i;
    btn_count();
    if (!rc_audio.pause_req)
        midi_in();
    sq_block(n);                              /* the sequencer's and MSEQ's (seq.c; update 59: before pl_block moves
                                                 pl_now on, so its window is this block, not the next) */
    dr_audio(n);                              /* the drums, on the sequencer's PLAY (drumrun.c) */
    pl_block(n);                              /* the arp's steps and the strums falling due */
    rc_render(lr, n, 8300000.0f);
    click_render(lr, n, 8300000.0f);          /* the click track and the count-in (seq.c, update 40) */
    for (i = 0; i < n; i++) {                 /* MASTER, gliding over about 5 ms */
        float g;
        gain_now += (gain - gain_now) * 0.004f;
        g = gain_now / 8300000.0f;
        lr[2u * i] = (int32_t)((float)lr[2u * i] * g);
        lr[2u * i + 1u] = (int32_t)((float)lr[2u * i + 1u] * g);
    }
}

void app_overload(void) { rc_overload(); }

void app_panic(void)
{
    rc_pause();
}

/* ------------------------------------------------------------------ presets --- */
/* poly presets start with at most this many voices, unless SAVE remembered another number. The second benchmark
 * (3 Nov 2026, the lighter engine) measured VA VCF at about 6 % plus 15 % per voice on one core (4 voices 66 %,
 * 6 voices 95 %) and about 7 % per voice on two (6 voices 52 %): so the preset's own voices (up to 6) on two cores,
 * 4 on one. The shedding takes off what doesn't fit (PD STRINGS: 104 % with 6 on two cores) */
/* update 8: with Clouds in a slot a poly preset starts with 3 voices on one core (each Braids voice is about 5 points of the core; the
 * 14 Nov run had Clouds with the reverb at 95 % with 4); SELECT asks for more. RACK_CLOUDS_VOICES (engine/rack.cc) is 3 to match. */
/* update 9 (RACK_FX_CORE2): on two cores a patch with Clouds runs Clouds on the second core and every voice on the first (rk_fx_core2_active():
 * the engine decided so at the layout), so it starts with 4 voices (about 6 points of the core each, plus the delay or reverb: about 51 % for
 * 4 voices with both, the overseer's arithmetic from update 8's figures; not measured); SELECT asks for up to 6. One core: still 3. */
static int start_voices(void)
{
    if (plat_cpu2_on())
        return rk_fx_core2_active() ? 4 : 6;
    return rc_patch.mod[RS_FX1] == 30 || rc_patch.mod[RS_FX2] == 30 ? 3 : 4;
}
#define START_VOICES start_voices()
static void load_preset(int k)
{
    rc_fade_pause();
    rc_load(k);
    if (set.voices && rc_patch.mode == RVM_POLY) {
        rk_set_voices(set.voices);
        rc_patch.voices_now = set.voices;
    } else if (rc_patch.mode == RVM_POLY && rc_patch.voices_now > START_VOICES) {
        rk_set_voices(START_VOICES);          /* (SELECT asks for more) */
        rc_patch.voices_now = START_VOICES;
    }
    glo_apply();                              /* what GLOBAL keeps across presets: tempo, EQ, tuning */
    rc_resume();
    rc_push(PLE_RESET, 0, 0.0f, 0.0f);        /* the notes counted sounding went with the old preset */
}


/* ------------------------------------------------------------------ UI --- */
static struct {
    int shown;                                /* the preset on the screen (turning PRESET: not loaded yet) */
    uint32_t turned_at;                       /* when PRESET or ALGORITHM last turned */
    uint32_t keys, btn;
    char msg[2][24];
    uint32_t msg_until;
    int redraw, clear;
    uint32_t zkeys;                           /* the keys held as SEL's zone keys */
    uint32_t pkeys;                           /* the keys sounding (not the layer's steps) */
    uint32_t jkeys;                           /* the keys held as JAM's moves (update 28) */
    uint32_t jowed;                           /* ...a move's key-up the queue refused (sent again) */
    uint8_t jam_owed;                         /* bit 0: JAM's "all moves end" the queue refused */
} ui;
static int cur_entry;                         /* the preset playing: < NPRESETS a factory one, else user slot - NPRESETS */

static void cat(char *d, const char *s)
{
    while (*d)
        d++;
    while (*s)
        *d++ = *s++;
    *d = 0;
}

static void int_text(int32_t v, char *b)
{
    char t[12];
    int n = 0;
    uint32_t u = (uint32_t)(v < 0 ? -v : v);
    do
        t[n++] = (char)('0' + u % 10u);
    while ((u /= 10u) && n < 11);
    if (v < 0)
        *b++ = '-';
    while (n)
        *b++ = t[--n];
    *b = 0;
}

void app_say(const char *a, const char *b)
{
    uint32_t i;
    if (plat_ms() < ui.msg_until)
        ui.clear = 1;                         /* a toast replacing one still up: a narrower one would leave its edges */
    for (i = 0; i < 23u && a && a[i]; i++)
        ui.msg[0][i] = a[i];
    ui.msg[0][i] = 0;
    for (i = 0; i < 23u && b && b[i]; i++)
        ui.msg[1][i] = b[i];
    ui.msg[1][i] = 0;
    ui.msg_until = plat_ms() + 1300u;
    ui.redraw = 1;
    if (ann_on()) {
        char t[48] = "";
        cat(t, ui.msg[0]);
        cat(t, " ");
        cat(t, ui.msg[1]);
        ann_say(t, 0, ANN_NOW);
    }
}

static void say_preset(int k)
{
    if (ann_on())
        ann_say(PRESETS[k].name, 0, ANN_NOW);
}

static int same(const char *a, const char *b)   /* (no C library: strcmp) */
{
    while (*a && *a == *b)
        a++, b++;
    return *a == *b;
}

static int cat_step(int from, int d)         /* the first preset of the next / previous category */
{
    int k = from;
    const char *c = PRESETS[from].cat;
    if (d > 0) {
        while (k < NPRESETS && same(PRESETS[k].cat, c))
            k++;
        return k >= NPRESETS ? 0 : k;
    }
    while (k > 0 && same(PRESETS[k].cat, c))
        k--;
    c = PRESETS[k].cat;                       /* the previous category: back to its first */
    while (k > 0 && same(PRESETS[k - 1].cat, c))
        k--;
    return k;
}

static void octave_step(int d)
{
    int o = set.octave + d;
    char t[4];
    if (o < -3 || o > 3)
        return;
    set.octave = (int8_t)o;
    t[0] = o < 0 ? '-' : '+';
    t[1] = (char)('0' + (o < 0 ? -o : o));
    t[2] = 0;
    app_say("OCTAVE", t);
}


/* ------------------------------------------------------------------ the screen --- */
/* The page's look (web/src/2-screen.js): its colours, its 12 x 12 module icons, a header bar with the category in a
 * coloured tab, a dark well with a dot grid (here: the output's scope, with a soft halo), the chain as icon tiles, the
 * macros as cells with rounded bars, and a footer with CPU and RAM bars, the voices' lights and the output meter.
 * Drawn in bands (the canvas is 240 x 40 pixels: the pool goes to the engine's arena). */
#include "icons.h"
/* the cells' place (after stage 4: the modulation strip gone, the cells taller) */
#define CELL_H 68
#define CELL_Y(i) ((i) < 2 ? 80 : 150)
#define HELP_Y 224
#define HOLD_MS 600u                          /* a button held this long is a hold: its second function (never a tap) */
#define EDIT_DOUBLE_MS 300u                   /* two LFO taps this close: the MATRIX */
#define LEARN_MS 500u                         /* SEL + EDIT held together this long: LEARN on or off */
#include "theme.h"                           /* the colours (update 51) */

/* a over b, t of the way (0-256), in RGB565 */
static uint16_t mix565(uint16_t a, uint16_t b, uint32_t t)
{
    uint32_t r = ((a >> 11) * t + (b >> 11) * (256u - t)) >> 8;
    uint32_t g = (((a >> 5) & 63u) * t + ((b >> 5) & 63u) * (256u - t)) >> 8;
    uint32_t l = ((a & 31u) * t + (b & 31u) * (256u - t)) >> 8;
    return (uint16_t)(r << 11 | g << 5 | l);
}

/* a rectangle with corners rounded by r (0-4) */
static void rrect(int32_t x, int32_t y, int32_t w, int32_t h, uint16_t c, int32_t r)
{
    static const uint8_t INSET[5][4] = {{0}, {1}, {1, 0}, {2, 1, 0}, {2, 1, 0, 0}};
    int32_t i;
    if (r > 4)
        r = 4;
    if (r * 2 > h)
        r = h / 2;
    cv_rect(x, y + r, w, h - 2 * r, c);
    for (i = 0; i < r; i++) {
        int32_t in = INSET[r][i];
        cv_rect(x + in, y + i, w - 2 * in, 1, c);
        cv_rect(x + in, y + h - 1 - i, w - 2 * in, 1, c);
    }
}

/* the page's bar(): a faint track, the value over it, rounded */
static void vbar(int32_t x, int32_t y, int32_t w, int32_t h, float v, uint16_t c)
{
    int32_t f;
    v = v < 0.0f ? 0.0f : v > 1.0f ? 1.0f : v;
    f = (int32_t)((float)w * v + 0.5f);
    rrect(x, y, w, h, K_FAINT, h / 2);
    if (f < h)
        f = h;
    rrect(x, y, f, h, c, h / 2);
}

/* a slider (after stage 4): a track, the value's fill and a thumb. With a modulation colour mc, a halo in that colour:
 * the range the value has swept lately (lo-hi) glows round the track, the live value fills it, the set value stays
 * as a tick, and the thumb glows; e (0-1, how fast the modulation moves) brightens the glow, so it pulses with it */
/* The sliders and the sound (update 50; update 51, after Josh: "a little over the top: subtle colour hues, halos and
 * small movements"). While the FM-1 plays, a slider's filled part warms towards ivory as the sound gets louder, a
 * faint halo breathes round it with the level and brightens for a moment on each new hit, and here and there along
 * it, where the sound itself swings hardest, a pixel lifts or drops: a ripple, not a trace. Each knob reads the
 * sound at its own zoom, so the four ripple differently. Silent: a plain line. The thumb (the setting) never moves. */
static float wv_lvl, wv_slow, wv_flash;       /* the level (fast up, slow down), a slower one, the hit's flash */
static uint32_t wv_start;                     /* where the ripple starts reading */
static int wv_idx;                            /* the knob being drawn: its zoom */
static void wv_tick(void)
{
    static uint32_t last;
    uint32_t end = rc_scope_w, k;
    float r = 0.0f;
    if (end == last)
        return;
    last = end;
    for (k = 0; k < 256u; k++) {
        float x = (float)rc_scope[(end - 256u + k) & (RC_SCOPE_N - 1u)] * (1.0f / 16384.0f);
        r += x * x;
    }
    r = fm_sqrtf(r / 256.0f);
    wv_lvl += (r - wv_lvl) * (r > wv_lvl ? 0.35f : 0.06f);   /* (slower than update 50's: it breathes, it doesn't jump) */
    wv_slow += (r - wv_slow) * 0.04f;
    if (r > 0.02f && r > wv_slow * 1.6f)
        wv_flash = 1.0f;
    wv_flash *= 0.82f;
    wv_start = end - 400u;
}
static int wv_on(void) { return wv_lvl > 0.008f; }
static void wv_fill(int32_t x, int32_t y, int32_t len, uint16_t c)
{
    static const uint8_t ZOOM[4] = {8, 5, 3, 2};   /* frames a column, per knob */
    int32_t k;
    float g;
    uint16_t col, halo;
    if (len <= 0)
        return;
    if (!wv_on()) {
        cv_rect(x, y + 1, len, 2, c);
        return;
    }
    g = wv_lvl * 6.0f;
    g = g > 1.0f ? 1.0f : g;
    col = mix565(K_FG, c, (uint32_t)(g * 60.0f + wv_flash * 70.0f));                /* warmer, lighter: at most a third */
    halo = mix565(col, K_BG, (uint32_t)(18.0f + g * 34.0f + wv_flash * 50.0f));     /* a faint glow: a fifth at most */
    cv_rect(x, y, len, 1, halo);
    cv_rect(x, y + 3, len, 1, halo);
    for (k = 0; k < len; k++) {
        float v = (float)rc_scope[(wv_start + (uint32_t)(k * ZOOM[wv_idx & 3])) & (RC_SCOPE_N - 1u)] * (1.0f / 16384.0f) * g;
        int32_t dy = v > 0.55f ? -1 : v < -0.55f ? 1 : 0;   /* a pixel up or down, only where it swings hardest */
        cv_rect(x + k, y + 1 + dy, 1, 2, col);
    }
}
/* an amount's slider in the style chosen (theme.h FM1_SLIDER): 1 a 2 px line, its fill, a thumb; 2 ten segments;
 * 3 a 4 px bar and no thumb; 4 a hairline and a dot. The fill takes the sound's tint as wv_fill does */
static uint16_t wv_col(uint16_t c)
{
    float g = wv_lvl * 6.0f;
    if (!wv_on()) return c;
    g = g > 1.0f ? 1.0f : g;
    return mix565(K_FG, c, (uint32_t)(g * 60.0f + wv_flash * 70.0f));
}
/* the hairline style's dot (FM1_SLIDER 4, update 53: Josh's pick): the knob's colour, warming with the sound; a faint
 * halo that breathes with the level and brightens on a hit; now and then a pixel's bob where the sound swings */
static void sl_dot(int32_t tx, int32_t y, uint16_t c)
{
    static const uint8_t ZOOM[4] = {8, 5, 3, 2};
    int32_t dy = 0;
    uint16_t col = wv_col(c);
    if (pal_day) {                            /* DAYLIGHT: a bigger, solid dot, still */
        rrect(tx - 4, y - 3, 9, 9, c, 4);
        return;
    }
    if (wv_on()) {
        float g = wv_lvl * 6.0f, v;
        g = g > 1.0f ? 1.0f : g;
        rrect(tx - 5, y - 4, 11, 11, mix565(col, K_BG, (uint32_t)(16.0f + g * 30.0f + wv_flash * 50.0f)), 5);
        v = (float)rc_scope[(wv_start + (uint32_t)(tx * ZOOM[wv_idx & 3])) & (RC_SCOPE_N - 1u)] * (1.0f / 16384.0f) * g;
        dy = v > 0.6f ? -1 : v < -0.6f ? 1 : 0;
    }
    rrect(tx - 3, y - 2 + dy, 7, 7, col, 3);
}
static void sl_line(int32_t x, int32_t y, int32_t w, float f, uint16_t c)
{
    int32_t tx;
    f = f < 0.0f ? 0.0f : f > 1.0f ? 1.0f : f;
    tx = x + (int32_t)(f * (float)w);
#if FM1_SLIDER == 2
    {
        int32_t k, sw = (w - 9) / 10, lit = (int32_t)(f * 10.0f + 0.5f);
        uint16_t col = wv_col(c);
        for (k = 0; k < 10; k++)
            cv_rect(x + k * (sw + 1), y, sw, 4, k < lit ? col : K_FAINT);
    }
#elif FM1_SLIDER == 3
    rrect(x, y, w, 4, K_FAINT, 2);
    if (tx > x + 1) rrect(x, y, tx - x, 4, wv_col(c), 2);
#elif FM1_SLIDER == 4
    if (pal_day) {                            /* DAYLIGHT: a solid bar, no thumb to look for */
        rrect(x, y - 1, w, 5, K_FAINT, 2);
        if (tx > x + 1) rrect(x, y - 1, tx - x, 5, c, 2);
        return;
    }
    cv_rect(x, y + 1, w, 1, K_FAINT);
    if (tx > x) cv_rect(x, y + 1, tx - x, 1, mix565(wv_col(c), K_BG, 140));
    sl_dot(tx, y, c);
#else
    cv_rect(x, y + 1, w, 2, K_FAINT);
    if (tx > x) wv_fill(x, y, tx - x, c);
    rrect(tx - 1, y - 3, 3, 10, K_FG, 1);
#endif
}
static void slider(int32_t x, int32_t y, int32_t w, float f, uint16_t c, uint16_t mc, float lo, float hi, float live, float e, uint16_t bg)
{
    int32_t tx;
    f = f < 0.0f ? 0.0f : f > 1.0f ? 1.0f : f;
    live = live < 0.0f ? 0.0f : live > 1.0f ? 1.0f : live;
    if (mc) {                                 /* moved by the matrix: the range it sweeps as a soft band (update 51:
                                                 one band, a quarter as bright), its live value the thumb */
        uint32_t k = 60u + (uint32_t)((e < 0.0f ? 0.0f : e > 1.0f ? 1.0f : e) * 90.0f);
        int32_t a = x + (int32_t)((lo < 0.0f ? 0.0f : lo) * (float)w), b = x + (int32_t)((hi > 1.0f ? 1.0f : hi) * (float)w);
        if (b > a)
            rrect(a - 2, y - 2, b - a + 4, 8, mix565(mc, bg, k / 3u), 3);
        tx = x + (int32_t)(live * (float)w);
#if FM1_SLIDER == 4                           /* the hairline style: the range, a tick where it's set, the live dot */
        cv_rect(x, y + (pal_day ? 0 : 1), w, pal_day ? 3 : 1, K_FAINT);
        cv_rect(x + (int32_t)(f * (float)w), y - 1, pal_day ? 2 : 1, 5, pal_day ? c : mix565(c, bg, 200));
        sl_dot(tx, y, mc);
#else
        cv_rect(x, y + 1, w, 2, K_FAINT);
        if (tx > x) wv_fill(x, y, tx - x, mc);
        cv_rect(x + (int32_t)(f * (float)w), y - 2, 1, 8, mix565(c, bg, 170));   /* where it's set */
        rrect(tx - 1, y - 3, 3, 10, K_FG, 1);
#endif
    } else {
        sl_line(x, y, w, f, c);
        (void)tx;
    }
}

static int pk_icon(int ic, int32_t x, int32_t y, uint16_t c);   /* (update 13: vis_pic.c's animated icons) */
static void icon(int ic, int32_t x, int32_t y, uint16_t c)
{
    int32_t r, b;
    if (pk_icon(ic, x, y, c))
        return;
    for (r = 0; r < 12; r++)
        for (b = 0; b < 12; b++)
            if (ICON[ic][r] >> (11 - b) & 1u)
                cv_pset(x + b, y + r, c);
}

static int module_icon(int id)
{
    switch (id) {
    case 1: return IC_PLAITS;
    case 2: return IC_BRAIDS;
    case 10: return IC_RINGS;
    case 12: return IC_FILTER;
    case 20: return IC_PEAKS;
    case 21: return IC_TIDES;
    case 23: return IC_GRIDS;
    case 24: return IC_FUNC;
    case 30: return IC_CLOUDS;
    case 31: return IC_DELAY;
    case 32: return IC_REVERB;
    case 33: return IC_CHORUS;
    default: return IC_EMPTY;
    }
}

/* the scope: the output, a rising zero crossing near the start so a steady note stands still, every second frame
 * (about 10 ms across), drawn twice as the page's glow(): a soft halo, then the line */
static int scope_keep;                        /* 1: draw the trace taken last time (the next band of the same well) */
static void scope(int32_t w, int32_t h, uint16_t c)
{
    static int16_t y[228];
    uint32_t end = rc_scope_w, start = end - RC_SCOPE_N + 8u, k, i;
    int32_t mid = h / 2, x, prev;
    uint16_t halo = mix565(c, K_WELL, 46);
    for (k = 0; k < 400u; k++) {              /* the first rise through zero in the oldest part */
        int16_t a = rc_scope[(start + k) & (RC_SCOPE_N - 1u)], b = rc_scope[(start + k + 1u) & (RC_SCOPE_N - 1u)];
        if (a < 0 && b >= 0)
            break;
    }
    if (k == 400u)
        k = 0;
    for (x = 0; x < w && x < 228 && !scope_keep; x++) {
        int32_t v = rc_scope[(start + k + 2u * (uint32_t)x) & (RC_SCOPE_N - 1u)];
        int32_t py = mid - (v * (mid - 3)) / 16384;
        y[x] = (int16_t)(py < 1 ? 1 : py > h - 2 ? h - 2 : py);
    }
    for (i = 0; i < 2; i++)
        for (x = 1, prev = y[0]; x < w && x < 228; prev = y[x], x++) {
            if (i == 0) {
                cv_line(x - 1, prev - 1, x, y[x] - 1, halo);
                cv_line(x - 1, prev + 1, x, y[x] + 1, halo);
            } else {
                cv_line(x - 1, prev, x, y[x], c);
            }
        }
}


#include "menus.c"
#include "save.c"

#include "frame.c"
#include "seqpages.c"
#include "drumpages.c"                       /* the DRUM page (drum track, step 2) */

#define HOME_WY 200u                          /* HOME's bottom strip (the scope until update 38) */
/* HOME's bottom strip (update 39: the scope is gone, as Josh asked): the tabs of what plays the keys besides the
 * preset (scale and chord, arp, STYLE, a tuning, the red FM-1 OFF warning), redrawn only when one of them changes */
#define HOME_SH 16
static uint32_t home_strip_sig = 1u;
static void home_strip(int force)
{
    uint32_t sig = (uint32_t)GI(G_TUNE) * 7919u ^ (uint32_t)AI(A_STYLE) * 131u ^ (uint32_t)AI(A_MODE) * 17u
                   ^ (uint32_t)CI(C_TYPE) * 1031u ^ (uint32_t)CI(C_QNT) * 3u ^ (uint32_t)CI(C_KEY) * 65537u
                   ^ (uint32_t)lay_local(L_CHORD) << 28 ^ (uint32_t)lay_local(L_ARP) << 29 ^ (uint32_t)rc_heavy << 30
                   ^ (uint32_t)(sq.run | dr.run << 1) * 40503u ^ (uint32_t)(tempo_bpm() + 0.5f) * 257u;   /* (update 40: the beat's chip) */
    if (!force && sig == home_strip_sig)
        return;
    home_strip_sig = sig;
    cv_begin(228, HOME_SH, K_BG);
    if (rc_heavy)
        cv_text(114 - text_w(&FONT_XS, "TOO HEAVY FOR FM-1: MUTED") / 2, 0, &FONT_XS, "TOO HEAVY FOR FM-1: MUTED", K_WARN);
    else
        home_chips();
    cv_blit(6, HOME_WY);
}

static void draw_footer(void)                 /* (the status strip on top; the modulation strip is gone) */
{
    status_strip();
}

#include "vis_home.c"

static void draw_home(void)                   /* (update 13: the morphing picture, the macros under it, the tabs between) */
{
    status_strip();
    clouds_screen(PG_HOME, 0, 0);
    home_strip(1);
}

/* the lit buttons: the page's button (as the page's syncLeds) */
static uint32_t page_leds(void)
{
    if (sv.open)
        return 1u << B_SAVE;
    switch (mn.page) {
    case PG_HOME: case PG_RACK: return 1u << B_HOME;
    case PG_SRC: case PG_OSC: return 1u << B_EDIT;
    case PG_SHP: case PG_AMP: case PG_FLT: return 1u << B_ENV;
    case PG_MODA: case PG_MODB: case PG_LFO: return 1u << B_LFO;
    case PG_FX1: case PG_FX2: case PG_JAM: return 1u << B_FX;
    case PG_ARP: return 1u << B_ARP;
    case PG_MATRIX: return 1u << B_LFO;
    case PG_SEL: return 1u << B_SEL;
    case PG_SEQ: return 1u << B_SEQ;
    case PG_MSEQ: return 1u << B_LFO;
    case PG_GLO: return 1u << B_GLO;
    }
    return 0;
}

/* the page's drawToast(): a light box with the message, over the lower part of the page */
static void draw_toast(void)
{
    int32_t w, x;
    if (plat_ms() >= ui.msg_until)
        return;
    w = text_w(&FONT_XS, ui.msg[0]) + (ui.msg[1][0] ? text_w(&FONT_XS, ui.msg[1]) + 6 : 0) + 34;
    /* (update 50) one band the width of the screen over the help line, as LEARN's: nothing ragged under it */
    (void)x;
    if (w > 236) w = 236;
    cv_begin(240, 17, K_BG);                  /* (update 51: a quiet grey band, not a white one) */
    rrect(2, 0, 236, 17, GREY(44), 4);
    cv_rect(2, 0, 236, 1, GREY(70));
    icon(IC_HOME, 120 - w / 2 + 8, 3, K_DIM);
    {
        int32_t e = cv_text(120 - w / 2 + 26, 1, &FONT_XS, ui.msg[0], K_FG);
        cv_text(e + 6, 1, &FONT_XS, ui.msg[1], K_GOLD);
    }
    cv_blit(0, 223);
}

/* the well alone, ~30 times a second between the full redraws: the visualisers move smoothly */
static int fixed_wv_was;
static void draw_well_only(void)
{
    wv_tick();
    if (sv.open || mn.pick || sp.layer || mn.page == PG_RACK || mn.page == PG_MATRIX)
        return;
    if (mn.page == PG_HOME) {                 /* (update 13: the picture and the macros under it; the tabs when they change) */
        clouds_screen(PG_HOME, 0, 0);
        home_strip(0);
    } else if (is_seq_page(mn.page)) {
        seq_area();
    } else {
        if (pic_scene(mn.page) >= 0) {
            clouds_screen(mn.page, page_mod(mn.page), 0);
            draw_toast();
            return;
        }
        page_well(mn.page);
        if (!is_fixed(mn.page) && !(mn.page == PG_LFO && mn.sub >= UI_LFO.npages) && !is_mod_sub(mn.page))
            param_cells(mn.page);             /* the modulated sliders' halos move with the well */
        else if (is_fixed(mn.page) && picture_first(mn.page) && (wv_on() || fixed_wv_was))
            fixed_controls(mn.page);          /* (update 50: the sliders move with the sound) */
        fixed_wv_was = wv_on();
    }
    draw_toast();                             /* a toast stays on top (HOME's macros and scope drew over it, update 36) */
}

static void draw(void)
{
    static int shown = -1;                    /* the page on the screen: a new one starts from a clear screen */
    int now = mn.page | mn.pick << 5 | sv.open << 6 | sv.lib << 7 | sp.layer << 8 | picture_first(mn.page) << 9 | is_mod_sub(mn.page) << 10 | (pic_scene(mn.page) >= 0) << 11;
    if (ui.clear) {                           /* a toast gone: what it covered is drawn again from clear */
        ui.clear = 0;
        shown = -1;
    }
    if (now != shown) {
        lcd_fill(0, 0, 240, 240, K_BG);
        cp.force = 1;
        shown = now;
    }
    if (sv.open) {
        draw_save();
        draw_footer();
    } else if (sp.layer) {
        status_strip();
        draw_seq_layer();
    } else if (is_seq_page(mn.page)) {
        draw_seq_page();
        draw_footer();
    } else if (mn.page == PG_HOME)
        draw_home();
    else {
        draw_menu();
        draw_footer();
    }
    draw_toast();
}

/* the lights: the page's button lit, the others glowing (GLOBAL > LED BRIGHT: off, low, high), SEL and ARP half lit
 * while chords, QNT or the arp are on; on the SEL pages the keys in the scale glow, the zone keys half lit */
static void leds(uint32_t lit, uint32_t keys)
{
    float lb = set.glo[G_LED];
    int glow = lb < 0.2f ? 0 : lb < 0.6f ? 1 : 2;
    uint32_t dim = glow ? ((1u << NB) - 1u) & ~lit : 0u, mid = 0u, dimk = 0u, midk = 0u, play = 1u << B_PLAY, k;
    if (CI(C_TYPE) != CT_OFF || CI(C_QNT))
        mid |= 1u << B_SEL;
    if (AI(A_MODE) != AM_OFF || AI(A_STYLE))
        mid |= 1u << B_ARP;
    if (mn.page == PG_SEL)
        for (k = 0; k < NKEYS; k++) {
            if (k < 7u && (zone_on() || zone_bass()))
                midk |= 1u << k;
            else if (in_scale(53 + (int)k))
                dimk |= 1u << k;
        }
    if (mn.learn && (plat_ms() / 250u) & 1u)  /* LEARN: HOME blinks */
        lit |= 1u << B_HOME;
    if (sq.run)                               /* the sequencer: PLAY lit while it plays, REC while it records (blinking ready) */
        lit |= 1u << B_PLAY;
    if ((sq.rec && sq.run) || (sq.armed && (plat_ms() / 300u) & 1u))
        lit |= 1u << B_REC;
    lit = (lit & ~play) | (lit & play ? LED_PLAY_GREEN : 0u);
    dim = (dim & ~play) | (dim & play ? LED_PLAY_GREEN : 0u);
    plat_lights(lit, keys, dim, dimk, glow ? glow : dimk ? 1 : 0);
    plat_lights_mid(mid & ~lit, midk & ~keys);
}

/* the keys' events into the render's queue; 0: it was full (a key-up is then sent again, input()) */
static uint32_t key_owed, key_owed_z;
#ifdef PLAT_HOST
static int host_drop_off;                     /* sim: "dropoffs 1" refuses the key-ups, as a full queue would */
#endif
static int key_push(int type, int k, float x)
{
#ifdef PLAT_HOST
    if (host_drop_off && type != PLE_KEY_ON)
        return 0;
#endif
    return rc_push(type, type == PLE_ZONE ? k : PL_RAW_KEY + k, x, 0.0f);
}
/* JAM: the keys from the split up play the moves, in order, round again if there are more keys than moves */
static int jam_key(uint32_t k) { return jam_v[JV_ON] > 0.5f && (int)k >= (int)(jam_v[JV_SPLIT] + 0.5f); }
static int jam_move_of(uint32_t k)
{
    int d = (int)k - (int)(jam_v[JV_SPLIT] + 0.5f);
    return (d < 0 ? 0 : d) % JAM_NM;
}
static void input(void)
{
    uint32_t keys, btn = plat_buttons(), press = btn & ~ui.btn, k;
    int32_t e;
    {   /* each press once (update 40): the frame's own edge, or the render's count of one that fell between two
         * frames. A press the frame saw first is skipped when the count catches up with it (so a press can't arrive a
         * frame late and undo what was done while it was held, e.g. SEL + K4) */
        static uint8_t seen[32], skip[32];
        uint32_t i, edge = press;
        int isr = btn_isr_on && (int32_t)(plat_ms() - btn_isr_at) < 60;
        press = 0;
        for (i = 0; i < 32u; i++) {
            uint8_t c = btn_downs[i];
            int counted = c != seen[i];
            seen[i] = c;
            if (!isr) {
                skip[i] = 0;
                if (edge >> i & 1u) press |= 1u << i;
            } else if (edge >> i & 1u) {
                press |= 1u << i;
                if (!counted) skip[i] = 1;    /* the count will come: not a second press */
            } else if (counted) {
                if (skip[i]) skip[i] = 0;
                else press |= 1u << i;        /* a tap the frames missed */
            }
        }
    }
    in_btn = btn;
    touch_poll();
    keys = touch_keys();
    seq_layer_keys(keys, ui.keys);            /* holding SEQ: the white keys are steps (seqpages.c) */
    if (ui.jam_owed & 1u && rc_push(PLE_JAM, 255, 0.0f, 0.0f))   /* JAM: the same for "every move ends" and a move's key-up */
        ui.jam_owed &= (uint8_t)~1u;
    if (ui.jowed)
        for (k = 0; k < NKEYS; k++)
            if ((ui.jowed >> k & 1u) && ((keys >> k & 1u) || rc_push(PLE_JAM, jam_move_of(k), 0.0f, 0.0f)))
                ui.jowed &= ~(1u << k);
    if (key_owed | key_owed_z)                /* a key-up the queue refused (full): sent again until it takes it */
        for (k = 0; k < NKEYS; k++) {
            uint32_t bit = 1u << k;
            if ((key_owed & bit) && ((keys & bit) || key_push(PLE_KEY_OFF, k, 0.0f)))
                key_owed &= ~bit;
            if ((key_owed_z & bit) && ((keys & bit) || key_push(PLE_ZONE, k, 0.0f)))
                key_owed_z &= ~bit;
        }
    for (k = 0; k < NKEYS; k++) {             /* key 0 is F3 (MIDI 53); play.c makes the notes */
        uint32_t bit = 1u << k;
        if ((keys & bit) && !(ui.keys & bit) && sp.layer) {
            continue;                         /* (a step, not a note) */
        } else if ((keys & bit) && !(ui.keys & bit)) {
            ui.pkeys |= bit;
            int note = 53 + (int)k + 12 * set.octave;
            uint32_t v = touch_vel(k);
            if (k < 7u && (zone_on() || zone_bass())) {        /* SEL > ZONE: the lowest keys change the chord */
                ui.zkeys |= bit;
                rc_push(PLE_ZONE, (int)k, 1.0f, 0.0f);
            } else if (jam_key(k)) {              /* JAM (update 28): the keys above the split are moves, not notes */
                int m = jam_move_of(k);
                const char *how = jam_v[JV_LAT(m)] > 0.5f ? (jam_active(m) && (jam_m[m].on & 2u) ? "LATCH OFF" : "LATCH ON") : "HOLD";
                ui.jkeys |= bit;
                rc_push(PLE_JAM, m, 1.0f, (float)v / 127.0f);
                app_say(JAM_NAME[m], how);
            } else {
                rc_push(PLE_KEY_ON, PL_RAW_KEY + (int)k, (float)note, (float)v / 127.0f);
            }
        } else if (!(keys & bit) && (ui.keys & bit) && (ui.pkeys & bit)) {
            ui.pkeys &= ~bit;
            if (ui.zkeys & bit) {
                ui.zkeys &= ~bit;
                if (!key_push(PLE_ZONE, (int)k, 0.0f))
                    key_owed_z |= bit;
            } else if (ui.jkeys & bit) {
                ui.jkeys &= ~bit;
                if (!rc_push(PLE_JAM, jam_move_of(k), 0.0f, 0.0f))
                    ui.jowed |= bit;
            } else if (!key_push(PLE_KEY_OFF, (int)k, 0.0f)) {
                key_owed |= bit;
            }
        }
    }
    if (keys != ui.keys)
        ui.redraw = 1;
    ui.keys = keys;
    if (save_input(press)) {                 /* the SAVE page and the library take every control */
        ui.btn = btn;
        leds(btn | page_leds(), keys);
        return;
    }
    seq_buttons(press, btn);                  /* SEQ (tap: the pages; hold: the layer), PLAY, REC */
    if (sp.layer) {                           /* the layer: K1-K4 the held step's, OCT its page of 16 */
        seq_layer_encoders();
        if (press >> B_OCTDN & 1u && sp.layer_page > 0) { sp.layer_page--; ui.redraw = 1; }
        if (press >> B_OCTUP & 1u && sp.layer_page < 3) { sp.layer_page++; ui.redraw = 1; }
        ui.btn = btn;
        leds(btn | page_leds() | 1u << B_SEQ, keys);
        return;
    }
    if ((e = plat_enc(EN_PRESET)) != 0) {    /* PRESET: the presets, factory then user, on every page (as the page) */
        int i;
        for (i = 0; i < (e > 0 ? e : -e) && i < 8; i++)
            ui.shown = entry_step(ui.shown, e > 0 ? 1 : -1);
        ui.turned_at = plat_ms();
        ui.redraw = 1;
    }
    if (press >> B_OCTDN & 1u)
        octave_step(-1);
    if (press >> B_OCTUP & 1u)
        octave_step(1);
    if (is_seq_page(mn.page) || sp.save_at)  /* SAVE on a sequencer page: the sequencer (held on NOTES: copy / paste) */
        seq_save_btn(press, btn);
    else if (press >> B_SAVE & 1u)           /* SAVE: the page to name and store what is playing */
        save_open();
    sweep_hold(press, btn);                   /* ENV held + K1: the filter's SWEEP, on any page (menus.c) */
    if (menu_input(press, btn)) {                 /* a menu page: its own encoders */
        ui.btn = btn;
        leds(btn | page_leds(), keys);
        return;
    }
    if ((e = plat_enc(EN_ALGO)) != 0) {
        ui.shown = cat_step(ui.shown < NPRESETS ? ui.shown : 0, e > 0 ? 1 : -1);
        ui.turned_at = plat_ms();
        ui.redraw = 1;
    }
    if (mn.learn && (e = plat_enc(EN_SELECT)) != 0) {   /* LEARN on home: SELECT picks the macro */
        mn.learn_m = (mn.learn_m + (e > 0 ? 1 : 3)) & 3;
        mn.learn_src = 0;
        ui.redraw = 1;
    }
    if ((e = plat_enc(EN_SELECT)) != 0 && rc_patch.mode == RVM_POLY) {
        int v = rc_patch.voices_now + (int)e;
        char t[4];
        v = v < 1 ? 1 : v > RC_MAXV ? RC_MAXV : v;
        set.voices = (uint8_t)v;
        set.glo[G_VOICES] = (float)v;
        settings_touch();
        rc_voices(v);
        int_text(v, t);
        app_say("VOICES", t);
    }
    for (k = 0; k < 4u; k++)
        if ((e = plat_enc(EN_K1 + (int)k)) != 0) {
            float m = rc_patch.macro[k] + (float)e * 0.02f;
            mn.learn_m = (int)k;              /* the macro LEARN links: the last one turned */
            mn.learn_src = 0;
            if (mn.learn) {                   /* LEARN: the knob picks its macro, its value stays */
                ui.redraw = 1;
                continue;
            }
            m = m < 0.0f ? 0.0f : m > 1.0f ? 1.0f : m;
            rc_macro((int)k, m);
            if (GI(G_MOUT) == 2)              /* MIDI OUT +CC: the macros as CC 21-24 (the page's) */
                plat_midi_send(0x0Bu | (0xB0u | ((uint32_t)GI(G_OUTCH) & 15u)) << 8 | (21u + k) << 16
                               | (uint32_t)(m * 127.0f + 0.5f) << 24, TO_USB);
            ui.redraw = 1;
        }
    ui.btn = btn;
    leds(btn | page_leds(), keys);
}

void app_input_only(void) { input(); }

void app_ui_init(void)
{
    touch_on(1);
    lcd_fill(0, 0, 240, 240, K_BG);
    ui.shown = cur_entry;
    ui.redraw = 1;
}

void app_frame(void)
{
    static uint32_t last_draw, last_well;
    uint32_t now = plat_ms();
    input();
    menu_tick();
    seq_tick();
    /* PRESET or ALGORITHM stopped turning: load what is shown */
    if (ui.shown != cur_entry && now - ui.turned_at > 180u) {
        load_entry(ui.shown);
        if (ui.shown < NPRESETS)
            say_preset(ui.shown);
        ui.redraw = 1;
    }
    if (ui.msg_until && now >= ui.msg_until) {
        ui.msg_until = 0;
        ui.clear = 1;
        ui.redraw = 1;
    }
    {                                         /* the notes played (keys, chords, arp, MIDI) for the visualisers */
        static uint32_t ring_r;
        uint32_t w = pv.ring_w;
        if (w - ring_r > 16u)
            ring_r = w - 16u;
        for (; ring_r != w; ring_r++)
            vis_note(pv.ring_n[ring_r & 15u], pv.ring_v[ring_r & 15u]);
    }
    if (pl_prog_req) {                        /* MIDI program change (GLOBAL > PROG CHG): factory, then user */
        int p = pl_prog_req - 1;
        pl_prog_req = 0;
        if (p < NPRESETS || (p < NPRESETS + NUSER && us.used[p - NPRESETS])) {
            ui.shown = p;
            ui.turned_at = now - 1000u;
        }
    }
    if (GI(G_CLOCK) == 1 && pv.bpm_in > 30.0f && now - pv.clock_at < 500u) {   /* MIDI clock: its tempo */
        float b = (float)(int32_t)(pv.bpm_in + 0.5f);
        b = b < 30.0f ? 30.0f : b > 300.0f ? 300.0f : b;
        if (b != set.glo[G_TEMPO]) {
            set.glo[G_TEMPO] = b;
            rk_set_param(RS_GLO, 0, b);
        }
    }
    settings_tick();
    vis_tick();
    if (now - last_draw > 100u)               /* the level and CPU, ten times a second */
        ui.redraw = 1;
    if (ui.redraw) {
        ui.redraw = 0;
        last_draw = now;
        draw();
    } else if (now - last_well > (GI(G_ANIM) == 0 ? 33u : GI(G_ANIM) == 1 ? 50u : 166u)) {   /* the well: 30, 20, 6 a second */
        last_well = now;
        draw_well_only();
    }
    ann_tick(now);
}

void app_init(void)
{
    int n, i;
    if (plat_cpu2_set(1) == 0) {              /* the second core (now: the one time it may start): half the voices, or Clouds (update 9) */
        rk_set_worker(plat_cpu2_run, plat_cpu2_wait);
        rk_set_worker_ex(plat_cpu2_sync, plat_cpu2_on);
        rk_set_ticks(plat_ticks24);               /* update 10: a clock for the engine to see which core is the slow one (the overload guard) */
    }
    n = plat_store_load(OBJ_SETTINGS, &set, sizeof set);
    if (n == SZ9 && set.magic == SET_MAGIC9) {   /* RKL9: everything kept (its record is the new one's first part), the GATE PATTERN at its defaults */
        for (i = 40; i < NA; i++)
            set.arp[i] = UP_ARP[i].def;
        set.magic = SET_MAGIC;
        n = (int)sizeof set;
    }
    if (!(n == (int)sizeof set && set.magic == SET_MAGIC)) {   /* an older record: up to RKL8 in its own layout */
    static settings8_t old;
    const int NG8 = 23;
    memset(&old, 0, sizeof old);
    memcpy(&old, &set, (uint32_t)(n < (int)sizeof old ? (n > 0 ? n : 0) : (int)sizeof old));
#pragma push_macro("SET_MAGIC")
#undef SET_MAGIC
#define SET_MAGIC SET_MAGIC8
#define set old                               /* (the steps below were written for RKL8's layout) */
    {   /* (the older records: RKL7 19 ARP values fewer than now (RHYTHM), RKL6 one fewer again) */
    const int SZ7 = (int)sizeof set - (int)(19u * sizeof(float)), SZ6 = SZ7 - (int)sizeof(float);
    if ((set.magic == SET_MAGIC2 || set.magic == SET_MAGIC3 || set.magic == SET_MAGIC4 || set.magic == SET_MAGIC5) &&
        n > (int)((char *)&set.glo[19] - (char *)&set) && n <= SZ6 - (int)(4u * sizeof(float))) {
        /* RKL2-5 had GLOBAL's 19 values (update 40 added CLICK's 4): SEL and ARP move up, CLICK at its defaults */
        float *f = set.glo;
        int tail = (n - (int)((char *)&set.glo[19] - (char *)&set)) / (int)sizeof(float);
        for (i = tail - 1; i >= 0; i--)
            f[NG8 + i] = f[19 + i];
        for (i = 19; i < NG8; i++)
            f[i] = UP_GLO[i].def;
        n += (int)(4u * sizeof(float));
    }
    if (n == SZ6 && set.magic == SET_MAGIC4) {   /* RKL4: two taps on SEL may have silenced the keys */
        set.arp[A_LCH] = 1.0f;
        set.magic = SET_MAGIC5;
    }
    if (n == SZ6 - (int)(2u * sizeof(float)) && set.magic == SET_MAGIC3) {   /* RKL3: ON FM-1 was CHORD, BASS, ARP */
        float lc = set.arp[15], lb = set.arp[16], la = set.arp[17];
        set.arp[A_LBS] = lb;
        set.arp[A_LCH] = lc;
        set.arp[A_SCH] = UP_ARP[A_SCH].def;
        set.arp[A_LAR] = la;
        set.arp[A_SAR] = UP_ARP[A_SAR].def;
        set.magic = SET_MAGIC5;
        n = SZ6;
    }
    if (n == SZ6 - (int)(12u * sizeof(float)) && set.magic == SET_MAGIC2) {   /* RKL2: the same, shorter */
        for (i = A_STYLE; i < 40; i++)        /* (RKL8's 40 ARP values: the layout these steps are written for) */
            set.arp[i] = UP_ARP[i].def;
        set.magic = SET_MAGIC5;
        n = SZ6;
    }
    if (n == SZ6 && set.magic == SET_MAGIC5)   /* (CLICK at its defaults, above) */
        set.magic = SET_MAGIC6;
    if (n == SZ6 && set.magic == SET_MAGIC6) {   /* RKL6: HUMAN at its default */
        set.arp[A_HUM] = UP_ARP[A_HUM].def;
        set.magic = SET_MAGIC7;
        n = SZ7;
    }
    if (n == SZ7 && set.magic == SET_MAGIC7) {   /* RKL7: RHYTHM at its defaults (BASS: the style's own pattern) */
        for (i = A_RGEN; i < 40; i++)
            set.arp[i] = UP_ARP[i].def;
        set.magic = SET_MAGIC;
        n = (int)sizeof set;
    }
    }
#undef set
#pragma pop_macro("SET_MAGIC")
    if (n == (int)sizeof old && old.magic == SET_MAGIC8) {   /* RKL8: DISPLAY at its default (NORMAL) */
        set.magic = SET_MAGIC;
        set.preset = old.preset;
        set.octave = old.octave;
        set.voices = old.voices;
        set.rsv = old.rsv;
        memcpy(set.glo, old.glo, sizeof old.glo);
        set.glo[G_DISP] = UP_GLO[G_DISP].def;
        memcpy(set.sel, old.sel, sizeof set.sel);
        memcpy(set.arp, old.arp, sizeof old.arp);
        for (i = (int)(sizeof old.arp / sizeof(float)); i < NA; i++)
            set.arp[i] = UP_ARP[i].def;   /* (the GATE PATTERN at its defaults: its RATE OFF) */
        n = (int)sizeof set;
    } else {
        set.magic = old.magic;                /* (stage 2's record: its first three kept, below) */
    }
    }
    if (n != (int)sizeof set || set.magic != SET_MAGIC) {
        int ok1 = n >= 8 && set.magic == SET_MAGIC1;   /* stage 2's record: its preset, octave and voices stay */
        uint8_t p = set.preset, v = set.voices;
        int8_t o = set.octave;
        settings_defaults();
        set.magic = SET_MAGIC;
        set.preset = ok1 ? p : 0;
        set.octave = ok1 ? o : 0;
        set.voices = ok1 ? v : 0;
    }
    if (!(set.rsv & 1)) {                     /* a record from before the 45-preset bank: its preset is the old bank's number (or the old bank's end + a user slot) */
        if (set.preset < OLD_NPRESETS)
            set.preset = OLD_TO_NEW[set.preset];
        else if (set.preset < OLD_NPRESETS + NUSER)
            set.preset = (uint8_t)(NPRESETS + set.preset - OLD_NPRESETS);
        else
            set.preset = 0;
        set.rsv |= 1;
    }
    if (set.preset >= NPRESETS + NUSER || set.octave < -3 || set.octave > 3 || set.voices > RC_MAXV) {
        set.preset = 0;
        set.octave = 0;
        set.voices = 0;
    }
    set.arp[A_HOLD] = 0.0f;                   /* HOLD is not kept across a power-up: a saved HOLD left every note sounding */
    for (i = 0; i < NG; i++)                  /* each setting inside its range (a NaN fails both tests) */
        if (!(set.glo[i] >= (float)UP_GLO[i].lo && set.glo[i] <= (float)UP_GLO[i].hi)) set.glo[i] = UP_GLO[i].def;
    for (i = 0; i < NC; i++)
        if (!(set.sel[i] >= (float)UP_SEL[i].lo && set.sel[i] <= (float)UP_SEL[i].hi)) set.sel[i] = UP_SEL[i].def;
    for (i = 0; i < NA; i++)
        if (!(set.arp[i] >= (float)UP_ARP[i].lo && set.arp[i] <= (float)UP_ARP[i].hi)) set.arp[i] = UP_ARP[i].def;
    set.glo[G_VOICES] = (float)set.voices;
    rc_ext = pl_event;
    jam_defaults();                           /* JAM's settings are not kept across a power-up (update 28) */
    jam_init();
    sq_init();
    dr_init();
    users_load();
    dr_load_store();
    if (set.preset >= NPRESETS && !us.used[set.preset - NPRESETS])
        set.preset = 0;
    rc_set_guard((set.rsv >> 1) & 1);         /* the VOICE GUARD (update 63): OFF unless saved ON (rsv bit 1; bit 0 is the bank marker) */
    pal_set(GI(G_DISP));                      /* the palette (update 55: NORMAL or DAYLIGHT) */
    load_entry(set.preset);
    vz.t0 = vz.at_ms = plat_ms();
}

#ifdef PLAT_HOST
/* the host simulator's "expect voices_on N" / "expect gates_on N" (sim_taps.txt; also flt_sweep, flt_cutoff, shp_mod, page, sub: sim_sweep.txt): the engine's voices not yet silent, and
 * those whose key is still held (fm1_status 8, 9) */
int app_expect(const char *what, int *got)
{
    if (same(what, "voices_on")) {
        *got = rk_fm1_status(8);
        return 1;
    }
    if (same(what, "gates_on")) {
        *got = rk_fm1_status(9);
        return 1;
    }
    if (same(what, "flt_sweep") || same(what, "flt_cutoff")) {   /* the filter's SWEEP / CUTOFF x 100 (update 26, sim_sweep.txt) */
        float v = rk_get_param(RS_GLO, same(what, "flt_sweep") ? FLT_SWEEP : FLT_P0 + 1) * 100.0f;
        *got = (int)(v < 0.0f ? v - 0.5f : v + 0.5f);
        return 1;
    }
    if (same(what, "jam_on")) {               /* JAM (update 28, sim_jam.txt): on, the moves running, a move's level x 100, the offset sent for the SWEEP x 100 */
        *got = jam_v[JV_ON] > 0.5f;
        return 1;
    }
    if (same(what, "jam_moves")) {
        int m, n = 0;
        for (m = 0; m < JAM_NM; m++)
            n += jam_m[m].lvl > 0.0f;
        *got = n;
        return 1;
    }
    if (what[0] == 'j' && what[1] == 'a' && what[2] == 'm' && what[3] == '_' && what[4] == 'l' && what[5] == 'v' && what[6] == 'l') {
        int n = 0, i = 7;                     /* jam_lvlN: move N's level x 100 (N from 0) */
        while (what[i] >= '0' && what[i] <= '9')
            n = n * 10 + what[i++] - '0';
        *got = n < JAM_NM ? (int)(jam_m[n].lvl * 100.0f + 0.5f) : -1;
        return 1;
    }
    if (same(what, "jam_sweep")) {
        *got = (int)(jam_off[T_SWEEP] * 100.0f + (jam_off[T_SWEEP] < 0.0f ? -0.5f : 0.5f));
        return 1;
    }
    if (same(what, "jam_level_off")) {        /* the VOICES' LEVEL offset x 100 (the stutters' gate) */
        *got = (int)(jam_off[T_LEVEL] * 100.0f + (jam_off[T_LEVEL] < 0.0f ? -0.5f : 0.5f));
        return 1;
    }
    if (what[0] == 'g' && what[1] == 'a' && what[2] == 't' && what[3] == 'e' && what[4] == '_' && what[5] && what[5] != '_') {   /* the GATE PATTERN (sim_gate.txt): hitsN, stepsN, onN, rateN (N 1 or 2), lane */
        int L = 0, n = 0;
        while (what[n]) n++;
        L = what[n - 1] == '2';
        if (same(what, "gate_lane")) *got = (int)(set.arp[A_GSEL] + 0.5f);
        else if (what[5] == 'h') *got = (int)gp_hits_total[L];
        else if (what[5] == 's') *got = (int)gp[L].seq;
        else if (what[5] == 'o') *got = gp[L].on;
        else if (what[5] == 'r') *got = (int)(set.arp[A_G0 + L * 6 + RF_MODE] + 0.5f);   /* RATE as set (0 OFF) */
        else return 0;
        return 1;
    }
    if (same(what, "entry")) {                /* the preset playing: 0-45 factory, 46 and on user slots */
        *got = cur_entry;
        return 1;
    }
    if (same(what, "src_model") || same(what, "shp_model")) {   /* update 61 (sim_shapes.txt): the source's MODEL (Braids' shape) or the shaper's (Rings' model), as the engine plays it */
        *got = (int)(rk_get_param(same(what, "src_model") ? RS_SRC : RS_SHP, 0) + 0.5f);
        return 1;
    }
    if (same(what, "shp_mod")) {              /* the module in the shaper slot (0 empty, 10 Rings) */
        *got = rc_patch.mod[RS_SHP];
        return 1;
    }
    if (same(what, "page") || same(what, "sub")) {   /* the menu's page (menus.c PG_*: 0 HOME, 8 AMP, 17 FILTER) and its page within */
        *got = same(what, "page") ? mn.page : mn.sub;
        return 1;
    }
    if (same(what, "drum_run") || same(what, "drum_hits") || same(what, "drum_mute") || same(what, "drum_lane") || same(what, "drum_cursor") ||
        same(what, "drum_cur") || same(what, "drum_snd") || same(what, "drum_on") || same(what, "drum_any") ||
        same(what, "drum_force") || same(what, "drum_ratchet") || same(what, "drum_chance")) {   /* the drum track (sim_drum.txt) */
        *got = same(what, "drum_run") ? dr.run : same(what, "drum_hits") ? (int)dr.hits : same(what, "drum_mute") ? dr.mute :
               same(what, "drum_lane") ? dp.lane : same(what, "drum_cursor") ? dp.step : same(what, "drum_cur") ? dr_col(0) :
               same(what, "drum_snd") ? rk_drum_status(0) : same(what, "drum_any") ? dr_any() :
               same(what, "drum_force") ? (int)DRS_FORCE(dr_pat[dp.lane][dp.step]) : same(what, "drum_ratchet") ? DRS_RATCH(dr_pat[dp.lane][dp.step]) :
               same(what, "drum_chance") ? (int)DR_CHANCE[DRS_CHANCE(dr_pat[dp.lane][dp.step])] : 0;
        if (same(what, "drum_on")) {          /* steps on, all lanes */
            int l, i;
            for (l = 0; l < DR_LANES; l++)
                for (i = 0; i < DR_STEPS; i++)
                    *got += (dr_pat[l][i] & DRS_ON) != 0;
        }
        return 1;
    }
    if (what[0] == 'd' && what[1] == 'r' && what[2] == 'u' && what[3] == 'm' && what[4] == '_' && what[5] == 'c' && what[6] == 'n' && what[7] == 't' && what[8] >= '0' && what[8] <= '9') {
        int n = 0, i = 8;                     /* drum_cntN: step N (from 0) of the lane on the page: 1 when on */
        while (what[i] >= '0' && what[i] <= '9')
            n = n * 10 + what[i++] - '0';
        *got = n < DR_STEPS ? (dr_pat[dp.lane][n] & DRS_ON) != 0 : -1;
        return 1;
    }
    if (drum_expect(what, got))
        return 1;
    if (same(what, "seq_cursor")) {           /* the sequencer's step cursor (0 is the first step) */
        *got = sq_cursor;
        return 1;
    }
    if (same(what, "seq_total")) {            /* the notes in the bank playing, all its steps */
        int i;
        *got = 0;
        for (i = 0; i < SQ_STEPS; i++) *got += sqb[sq.bank].s[i].cnt;
        return 1;
    }
    if (same(what, "seq_first")) {            /* the first step (from 0) holding notes, -1 none */
        int i;
        *got = -1;
        for (i = 0; i < SQ_STEPS && *got < 0; i++) if (sqb[sq.bank].s[i].cnt) *got = i;
        return 1;
    }
    if (same(what, "seq_run")) { *got = sq.run; return 1; }
    if (same(what, "seq_step")) { *got = sq.cur; return 1; }   /* the step playing (the playhead), -1 stopped */
    if (what[0] == 's' && what[1] == 'e' && what[2] == 'q' && what[3] == '_' && what[4] == 'c' && what[5] == 'n' && what[6] == 't' && what[7] >= '0' && what[7] <= '9') {
        int n = 0, i = 7;                     /* seq_cntN: how many notes step N (from 0) of the bank playing holds */
        while (what[i] >= '0' && what[i] <= '9')
            n = n * 10 + what[i++] - '0';
        *got = n < SQ_STEPS ? sqb[sq.bank].s[n].cnt : -1;
        return 1;
    }
    return 0;
}
/* sim: "ghost NOTE" starts a voice behind the play layer's back (a note-on whose off was lost); "dropoffs 1|0" */
int app_host_cmd(const char *cmd, const char *a, const char *b, const char *c, const char *d)
{
    int v = 0;
    (void)c;
    (void)d;
    while (*a >= '0' && *a <= '9')
        v = v * 10 + *a++ - '0';
    if (same(cmd, "ghost")) {
        rk_note_on((float)v, 0.8f);
        return 1;
    }
    if (same(cmd, "seqclear")) {              /* update 41 step 6: the bank playing emptied, the cursor on the first step (the sims' fresh start) */
        sq_bank_t *bk = &sqb[sq.bank];
        int i;
        for (i = 0; i < SQ_STEPS; i++) bk->s[i].cnt = 0;
        sq_cursor = 0;
        return 1;
    }
    if (same(cmd, "seqstop")) {               /* stopped, REC READY off, the bank's start (the sims' fresh start) */
        if (sq.run) sq.req = 2;
        sq.armed = 0;
        sq.rec = 0;
        return 1;
    }
    if (same(cmd, "seqlen")) {                /* the pattern's length in steps */
        sqb[sq.bank].len = (uint8_t)(v < 1 ? 1 : v > SQ_STEPS ? SQ_STEPS : v);
        if (sq_cursor >= sqb[sq.bank].len) sq_cursor = 0;
        return 1;
    }
    if (same(cmd, "dropoffs")) {
        host_drop_off = v;
        return 1;
    }
    return 0;
}
#endif
