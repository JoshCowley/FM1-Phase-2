/* SPDX-License-Identifier: MPL-2.0 */
/* FM1 Phase 2 firmware: update 13, HOME's picture, part of app.c (included before draw_home).
 *
 * One morphing picture, built round the granular field: the CLOUDS buffer ribbon with its write-head comet and the grains rising from it (driven
 * exactly as on the Clouds page: vis_clouds.c's cp_step, cp_ribbon, cp_motes_draw), and every other process brought into it:
 *   source    glowing strings from the left, one per voice, vibrating with that voice's envelope, in the colour and shape of the oscillator's family
 *   filter    an aperture the strings and the ribbon pass through: it closes as the cutoff falls (the ribbon thins, the grains turn warm) and blooms at resonance
 *   modulation the whole field sways and breathes with the real LFO / Tides / Peaks values, and their recent history runs across the top
 *   delay     echo ghosts of the grains thrown to the right, as many and as bright as FEEDBACK and MIX say, as far apart as the delay time
 *   reverb    a haze behind it all that swells with the output's tail and the room's SIZE
 *   arp / sequencer   beads along the base, the step lit
 * A strip of the active chain's process icons in signal order runs under it, each alive with its process. When a module is added or removed, a preset
 * loads or a knob moves, every amount, position and colour below is not set but eased toward its new value (the time constant is 150 ms, so the picture
 * has arrived in about half a second): nothing cuts. With no Clouds in the patch the granular field fades out and the strings widen to fill the centre.
 *
 * Real: the voices' envelopes, gates and notes, the output peaks, the LFO / modulator values and history, the clock step, the module parameters (as the engine hears them).
 * Modelled: the delay time in ms (division and tempo), the reverb tail (the output level held and released at SIZE's rate), which family an oscillator model is of,
 *           the grains (as on the Clouds page: their births are Clouds' own rates and ranges; the engine's real grains are not read). */

enum { H_G, H_FLT, H_DLY, H_REV, H_CHO, H_MOD, H_ARP, H_CUT, H_RES, H_DT, H_FB, H_RSZ, H_SHP, H_SR, H_SG, H_SB, H_DAMP, H_RNG, H_N };
static struct {
    float v[H_N], t[H_N];                     /* where each amount is, where it is going */
    float env[6], mod[4], lfo[3], clk, tail, flash, pkl;
    uint32_t sig, ids, gate;
    int nv, init;
    int8_t slot_clouds;
} hm;

static void hm_set(int i, float v) { hm.t[i] = v; }

/* the patch's arrangement and parameters, the engine's live values, one monitor() call a slot (the 1st gives the peaks, the clouds' one is the last) */
static const float *home_frame(uint32_t dt)
{
    static const uint8_t FXS[2] = {RS_FX1, RS_FX2};
    const float *mon = rk_monitor(RS_AMP);
    float k = dt >= 150u ? 1.0f : (float)dt / 150.0f, pk_l = mon[48] > mon[49] ? mon[48] : mon[49], pk_a = mon[48], pk_b = mon[49];
    int i, cl = -1, sid = rc_patch.mod[RS_SRC], fid = rc_patch.mod[RS_SHP];
    uint32_t ids = 0;
    float g = 0.0f, flt = 0.0f, dly = 0.0f, rev = 0.0f, cho = 0.0f, cut = 0.5f, res = 0.0f, shp = 0.0f, dt_px = 30.0f, fb = 0.3f, rsz = 0.5f, damp = 0.5f;
    float sway, breath;
    uint32_t colr = PK_FAM[0];
    hm.gate = 0u;
    for (i = 0; i < 6; i++) {
        hm.env[i] = mon[16 + i];
        hm.gate |= mon[28 + i] >= 1.0f ? 1u << i : 0u;
        pk.sp[i] = (uint16_t)(pk.sp[i] + dt * (uint32_t)(30.0f + (hm.env[i] > 0.0f ? hm.env[i] : 0.0f) * 70.0f + (float)i * 5.0f));
    }
    for (i = 0; i < 4; i++) hm.mod[i] = mon[40 + i];
    for (i = 0; i < 3; i++) hm.lfo[i] = mon[44 + i];
    hm.clk = mon[47];
    hm.nv = (int)(mon[63] + 0.5f);
    hm.nv = hm.nv < 1 ? 1 : hm.nv > 6 ? 6 : hm.nv;
    hm.pkl = pk_l;
    for (i = 0; i < RS_N; i++)
        ids = ids * 37u + (uint32_t)rc_patch.mod[i] + ((i == RS_FX1 || i == RS_FX2) && rk_fx_bypass(i) ? 101u : 0u);
    if (ids != hm.ids) {                      /* an arrangement change (a module in or out, a preset): a ring of light leaves the centre */
        if (hm.init) hm.flash = 1.0f;
        hm.ids = ids;
    }
    if (sid == 2 || sid == 10) {              /* the source: Braids' model family sets the colour and the shape; Rings is bronze, sine */
        mon = rk_monitor(RS_SRC);
        if (sid == 2) {
            int f = pk_fam((int)(P(mon, 0) + 0.5f));
            shp = (float)f;
            colr = pk_mix(PK_FAM[f], PK_FAM[(f + 1) % 6], (uint32_t)(P(mon, 2) * 140.0f));
        } else {
            shp = 0.0f;
            colr = PK_RING;
        }
    } else {
        colr = PK_FAM[0];
    }
    if (fid == 12) {
        float e = 0.0f, fc;
        mon = rk_monitor(RS_SHP);
        for (i = 0; i < 6; i++) if (hm.env[i] > e) e = hm.env[i];
        fc = 440.0f * fm_exp2f((P(mon, 1) * 120.0f - 30.0f + P(mon, 4) * e * 60.0f - 9.0f) / 12.0f);
        fc = fc < 20.0f ? 20.0f : fc > 20000.0f ? 20000.0f : fc;
        flt = 1.0f;
        cut = fm_clampf(fm_log2f(fc / 20.0f) / 9.9658f, 0.0f, 1.0f);
        res = P(mon, 2);
    }
    for (i = 0; i < 2; i++) {
        int s = FXS[i], id = rc_patch.mod[s];
        if (id == 30) { cl = s; continue; }
        if (id < 31 || id > 33 || rk_fx_bypass(s))
            continue;
        mon = rk_monitor(s);
        if (id == 31) {
            float t = pk_delay_s(mon);
            dly = 0.35f + 0.65f * P(mon, 3);
            fb = P(mon, 1);
            dt_px = fm_clampf(t * 100.0f, 14.0f, 50.0f);
        } else if (id == 32) {
            rev = 0.35f + 0.65f * P(mon, 3);
            rsz = P(mon, 0);
            damp = P(mon, 1);
        } else {
            cho = 0.3f + 0.7f * P(mon, 3);
        }
    }
    if (cl >= 0 && !rk_fx_bypass(cl))
        g = 1.0f;
    hm_set(H_G, g); hm_set(H_FLT, flt); hm_set(H_DLY, dly); hm_set(H_REV, rev); hm_set(H_CHO, cho);
    hm_set(H_MOD, rc_patch.mod[RS_MODA] || rc_patch.mod[RS_MODB] ? 1.0f : 0.35f);
    hm_set(H_ARP, (AI(A_MODE) != AM_OFF || sq.run) ? 1.0f : 0.0f);
    hm_set(H_RNG, fid == 10 ? 1.0f : 0.0f);
    hm_set(H_CUT, cut); hm_set(H_RES, res); hm_set(H_DT, dt_px); hm_set(H_FB, fb); hm_set(H_RSZ, rsz); hm_set(H_DAMP, damp); hm_set(H_SHP, shp);
    hm_set(H_SR, (float)(colr >> 16)); hm_set(H_SG, (float)((colr >> 8) & 255u)); hm_set(H_SB, (float)(colr & 255u));
    if (!hm.init) {
        for (i = 0; i < H_N; i++) hm.v[i] = hm.t[i];
        hm.init = 1;
    } else {
        for (i = 0; i < H_N; i++) hm.v[i] += (hm.t[i] - hm.v[i]) * k;
    }
    hm.flash -= (float)dt * 0.0017f;
    if (hm.flash < 0.0f) hm.flash = 0.0f;
    /* the granular field: Clouds' parameters when it is loaded; without it only what is left fades */
    cp.nobirth = cl < 0 || rk_fx_bypass(cl);
    sway = (hm.lfo[0] * 5.0f + hm.mod[0] * 4.0f + hm.mod[2] * 3.0f) * hm.v[H_MOD];
    sway = fm_clampf(sway, -9.0f, 9.0f);
    breath = fm_clampf((hm.lfo[1] * 0.10f + hm.mod[1] * 0.08f) * hm.v[H_MOD], -0.18f, 0.18f);
    cp.cy = 98 + (int)(sway + (sway >= 0 ? 0.5f : -0.5f));
    cp.norefl = 1;
    cp.gm = (int)(hm.v[H_G] * 256.0f);
    cp.lf = (int)(110.0f * hm.v[H_G]);
    cp.hsc = 150 + (int)(hm.v[H_FLT] * hm.v[H_CUT] * 106.0f) + (int)((1.0f - hm.v[H_FLT]) * 106.0f);
    cp.hs = (int)((hm.v[H_FLT] * (hm.v[H_CUT] - 0.5f)) * 9.0f);
    if (cl >= 0)
        mon = rk_monitor(cl);
    ((float *)mon)[48] = pk_a;
    ((float *)mon)[49] = pk_b;
    cp_step(mon, dt);
    cp.gain = (int32_t)((float)cp.gain * (1.0f + breath));
    cp.gain = cp.gain > 255 ? 255 : cp.gain < 30 ? 30 : cp.gain;
    hm.tail -= hm.tail * (float)dt / (380.0f + hm.v[H_RSZ] * 2600.0f);
    if (cp.lv > hm.tail) hm.tail = cp.lv;
    if (hm.tail < 0.004f) hm.tail = 0.0f;
    return mon;
}

static uint32_t home_sig(void)
{
    uint32_t s = hm.ids * 2654435761u ^ (uint32_t)ui.shown * 40503u ^ (uint32_t)(ui.shown != cur_entry) * 977u ^ (uint32_t)mn.learn * 7u ^ (uint32_t)mn.learn_m * 31u;
    int i;
    for (i = 0; i < H_N; i++)
        s = s * 31u + (uint32_t)(hm.v[i] * 64.0f + 400.0f);
    for (i = 0; i < 4; i++)
        s = s * 131u + (uint32_t)(rc_patch.macro[i] * 1000.0f);
    s ^= (uint32_t)(hm.flash * 100.0f) * 977u ^ (uint32_t)(mn.learn ? (plat_ms() / 250u) & 1u : 0u);
    return s;
}

static uint32_t hm_col(void) { return (uint32_t)(hm.v[H_SR] + 0.5f) << 16 | (uint32_t)(hm.v[H_SG] + 0.5f) << 8 | (uint32_t)(hm.v[H_SB] + 0.5f); }

static void home_draw(const float *mon, int pf)
{
    float g = hm.v[H_G], flt = hm.v[H_FLT], cut = hm.v[H_CUT], res = hm.v[H_RES], rng = hm.v[H_RNG], emax = 0.0f;
    int i, n = hm.nv, cy = cp.cy, XE = 72 + (int)((1.0f - g) * 160.0f), XL = 72 + (int)((1.0f - g) * 56.0f), shp = (int)hm.v[H_SHP], loud = 0;
    int32_t ap = 14 + (int32_t)(cut * 72.0f), hc = (int32_t)(cp.head >> 8);
    uint32_t col = hm_col(), gn = (uint32_t)cp.gain;
    for (i = 0; i < n; i++)
        if (hm.env[i] > emax) { emax = hm.env[i]; loud = i; }
    /* reverb: the haze behind everything; the source's own glow in the middle when there are no grains */
    if (hm.v[H_REV] > 0.02f) {
        int32_t rx = 56 + (int32_t)(hm.v[H_RSZ] * 84.0f), ry = 30 + (int32_t)(hm.v[H_RSZ] * 44.0f);
        uint32_t hcol = pk_mix(0x8E80FFu, 0xFF9A70u, (uint32_t)(hm.v[H_DAMP] * 190.0f)), ha = (uint32_t)(hm.v[H_REV] * (10.0f + hm.tail * 70.0f));
        pk_haze2(150, cy + 6, rx, ry, hcol, ha, 2);
    }
    if (g < 0.9f)
        pk_haze2(120, cy, 80, 36, col, (uint32_t)((1.0f - g) * (6.0f + cp.lv * 50.0f)), 2);
    if (hm.flash > 0.0f) {                    /* a change of arrangement: light spreads from the centre */
        int32_t r = 10 + (int32_t)((1.0f - hm.flash) * 120.0f);
        pk_ring(120, cy, r, r * 7 / 10, 0xD8E4FFu, (uint32_t)(hm.flash * 150.0f));
        pk_ring(120, cy, r * 7 / 10, r / 2, 0xD8E4FFu, (uint32_t)(hm.flash * 70.0f));
        pk_haze2(120, cy, 60, 40, 0xB0C0FFu, (uint32_t)(hm.flash * 30.0f), 2);
    }
    /* modulation: the history of an LFO and a modulator, thin, across the top */
    if (hm.v[H_MOD] > 0.05f) {
        pk_trace(4, 40, 9, 0, 0xE878FFu, (uint32_t)(hm.v[H_MOD] * 120.0f));
        if (rc_patch.mod[RS_MODA])
            pk_trace(0, 40, 7, 0, 0x70C0FFu, (uint32_t)(hm.v[H_MOD] * 90.0f));
    }
    /* the source: one string a voice, fanning in from the left to the granular ribbon (or, with no Clouds, across the whole picture) */
    pk_soft = 5;
    for (i = 0; i < n; i++) {
        float e = hm.env[i] > 0.0f ? hm.env[i] : 0.0f, off = (float)i - (float)(n - 1) * 0.5f;
        int32_t y0 = cy + (int32_t)(off * (26.0f - 10.0f * (1.0f - g))), y1 = cy + (int32_t)(off * (2.5f + 13.5f * (1.0f - g)));
        int32_t amp = 3 + (int32_t)(e * (float)(12 + (int)(12.0f * (1.0f - g)))), cyc = 2 * 256 + (int32_t)((1.0f - g) * 300.0f) + i * 30;
        uint32_t a = 34u + (uint32_t)(e * 221.0f), c = pk_mix(col, 0xFFFFFFu, (uint32_t)(e * 80.0f));
        float f = hm.v[H_SHP] - (float)shp;
        int ph = (int)(pk.sp[i] >> 8);
        if (f > 0.06f && shp < 5) {            /* between two families: both, one fading out and the other in */
            pk_string(4, y0, XE, y1, amp, cyc, ph, shp, 60, 0, 1, c, a * (uint32_t)(256.0f - f * 256.0f) >> 8);
            pk_string(4, y0, XE, y1, amp, cyc, ph, shp + 1, 60, 0, 1, c, a * (uint32_t)(f * 256.0f) >> 8);
        } else {
            pk_string(4, y0, XE, y1, amp + 1, cyc, ph, shp, 60, 0, 1, c, a / 2u);
            pk_string(4, y0, XE, y1, amp, cyc, ph, shp, 60, 0, 1, c, a);
        }
        pk_blob(1, 0, 4, y0, c, 50u + (uint32_t)(e * 205.0f));
        if (i == loud && hm.v[H_DLY] > 0.05f && g < 0.6f) {   /* the delay, with no grains to throw: the loudest string, again, to the right, dimmer each time */
            int kk;
            uint32_t ga = (uint32_t)(a * hm.v[H_DLY] * (1.0f - g));
            for (kk = 1; kk <= 2; kk++) {
                ga = (uint32_t)((float)ga * (0.25f + hm.v[H_FB] * 0.7f));
                pk_string(4 + (int32_t)hm.v[H_DT] * kk * 2, y0, XE + (int32_t)hm.v[H_DT] * kk * 2, y1, amp, cyc, ph - kk * 40, shp, 60, 0, 1, pk_mix(PK_DLY, c, 80), ga);
            }
        }
    }
    pk_soft = 0;
    /* the granular field */
    if (g > 0.02f) {
        int32_t xt = (int32_t)(P(mon, 0) * 239.0f), y;
        cp_ribbon(pf);
        for (y = cy - 46; y < cy - 3; y++)                 /* POSITION: a short thread up from the ribbon */
            if (y >= cp.by && y < cp.by + cp.bh)
                cp_add(xt, y, 221, 230, 255, (uint32_t)(10 + (y - (cy - 46)) * 3) * gn * (uint32_t)cp.gm >> 16);
        cp_motes_draw();
        if (cp.freeze)
            cp_spr(SPR_RING, 15, 15, hc - 7, cy - 7, 0xCFEFFFu, (uint32_t)cp.gm);
        else
            cp_spr(SPR_COMET, 40, 9, hc - 39, cy - 4, 0xFFF4E4u, gn * (uint32_t)cp.gm >> 8);
        if (hm.v[H_DLY] > 0.05f) {                          /* the delay: ghosts of the grains, to the right, dimmer each time */
            int32_t dx = (int32_t)hm.v[H_DT];
            for (i = 0; i < CP_NM; i += 2) {
                cp_mote_t *o = &cp.m[i];
                int kk;
                uint32_t a = (uint32_t)(hm.v[H_DLY] * 150.0f * g);
                if (!o->life)
                    continue;
                for (kk = 1; kk <= 2; kk++) {
                    a = (uint32_t)((float)a * (0.2f + hm.v[H_FB] * 0.75f));
                    pk_blob(0, 1, (o->x >> 8) + dx * kk, (o->y >> 8) + kk * 2, pk_mix(PK_DLY, cp_hue(o->hue + cp.hs), 60), a * (uint32_t)o->gain >> 8);
                }
            }
        }
    }
    /* the filter: an aperture, the strings' and the ribbon's gate (the shutters close as the cutoff falls) */
    if (flt > 0.03f) {
        uint32_t lc = pk_mix(0xFFA458u, 0x70FFE0u, (uint32_t)(cut * 256.0f)), la = (uint32_t)(flt * (60.0f + gn * 0.4f));
        int32_t y;
        for (y = 14; y < 150; y++) {
            int32_t d = y < cy ? cy - y : y - cy, fade;
            if (d < ap || y < cp.by || y >= cp.by + cp.bh)
                continue;
            fade = d - ap > 52 ? 0 : 255 - (d - ap) * 5;
            cp_add(XL, y, lc >> 16, (lc >> 8) & 255u, lc & 255u, la * (uint32_t)fade >> 8 > 255u ? 255u : la * (uint32_t)fade >> 8);
            cp_add(XL + 1, y, lc >> 16, (lc >> 8) & 255u, lc & 255u, la * (uint32_t)fade >> 9);
            cp_add(XL - 1, y, lc >> 16, (lc >> 8) & 255u, lc & 255u, la * (uint32_t)fade >> 9);
            cp_add(XL + 2, y, lc >> 16, (lc >> 8) & 255u, lc & 255u, la * (uint32_t)fade >> 11);
            cp_add(XL - 2, y, lc >> 16, (lc >> 8) & 255u, lc & 255u, la * (uint32_t)fade >> 11);
        }
        pk_blob(2, 1, XL, cy - ap, lc, (uint32_t)(flt * (100.0f + res * 155.0f)));
        pk_blob(2, 1, XL, cy + ap, lc, (uint32_t)(flt * (100.0f + res * 155.0f)));
        if (res > 0.25f)                                    /* resonance: the aperture blooms */
            pk_blob(3, 1, XL, cy, 0xFFFFF0u, (uint32_t)(flt * res * (60.0f + gn * 0.5f)));
    }
    /* Rings: a ring where the strings pass, wobbling in lobes, struck by the loudest voice */
    if (rng > 0.03f) {
        int32_t px = 0, py = 0, w = 2 + (int32_t)(emax * 7.0f), a = (int32_t)(pk.sp[1] >> 7);
        for (i = 0; i <= 40; i++) {
            int32_t ang = i * 256 / 40, r = 24 + (w * cp_cos(3 * ang + a) >> 14), x = XL + (r * cp_cos(ang) >> 14), y = cy + (r * cp_sin(ang) >> 14);
            if (i)
                pk_line(px, py, x, y, PK_RING, (uint32_t)(rng * (50.0f + emax * 205.0f)));
            px = x; py = y;
        }
    }
    /* the arp and the sequencer: beads along the base, the step travelling */
    if (hm.v[H_ARP] > 0.04f) {
        int step = (int)hm.clk & 15;
        for (i = 0; i < 16; i++) {
            int d = (step - i) & 15;
            uint32_t a = d < 6 ? 255u - (uint32_t)d * 36u : 30u;
            pk_blob(d == 0 ? 1 : 0, d != 0, 14 + i * 14, cy + 52, d == 0 ? 0xFFFFFFu : col, (uint32_t)(hm.v[H_ARP] * (float)a));
        }
    }
    /* the chain: each active process's icon in signal order, joined, a spark running along at the output's level */
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
                pk_line(cx - 30 + 11, 162, cx - 11, 162, 0x9098B8u, 50u);
                pk_blob(0, 0, cx - 30 + 11 + (int32_t)t * 8 / 30, 162, 0xE8ECFFu, (uint32_t)(60 + hm.pkl * 195.0f > 255.0f ? 255.0f : 60 + hm.pkl * 195.0f));
            }
            pk_icon_draw(ic[kk], cx - 6, 156, cc[kk]);
        }
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
        if (nt) {
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
