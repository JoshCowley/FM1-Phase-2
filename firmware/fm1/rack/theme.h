// SPDX-License-Identifier: GPL-3.0
// FM1 Phase 2: licensed GPL-3.0, see LICENSE; third-party code and its licences: NOTICE.md.
/* FM1 Rack's look (update 51; update 53: split into choices, one macro each, and set from Josh's A/B picks:
 * near-black, dusty knob colours, a quiet colour per section, hairlines with a faint glow, no header tag, hairline
 * sliders with a dot, the lighter type; update 54: the pictures straight on the page, no cards or edges).
 *
 * Josh: the old look (bright pastels on black, a pill of colour on every page, neon halos) read as a "common AI
 * colour scheme". Update 51 settled the direction: neutral chrome, colour kept for what it means (mostly the four
 * knobs, K1-K4, the same on every page), softer hues of about one brightness, thin lines. Update 53 makes each part of
 * that a switch, so the A/B rounds can be rendered from the same code:
 *   FM1_BG      1 near-black   2 charcoal      3 slate (cool, lifted)
 *   FM1_KNOBS   1 cobalt/jade/yellow/vermilion   2 primaries   3 dusty pastels   4 one warm family (amber to ivory)
 *   FM1_PIC     1 pictures in ivory   2 each section its own quiet colour   3 amber, like a phosphor screen
 *   FM1_LINE    1 hairline, no halo   2 hairline, faint halo   3 two pixels, soft
 *   FM1_HEAD    1 tag outlined   2 no tag   3 tag as coloured words
 *   FM1_SLIDER  1 line and thumb   2 segments   3 bar, no thumb   4 hairline and a dot
 *   FM1_WELL    1 dark well, dot grid   2 no well   3 well with a hairline edge, no grid
 * (the typeface's weight is make_font.py's FM1_FONTW) */
#pragma once
#ifndef FM1_BG
#define FM1_BG 1
#endif
#ifndef FM1_KNOBS
#define FM1_KNOBS 3
#endif
#ifndef FM1_PIC
#define FM1_PIC 2
#endif
#ifndef FM1_LINE
#define FM1_LINE 2
#endif
#ifndef FM1_HEAD
#define FM1_HEAD 2
#endif
#ifndef FM1_SLIDER
#define FM1_SLIDER 4
#endif
#ifndef FM1_WELL
#define FM1_WELL 2
#endif

/* the greys: GREY(n), n the brightness 0-255, with the background's temperature; L(n) lifts the dark steps */
#if FM1_BG == 3
#define GREY(n) RGB((n) * 88 / 100, (n) * 94 / 100, (n))
#define L(n) ((n) + 14)
#elif FM1_BG == 2
#define GREY(n) RGB((n), (n) * 98 / 100, (n) * 95 / 100)
#define L(n) ((n) + 12)
#else
#define GREY(n) RGB((n), (n) * 98 / 100, (n) * 94 / 100)
#define L(n) (n)
#endif
/* (update 53, after Josh: "graphics need to be separated from the background without losing clarity") FM1_WELL
 * 4-6 set the page and the picture panels apart by brightness and an edge, not by blur:
 *   4 a lifted card (the panel lighter than the page, a hairline edge)   5 an inset (a lifted page, the panel deeper
 *   than it, a lighter edge)   6 a bevelled card (lit top edge, dark bottom edge, like a raised panel) */
#if FM1_WELL == 4
#define PN_K_BG GREY(5)
#define PN_K_WELL GREY(22)
#define PN_K_EDGE GREY(46)
#elif FM1_WELL == 5
#define PN_K_BG GREY(24)
#define PN_K_WELL GREY(5)
#define PN_K_EDGE GREY(54)
#elif FM1_WELL == 6
#define PN_K_BG GREY(7)
#define PN_K_WELL GREY(20)
#define PN_K_EDGE GREY(34)
#define PN_K_EDGE_HI GREY(64)
#define PN_K_EDGE_LO GREY(2)
#else
#define PN_K_BG GREY(L(0))                    /* (update 55: true black, the most contrast) */
#define PN_K_WELL GREY(L(0))
#define PN_K_EDGE GREY(L(30))
#endif
#define PN_K_BAR GREY(L(17))
#define PN_K_CELL GREY(L(21))
#define PN_K_OFF GREY(L(25))
#define PN_K_GRID GREY(L(30))
#define PN_K_SEL GREY(L(34))
#define PN_K_MID GREY(L(58))
#define PN_K_FAINT GREY(L(80))                /* (update 55: tracks and unlit parts show) */
#define PN_K_DIM GREY(192)                    /* (update 55: labels brighter) */
#define PN_K_FG GREY(248)
#define PN_K_INK GREY(18)                        /* words on a light band */
#define PN_K_GLO GREY(200)

/* the four knobs */
#if FM1_KNOBS == 2
#define PN_KN1 RGB(60, 120, 230)                 /* primaries, after the OP-1's */
#define PN_KN2 RGB(60, 190, 100)
#define PN_KN3 RGB(220, 220, 214)
#define PN_KN4 RGB(246, 110, 40)
#elif FM1_KNOBS == 3
#define PN_KN1 RGB(140, 176, 230)                /* dusty: grey-blue, sage, sand, clay (update 55: brighter) */
#define PN_KN2 RGB(146, 210, 160)
#define PN_KN3 RGB(236, 214, 156)
#define PN_KN4 RGB(236, 150, 120)
#elif FM1_KNOBS == 4
#define PN_KN1 RGB(236, 156, 64)                 /* one warm family: amber, gold, cream, ivory */
#define PN_KN2 RGB(230, 190, 96)
#define PN_KN3 RGB(236, 216, 160)
#define PN_KN4 RGB(244, 236, 214)
#else
#define PN_KN1 RGB(86, 128, 214)                 /* cobalt */
#define PN_KN2 RGB(84, 176, 126)                 /* jade */
#define PN_KN3 RGB(226, 196, 104)                /* pale chrome yellow (white is the drawings') */
#define PN_KN4 RGB(232, 100, 64)                 /* vermilion */
#endif

/* the pictures and the sections' colours */
#if FM1_PIC == 2
#define PN_K_SRC RGB(240, 184, 104)              /* ochre (update 55: each a step brighter) */
#define PN_K_SHP RGB(116, 206, 196)              /* verdigris */
#define PN_K_MOD RGB(184, 164, 236)              /* dusty violet */
#define PN_K_FX RGB(128, 178, 236)               /* steel blue */
#define PN_K_OSC RGB(244, 214, 160)
#define PN_K_AMP RGB(240, 128, 104)              /* terracotta */
#define PN_K_LFO RGB(196, 176, 240)
#define PN_K_MX RGB(240, 206, 136)
#elif FM1_PIC == 3
#define PN_K_SRC RGB(240, 176, 80)               /* phosphor amber, everything */
#define PN_K_SHP PN_K_SRC
#define PN_K_MOD PN_K_SRC
#define PN_K_FX PN_K_SRC
#define PN_K_OSC PN_K_SRC
#define PN_K_AMP PN_K_SRC
#define PN_K_LFO PN_K_SRC
#define PN_K_MX PN_K_SRC
#else
#define PN_K_SRC RGB(232, 226, 210)              /* ivory */
#define PN_K_SHP PN_K_SRC
#define PN_K_MOD PN_K_SRC
#define PN_K_FX PN_K_SRC
#define PN_K_OSC PN_K_SRC
#define PN_K_AMP PN_K_SRC
#define PN_K_LFO PN_K_SRC
#define PN_K_MX PN_K_SRC
#endif
#define PN_K_CHD RGB(170, 198, 150)              /* the playing pages keep a faint tint: SEL sage, ARP sky, SEQ rose */
#define PN_K_ARP RGB(150, 184, 210)
#define PN_K_SEQC RGB(214, 162, 180)
#define PN_L_BASS RGB(214, 166, 100)             /* STYLE's bass layer (RHYTHM's rings), beside CHORD and ARP */

/* what things mean, whatever the look */
#define PN_K_WARN RGB(232, 96, 78)
#define PN_K_OK RGB(110, 192, 136)
#define PN_K_GOLD RGB(226, 190, 118)
#define PN_K_INKACC RGB(120, 92, 40)             /* a light band's second words */
/* the matrix's sources, by kind (mod_colour) */
#define PN_MC_MACRO PN_K_GOLD
#define PN_MC_VEL PN_K_OK
#define PN_MC_KEY RGB(110, 176, 214)
#define PN_MC_RAND PN_K_FG
#define PN_MC_ENV RGB(222, 116, 96)
#define PN_MC_MOD RGB(224, 150, 72)
#define PN_MC_LFO RGB(164, 136, 220)
#define PN_MC_MSEQ RGB(214, 120, 170)


/* update 40: each kind of source, effect and module its own colour, the colour of its icon on HOME and of its page (page_colour); chosen apart in CIELAB under normal,
 * deuteranope and protanope sight (make_home_icons.py --colours prints the table), bright enough on black. Daylight keeps them. */
/* the table (name RGB565: page):
 *   BRAIDS  F5CD  SRC: BRAIDS
 *   RINGS   EB65  SHP: RINGS
 *   FILTER  3668  ENV: FILTER
 *   WAVE    CA10  SRC: WAVETABLE (Plaits' wavetable engine, 2.48)
 *   REVERB  EF7F  FX: REVERB
 *   DELAY   433E  FX: DELAY
 *   CHORUS  1537  FX: CHORUS
 *   LFO     AB7E  LFO: THREE LFOS
 *   TIDES   075F  MOD: TIDES
 *   PEAKS   FA4A  MOD: PEAKS
 *   FUNC    EF08  MOD: FUNC
 *   GRIDS   B70A  MOD: GRIDS
 *   MSEQ    F2B8  LFO menu: MSEQ
 *   GATE    94BF  LFO menu: GATE PATTERN
 *   KNOB    FBD4  MATRIX (the knob layer)
 *   DRUMS   FE35  SEQ: the DRUM pages
 *   JAM     FFB7  FX menu: JAM
 */
#define PN_C_BRAIDS RGB(240, 184, 104)
#define PN_C_RINGS RGB(232, 108, 40)
#define PN_C_FILTER RGB(50, 205, 70)
#define PN_C_WAVE RGB(205, 65, 135)
#define PN_C_REVERB RGB(232, 238, 255)
#define PN_C_DELAY RGB(70, 100, 240)
#define PN_C_CHORUS RGB(20, 165, 185)
#define PN_C_LFO RGB(170, 110, 245)
#define PN_C_TIDES RGB(0, 235, 255)
#define PN_C_PEAKS RGB(255, 74, 82)
#define PN_C_FUNC RGB(232, 226, 64)
#define PN_C_GRIDS RGB(176, 226, 84)
#define PN_C_MSEQ RGB(246, 86, 198)
#define PN_C_GATE RGB(150, 150, 255)
#define PN_C_KNOB RGB(255, 120, 160)
#define PN_C_DRUMS RGB(255, 196, 170)
#define PN_C_JAM RGB(255, 244, 184)

/* (update 55) The palette at run time: NORMAL (the look above) or DAYLIGHT (GLOBAL > DISPLAY, or hold GLO): true
 * black, white words, the knobs' and sections' colours at full strength, no see-through, thicker lines, a solid
 * slider bar and bold values, for a screen read in the sun. K_* and the rest read fm1_pal[], set by pal_set(). */
enum { P_BG, P_WELL, P_EDGE, P_BAR, P_CELL, P_OFF, P_GRID, P_SEL, P_MID, P_FAINT, P_DIM, P_FG, P_INK, P_GLO,
       P_KN1, P_KN2, P_KN3, P_KN4, P_SRC, P_SHP, P_MOD, P_FX, P_OSC, P_AMP, P_LFO, P_MX, P_CHD, P_ARP, P_SEQC,
       P_LBASS, P_WARN, P_OK, P_GOLD, P_INKACC, P_MCKEY, P_MCENV, P_MCMOD, P_MCLFO, P_MCMSEQ,
       P_C_BRAIDS, P_C_RINGS, P_C_FILTER, P_C_WAVE, P_C_REVERB, P_C_DELAY, P_C_CHORUS, P_C_LFO, P_C_TIDES, P_C_PEAKS, P_C_FUNC, P_C_GRIDS, P_C_MSEQ, P_C_GATE, P_C_KNOB, P_C_DRUMS, P_C_JAM, NPAL };
static const uint16_t PAL_NORMAL[NPAL] = {
    PN_K_BG, PN_K_WELL, PN_K_EDGE, PN_K_BAR, PN_K_CELL, PN_K_OFF, PN_K_GRID, PN_K_SEL, PN_K_MID, PN_K_FAINT, PN_K_DIM,
    PN_K_FG, PN_K_INK, PN_K_GLO, PN_KN1, PN_KN2, PN_KN3, PN_KN4, PN_K_SRC, PN_K_SHP, PN_K_MOD, PN_K_FX, PN_K_OSC,
    PN_K_AMP, PN_K_LFO, PN_K_MX, PN_K_CHD, PN_K_ARP, PN_K_SEQC, PN_L_BASS, PN_K_WARN, PN_K_OK, PN_K_GOLD, PN_K_INKACC,
    PN_MC_KEY, PN_MC_ENV, PN_MC_MOD, PN_MC_LFO, PN_MC_MSEQ,
    PN_C_BRAIDS, PN_C_RINGS, PN_C_FILTER, PN_C_WAVE, PN_C_REVERB, PN_C_DELAY, PN_C_CHORUS, PN_C_LFO, PN_C_TIDES, PN_C_PEAKS, PN_C_FUNC, PN_C_GRIDS, PN_C_MSEQ, PN_C_GATE, PN_C_KNOB, PN_C_DRUMS, PN_C_JAM};
static const uint16_t PAL_DAY[NPAL] = {
    RGB(0, 0, 0), RGB(0, 0, 0), RGB(90, 90, 90), RGB(16, 16, 16), RGB(26, 26, 26), RGB(34, 34, 34), RGB(44, 44, 44),
    RGB(54, 54, 54), RGB(84, 84, 84), RGB(120, 120, 120), RGB(222, 222, 222), RGB(255, 255, 255), RGB(0, 0, 0),
    RGB(240, 240, 240),
    RGB(90, 165, 255), RGB(70, 235, 120), RGB(255, 225, 70), RGB(255, 110, 60),          /* the knobs, saturated */
    RGB(255, 196, 80), RGB(80, 235, 220), RGB(196, 160, 255), RGB(110, 185, 255), RGB(255, 220, 150),
    RGB(255, 120, 90), RGB(210, 170, 255), RGB(255, 214, 110), RGB(170, 245, 120), RGB(120, 220, 255),
    RGB(255, 150, 210), RGB(255, 186, 80), RGB(255, 90, 70), RGB(90, 240, 140), RGB(255, 214, 110),
    RGB(140, 100, 20), RGB(110, 210, 255), RGB(255, 120, 90), RGB(255, 170, 60), RGB(190, 150, 255), RGB(255, 120, 200),
    PN_C_BRAIDS, PN_C_RINGS, PN_C_FILTER, PN_C_WAVE, PN_C_REVERB, PN_C_DELAY, PN_C_CHORUS, PN_C_LFO, PN_C_TIDES, PN_C_PEAKS, PN_C_FUNC, PN_C_GRIDS, PN_C_MSEQ, PN_C_GATE, PN_C_KNOB, PN_C_DRUMS, PN_C_JAM};
static uint16_t fm1_pal[NPAL];
static int pal_day;                           /* DAYLIGHT on: also no halos, thicker lines, a solid slider, bold values */
static void pal_set(int day)
{
    int i;
    pal_day = day;
    for (i = 0; i < NPAL; i++)
        fm1_pal[i] = day ? PAL_DAY[i] : PAL_NORMAL[i];
}
#define K_BG fm1_pal[P_BG]
#define K_WELL fm1_pal[P_WELL]
#define K_EDGE fm1_pal[P_EDGE]
#define K_EDGE_HI GREY(64)
#define K_EDGE_LO GREY(2)
#define K_BAR fm1_pal[P_BAR]
#define K_CELL fm1_pal[P_CELL]
#define K_OFF fm1_pal[P_OFF]
#define K_GRID fm1_pal[P_GRID]
#define K_SEL fm1_pal[P_SEL]
#define K_MID fm1_pal[P_MID]
#define K_FAINT fm1_pal[P_FAINT]
#define K_DIM fm1_pal[P_DIM]
#define K_FG fm1_pal[P_FG]
#define K_INK fm1_pal[P_INK]
#define K_GLO fm1_pal[P_GLO]
#define KN1 fm1_pal[P_KN1]
#define KN2 fm1_pal[P_KN2]
#define KN3 fm1_pal[P_KN3]
#define KN4 fm1_pal[P_KN4]
#define K_SRC fm1_pal[P_SRC]
#define K_SHP fm1_pal[P_SHP]
#define K_MOD fm1_pal[P_MOD]
#define K_FX fm1_pal[P_FX]
#define K_OSC fm1_pal[P_OSC]
#define K_AMP fm1_pal[P_AMP]
#define K_LFO fm1_pal[P_LFO]
#define K_MX fm1_pal[P_MX]
#define K_CHD fm1_pal[P_CHD]
#define K_ARP fm1_pal[P_ARP]
#define K_SEQC fm1_pal[P_SEQC]
#define L_BASS fm1_pal[P_LBASS]
#define K_WARN fm1_pal[P_WARN]
#define K_OK fm1_pal[P_OK]
#define K_GOLD fm1_pal[P_GOLD]
#define K_INKACC fm1_pal[P_INKACC]
#define MC_MACRO K_GOLD
#define MC_VEL K_OK
#define MC_KEY fm1_pal[P_MCKEY]
#define MC_RAND K_FG
#define MC_ENV fm1_pal[P_MCENV]
#define MC_MOD fm1_pal[P_MCMOD]
#define MC_LFO fm1_pal[P_MCLFO]
#define MC_MSEQ fm1_pal[P_MCMSEQ]
#define K_C_BRAIDS fm1_pal[P_C_BRAIDS]
#define K_C_RINGS fm1_pal[P_C_RINGS]
#define K_C_FILTER fm1_pal[P_C_FILTER]
#define K_C_WAVE fm1_pal[P_C_WAVE]
#define K_C_REVERB fm1_pal[P_C_REVERB]
#define K_C_DELAY fm1_pal[P_C_DELAY]
#define K_C_CHORUS fm1_pal[P_C_CHORUS]
#define K_C_LFO fm1_pal[P_C_LFO]
#define K_C_TIDES fm1_pal[P_C_TIDES]
#define K_C_PEAKS fm1_pal[P_C_PEAKS]
#define K_C_FUNC fm1_pal[P_C_FUNC]
#define K_C_GRIDS fm1_pal[P_C_GRIDS]
#define K_C_MSEQ fm1_pal[P_C_MSEQ]
#define K_C_GATE fm1_pal[P_C_GATE]
#define K_C_KNOB fm1_pal[P_C_KNOB]
#define K_C_DRUMS fm1_pal[P_C_DRUMS]
#define K_C_JAM fm1_pal[P_C_JAM]

/* lines: how strong a line's core and its halo are (vis.c) */
#if FM1_LINE == 1
#define VF_LINE 224u
#define VF_HALO 0u
#elif FM1_LINE == 3
#define VF_LINE 232u
#define VF_HALO 120u
#elif FM1_LINE == 4                           /* a crisp line with a dark keyline either side: it stands off the grid */
#define VF_LINE 236u
#define VF_HALO 200u
#define VF_HALO_C(c) K_WELL
#else
#define VF_LINE_N 240u                         /* (update 55: crisper) */
#define VF_HALO_N 18u
#define VF_LINE (pal_day ? 255u : VF_LINE_N)  /* DAYLIGHT: solid, no halo */
#define VF_HALO (pal_day ? 0u : VF_HALO_N)
#endif
#ifndef VF_HALO_C
#define VF_HALO_C(c) (c)                      /* the halo's colour: the line's own (FM1_LINE 4: a dark keyline) */
#endif
