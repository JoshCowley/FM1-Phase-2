// SPDX-License-Identifier: GPL-3.0
// FM1 Phase 2: licensed GPL-3.0, see LICENSE; third-party code and its licences: NOTICE.md.
/* The DRUM page (drum track, step 2): the SEQ button's seventh page, after SONG. 6 lanes x 16 steps (drumseq.c), the
 * cursor, the playhead, force as brightness, ratchet ticks, chance as dots, M for a muted lane.
 * K1 LANE, K2 FORCE, K3 RATCHET, K4 CHANCE (on the cursor's step: turning one on a step that is off turns it on);
 * SELECT moves the step, EDIT turns it on / off, ALGORITHM right mutes the lane, left clears it (with SEL held: the
 * 909 ISH beat, when the grid is empty). HOLD SEQ: the 16 white keys are the lane's steps (tap: on / off; held + K1-K4
 * force, ratchet, chance, nudge; SELECT: lane). Included by app.c after seqpages.c. */
static struct { int lane, step; } dp = {0, 0};

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
}
/* the header's text, the four dials and the help line (draw_seq_page) */
static void drum_header_text(char *t, const char *state)
{
    t[0] = 0;
    cat(t, "A ");
    cat(t, state);
    cat(t, "  909 ISH");
}
static void drum_dials(const char **l, char v[4][16], float *f)
{
    uint16_t s = dr_pat[dp.lane][dp.step];
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
static void drum_encoders(const int32_t *e)   /* e: SELECT, ALGO, K1-K4 */
{
    int d;
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
