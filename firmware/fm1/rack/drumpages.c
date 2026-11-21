// SPDX-License-Identifier: GPL-3.0
// FM1 Phase 2: licensed GPL-3.0, see LICENSE; third-party code and its licences: NOTICE.md.
/* The drum pages (drum track, steps 2-3): the SEQ button's seventh page on, after SONG: DRUM, LANE SOUND, LANE MIX.
 * DRUM: 6 lanes x 16 steps (drumseq.c), the
 * cursor, the playhead, force as brightness, ratchet ticks, chance as dots, M for a muted lane.
 * K1 LANE, K2 FORCE, K3 RATCHET, K4 CHANCE (on the cursor's step: turning one on a step that is off turns it on);
 * SELECT moves the step, EDIT turns it on / off, ALGORITHM right mutes the lane, left clears it (with SEL held: the
 * 909 ISH beat, when the grid is empty). HOLD SEQ: the 16 white keys are the lane's steps (tap: on / off; held + K1-K4
 * force, ratchet, chance, nudge; SELECT: lane). Included by app.c after seqpages.c. */
static struct { int8_t lane, step; uint8_t pop, ga, gh, gt, gv, pc; uint32_t pop_at; } dp = {0, 0, 0, 0, 7, 0, 0, 0, 0};   /* pc: the pattern chip the KIT page points at */   /* GEN: the algorithm, HITS, TURN, VARY */
enum { DPOP_NONE, DPOP_MODEL, DPOP_KIT, DPOP_DIV, DPOP_CHOKE, DPOP_GEN, DPOP_OUT, DPOP_BUSY };     /* the list showing over the picture while its knob turns */
static void dp_pop(int kind) { dp.pop = (uint8_t)kind; dp.pop_at = plat_ms(); ui.redraw = 1; }
static void drum_tick(void)                   /* the main loop's: a list goes when its knob has rested */
{
    if (dp.pop && plat_ms() - dp.pop_at > 1500u) {
        dp.pop = DPOP_NONE;
        ui.redraw = 1;
    }
}

/* ---- the store: OBJ_DRM keeps the live kit, the settings and the eight patterns (A-H); the live pattern is in RAM, the others only here.
 * Read and written whole through save.c's scratch (share: 3,792 B; the object is 1,808 B). A save, or a switch while stopped, is a write (the
 * sound stops for a moment, as the sequencer's SAVE does); a switch while the drums play only reads, ahead, into dr_stage. */
#define DRM_MAGIC 0x44524D31u                 /* "DRM1" */
typedef struct {
    uint32_t magic;
    uint8_t kit, swing, busy, in_mask, pat, flags, rsv0, rsv1;   /* the kit loaded last, SWING, IF BUSY, the lanes MIDI in plays, the pattern live, (bit 0: STYLE starts the drums) */
    dr_snd_t snd[DR_LANES];                   /* the live kit */
    uint8_t note[DR_LANES], ch[DR_LANES], out[DR_LANES], rsv2[2];   /* the lanes' MIDI: GM note, channel (0-15), OUT (SOUND MIDI ALSO OFF) */
    dr_pat_t p[DR_NPAT];
} dr_store_t;
typedef char dr_store_fits[sizeof(dr_store_t) <= sizeof share && sizeof(dr_store_t) <= 3840 ? 1 : -1];
static dr_store_t *drm(void) { return (dr_store_t *)&share; }
static void drm_blank(void)                   /* an object never saved, or a bad one: the defaults */
{
    dr_store_t *S = drm();
    int l, k;
    memset(S, 0, sizeof *S);
    S->magic = DRM_MAGIC;
    S->swing = 50;
    S->busy = 1;
    S->in_mask = 0x3Fu;
    S->flags = 1;
    memcpy(S->snd, DR_KITS[0], sizeof S->snd);
    for (l = 0; l < DR_LANES; l++) { S->note[l] = DR_GM[l]; S->ch[l] = DR_CH; S->out[l] = 2; }
    for (k = 0; k < DR_NPAT; k++) {
        for (l = 0; l < DR_LANES; l++) { S->p[k].f[l].len = 16; S->p[k].f[l].div = 1; S->p[k].f[l].choke = l == 2 ? 4 : 0; }
    }
}
static int drm_valid(const dr_store_t *S)     /* every value inside its range (a bad object is not half-loaded) */
{
    int l, k;
    if (S->magic != DRM_MAGIC || S->kit >= DR_NKIT || S->swing < 50 || S->swing > 75 || S->busy > 2 || S->pat >= DR_NPAT)
        return 0;
    for (l = 0; l < DR_LANES; l++) {
        const dr_snd_t *d = &S->snd[l];
        if (d->model >= DR_NMODEL || d->pitch < 12 || d->pitch > 96 || d->p1 > 100 || d->p2 > 100 || d->decay_cs < 2 || d->level > 150 || d->send > 100 ||
            d->pan < -100 || d->pan > 100 || S->note[l] > 127 || S->ch[l] > 15 || S->out[l] > 3)
            return 0;
    }
    for (k = 0; k < DR_NPAT; k++)
        for (l = 0; l < DR_LANES; l++) {
            const dr_feel_t *f = &S->p[k].f[l];
            if (f->len < 1 || f->len > DR_STEPS || f->div >= DR_NDIV || f->choke > DR_LANES || f->shift < -15 || f->shift > 15)
                return 0;
        }
    return 1;
}
static int drm_read(void)                     /* the store into share: 1 read, 0 nothing (or a bad object): share holds the defaults */
{
    dr_store_t *S = drm();
    int n;
    share_undo = 0;
    n = plat_store_load(OBJ_DRM, S, sizeof *S);
    if (n == (int)sizeof *S && drm_valid(S))
        return 1;
    drm_blank();
    return 0;
}
static void dr_live_to(dr_pat_t *o) { memcpy(o->s, dr_pat, sizeof dr_pat); memcpy(o->f, dr_feel, sizeof dr_feel); }
static int dr_save_store(void)                /* the live pattern (into its slot), the kit and the settings; 0 saved */
{
    dr_store_t *S;
    int e, l;
    drm_read();
    S = drm();
    S->kit = dr_kit_ix; S->swing = dr.swing; S->pat = dr.pat;
    S->busy = dr_busy; S->in_mask = dr_in; S->flags = (uint8_t)(dr_style ? 1u : 0u);
    memcpy(S->snd, dr_kit, sizeof S->snd);
    for (l = 0; l < DR_LANES; l++) { S->note[l] = dr_mid[l].note; S->ch[l] = dr_mid[l].ch; S->out[l] = dr_mid[l].out; }
    dr_live_to(&S->p[dr.pat]);
    e = plat_store_save(OBJ_DRM, S, sizeof *S);
    if (!e)
        dr_dirty = 0;
    return e;
}
static void dr_load_store(void)               /* power-up: the store, if there is one (an older save has none: the defaults stay) */
{
    dr_store_t *S = drm();
    int l;
    if (!drm_read())
        return;
    dr_kit_ix = S->kit;
    memcpy(dr_kit, S->snd, sizeof dr_kit);
    dr.swing = S->swing;
    dr.pat = S->pat;
    dr_busy = S->busy; dr_in = S->in_mask; dr_style = S->flags & 1u;
    for (l = 0; l < DR_LANES; l++) { dr_mid[l].note = S->note[l]; dr_mid[l].ch = S->ch[l]; dr_mid[l].out = S->out[l]; }
    rk_drum_busy(dr_busy);
    memcpy(dr_pat, S->p[S->pat].s, sizeof dr_pat);
    memcpy(dr_feel, S->p[S->pat].f, sizeof dr_feel);
    dr_dirty = 0;
}
static void drum_say(const char *a, const char *b);
/* queue pattern p (0-7): stopped, it is live at once (the one left is kept first, if it has edits); playing, it is read ahead and comes in at the bar line;
 * playing with edits not saved, it is refused (they would be lost); the live pattern again calls a queued one off */
static void dr_queue(int p)
{
    char t[16];
    if (p < 0 || p >= DR_NPAT)
        return;
    t[0] = (char)('A' + p); t[1] = 0;
    if (!dr.run) {
        if (p == dr.pat)
            return;
        if ((dr_dirty & 1u) && dr_save_store()) {
            drum_say("NOT SAVED", "STORE FULL");
            return;
        }
        drm_read();
        memcpy(dr_pat, drm()->p[p].s, sizeof dr_pat);
        memcpy(dr_feel, drm()->p[p].f, sizeof dr_feel);
        dr.pat = (uint8_t)p;
        dr.next = 255;
        dr_staged = 0;
        dr_dirty &= (uint8_t)~1u;
        drum_say("PATTERN", t);
        return;
    }
    if (p == dr.pat) {
        dr.next = 255;
        dr_staged = 0;
        drum_say("PATTERN", "STAYS");
        return;
    }
    if (dr_dirty & 1u) {
        drum_say("SAVE FIRST", "EDITS WOULD BE LOST");
        return;
    }
    drm_read();
    memcpy(&dr_stage, &drm()->p[p], sizeof dr_stage);
    dr.next = (uint8_t)p;
    dr_staged = 1;
    drum_say("NEXT", t);
}
static int drum_save(void)
{
    return dr_save_store();
}
static int drum_lane_step(int d) { int v = dp.lane + (d > 0 ? 1 : -1); return v < 0 ? 0 : v >= DR_LANES ? DR_LANES - 1 : v; }

static void pic_drum(void)
{
    int l, i, pc;
    for (l = 0; l < DR_LANES; l++) {
        int32_t y = 3 + l * 24;
        pc = dr_col(l);
        int sel = l == dp.lane, mut = (dr.mute >> l & 1u) != 0;
        cv_text(5, y + 3, &FONT_XS, DR_LNAME[l], mut ? GREY(80) : sel ? K_FG : K_DIM);
        if (sel)
            cv_rect(1, y, 2, 20, K_SEQC);
        if (mut)
            cv_text(36, y + 3, &FONT_T, "M", K_WARN);
        for (i = 0; i < DR_STEPS; i++) {
            uint16_t s = dr_pat[l][i];
            int32_t x = 50 + i * 11;
            int on = (s & DRS_ON) != 0;
            uint16_t col = on ? mix565(K_SEQC, K_BG, 90 + (int)DRS_FORCE(s) * 41) : (i % 4 == 0 ? SQ_OFF_BEAT : SQ_OFF);
            if (i >= dr_feel[l].len)           /* past the lane's own length: dark */
                col = SQ_PAST;
            if (on && dr.run && pl_now - dr_flash[l] < 6000u)
                col = mix565(K_FG, col, 110);
            if (mut)
                col = mix565(col, K_BG, 110);
            rrect(x, y, 9, 20, col, 2);
            if (on && DRS_RATCH(s) > 1) {      /* ticks along the foot: one a hit */
                int k;
                for (k = 0; k < DRS_RATCH(s); k++)
                    cv_rect(x + 1 + k * 2, y + 15, 1, 4, K_BG);
            }
            if (on && DRS_CHANCE(s) < 5) {     /* dots along the top: the less the chance, the more */
                int k;
                for (k = 0; k < 5 - (int)DRS_CHANCE(s); k++)
                    cv_rect(x + 1 + k * 2, y + 1, 1, 1, K_BG);
            }
            if (sel && i == dp.step) {
                cv_rect(x - 1, y - 1, 11, 1, K_FG); cv_rect(x - 1, y + 20, 11, 1, K_FG);
                cv_rect(x - 1, y - 1, 1, 22, K_FG); cv_rect(x + 9, y - 1, 1, 22, K_FG);
            }
            if (i == pc)
                cv_rect(x, y + 21, 9, 2, SQ_PLAY);
        }
    }
    if (!dr_any()) {                           /* an empty grid says how to fill it */
        static const char H[] = "SEL+ALGO: LOAD BEAT";
        int32_t w = text_w(&FONT_XS, H) + 14;
        rrect(114 - w / 2, 56, w, 20, K_CELL, 4);
        cv_text_acc(121 - w / 2, 59, &FONT_XS, H, K_FG);
    }
}
static const uint16_t DR_LCOL[DR_LANES] = {RGB(214, 112, 92), RGB(224, 194, 112), RGB(112, 194, 142), RGB(102, 152, 204), RGB(204, 92, 134), RGB(152, 122, 212)};
static void dr_tabs(void)                      /* the six lanes as tabs along the top of the area: the lane's own colour on the one in use */
{
    int l;
    for (l = 0; l < DR_LANES; l++) {
        int32_t x = 2 + l * 37;
        int sel = l == dp.lane;
        rrect(x, 2, 35, 15, sel ? DR_LCOL[l] : K_CELL, 3);
        cv_text(x + 17 - text_w(&FONT_XS, DR_LNAME[l]) / 2, 3, &FONT_XS, DR_LNAME[l], sel ? K_BG : K_DIM);
    }
}
static void dr_pts_text(int v, char *t) { int_text(v / 10, t); cat(t, "."); int_text(v % 10, t + (v >= 100 ? 3 : 2)); }
static void dr_pop_list(const char *title, int n, int sel, const char *const *names, const uint8_t *pts)   /* a list over the picture, the choice lit */
{
    int i;
    char t[12];
    rrect(10, 6, 208, 20 + n * 13 + 4, K_BG, 5);
    rrect(11, 7, 206, 20 + n * 13 + 2, K_CELL, 4);
    cv_text_acc(18, 8, &FONT_XS, title, K_FG);
    for (i = 0; i < n; i++) {
        int32_t y = 24 + i * 13;
        if (i == sel)
            rrect(14, y, 200, 13, mix565(K_SEQC, K_CELL, 100), 2);
        cv_text(20, y, &FONT_XS, names[i], i == sel ? K_FG : K_DIM);
        if (pts) {
            t[0] = 0;
            dr_pts_text(pts[i], t);
            cv_text(208 - text_w(&FONT_XS, t), y, &FONT_XS, t, i == sel ? K_FG : K_DIM);
        }
    }
}
static void pic_dsound(void)                   /* LANE SOUND: the lane's tabs, the voice's decay, its model and cost */
{
    const dr_snd_t *k = &dr_kit[dp.lane];
    char t[24];
    int32_t x, py = 0;
    float a = 1.0f, r = fm_exp2f(-9.9658f * (1.6f / 216.0f) / ((float)k->decay_cs * 0.01f));
    dr_tabs();
    rrect(2, 22, 224, 98, K_BG, 4);
    for (x = 0; x < 216; x++, a *= r) {
        int32_t y = 112 - (int32_t)(a * 82.0f);
        if (x)
            { cv_line(x + 5, py, x + 6, y, DR_LCOL[dp.lane]); cv_line(x + 5, py + 1, x + 6, y + 1, DR_LCOL[dp.lane]); }
        py = y;
    }
    cv_text(8, 104, &FONT_XS, "1.6 S ACROSS", K_DIM);
    cv_text(8, 126, &FONT_XS, DR_MNAME[k->model], K_FG);
    t[0] = 0;
    cat(t, "~");
    dr_pts_text(DR_MCPU[k->model], t + 1);
    cat(t, " POINTS OF A CORE");
    cv_text(220 - text_w(&FONT_XS, t), 126, &FONT_XS, t, K_DIM);
    if (dp.pop == DPOP_MODEL)
        dr_pop_list("DRUM MODEL (CPU PTS)", DR_NMODEL, k->model, DR_MNAME, DR_MCPU);
    else if (dp.pop == DPOP_KIT)
        dr_pop_list("DRUM KIT", DR_NKIT, dr_kit_ix, DR_KNAME, 0);
}
static const char *const DR_CHOKEN[DR_LANES + 1] = {"OFF", "KICK", "SNARE", "C HAT", "O HAT", "CLAP", "PERC"};
static void pic_dfeel(void)                    /* LANE FEEL: each lane's loop as a bar of 16, its own length lit */
{
    int l, i;
    char t[8];
    for (l = 0; l < DR_LANES; l++) {
        int32_t y = 3 + l * 24;
        int sel = l == dp.lane;
        cv_text(3, y + 5, &FONT_XS, DR_LNAME[l], sel ? K_FG : K_DIM);
        for (i = 0; i < DR_STEPS; i++) {
            uint16_t c = i < dr_feel[l].len ? ((dr_pat[l][i] & DRS_ON) ? DR_LCOL[l] : mix565(DR_LCOL[l], K_BG, 110)) : SQ_PAST;
            rrect(40 + i * 11, y + 2, 9, 16, c, 2);
        }
        if (sel) {
            cv_rect(38, y, 1, 20, K_FG);
            t[0] = 0;
            int_text(dr_feel[l].len, t);
            cv_text_acc(218 - text_w(&FONT_XS, t) + 6, y + 4, &FONT_XS, t, K_BG);
        }
    }
    if (dp.pop == DPOP_DIV)
        dr_pop_list("DIVIDE: A STEP IS", DR_NDIV, dr_feel[dp.lane].div, DR_DIVN, 0);
    else if (dp.pop == DPOP_CHOKE)
        dr_pop_list("CHOKE: A HIT CUTS", DR_LANES + 1, dr_feel[dp.lane].choke, DR_CHOKEN, 0);
}
static void pic_dgen(void)                     /* DRUM GEN: the lane's tabs, the mask the settings make, and what it is */
{
    uint8_t m[DR_STEPS];
    int i, len = dr_feel[dp.lane].len;
    char t[40], u[8];
    dr_tabs();
    dr_gen_mask(dp.ga, len, dp.gh, dp.gt, m);
    for (i = 0; i < DR_STEPS; i++)
        rrect(6 + i * 13, 34, 11, 34, i >= len ? SQ_PAST : m[i] ? DR_LCOL[dp.lane] : (i % 4 == 0 ? SQ_OFF_BEAT : SQ_OFF), 3);
    t[0] = 0;
    cat(t, DG_NAME[dp.ga]);
    if (dp.ga == DG_EUCLID || dp.ga == DG_RAND) {
        int_text(dp.gh > len ? len : dp.gh, u);
        cat(t, " "); cat(t, u); cat(t, " HITS IN "); int_text(len, u); cat(t, u);
        if (dp.ga == DG_EUCLID && dp.gt % (len < 1 ? 1 : len)) { cat(t, "  TURNED "); int_text(dp.gt % len, u); cat(t, u); }
    }
    cv_text_acc(114 - text_w(&FONT_XS, t) / 2, 84, &FONT_XS, t, K_FG);
    cv_text(114 - text_w(&FONT_XS, "FILLS THE LANE, THEN EDIT IT BY HAND") / 2, 100, &FONT_XS, "FILLS THE LANE, THEN EDIT IT BY HAND", K_DIM);
    cv_text(114 - text_w(&FONT_XS, "ALGO RIGHT: WRITE THE LANE") / 2, 116, &FONT_XS, "ALGO RIGHT: WRITE THE LANE", K_DIM);
    if (dp.pop == DPOP_GEN)
        dr_pop_list("DRUM GEN", DG_N, dp.ga, DG_NAME, 0);
}
static const char *const DR_OUTN[4] = {"SOUND", "MIDI", "ALSO", "OFF"};
static const char *const DR_BUSYN[3] = {"NONE", "CLOUD", "VOICES"};
static const uint8_t DR_BUSYV[3] = {1, 2, 0};   /* the list's order (CLOUD, VOICES, NONE) in the engine's numbers */
static const char *const DR_BUSYL[3] = {"CLOUD", "VOICES", "NONE"};
static const char *const DR_GMN[47] = {"ACOUSTIC BASS DRUM", "BASS DRUM 1", "SIDE STICK", "ACOUSTIC SNARE", "HAND CLAP", "ELECTRIC SNARE", "LOW FLOOR TOM", "CLOSED HI-HAT",
    "HIGH FLOOR TOM", "PEDAL HI-HAT", "LOW TOM", "OPEN HI-HAT", "LOW-MID TOM", "HI-MID TOM", "CRASH CYMBAL 1", "HIGH TOM", "RIDE CYMBAL 1", "CHINESE CYMBAL",
    "RIDE BELL", "TAMBOURINE", "SPLASH CYMBAL", "COWBELL", "CRASH CYMBAL 2", "VIBRASLAP", "RIDE CYMBAL 2", "HI BONGO", "LOW BONGO", "MUTE HI CONGA",
    "OPEN HI CONGA", "LOW CONGA", "HIGH TIMBALE", "LOW TIMBALE", "HIGH AGOGO", "LOW AGOGO", "CABASA", "MARACAS", "SHORT WHISTLE", "LONG WHISTLE",
    "SHORT GUIRO", "LONG GUIRO", "CLAVES", "HI WOOD BLOCK", "LOW WOOD BLOCK", "MUTE CUICA", "OPEN CUICA", "MUTE TRIANGLE", "OPEN TRIANGLE"};   /* General MIDI percussion, notes 35-81 */
static const char *dr_gm_name(int n) { return n >= 35 && n <= 81 ? DR_GMN[n - 35] : "-"; }
static void pic_dmidi(void)                    /* DRUM MIDI: the lanes' notes, as General MIDI names, their channel and OUT and IN */
{
    int l;
    char t[12];
    cv_text(4, 2, &FONT_XS, "LANE", K_DIM); cv_text(50, 2, &FONT_XS, "NOTE", K_DIM); cv_text(76, 2, &FONT_XS, "GM NAME", K_DIM);
    cv_text(168, 2, &FONT_XS, "OUT  CH", K_DIM); cv_text(212, 2, &FONT_XS, "IN", K_DIM);
    for (l = 0; l < DR_LANES; l++) {
        int32_t y = 16 + l * 19;
        int sel = l == dp.lane;
        if (sel)
            rrect(1, y - 1, 226, 18, mix565(K_SEQC, K_BG, 70), 3);
        cv_text(4, y + 2, &FONT_XS, DR_LNAME[l], DR_LCOL[l]);
        int_text(dr_mid[l].note, t); cv_text(50, y + 2, &FONT_XS, t, K_FG);
        cv_text(76, y + 2, &FONT_XS, dr_gm_name(dr_mid[l].note), sel ? K_FG : K_DIM);
        cv_text(168, y + 2, &FONT_XS, DR_OUTN[dr_mid[l].out], dr_mid[l].out == DO_OFF ? K_WARN : K_FG);
        int_text(dr_mid[l].ch + 1, t); cv_text(196 - (dr_mid[l].ch > 8 ? 0 : 4), y + 2, &FONT_XS, t, K_DIM);
        rrect(214, y + 3, 9, 9, (dr_in >> l & 1u) ? K_SEQC : SQ_PAST, 2);
    }
    cv_text(4, 134, &FONT_XS, GI(G_MOUT) ? "OUT MIDI/ALSO: NOTES GO OUT  IN: PADS PLAY LANES" : "GLOBAL > MIDI OUT MUST BE ON", GI(G_MOUT) ? K_DIM : K_WARN);
    if (dp.pop == DPOP_OUT)
        dr_pop_list("OUT: WHAT THE LANE DOES", 4, dr_mid[dp.lane].out, DR_OUTN, 0);
}
static void pic_dkit(void)                     /* DRUM KIT: the patterns A-H, the kit, the load, IF BUSY */
{
    int p, ld = dr_load_x10();
    char t[40];
    for (p = 0; p < DR_NPAT; p++) {
        int32_t x = 3 + p * 28;
        int live = p == dr.pat, nxt = p == dr.next, cur = p == dp.pc;
        char n[2] = "A";
        n[0] = (char)('A' + p);
        rrect(x, 3, 26, 24, live ? K_SEQC : K_CELL, 4);
        if (nxt || cur) {
            cv_rect(x, 3, 26, 1, nxt ? SQ_PLAY : K_FG); cv_rect(x, 26, 26, 1, nxt ? SQ_PLAY : K_FG);
            cv_rect(x, 3, 1, 24, nxt ? SQ_PLAY : K_FG); cv_rect(x + 25, 3, 1, 24, nxt ? SQ_PLAY : K_FG);
        }
        cv_text(x + 13 - text_w(&FONT_B, n) / 2, 6, &FONT_B, n, live ? K_BG : K_FG);
    }
    t[0] = 0;
    cat(t, "PLAYING "); { char n[2] = "A"; n[0] = (char)('A' + dr.pat); cat(t, n); }
    if (dr.next != 255) { char n[2] = "A"; n[0] = (char)('A' + dr.next); cat(t, "   NEXT "); cat(t, n); cat(t, " AT THE BAR LINE"); }
    cv_text(4, 32, &FONT_XS, t, K_DIM);
    rrect(2, 48, 224, 98, K_BG, 4);
    t[0] = 0; cat(t, "KIT "); cat(t, DR_KNAME[dr_kit_ix]);
    cv_text_acc(10, 52, &FONT_XS, t, K_FG);
    t[0] = 0; cat(t, "LOAD ~"); dr_pts_text(ld, t + 6);
    cv_text(220 - text_w(&FONT_XS, t), 52, &FONT_XS, t, ld > 250 ? K_WARN : K_FG);
    cv_text(10, 66, &FONT_XS, "KICK, SNARE, 2 HATS, CLAP, PERC", K_DIM);
    cv_text(220 - text_w(&FONT_XS, "(ESTIMATE: ALL FOUR VOICES AT ONCE)"), 66, &FONT_XS, "(ESTIMATE: ALL FOUR VOICES AT ONCE)", K_DIM);
    cv_text(10, 82, &FONT_XS, "DRUMS ON THE SECOND CORE", K_DIM);
    t[0] = 0; cat(t, "IF BUSY: "); cat(t, dr_busy == 1 ? "CLOUDS EASES" : dr_busy == 2 ? "A SYNTH VOICE SHEDS" : "NOTHING GIVES");
    cv_text(10, 98, &FONT_XS, t, K_SEQC);
    if (rk_drum_status(2) == 1)
        cv_text(10, 112, &FONT_XS, "CLOUDS HELD BACK NOW (DENSITY 0.9)", K_WARN);
    t[0] = 0; cat(t, "STYLE STARTS AND STOPS THE DRUMS: "); cat(t, dr_style ? "ON" : "OFF");
    cv_text(10, 130, &FONT_XS, t, K_DIM);
    if (dp.pop == DPOP_KIT)
        dr_pop_list("DRUM KIT", DR_NKIT, dr_kit_ix, DR_KNAME, 0);
    else if (dp.pop == DPOP_BUSY)
        dr_pop_list("IF BUSY: WHAT GIVES", 3, dr_busy == 1 ? 0 : dr_busy == 2 ? 1 : 2, DR_BUSYL, 0);
}
static void pic_dmix(void)                     /* LANE MIX: six faders: the level bar, the pan dot, the send bar */
{
    int l;
    for (l = 0; l < DR_LANES; l++) {
        const dr_snd_t *k = &dr_kit[l];
        int32_t x = 3 + l * 37, h = (int32_t)k->level * 84 / 150;
        rrect(x, 4, 33, 96, K_BG, 4);
        if (l == dp.lane) {
            cv_rect(x, 4, 33, 1, K_FG); cv_rect(x, 99, 33, 1, K_FG); cv_rect(x, 4, 1, 96, K_FG); cv_rect(x + 32, 4, 1, 96, K_FG);
        }
        rrect(x + 6, 95 - h, 21, h + 1, DR_LCOL[l], 2);
        cv_text(x + 17 - text_w(&FONT_XS, DR_LNAME[l]) / 2, 103, &FONT_XS, DR_LNAME[l], l == dp.lane ? K_FG : K_DIM);
        cv_rect(x + 3, 120, 27, 1, K_DIM);                          /* PAN: a dot on a line */
        rrect(x + 3 + ((int32_t)k->pan + 100) * 22 / 200, 117, 5, 7, DR_LCOL[l], 2);
        cv_rect(x + 3, 133, 27, 3, K_CELL);                          /* SEND: a bar in the FX's blue */
        cv_rect(x + 3, 133, (int32_t)k->send * 27 / 100, 3, KNOB_C[3]);
    }
    cv_text(3, 140, &FONT_XS, "LEVEL BAR   PAN DOT   SEND TO FX", K_DIM);
}
/* the header's text, the four dials and the help line (draw_seq_page) */
static void drum_header_text(char *t, const char *state)
{
    t[0] = 0;
    if (mn.sub == SQP_DKIT) {
        cat(t, dr_dirty ? "UNSAVED" : "SAVED");
        cat(t, " ");
        cat(t, state);
        return;
    }
    if (mn.sub == SQP_DSOUND || mn.sub == SQP_DMIX || mn.sub == SQP_DFEEL || mn.sub == SQP_DGEN || mn.sub == SQP_DMIDI) {
        cat(t, DR_LNAME[dp.lane]);
        cat(t, " ");
        cat(t, state);
        return;
    }
    t[0] = (char)('A' + dr.pat);
    t[1] = 0;
    if (dr_dirty)
        cat(t, "*");
    if (dr.next != 255) {
        char n[3] = ">A";
        n[1] = (char)('A' + dr.next);
        cat(t, n);
    }
    cat(t, " ");
    cat(t, state);
    cat(t, "  ");
    cat(t, DR_KNAME[dr_kit_ix]);
}
static void dr_pan_text(int p, char *t)
{
    t[0] = 0;
    if (!p) { cat(t, "C"); return; }
    cat(t, p < 0 ? "L" : "R");
    int_text(p < 0 ? -p : p, t + 1);
}
static void drum_dials(const char **l, char v[4][16], float *f)
{
    uint16_t s = dr_pat[dp.lane][dp.step];
    const dr_snd_t *k = &dr_kit[dp.lane];
    if (mn.sub == SQP_DSOUND) {
        l[0] = "MODEL"; l[1] = DR_P1[k->model]; l[2] = DR_P2[k->model]; l[3] = "PITCH";
        cat(v[0], DR_MSHORT[k->model]); f[0] = (float)k->model / (float)(DR_NMODEL - 1);
        int_text(k->p1, v[1]); f[1] = (float)k->p1 / 100.0f;
        int_text(k->p2, v[2]); f[2] = (float)k->p2 / 100.0f;
        note_name(k->pitch, v[3]); f[3] = (float)(k->pitch - 12) / 84.0f;
        return;
    }
    if (mn.sub == SQP_DFEEL) {
        l[0] = "LENGTH"; l[1] = "DIVIDE"; l[2] = "SHIFT"; l[3] = "CHOKE";
        int_text(dr_feel[dp.lane].len, v[0]); f[0] = (float)(dr_feel[dp.lane].len - 1) / 15.0f;
        cat(v[1], DR_DIVN[dr_feel[dp.lane].div]); f[1] = (float)dr_feel[dp.lane].div / (float)(DR_NDIV - 1);
        if (dr_feel[dp.lane].shift > 0) cat(v[2], "+");
        int_text(dr_feel[dp.lane].shift, v[2] + (dr_feel[dp.lane].shift > 0)); f[2] = (float)(dr_feel[dp.lane].shift + 15) / 30.0f;
        cat(v[3], DR_CHOKEN[dr_feel[dp.lane].choke]); f[3] = (float)dr_feel[dp.lane].choke / (float)DR_LANES;
        return;
    }
    if (mn.sub == SQP_DGEN) {
        int hits = dp.ga == DG_EUCLID || dp.ga == DG_RAND;
        l[0] = "ALGO"; l[1] = "HITS"; l[2] = dp.ga == DG_RAND ? "SEED" : "TURN"; l[3] = "VARY";
        cat(v[0], DG_NAME[dp.ga]); f[0] = (float)dp.ga / (float)(DG_N - 1);
        if (hits) int_text(dp.gh, v[1]); else cat(v[1], "-");
        f[1] = (float)dp.gh / 16.0f;
        int_text(dp.gt, v[2]); f[2] = (float)dp.gt / 15.0f;
        int_text(dp.gv, v[3]); f[3] = (float)dp.gv / 100.0f;
        return;
    }
    if (mn.sub == SQP_DMIDI) {
        const dr_mid_t *m = &dr_mid[dp.lane];
        l[0] = "OUT"; l[1] = "CHANNEL"; l[2] = "NOTE"; l[3] = "IN";
        cat(v[0], DR_OUTN[m->out]); f[0] = (float)m->out / 3.0f;
        int_text(m->ch + 1, v[1]); f[1] = (float)m->ch / 15.0f;
        int_text(m->note, v[2]); f[2] = (float)m->note / 127.0f;
        cat(v[3], (dr_in >> dp.lane & 1u) ? "ON" : "OFF"); f[3] = (dr_in >> dp.lane & 1u) ? 1.0f : 0.0f;
        return;
    }
    if (mn.sub == SQP_DKIT) {
        l[0] = "PATTERN"; l[1] = "KIT"; l[2] = "SWING"; l[3] = "IF BUSY";
        v[0][0] = (char)('A' + dp.pc); v[0][1] = 0; f[0] = (float)dp.pc / 7.0f;
        cat(v[1], DR_KNAME[dr_kit_ix]); f[1] = (float)dr_kit_ix / (float)(DR_NKIT - 1);
        int_text(dr.swing, v[2]); f[2] = (float)(dr.swing - 50) / 25.0f;
        cat(v[3], DR_BUSYN[dr_busy > 2 ? 1 : dr_busy]); f[3] = dr_busy == 1 ? 0.0f : dr_busy == 2 ? 0.5f : 1.0f;
        return;
    }
    if (mn.sub == SQP_DMIX) {
        l[0] = "DECAY"; l[1] = "LEVEL"; l[2] = "PAN"; l[3] = "SEND";
        int_text(k->decay_cs, v[0]); f[0] = (float)k->decay_cs / 255.0f;
        int_text(k->level, v[1]); f[1] = (float)k->level / 150.0f;
        dr_pan_text(k->pan, v[2]); f[2] = (float)(k->pan + 100) / 200.0f;
        int_text(k->send, v[3]); f[3] = (float)k->send / 100.0f;
        return;
    }
    l[0] = "LANE"; l[1] = "FORCE"; l[2] = "RATCHET"; l[3] = "CHANCE";
    cat(v[0], DR_LNAME[dp.lane]); f[0] = (float)dp.lane / 5.0f;
    if (s & DRS_ON) {
        cat(v[1], DR_FORCE[DRS_FORCE(s) > 4 ? 4 : DRS_FORCE(s)]); f[1] = (float)DRS_FORCE(s) / 4.0f;
        int_text(DRS_RATCH(s), v[2]); f[2] = (float)(DRS_RATCH(s) - 1) / 3.0f;
        int_text(DR_CHANCE[DRS_CHANCE(s) > 5 ? 5 : DRS_CHANCE(s)], v[3]); f[3] = (float)DRS_CHANCE(s) / 5.0f;
    } else {
        cat(v[1], "-"); cat(v[2], "-"); cat(v[3], "-");
    }
}
static void drum_say(const char *a, const char *b) { app_say(a, b); ui.redraw = 1; }
static void drum_step_toast(void)
{
    char t[16];
    t[0] = 0;
    int_text(dp.step + 1, t);
    drum_say(DR_LNAME[dp.lane], t);
}
static void drum_hear(void) { dr_dirty |= 2u; if (!dr.run) dr_aud = (uint8_t)(dp.lane + 1); }   /* turning a lane's sound: it sounds (stopped) */
static int dp_nudge(uint8_t *v, int d, int lo, int hi)
{
    int n = dr_clamp((int)*v + d, lo, hi), ch = n != (int)*v;
    *v = (uint8_t)n;
    return ch;
}
static void drum_enc_sound(const int32_t *e)   /* LANE SOUND: SELECT lane, K1 / ALGO model (SEL + ALGO: kit), K2 K3 the two knobs, K4 pitch */
{
    dr_snd_t *k = &dr_kit[dp.lane];
    int ch = 0, m = e[1] ? (int)e[1] : (int)e[2];
    if (e[0]) { dp.lane = (int8_t)drum_lane_step((int)e[0]); ui.redraw = 1; }
    if (e[1] && (in_btn >> B_SEL & 1u)) {          /* SEL held + ALGO: the next kit (it replaces the lanes' sounds) */
        int n = (dr_kit_ix + (e[1] > 0 ? 1 : DR_NKIT - 1)) % DR_NKIT;
        dr_load_kit(n);
        dr_dirty |= 2u;
        mn.sel_long = 1;
        mn.sel_n = 0;
        dp_pop(DPOP_KIT);
        drum_say(DR_KNAME[n], "KIT LOADED");
        drum_hear();
        return;
    }
    if (m) {
        int n = dr_clamp(k->model + (m > 0 ? 1 : -1), 0, DR_NMODEL - 1);
        ch |= n != k->model;
        k->model = (uint8_t)n;
        dp_pop(DPOP_MODEL);
    }
    if (e[3]) ch |= dp_nudge(&k->p1, (int)e[3], 0, 100);
    if (e[4]) ch |= dp_nudge(&k->p2, (int)e[4], 0, 100);
    if (e[5]) ch |= dp_nudge(&k->pitch, (int)e[5], 12, 96);
    if (ch) { drum_hear(); ui.redraw = 1; }
}
static void drum_enc_mix(const int32_t *e)     /* LANE MIX: SELECT lane, K1 DECAY, K2 LEVEL, K3 PAN, K4 SEND */
{
    dr_snd_t *k = &dr_kit[dp.lane];
    int ch = 0;
    if (e[0]) { dp.lane = (int8_t)drum_lane_step((int)e[0]); ui.redraw = 1; }
    if (e[2]) ch |= dp_nudge(&k->decay_cs, (int)e[2] * 2, 2, 255);
    if (e[3]) ch |= dp_nudge(&k->level, (int)e[3], 0, 150);
    if (e[4]) { int n = dr_clamp(k->pan + (int)e[4] * 2, -100, 100); ch |= n != k->pan; k->pan = (int8_t)n; }
    if (e[5]) ch |= dp_nudge(&k->send, (int)e[5], 0, 100);
    if (ch) { drum_hear(); ui.redraw = 1; }
}
static void drum_enc_feel(const int32_t *e)    /* LANE FEEL: SELECT lane, K1 LENGTH, K2 DIVIDE, K3 SHIFT, K4 CHOKE */
{
    dr_feel_t *f = &dr_feel[dp.lane];
    int n;
    if (e[0]) { dp.lane = (int8_t)drum_lane_step((int)e[0]); ui.redraw = 1; }
    if (e[2]) { n = dr_clamp(f->len + (int)e[2], 1, DR_STEPS); if (n != f->len) { f->len = (uint8_t)n; dr_touch(); } ui.redraw = 1; }
    if (e[3]) { n = dr_clamp(f->div + (e[3] > 0 ? 1 : -1), 0, DR_NDIV - 1); if (n != f->div) { f->div = (uint8_t)n; dr_touch(); } dp_pop(DPOP_DIV); }
    if (e[4]) { n = dr_clamp(f->shift + (int)e[4], -15, 15); if (n != f->shift) { f->shift = (int8_t)n; dr_touch(); } ui.redraw = 1; }
    if (e[5]) { n = dr_clamp(f->choke + (e[5] > 0 ? 1 : -1), 0, DR_LANES); if (n != f->choke) { f->choke = (uint8_t)n; dr_touch(); } dp_pop(DPOP_CHOKE); }
}
static void drum_enc_gen(const int32_t *e)     /* DRUM GEN: SELECT lane, K1 ALGO, K2 HITS, K3 TURN, K4 VARY; ALGO right writes the lane */
{
    if (e[0]) { dp.lane = (int8_t)drum_lane_step((int)e[0]); ui.redraw = 1; }
    if (e[2]) { dp.ga = (uint8_t)dr_clamp(dp.ga + (e[2] > 0 ? 1 : -1), 0, DG_N - 1); dp_pop(DPOP_GEN); }
    if (e[3]) { dp.gh = (uint8_t)dr_clamp(dp.gh + (int)e[3], 0, 16); ui.redraw = 1; }
    if (e[4]) { dp.gt = (uint8_t)dr_clamp(dp.gt + (int)e[4], 0, 15); ui.redraw = 1; }
    if (e[5]) { dp.gv = (uint8_t)dr_clamp(dp.gv + (int)e[5], 0, 100); ui.redraw = 1; }
    if (e[1] > 0) {
        dr_gen_write(dp.lane, dp.ga, dp.gh, dp.gt, dp.gv);
        drum_say(DR_LNAME[dp.lane], dp.ga == DG_CLEAR ? "CLEARED" : "FILLED");
    }
}
static void drum_enc_midi(const int32_t *e)    /* DRUM MIDI: SELECT lane, K1 OUT, K2 CHANNEL, K3 NOTE, K4 IN (on / off) */
{
    const dr_mid_t *m = &dr_mid[dp.lane];
    if (e[0]) { dp.lane = (int8_t)drum_lane_step((int)e[0]); ui.redraw = 1; }
    if (e[2]) { dr_mid_set(dp.lane, 0, m->out + (e[2] > 0 ? 1 : -1)); dr_dirty |= 2u; dp_pop(DPOP_OUT); }
    if (e[3]) { dr_mid_set(dp.lane, 1, m->ch + (e[3] > 0 ? 1 : -1)); dr_dirty |= 2u; ui.redraw = 1; }
    if (e[4]) { dr_mid_set(dp.lane, 2, m->note + (int)e[4]); dr_dirty |= 2u; ui.redraw = 1; }
    if (e[5]) { dr_in = (uint8_t)((e[5] > 0) ? (dr_in | (1u << dp.lane)) : (dr_in & ~(1u << dp.lane))); dr_dirty |= 2u; ui.redraw = 1; }
}
static void drum_enc_kit(const int32_t *e)     /* DRUM KIT: K1 the pattern chip, ALGO right queues it (left calls the queue off), K2 KIT, K3 SWING, K4 IF BUSY, SELECT STYLE */
{
    if (e[0]) { dr_style = !dr_style; dr_dirty |= 2u; drum_say("STYLE STARTS DRUMS", dr_style ? "ON" : "OFF"); }
    if (e[2]) { dp.pc = (uint8_t)dr_clamp(dp.pc + (e[2] > 0 ? 1 : -1), 0, DR_NPAT - 1); ui.redraw = 1; }
    if (e[1] > 0) dr_queue(dp.pc);
    else if (e[1] < 0 && dr.next != 255) { dr.next = 255; dr_staged = 0; drum_say("NEXT", "CALLED OFF"); }
    if (e[3]) {
        int n = dr_clamp(dr_kit_ix + (e[3] > 0 ? 1 : -1), 0, DR_NKIT - 1);
        if (n != dr_kit_ix) { dr_load_kit(n); dr_dirty |= 2u; dr_aud = 1; }
        dp_pop(DPOP_KIT);
    }
    if (e[4]) { dr.swing = (uint8_t)dr_clamp(dr.swing + (int)e[4], 50, 75); dr_dirty |= 2u; ui.redraw = 1; }
    if (e[5]) {
        int i = dr_busy == 1 ? 0 : dr_busy == 2 ? 1 : 2;               /* the list's place of the mode */
        i = dr_clamp(i + (e[5] > 0 ? 1 : -1), 0, 2);
        dr_busy = DR_BUSYV[i];
        rk_drum_busy(dr_busy);
        dr_dirty |= 2u;
        dp_pop(DPOP_BUSY);
    }
}
static const char *drum_help(void)
{
    return mn.sub == SQP_DSOUND ? "SELECT: LANE  ALGO: MODEL  SEL+ALGO: KIT" : mn.sub == SQP_DMIX ? "SELECT: LANE  K4: HOW MUCH GOES TO FX" :
           mn.sub == SQP_DMIDI ? "OUT: SOUND / MIDI / ALSO / OFF   IN: PADS PLAY LANES" :
           mn.sub == SQP_DKIT ? "ALGO: QUEUE   SELECT: STYLE STARTS DRUMS" : mn.sub == SQP_DFEEL ? "LENGTH 12 AGAINST 16 DRIFTS EVERY BAR" : mn.sub == SQP_DGEN ? "SELECT: LANE  ALGO RIGHT: WRITE IT" :
           "EDIT: ON/OFF  SEL+K1: PATTERN  SEL+SELECT: PAGE";
}
static void drum_encoders(const int32_t *e)   /* e: SELECT, ALGO, K1-K4 */
{
    int d;
    if (e[0] && (in_btn >> B_SEL & 1u)) {          /* (2.34) SEL held + SELECT: the group's other page (SELECT alone is the step or the lane here) */
        int g = sq_group(mn.sub), n = SQG_LO[g + 1] - SQG_LO[g];
        mn.sub = sp.seq_sub = SQG_LO[g] + (mn.sub - SQG_LO[g] + (e[0] > 0 ? 1 : n - 1)) % n;
        mn.sel_long = 1;
        mn.sel_n = 0;
        ui.clear = 1;
        ui.redraw = 1;
        return;
    }
    if (mn.sub == SQP_DSOUND) { drum_enc_sound(e); return; }
    if (mn.sub == SQP_DMIX) { drum_enc_mix(e); return; }
    if (mn.sub == SQP_DFEEL) { drum_enc_feel(e); return; }
    if (mn.sub == SQP_DGEN) { drum_enc_gen(e); return; }
    if (mn.sub == SQP_DMIDI) { drum_enc_midi(e); return; }
    if (mn.sub == SQP_DKIT) { drum_enc_kit(e); return; }
    int k1 = (int)e[2];
    if (k1 && (in_btn >> B_SEL & 1u)) {            /* SEL held + K1: the pattern (A-H), at the bar line while playing */
        int p = dr.next != 255 ? dr.next : dr.pat;
        dr_queue(dr_clamp(p + (k1 > 0 ? 1 : -1), 0, DR_NPAT - 1));
        mn.sel_long = 1;
        mn.sel_n = 0;
        k1 = 0;
        ui.redraw = 1;
    }
    if (e[0]) dp.step = (int)(((dp.step + e[0]) % DR_STEPS + DR_STEPS) % DR_STEPS);
    if (k1) { d = dp.lane + (k1 > 0 ? 1 : -1); dp.lane = d < 0 ? 0 : d >= DR_LANES ? DR_LANES - 1 : d; }
    if (e[3]) dr_adjust(dp.lane, dp.step, 0, e[3] > 0 ? 1 : -1);
    if (e[4]) dr_adjust(dp.lane, dp.step, 1, e[4] > 0 ? 1 : -1);
    if (e[5]) dr_adjust(dp.lane, dp.step, 2, e[5] > 0 ? 1 : -1);
    if (e[1] > 0) {
        dr.mute ^= (uint8_t)(1u << dp.lane);
        drum_say(DR_LNAME[dp.lane], (dr.mute >> dp.lane & 1u) ? "MUTED" : "ON");
    } else if (e[1] < 0) {
        if ((in_btn >> B_SEL & 1u) && !dr_any()) {
            dr_load_beat();
            mn.sel_long = 1;                  /* (SEL let go now is no tap) */
            mn.sel_n = 0;
            drum_say("909 ISH", "BEAT LOADED");
        } else if (in_btn >> B_SEL & 1u) {
            mn.sel_long = 1;
            mn.sel_n = 0;
            drum_say("GRID NOT EMPTY", "CLEAR THE LANES FIRST");
        } else {
            dr_clear_lane(dp.lane);
            drum_say(DR_LNAME[dp.lane], "CLEARED");
        }
    }
}
static void drum_edit_btn(void)                /* EDIT: the cursor's step on / off */
{
    dr_toggle(dp.lane, dp.step);
    ui.redraw = 1;
}
/* the layer (hold SEQ): the white keys are the lane's 16 steps */
static int drum_layer_keys(uint32_t keys, uint32_t prev)
{
    static const uint8_t WK[16] = PLAT_WHITE_KEYS;
    int w;
    for (w = 0; w < 16; w++) {
        uint32_t bit = 1u << WK[w];
        if ((keys & bit) && !(prev & bit)) {
            sp.held = w;
            sp.turned = 0;
            dp.step = w;
            ui.redraw = 1;
        } else if (!(keys & bit) && (prev & bit) && sp.held == w) {
            if (!sp.turned)
                dr_toggle(dp.lane, w);
            sp.held = -1;
            ui.redraw = 1;
        }
    }
    return 1;
}
static void drum_layer_encoders(void)
{
    int k, d;
    int32_t e = plat_enc(EN_SELECT);
    if (e) { d = dp.lane + (e > 0 ? 1 : -1); dp.lane = d < 0 ? 0 : d >= DR_LANES ? DR_LANES - 1 : d; ui.redraw = 1; }
    plat_enc(EN_ALGO);
    for (k = 0; k < 4; k++) {
        e = plat_enc(EN_K1 + k);
        if (!e || sp.held < 0)
            continue;
        dr_adjust(dp.lane, sp.held, k, k == 3 ? (int)e : e > 0 ? 1 : -1);
        sp.turned = 1;
        ui.redraw = 1;
    }
}
static void draw_drum_layer(void)
{
    char t[48], u[8];
    int r, w, pc = dr_col(dp.lane);
    cv_begin(240, 16, K_SEQC);
    t[0] = 0;
    cat(t, "DRUM ");
    cat(t, DR_LNAME[dp.lane]);
    cat(t, "   HOLD ONE + KNOBS   SELECT: LANE");
    cv_text(6, 1, &FONT_T, t, K_BG);
    cv_blit(0, 10);
    for (r = 0; r < 4; r++) {
        cv_begin(240, 36, K_BG);
        for (w = 0; w < 4; w++) {
            int i = r * 4 + w;
            int32_t x = 6 + w * 37;
            uint16_t s = dr_pat[dp.lane][i];
            uint16_t col = (s & DRS_ON) ? mix565(K_SEQC, K_BG, 90 + (int)DRS_FORCE(s) * 41) : (i % 4 == 0 ? SQ_OFF_BEAT : SQ_OFF);
            rrect(x, 1, 34, 33, col, 4);
            if (i == sp.held) { cv_rect(x, 1, 34, 2, K_FG); cv_rect(x, 32, 34, 2, K_FG); cv_rect(x, 1, 2, 33, K_FG); cv_rect(x + 32, 1, 2, 33, K_FG); }
            if (i == pc) { cv_rect(x + 2, 30, 30, 1, K_BG); cv_rect(x + 2, 31, 30, 2, SQ_PLAY); }
            int_text(i + 1, t);
            cv_text(x + 17 - text_w(&FONT_XS, t) / 2, 3, &FONT_XS, t, (s & DRS_ON) ? K_BG : K_FG);
            if ((s & DRS_ON) && DRS_RATCH(s) > 1) { int k; for (k = 0; k < DRS_RATCH(s); k++) cv_rect(x + 3 + k * 4, 28, 2, 2, K_BG); }
        }
        {
            static const char *const LB[4] = {"K1 FORCE", "K2 RATCHET", "K3 CHANCE", "K4 NUDGE"};
            uint16_t s = sp.held >= 0 ? dr_pat[dp.lane][sp.held] : 0;
            rrect(158, 1, 78, 33, K_CELL, 4);
            cv_text(162, -1, &FONT_T, LB[r], K_DIM);
            t[0] = 0;
            if (sp.held < 0) cat(t, "-");
            else if (!(s & DRS_ON)) cat(t, "EMPTY");
            else if (r == 0) cat(t, DR_FORCE[DRS_FORCE(s) > 4 ? 4 : DRS_FORCE(s)]);
            else if (r == 1) { cat(t, "X"); int_text(DRS_RATCH(s), t + 1); }
            else if (r == 2) { int_text(DR_CHANCE[DRS_CHANCE(s) > 5 ? 5 : DRS_CHANCE(s)], t); cat(t, " %"); }
            else { if (DRS_NUDGE(s) >= 0) cat(t, "+"); int_text(DRS_NUDGE(s), t + (DRS_NUDGE(s) >= 0)); }
            cv_text_acc(162, 13, &FONT_XS, t, K_FG);
        }
        cv_blit(0, 28u + (uint32_t)r * 40u);
    }
    (void)u;
    help("TAP: ON / OFF   SELECT: LANE");
}
#ifdef APP_HOST_EXPECT
/* the simulator's expect (sim_drumsound.txt and the drum sims): the live kit's values for the lane on the page, the list showing */
static int drum_expect(const char *what, int *got)
{
    const dr_snd_t *k = &dr_kit[dp.lane];
    if (same(what, "drum_model")) *got = k->model;
    else if (same(what, "drum_p1")) *got = k->p1;
    else if (same(what, "drum_p2")) *got = k->p2;
    else if (same(what, "drum_pitch")) *got = k->pitch;
    else if (same(what, "drum_decay")) *got = k->decay_cs;
    else if (same(what, "drum_level")) *got = k->level;
    else if (same(what, "drum_pan")) *got = k->pan;
    else if (same(what, "drum_send")) *got = k->send;
    else if (same(what, "drum_kit")) *got = dr_kit_ix;
    else if (same(what, "drum_pop")) *got = dp.pop;
    else if (same(what, "drum_pat")) *got = dr.pat;
    else if (same(what, "drum_next")) *got = dr.next == 255 ? -1 : dr.next;
    else if (same(what, "drum_staged")) *got = dr_staged;
    else if (same(what, "drum_dirty")) *got = dr_dirty;
    else if (same(what, "drum_len")) *got = dr_feel[dp.lane].len;
    else if (same(what, "drum_div")) *got = dr_feel[dp.lane].div;
    else if (same(what, "drum_shift")) *got = dr_feel[dp.lane].shift;
    else if (same(what, "drum_choke")) *got = dr_feel[dp.lane].choke;
    else if (same(what, "drum_ga")) *got = dp.ga;
    else if (same(what, "drum_gh")) *got = dp.gh;
    else if (same(what, "drum_gt")) *got = dp.gt;
    else if (same(what, "drum_gv")) *got = dp.gv;
    else if (same(what, "drum_swing")) *got = dr.swing;
    else if (same(what, "drum_colp")) *got = dr_col(dp.lane);
    else if (same(what, "drum_lanehits")) *got = (int)dr.hits;
    else if (same(what, "drum_out")) *got = dr_mid[dp.lane].out;
    else if (same(what, "drum_ch")) *got = dr_mid[dp.lane].ch + 1;
    else if (same(what, "drum_note")) *got = dr_mid[dp.lane].note;
    else if (same(what, "drum_in")) *got = dr_in >> dp.lane & 1u;
    else if (same(what, "drum_busy")) *got = dr_busy;
    else if (same(what, "drum_style")) *got = dr_style;
    else if (same(what, "drum_pc")) *got = dp.pc;
    else if (same(what, "drum_load")) *got = dr_load_x10();
    else if (same(what, "drum_held")) *got = rk_drum_status(2);
    else return 0;
    return 1;
}
#endif
