// SPDX-License-Identifier: GPL-3.0
// FM1 Phase 2: licensed GPL-3.0, see LICENSE; third-party code and its licences: NOTICE.md.
/* The drum pages (drum track, steps 2-3): the SEQ button's seventh page on, after SONG: DRUM, LANE SOUND, LANE MIX.
 * DRUM: 6 lanes x 16 steps (drumseq.c), the
 * cursor, the playhead, force as brightness, ratchet ticks, chance as dots, M for a muted lane.
 * K1 LANE, K2 FORCE, K3 RATCHET, K4 CHANCE (on the cursor's step: turning one on a step that is off turns it on);
 * SELECT moves the step, EDIT turns it on / off, ALGORITHM right mutes the lane, left clears it (with SEL held: the
 * 909 ISH beat, when the grid is empty). HOLD SEQ: the 16 white keys are the lane's steps (tap: on / off; held + K1-K4
 * force, ratchet, chance, nudge; SELECT: lane). Included by app.c after seqpages.c. */
static struct { int8_t lane, step; uint8_t pop; uint32_t pop_at; } dp = {0, 0, 0, 0};
enum { DPOP_NONE, DPOP_MODEL, DPOP_KIT };     /* the list showing over the picture while its knob turns */
static void dp_pop(int kind) { dp.pop = (uint8_t)kind; dp.pop_at = plat_ms(); ui.redraw = 1; }
static void drum_tick(void)                   /* the main loop's: a list goes when its knob has rested */
{
    if (dp.pop && plat_ms() - dp.pop_at > 1500u) {
        dp.pop = DPOP_NONE;
        ui.redraw = 1;
    }
}
static int drum_lane_step(int d) { int v = dp.lane + (d > 0 ? 1 : -1); return v < 0 ? 0 : v >= DR_LANES ? DR_LANES - 1 : v; }

static void pic_drum(void)
{
    int l, i, pc = dr.run ? dr.cur : -1;
    for (l = 0; l < DR_LANES; l++) {
        int32_t y = 3 + l * 24;
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
    if (mn.sub == SQP_DSOUND || mn.sub == SQP_DMIX) {
        cat(t, DR_LNAME[dp.lane]);
        cat(t, " ");
        cat(t, state);
        return;
    }
    cat(t, "A ");
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
static void drum_hear(void) { if (!dr.run) dr_aud = (uint8_t)(dp.lane + 1); }   /* turning a lane's sound: it sounds (stopped) */
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
static const char *drum_help(void)
{
    return mn.sub == SQP_DSOUND ? "SELECT: LANE  ALGO: MODEL  SEL+ALGO: KIT" : mn.sub == SQP_DMIX ? "SELECT: LANE  K4: HOW MUCH GOES TO FX" :
           "SEL: STEP  EDIT: ON/OFF  ALGO RIGHT: MUTE";
}
static void drum_encoders(const int32_t *e)   /* e: SELECT, ALGO, K1-K4 */
{
    int d;
    if (mn.sub == SQP_DSOUND) { drum_enc_sound(e); return; }
    if (mn.sub == SQP_DMIX) { drum_enc_mix(e); return; }
    if (e[0]) dp.step = (int)(((dp.step + e[0]) % DR_STEPS + DR_STEPS) % DR_STEPS);
    if (e[2]) { d = dp.lane + (e[2] > 0 ? 1 : -1); dp.lane = d < 0 ? 0 : d >= DR_LANES ? DR_LANES - 1 : d; }
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
    int r, w, pc = dr.run ? dr.cur : -1;
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
    else return 0;
    return 1;
}
#endif
