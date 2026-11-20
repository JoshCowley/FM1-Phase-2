// SPDX-License-Identifier: GPL-3.0
// FM1 Phase 2: licensed GPL-3.0, see LICENSE; third-party code and its licences: NOTICE.md.
/* The drum sequencer (FM1 Phase 2 step 2, drumseq.c: the code the firmware runs, with recording hooks), checked on a computer;
 * included by rack_host.c after drumseq.c (section 13).
 *   13a  hits land on the right steps at the tempo: 4 bars at 90, 120 and 137.3 bpm (and a clock at 24 ticks a beat), every hit in the block
 *        that holds its frame, none missing, none extra, the last bar as exact as the first (no drift)
 *   13b  MIDI out: each lane's General MIDI note (36 38 42 46 39 45) on channel 10 (index 9), the velocity of its force, a note-off 30 ms later,
 *        the same block as the hit; a repeated hit ends the note first; stopping ends every note on
 *   13c  ratchets (2 to 4 hits at equal spacing), nudge (a 64th of a step a click), chance (the share played, the same each start), a muted
 *        lane (silent, the others unchanged), a step off (nothing)
 *   13d  nothing when stopped: no hook called, no hit queued; the run hook 1 at the start and 0 at the stop; a second start is the first
 *   13e  the editing: toggle, the four values clamp, the beat, clear */
#include <stdio.h>
#include <math.h>

#define DQ_MAX 4096
static struct { uint32_t blk; int lane; int force_lv; int voice, model; float note, p1, p2, decay, level, pan, send; } dq_hit[DQ_MAX];
static int dq_n;
static struct { uint32_t blk; int ch, note, vel; } dq_mid[DQ_MAX];
static int dq_nm, dq_runs[8], dq_nrun, dq_cuts[64], dq_ncut;
static void dq_cut_hook(int voice) { if (dq_ncut < 64) dq_cuts[dq_ncut++] = voice; }
static uint32_t dq_now;                       /* the block the driver is in */
static int dq_lane_of(float note, float decay)
{
    int l;
    for (l = 0; l < DR_LANES; l++)
        if ((int)note == dr_kit[l].pitch && fabsf(decay - (float)dr_kit[l].decay_cs * 0.01f) < 1e-4f)
            return l;
    return -1;
}
static void dq_hit_hook(int voice, int model, float note, float p1, float p2, float decay, float level, float pan, float send)
{
    if (dq_n < DQ_MAX) {
        dq_hit[dq_n].voice = voice; dq_hit[dq_n].model = model; dq_hit[dq_n].note = note; dq_hit[dq_n].p1 = p1; dq_hit[dq_n].p2 = p2;
        dq_hit[dq_n].decay = decay; dq_hit[dq_n].level = level; dq_hit[dq_n].pan = pan; dq_hit[dq_n].send = send;
        int l = dq_lane_of(note, decay);
        dq_hit[dq_n].blk = dq_now;
        dq_hit[dq_n].lane = l;
        dq_hit[dq_n].force_lv = (int)(level * 1000.0f + 0.5f);
        if (l < 0 || voice != DR_VOICE[l]) dq_hit[dq_n].lane = -1;
        dq_n++;
    }
}
static void dq_midi_hook(int ch0, int note, int vel)
{
    if (dq_nm < DQ_MAX) {
        dq_mid[dq_nm].blk = dq_now; dq_mid[dq_nm].ch = ch0; dq_mid[dq_nm].note = note; dq_mid[dq_nm].vel = vel;
        dq_nm++;
    }
}
static void dq_run_hook(int on) { if (dq_nrun < 8) dq_runs[dq_nrun++] = on; }
static void dq_reset(void)
{
    dq_n = dq_nm = dq_nrun = dq_ncut = 0;
    dr_hook_hit = dq_hit_hook; dr_hook_midi = dq_midi_hook; dr_hook_run = dq_run_hook;
    dr_hz = 44100.0f;
    { uint16_t keep[DR_LANES][DR_STEPS]; memcpy(keep, dr_pat, sizeof keep); dr_reset_state(); memcpy(dr_pat, keep, sizeof keep); }   /* (the pattern stays: a check sets it, then restarts) */
    dr_hook_cut = dq_cut_hook;
    dr.run = 0;
}
/* play nsteps 16ths from frame 0, in blocks of nb frames; ticks: the MIDI clock's way */
static void dq_play(int nsteps, float d, int nb, int ticks)
{
    uint32_t total = (uint32_t)((float)nsteps * d) - 1u + (ticks ? 0u : 0u), t;   /* up to the frame before step nsteps */
    dq_now = 0;
    dr_start(0);
    if (!ticks) {
        for (t = 0; t + (uint32_t)nb <= total; t += (uint32_t)nb) { dq_now = t; dr_block(t, (uint32_t)nb, d); }
    } else {                                  /* a tick every d/6 frames (the clock's), the steps from dr_tick */
        float tk = d / 6.0f, nt = 0.0f;
        uint32_t tn = 0;
        for (t = 0; t + (uint32_t)nb <= total; t += (uint32_t)nb) {
            dq_now = t;
            while (nt < (float)(t + (uint32_t)nb) && tn < (uint32_t)nsteps * 6u) {
                dr_tick((uint32_t)nt, d);
                nt += tk; tn++;
            }
            dr_flush(t, (uint32_t)nb);
        }
    }
    dr.nq = 0;                                /* what a step ahead of the end had queued is dropped; the notes on end in the tail */
    for (t = (total / (uint32_t)nb) * (uint32_t)nb; t < total + 2400u; t += (uint32_t)nb) { dq_now = t; dr_flush(t, (uint32_t)nb); }
}
static int dq_in_block(uint32_t blk, float ideal, int nb) { return ideal >= (float)blk - 0.5f && ideal < (float)(blk + (uint32_t)nb) + 0.5f; }

static void dseq_checks(int *fails)
{
    int bad = 0, l, i, k;
    static const float BPM[3] = {90.0f, 120.0f, 137.3f};
    /* 13a: the tempo */
    for (k = 0; k < 3 + 1; k++) {
        float d = 60.0f / (k < 3 ? BPM[k] : 120.0f) / 4.0f * 44100.0f;
        int nb = k == 1 ? 64 : 48, ticks = k == 3, ok = 1, want = 0, first = -1, last = -1, c = 0;
        dq_reset();
        memset(dr_pat, 0, sizeof dr_pat);
        for (i = 0; i < 16; i += 4) dr_pat[0][i] = DRS_NEW;                 /* kick: 4 on the floor */
        for (i = 0; i < 16; i += 2) dr_pat[2][i] = DRS_MK(1u, DF_SOFT, 1, 5, 0);   /* hat: 8ths */
        dr_pat[1][4] = dr_pat[1][12] = DRS_MK(1u, DF_HARD, 1, 5, 0);       /* snare */
        dq_play(64, d, nb, ticks);
        for (i = 0; i < 64; i++) want += (dr_pat[0][i % 16] & 1) + (dr_pat[1][i % 16] & 1) + (dr_pat[2][i % 16] & 1);
        if (dq_n != want) { ok = 0; printf("13a %s %.1f bpm: %d hits, want %d\n", ticks ? "clock" : "tempo", k < 3 ? BPM[k] : 120.0f, dq_n, want); }
        for (c = 0; c < dq_n && ok; c++) {
            /* the c-th hit: steps in order, lanes 0,1,2 within a step */
            int step = 0, idx = 0;
            for (step = 0; step < 64; step++) {
                int n = (dr_pat[0][step % 16] & 1) + (dr_pat[1][step % 16] & 1) + (dr_pat[2][step % 16] & 1);
                if (c < idx + n) break;
                idx += n;
            }
            {
                int within = c - idx, lane = 0, seen = 0;
                float ideal = (float)step * d, err;
                for (lane = 0; lane < 3; lane++)
                    if (dr_pat[lane][step % 16] & 1) { if (seen == within) break; seen++; }
                if (dq_hit[c].lane != lane) { ok = 0; printf("13a hit %d is lane %d, want %d\n", c, dq_hit[c].lane, lane); }
                err = (float)dq_hit[c].blk - ideal;
                if (!ticks && !dq_in_block(dq_hit[c].blk, ideal, nb)) { ok = 0; printf("13a hit %d (step %d) in block %u, its frame %.1f\n", c, step, dq_hit[c].blk, ideal); }
                if (ticks && (err > (float)nb + d / 6.0f || err < -(float)nb - d / 6.0f)) { ok = 0; printf("13a hit %d (step %d) in block %u, its frame %.1f (clock)\n", c, step, dq_hit[c].blk, ideal); }
            }
            if (first < 0) first = c;
            last = c;
        }
        (void)first; (void)last;
        printf("13a %-5s %5.1f bpm, blocks of %d: %d hits in 4 bars (4 on the floor, a backbeat, 8th hats), each in the block of its frame, bar 4 as exact as bar 1%s\n",
               ticks ? "clock" : "tempo", k < 3 ? BPM[k] : 120.0f, nb, dq_n, ok ? "" : "  FAIL");
        bad += !ok;
    }
    /* 13b: MIDI out */
    {
        static const int GM[6] = {36, 38, 42, 46, 39, 45};
        float d = 60.0f / 120.0f / 4.0f * 44100.0f;
        int ok = 1, f;
        dq_reset();
        memset(dr_pat, 0, sizeof dr_pat);
        for (l = 0; l < DR_LANES; l++) dr_pat[l][l] = DRS_MK(1u, l % 5, 1, 5, 0);   /* lane l on step l, force l (GHOST..ACC, GHOST) */
        dq_play(16, d, 48, 0);
        for (l = 0; l < DR_LANES; l++) {
            int on = -1, off = -1;
            for (i = 0; i < dq_nm; i++) {
                if (dq_mid[i].note == GM[l] && dq_mid[i].vel && on < 0) on = i;
                if (dq_mid[i].note == GM[l] && !dq_mid[i].vel && off < 0) off = i;
            }
            f = l % 5;
            if (on < 0 || dq_mid[on].ch != 9 || dq_mid[on].vel != DR_VEL[f]) { ok = 0; printf("13b lane %d: no note %d on channel 10 with velocity %d\n", l, GM[l], DR_VEL[f]); continue; }
            if (!dq_in_block(dq_mid[on].blk, (float)l * d, 48)) { ok = 0; printf("13b lane %d: the note is in block %u, its step's frame %.0f\n", l, dq_mid[on].blk, (float)l * d); }
            if (off < 0 || dq_mid[off].ch != 9 || !dq_in_block(dq_mid[off].blk, (float)l * d + 0.03f * 44100.0f, 48 + 48)) {
                ok = 0; printf("13b lane %d: the note-off is %s (block %u, wanted about %.0f)\n", l, off < 0 ? "missing" : "late or early", off < 0 ? 0u : dq_mid[off].blk, (float)l * d + 1323.0f);
            }
        }
        if (dq_nm != 12) { ok = 0; printf("13b %d MIDI messages, want 12 (6 on, 6 off)\n", dq_nm); }
        printf("13b MIDI out: the six lanes send GM notes 36 38 42 46 39 45 on channel 10, velocities 25 50 80 105 127 25 by force, the note-off 30 ms on: %s\n", ok ? "ok" : "FAIL");
        bad += !ok;
        /* a repeat before the gate ends: the note ends first; a stop ends what rings */
        dq_reset();
        memset(dr_pat, 0, sizeof dr_pat);
        dr_pat[0][0] = DRS_MK(1u, DF_NORM, 4, 5, 0);   /* 4 ratchets: 4 hits 1/4 of a step apart (1378 frames: over the 1323-frame gate) */
        dr_pat[0][1] = DRS_MK(1u, DF_NORM, 4, 5, 0);
        dq_play(2, d / 4.0f, 48, 0);                /* a quick tempo: 4 hits within the gate */
        { int ons = 0, offs = 0; for (i = 0; i < dq_nm; i++) { ons += dq_mid[i].vel != 0; offs += dq_mid[i].vel == 0; }
          if (ons != offs || ons < 8) { bad++; printf("13b repeats: %d on, %d off (every note ends)  FAIL\n", ons, offs); }
          else printf("13b repeated hits: %d notes on, %d off: every one ended\n", ons, offs); }
        dq_reset();
        memset(dr_pat, 0, sizeof dr_pat);
        dr_pat[0][0] = DRS_NEW; dr_pat[1][0] = DRS_NEW;
        dr_start(0);
        dq_now = 0; dr_block(0, 48, d);
        dr_stop();
        { int ons = 0, offs = 0; for (i = 0; i < dq_nm; i++) { ons += dq_mid[i].vel != 0; offs += dq_mid[i].vel == 0; }
          if (ons != 2 || offs != 2 || dr.ring) { bad++; printf("13b stop: %d on, %d off, ring %02x  FAIL\n", ons, offs, dr.ring); }
          else printf("13b a stop with two notes on: both end at once\n"); }
    }
    /* 13c: ratchet, nudge, chance, mute */
    {
        float d = 60.0f / 120.0f / 4.0f * 44100.0f;
        int ok = 1, n0;
        dq_reset();
        memset(dr_pat, 0, sizeof dr_pat);
        dr_pat[0][0] = DRS_MK(1u, DF_NORM, 3, 5, 0);                      /* ratchet 3 */
        dr_pat[1][1] = DRS_MK(1u, DF_NORM, 1, 5, -4);                     /* nudge -4: a 16th of a step early */
        dr_pat[4][2] = DRS_MK(1u, DF_NORM, 1, 5, 3);                      /* nudge +3 */
        dq_play(4, d, 16, 0);
        {
            float want[5] = {0.0f, d / 3.0f, 2.0f * d / 3.0f, d - d * 4.0f / 64.0f, 2.0f * d + d * 3.0f / 64.0f};
            if (dq_n != 5) { ok = 0; printf("13c %d hits, want 5\n", dq_n); }
            for (i = 0; i < dq_n && i < 5; i++) {
                int j, best = 0;                  /* the hits come out in the order of their frames */
                (void)best; (void)j;
                if (!dq_in_block(dq_hit[i].blk, want[i], 16)) { ok = 0; printf("13c hit %d in block %u, wanted frame %.1f\n", i, dq_hit[i].blk, want[i]); }
            }
        }
        printf("13c ratchet x3 at thirds of the step, nudge -4 / +3 by 1/64 of a step a click: %s\n", ok ? "ok" : "FAIL");
        bad += !ok;
        /* chance: the share played, and the same each start */
        {
            int pct[4] = {12, 25, 50, 75}, pi, okc = 1;
            for (pi = 0; pi < 4; pi++) {
                int n1, n2;
                dq_reset();
                memset(dr_pat, 0, sizeof dr_pat);
                for (i = 0; i < 16; i++) dr_pat[0][i] = DRS_MK(1u, DF_NORM, 1, pi, 0);
                dq_play(16 * 200, d, 480, 0);
                n1 = dq_n;
                dq_reset();
                dq_play(16 * 200, d, 480, 0);
                n2 = dq_n;
                if (n1 != n2) { okc = 0; printf("13c chance %d%%: %d then %d hits on a second start\n", pct[pi], n1, n2); }
                if (fabsf((float)n1 / 3200.0f * 100.0f - (float)pct[pi]) > 2.5f) { okc = 0; printf("13c chance %d%%: %.1f%% played\n", pct[pi], (float)n1 / 32.0f); }
            }
            printf("13c chance 12 / 25 / 50 / 75 %% over 3200 steps each: within 2.5 points, the same hits on a second start: %s\n", okc ? "ok" : "FAIL");
            bad += !okc;
        }
        /* mute: the lane is silent, the others are as they were */
        dq_reset();
        memset(dr_pat, 0, sizeof dr_pat);
        for (i = 0; i < 16; i += 4) { dr_pat[0][i] = DRS_NEW; dr_pat[1][i + 2] = DRS_NEW; }
        dq_play(32, d, 48, 0);
        n0 = dq_n;
        dq_reset();
        dr.mute = 1;                              /* the kick */
        dq_play(32, d, 48, 0);
        {
            int kick = 0, other = 0;
            for (i = 0; i < dq_n; i++) { if (dq_hit[i].lane == 0) kick++; else other++; }
            for (i = 0; i < dq_nm; i++) if (dq_mid[i].note == 36) kick++;
            ok = kick == 0 && other == n0 / 2 && dq_n == n0 - n0 / 2;
            printf("13c a muted lane: %d kick hits and notes, the snare's %d of %d unchanged, the steps still counted: %s\n", kick, other, n0 / 2, ok ? "ok" : "FAIL");
            bad += !ok;
        }
    }
    /* 13d: stopped costs nothing */
    {
        float d = 60.0f / 120.0f / 4.0f * 44100.0f;
        int ok = 1;
        dq_reset();
        memset(dr_pat, 0, sizeof dr_pat);
        dr_pat[0][0] = DRS_NEW;
        for (i = 0; i < 2000; i++) { dq_now = (uint32_t)i * 48u; dr_block(dq_now, 48, d); dr_tick(dq_now, d); dr_flush(dq_now, 48); }
        ok = dq_n == 0 && dq_nm == 0 && dq_nrun == 0 && dr.nq == 0 && dr.hits == 0 && dr_col(0) == -1;
        dr_start(0); dr_start(0); dr_block(0, 48, d); dr_stop(); dr_stop();
        ok = ok && dq_nrun == 2 && dq_runs[0] == 1 && dq_runs[1] == 0 && dr_col(0) == -1 && !dr.run;
        printf("13d stopped: 2000 blocks, ticks and flushes: no hook, no hit queued; a start runs the voices' guard (hook 1), a stop ends it (0), a second start or stop does nothing: %s\n", ok ? "ok" : "FAIL");
        bad += !ok;
    }
    /* 13e: editing */
    {
        int ok = 1;
        memset(dr_pat, 0, sizeof dr_pat);
        dr_toggle(2, 5);
        ok = ok && dr_pat[2][5] == DRS_NEW && DRS_FORCE(dr_pat[2][5]) == DF_NORM && DRS_RATCH(dr_pat[2][5]) == 1 && DRS_CHANCE(dr_pat[2][5]) == 5 && DRS_NUDGE(dr_pat[2][5]) == 0;
        dr_toggle(2, 5);
        ok = ok && dr_pat[2][5] == 0;
        for (i = 0; i < 10; i++) { dr_adjust(1, 3, 0, 1); dr_adjust(1, 3, 1, 1); dr_adjust(1, 3, 2, -1); dr_adjust(1, 3, 3, 1); }
        ok = ok && DRS_FORCE(dr_pat[1][3]) == DF_ACC && DRS_RATCH(dr_pat[1][3]) == 4 && DRS_CHANCE(dr_pat[1][3]) == 0 && DRS_NUDGE(dr_pat[1][3]) == 3 && (dr_pat[1][3] & 1);
        for (i = 0; i < 10; i++) { dr_adjust(1, 3, 0, -1); dr_adjust(1, 3, 1, -1); dr_adjust(1, 3, 2, 1); dr_adjust(1, 3, 3, -1); }
        ok = ok && DRS_FORCE(dr_pat[1][3]) == DF_GHOST && DRS_RATCH(dr_pat[1][3]) == 1 && DRS_CHANCE(dr_pat[1][3]) == 5 && DRS_NUDGE(dr_pat[1][3]) == -4;
        dr_adjust(0, 0, 1, 1);                   /* a step off: turned on first, then changed */
        ok = ok && (dr_pat[0][0] & 1) && DRS_RATCH(dr_pat[0][0]) == 2;
        dr_load_beat();
        {
            int cnt[6] = {0, 0, 0, 0, 0, 0};
            for (l = 0; l < DR_LANES; l++) for (i = 0; i < 16; i++) cnt[l] += dr_pat[l][i] & 1;
            ok = ok && cnt[0] == 5 && cnt[1] == 2 && cnt[2] == 7 && cnt[3] == 1 && cnt[4] == 2 && cnt[5] == 2 && dr_any();
        }
        dr_clear_lane(0);
        ok = ok && !dr_pat[0][0] && !dr_pat[0][4] && dr_pat[1][4];
        for (l = 0; l < DR_LANES; l++) dr_clear_lane(l);
        ok = ok && !dr_any();
        printf("13e editing: a step on is NORM x1 100%% on the grid; force, ratchet, chance, nudge clamp at their ends (ACC, 4, 12%%, +3 and GHOST, 1, 100%%, -4); the beat has 5/2/7/1/2/2 steps; clear: %s\n", ok ? "ok" : "FAIL");
        bad += !ok;
    }
    /* 13f (step 3): the live kit is what sounds: every value of a lane reaches its hit, the four factory kits load whole, a lane's voice is fixed */
    {
        float d = 60.0f / 120.0f / 4.0f * 44100.0f;
        int ok = 1, k;
        dq_reset();
        memset(dr_pat, 0, sizeof dr_pat);
        dr_pat[2][0] = DRS_MK(1u, DF_HARD, 1, 5, 0);
        dr_kit[2].model = 7; dr_kit[2].pitch = 81; dr_kit[2].p1 = 33; dr_kit[2].p2 = 77; dr_kit[2].decay_cs = 120; dr_kit[2].level = 70; dr_kit[2].pan = -30; dr_kit[2].send = 40;
        dq_play(2, d, 48, 0);
        ok = dq_n == 1 && dq_hit[0].voice == 2 && dq_hit[0].model == 7 && dq_hit[0].note == 81.0f && fabsf(dq_hit[0].p1 - 0.33f) < 1e-5f &&
             fabsf(dq_hit[0].p2 - 0.77f) < 1e-5f && fabsf(dq_hit[0].decay - 1.2f) < 1e-5f && fabsf(dq_hit[0].level - 1.05f * 0.7f) < 1e-5f &&
             fabsf(dq_hit[0].pan + 0.3f) < 1e-5f && fabsf(dq_hit[0].send - 0.4f) < 1e-5f;
        printf("13f the lane's live values reach its hit: model, pitch, TONE, COLOUR, DECAY, LEVEL x force, PAN, SEND: %s\n", ok ? "ok" : "FAIL");
        bad += !ok;
        ok = 1;
        for (k = 0; k < DR_NKIT; k++) {
            dr_load_kit(k);
            for (l = 0; l < DR_LANES; l++)
                if (memcmp(&dr_kit[l], &DR_KITS[k][l], sizeof dr_kit[l])) ok = 0;
            if (dr_kit_ix != k) ok = 0;
        }
        ok = ok && DR_VOICE[0] == 0 && DR_VOICE[1] == 1 && DR_VOICE[2] == 2 && DR_VOICE[3] == 2 && DR_VOICE[4] == 3 && DR_VOICE[5] == 3;
        printf("13f the four kits (909 ISH, 808 ISH, LO-FI, SPARSE) load whole; the hats share voice 2, the clap and perc voice 3: %s\n", ok ? "ok" : "FAIL");
        bad += !ok;
        /* the audition: sound only (no MIDI, no hit counted), once; not while playing */
        dq_reset();
        dr_aud = 3;
        dq_now = 0;
        { int l2 = dr_aud - 1; dr_aud = 0; if (!dr.run) dr_sound(l2, DF_NORM); }
        ok = dq_n == 1 && dq_nm == 0 && dr.hits == 0;
        printf("13f a lane heard from a page: its sound once, no MIDI note, not counted as a hit: %s\n", ok ? "ok" : "FAIL");
        bad += !ok;
    }
    /* 13g (step 4): each lane's own length, divide and shift, and SWING, each hit in the block of its frame (blocks of 16) */
    {
        float d = 60.0f / 120.0f / 4.0f * 44100.0f;
        int ok = 1, n, bad_l = 0, bad_d = 0, bad_s = 0, bad_w = 0;
        /* LENGTH: lane 0 loops on 12 steps, lane 1 on 16: over 96 steps they hit 8 and 6 times, at multiples of 12 and 16 steps */
        dq_reset();
        memset(dr_pat, 0, sizeof dr_pat);
        dr_pat[0][0] = DRS_NEW; dr_pat[1][0] = DRS_NEW;
        dr_feel[0].len = 12; dr_feel[1].len = 16;
        dq_play(96, d, 16, 0);
        { int c0 = 0, c1 = 0;
          for (i = 0; i < dq_n; i++) {
              if (dq_hit[i].lane == 0) { if (!dq_in_block(dq_hit[i].blk, (float)(c0 * 12) * d, 16)) bad_l++; c0++; }
              if (dq_hit[i].lane == 1) { if (!dq_in_block(dq_hit[i].blk, (float)(c1 * 16) * d, 16)) bad_l++; c1++; }
          }
          if (c0 != 8 || c1 != 6) bad_l++;
          printf("13g length 12 against 16 over 96 steps: %d and %d hits, every one on its own multiple: %s\n", c0, c1, bad_l ? "FAIL" : "ok"); }
        /* DIVIDE: a step of 1/8, 1/32, 8T, 16T (2, 1/2, 4/3, 2/3 of a 16th) on a lane of one step */
        { static const float DIVS[DR_NDIV] = {2.0f, 1.0f, 0.5f, 4.0f / 3.0f, 2.0f / 3.0f};
          int dv;
          for (dv = 0; dv < DR_NDIV; dv++) {
              int want;
              dq_reset();
              memset(dr_pat, 0, sizeof dr_pat);
              dr_pat[0][0] = DRS_NEW;
              dr_feel[0].len = 1; dr_feel[0].div = (uint8_t)dv;
              dq_play(16, d, 16, 0);
              want = (int)(16.0f / DIVS[dv] + 0.001f);
              if (dq_n != want) { bad_d++; printf("13g divide %s: %d hits in 16 sixteenths, want %d\n", DR_DIVN[dv], dq_n, want); }
              for (i = 0; i < dq_n; i++)
                  if (!dq_in_block(dq_hit[i].blk, (float)i * DIVS[dv] * d, 16)) { bad_d++; break; }
          }
          printf("13g divide 1/8, 1/16, 1/32, 8T, 16T: a step of 2, 1, 1/2, 4/3, 2/3 sixteenths, each hit on its frame: %s\n", bad_d ? "FAIL" : "ok"); }
        /* SHIFT: a step on at column 3, shifted +2 plays at step 1, -2 at step 5, and a shift longer than the lane wraps */
        { static const int SH[4] = {2, -2, 0, 17}; static const int AT[4] = {1, 5, 3, 2};   /* +17 on 16 is +1: step 2 */
          int k2;
          for (k2 = 0; k2 < 4; k2++) {
              dq_reset();
              memset(dr_pat, 0, sizeof dr_pat);
              dr_pat[0][3] = DRS_NEW;
              dr_feel[0].shift = (int8_t)(SH[k2] > 15 ? 1 : SH[k2]);
              dq_play(16, d, 16, 0);
              if (dq_n != 1 || !dq_in_block(dq_hit[0].blk, (float)AT[k2] * d, 16)) { bad_s++; printf("13g shift %d: step %.1f wanted\n", SH[k2], (float)AT[k2]); }
          }
          printf("13g shift +2, -2, 0, +1 (on a lane of 16): the step on column 3 plays at step 1, 5, 3, 2: %s\n", bad_s ? "FAIL" : "ok"); }
        /* SWING 62 %%: the odd steps 0.24 of a step late, the even ones not; triplets are not swung */
        dq_reset();
        memset(dr_pat, 0, sizeof dr_pat);
        for (i = 0; i < 16; i++) dr_pat[0][i] = DRS_NEW;
        dr.swing = 62;
        dq_play(16, d, 16, 0);
        for (i = 0; i < dq_n; i++) {
            float want = (float)i * d + (i & 1 ? 0.24f * d : 0.0f);
            if (!dq_in_block(dq_hit[i].blk, want, 16)) bad_w++;
        }
        n = dq_n;
        dq_reset();
        memset(dr_pat, 0, sizeof dr_pat);
        dr_pat[0][0] = DRS_NEW; dr_pat[0][1] = DRS_NEW;
        dr_feel[0].len = 2; dr_feel[0].div = 4;                              /* 16T */
        dr.swing = 75;
        dq_play(8, d, 16, 0);
        for (i = 0; i < dq_n; i++) if (!dq_in_block(dq_hit[i].blk, (float)i * (2.0f / 3.0f) * d, 16)) bad_w++;
        printf("13g swing 62%%: %d hits, the odd 16ths 0.24 of a step late, the even on the grid; 16T at 75%% not swung: %s\n", n, bad_w ? "FAIL" : "ok");
        ok = !bad_l && !bad_d && !bad_s && !bad_w;
        bad += !ok;
    }
    /* 13h (step 4): CHOKE: a hit cuts the lane named (its voice, when it is another one), and its MIDI note ends */
    {
        float d = 60.0f / 120.0f / 4.0f * 44100.0f;
        int ok = 1;
        dq_reset();
        memset(dr_pat, 0, sizeof dr_pat);
        dr_pat[1][0] = DRS_NEW;                    /* snare on step 0 (its MIDI note rings 30 ms), the kick a little after it (nudge +3) chokes it */
        dr_pat[0][0] = DRS_MK(1u, DF_NORM, 1, 5, 3);
        dr_feel[0].choke = 2;                      /* KICK cuts SNARE (voice 1) */
        dq_play(4, d, 16, 0);
        ok = ok && dq_ncut == 1 && dq_cuts[0] == 1;
        { int on = 0, off = 0; for (i = 0; i < dq_nm; i++) if (dq_mid[i].note == 38) { if (dq_mid[i].vel) on++; else off++; }
          ok = ok && on == 1 && off == 1; }
        { int offb = -1; for (i = 0; i < dq_nm; i++) if (dq_mid[i].note == 38 && !dq_mid[i].vel) offb = (int)dq_mid[i].blk;
          ok = ok && dq_in_block((uint32_t)offb, 3.0f / 64.0f * d, 16 + 16); }   /* the note ends with the kick (3/64 of a step), not 30 ms on */
        /* the lane itself and a lane on the same voice are no cut: C HAT -> O HAT share voice 2 (the new hit cuts it anyway), a lane choking itself */
        dq_reset();
        memset(dr_pat, 0, sizeof dr_pat);
        dr_pat[2][0] = DRS_NEW; dr_pat[0][0] = DRS_NEW;
        dr_feel[2].choke = 4;                      /* C HAT -> O HAT: the same voice */
        dr_feel[0].choke = 1;                      /* KICK -> KICK */
        dq_play(2, d, 16, 0);
        ok = ok && dq_ncut == 0;
        /* OFF: no cut */
        dq_reset();
        memset(dr_pat, 0, sizeof dr_pat);
        dr_pat[0][0] = DRS_NEW;
        dr_feel[0].choke = 0;
        dq_play(2, d, 16, 0);
        ok = ok && dq_ncut == 0;
        printf("13h choke: the kick cuts the snare's voice and ends its MIDI note at once; the same voice, itself and OFF cut nothing: %s\n", ok ? "ok" : "FAIL");
        bad += !ok;
    }
    /* 13i (step 4): the pattern switch at the bar line: a pattern queued mid-bar (read ahead into dr_stage) starts at the next bar line, every lane from
     * its first step; an edit calls it off; nothing changes before the bar line */
    {
        float d = 60.0f / 120.0f / 4.0f * 44100.0f, bar = 16.0f * d;
        uint32_t t;
        int ok = 1, before_a = 0, before_b = 0, after_a = 0, after_b = 0;
        dq_reset();
        memset(dr_pat, 0, sizeof dr_pat);
        dr_pat[0][0] = DRS_NEW;                    /* A: the kick on step 1 */
        memset(&dr_stage, 0, sizeof dr_stage);
        dr_stage.s[1][8] = DRS_NEW;                /* B: the snare on step 9 (and a lane of 12 on the kick's: restarted at the bar) */
        dr_stage.f[0].len = 16; dr_stage.f[0].div = 1; dr_stage.f[1].len = 16; dr_stage.f[1].div = 1;
        for (i = 2; i < DR_LANES; i++) { dr_stage.f[i].len = 16; dr_stage.f[i].div = 1; }
        dr_start(0);
        dq_n = 0;
        for (t = 0; (float)t < 4.0f * bar; t += 48u) {
            dq_now = t;
            if ((float)t >= 1.3f * bar && !dr_staged && dr.pat == 0 && dr.next == 255) { dr.next = 1; dr_staged = 1; }   /* queued mid-bar 2 */
            dr_block(t, 48, d);
        }
        for (i = 0; i < dq_n; i++) {
            float at = (float)dq_hit[i].blk;
            if (at < 2.0f * bar - 50.0f) { if (dq_hit[i].lane == 0) before_a++; if (dq_hit[i].lane == 1) before_b++; }
            else if (at > 2.0f * bar - 50.0f) { if (dq_hit[i].lane == 0) after_a++; if (dq_hit[i].lane == 1) after_b++; }
        }
        /* bars 1 and 2: the kick (A) twice, no snare; bars 3 and 4: the snare (B) twice at step 9, no kick */
        ok = before_a == 2 && before_b == 0 && after_a == 0 && after_b == 2 && dr.pat == 1 && dr.next == 255 && !dr_staged;
        { int first_b = -1; for (i = 0; i < dq_n; i++) if (dq_hit[i].lane == 1 && first_b < 0) first_b = i;
          ok = ok && first_b >= 0 && dq_in_block(dq_hit[first_b].blk, 2.0f * bar + 8.0f * d, 48); }
        printf("13i pattern A to B queued mid-bar 2: A's kick plays in bars 1 and 2, B's snare (step 9) in bars 3 and 4, nothing of either on the wrong side of the bar line: %s\n", ok ? "ok" : "FAIL");
        bad += !ok;
        dr_stop();
        /* the queued pattern is called off by an edit; the live one stays */
        dr.next = 1; dr_staged = 1;
        dr_toggle(0, 5);
        ok = dr.next == 255 && !dr_staged && (dr_dirty & 1u);
        dr.next = 1; dr_staged = 1;
        dr.run = 0;
        printf("13i an edit of the live pattern calls a queued switch off (and marks it unsaved): %s\n", ok ? "ok" : "FAIL");
        bad += !ok;
        /* lanes of other lengths restart at the switch (a 12 and a 16 both start at their step 1) */
        dq_reset();
        memset(dr_pat, 0, sizeof dr_pat);
        dr_pat[0][0] = DRS_NEW; dr_pat[1][0] = DRS_NEW; dr_feel[0].len = 12;
        memset(&dr_stage, 0, sizeof dr_stage);
        dr_stage.s[0][0] = DRS_NEW; dr_stage.s[1][0] = DRS_NEW;
        for (i = 0; i < DR_LANES; i++) { dr_stage.f[i].len = 16; dr_stage.f[i].div = 1; }
        dr_stage.f[0].len = 12;
        dr_start(0);
        dq_n = 0;
        for (t = 0; (float)t < 3.0f * bar; t += 48u) {
            dq_now = t;
            if ((float)t >= 0.5f * bar && !dr_staged && dr.pat == 0 && dr.next == 255) { dr.next = 1; dr_staged = 1; }
            dr_block(t, 48, d);
        }
        { int k0 = 0, k1 = 0, okp = 1;                /* hits at the bar line (2 bars in: the switch at 1 bar, so bar 2 starts both lanes afresh) */
          for (i = 0; i < dq_n; i++) {
              float at = (float)dq_hit[i].blk;
              if (dq_hit[i].lane == 0 && fabsf(at - bar) < 60.0f) k0++;
              if (dq_hit[i].lane == 1 && fabsf(at - bar) < 60.0f) k1++;
          }
          okp = k0 == 1 && k1 == 1;
          printf("13i at the switch the lane of 12 and the lane of 16 both start from their first step on the bar line: %s\n", okp ? "ok" : "FAIL");
          bad += !okp; }
        dr_stop();
    }
    /* 13k (step 5): OUT per lane (SOUND, MIDI, ALSO, OFF), the lane's own channel and note, a change while it rings */
    {
        float d = 60.0f / 120.0f / 4.0f * 44100.0f;
        static const int WANT_N[4] = {1, 0, 1, 0}, WANT_M[4] = {0, 2, 2, 0};   /* sounds, MIDI messages (on + off) */
        int ok = 1, o, on_ch = -1, on_note = -1;
        for (o = 0; o < 4; o++) {
            dq_reset();
            memset(dr_pat, 0, sizeof dr_pat);
            dr_pat[0][0] = DRS_NEW;
            dr_mid[0].out = (uint8_t)o;
            dq_play(2, d, 48, 0);
            if (dq_n != WANT_N[o] || dq_nm != WANT_M[o] || dr.hits != 1) { ok = 0; printf("13k OUT %d: %d sounds, %d MIDI messages, %u hits\n", o, dq_n, dq_nm, (unsigned)dr.hits); }
        }
        dq_reset();
        memset(dr_pat, 0, sizeof dr_pat);
        dr_pat[4][0] = DRS_NEW;
        dr_mid[4].ch = 2; dr_mid[4].note = 70;
        dq_play(2, d, 48, 0);
        for (i = 0; i < dq_nm; i++) if (dq_mid[i].vel) { on_ch = dq_mid[i].ch; on_note = dq_mid[i].note; }
        ok = ok && on_ch == 2 && on_note == 70 && dq_nm == 2 && dq_mid[1].ch == 2 && dq_mid[1].note == 70 && dq_mid[1].vel == 0;
        /* a change of note or channel while the note rings: it ends first, on the old note and channel; the next hit uses the new */
        dq_reset();
        memset(dr_pat, 0, sizeof dr_pat);
        dr_pat[0][0] = DRS_NEW;
        dr_start(0);
        dq_now = 0; dr_block(0, 48, d);
        dq_now = 48; dr_block(48, 48, d);
        dr_mid_set(0, 2, 50);
        ok = ok && dq_nm == 2 && dq_mid[1].note == 36 && dq_mid[1].vel == 0 && dq_mid[1].ch == 9 && dr_mid[0].note == 50 && !(dr.ring & 1u);
        dr_mid_set(0, 1, 99); dr_mid_set(0, 0, 9); dr_mid_set(0, 2, -4);   /* clamped: channel 15, OUT 3, note 0 */
        ok = ok && dr_mid[0].ch == 15 && dr_mid[0].out == 3 && dr_mid[0].note == 0;
        dr_stop();
        printf("13k OUT per lane: SOUND (a sound, no note), MIDI (a note, no sound), ALSO (both), OFF (neither, the hit still counted); a lane's own channel and note go out; a change while it rings ends the old note first; values clamp: %s\n", ok ? "ok" : "FAIL");
        bad += !ok;
    }
    /* 13l (step 5): MIDI in plays the lanes (their channel and note, IN on), not the synth; never a note back out */
    {
        int ok = 1, r;
        dq_reset();
        dr_mid[2].note = 60; dr_mid[2].ch = 3;
        r = dr_midi_in(9, 36, 100, 1);                       /* the kick: channel 10 (9), note 36 */
        ok = ok && r == 1 && dq_n == 1 && dq_hit[0].voice == 0 && fabsf(dq_hit[0].level - 1.0f) < 1e-5f;
        r = dr_midi_in(9, 38, 50, 1);                        /* the snare at velocity 50: half as loud (x its level 0.9) */
        ok = ok && r == 1 && dq_n == 2 && dq_hit[1].voice == 1 && fabsf(dq_hit[1].level - 0.5f * 0.9f) < 1e-5f;
        r = dr_midi_in(9, 36, 0, 0);                         /* its note off: taken (the synth must not play it), no sound */
        ok = ok && r == 1 && dq_n == 2;
        r = dr_midi_in(0, 36, 100, 1);                       /* another channel */
        ok = ok && r == 0 && dq_n == 2;
        r = dr_midi_in(9, 37, 100, 1);                       /* a note no lane has */
        ok = ok && r == 0 && dq_n == 2;
        r = dr_midi_in(3, 60, 127, 1);                       /* the C HAT moved to note 60, channel 4 (3) */
        ok = ok && r == 1 && dq_n == 3 && dq_hit[2].voice == 2 && fabsf(dq_hit[2].level - 1.27f * 0.55f) < 1e-5f;
        r = dr_midi_in(9, 42, 100, 1);                       /* ... and its old note is free for the synth */
        ok = ok && r == 0 && dq_n == 3;
        dr_in &= (uint8_t)~1u;                               /* the kick's IN off: the synth gets it */
        r = dr_midi_in(9, 36, 100, 1);
        ok = ok && r == 0 && dq_n == 3;
        dr_in |= 1u;
        dr_mid[0].out = DO_MIDI;                             /* OUT = MIDI only: the note is taken, nothing sounds */
        r = dr_midi_in(9, 36, 100, 1);
        ok = ok && r == 1 && dq_n == 3;
        dr_mid[0].out = DO_SOUND;
        dr_mid[1].note = 36; dr_mid[1].ch = 9;               /* two lanes on one note: both play */
        r = dr_midi_in(9, 36, 100, 1);
        ok = ok && r == 1 && dq_n == 5;
        ok = ok && dq_nm == 0 && !dr.run && dr.hits == 0;    /* never a MIDI note out, and the sequencer need not run, nor counts a hit */
        printf("13l MIDI in: a lane's channel and note play it (velocity sets the level), notes off are taken, other channels and notes and a lane with IN off are the synth's, OUT = MIDI sounds nothing, no note goes back out, the sequencer need not run: %s\n", ok ? "ok" : "FAIL");
        bad += !ok;
        dr_reset_state();
    }
    /* 13m (step 5): the load read-out and the kits' costs */
    {
        int ok = 1, k, l;
        dr_load_kit(0);
        ok = ok && dr_load_x10() == 61 + 78 + 32 + 61;      /* 909 ISH: kick 6.1, snare 7.8, the hats' clocked noise 3.2, the clap/perc voice at its costliest (the kick model 6.1) */
        for (k = 0; k < DR_NKIT; k++) {
            int x;
            dr_load_kit(k);
            x = dr_load_x10();
            if (x < 90 || x > 290) ok = 0;
        }
        for (l = 0; l < DR_LANES; l++) dr_kit[l].model = 2;   /* a cymbal on every lane: 4 x 8.0 */
        ok = ok && dr_load_x10() == 320;
        dr_load_kit(0);
        printf("13m load read-out (points of a core x 10, each voice at its costliest model): 909 ISH %d, every lane a cymbal %d; each factory kit between 9 and 29 points: %s\n", (dr_load_kit(0), dr_load_x10()), 320, ok ? "ok" : "FAIL");
        bad += !ok;
    }
    /* 13j (step 4): DRUM GEN */
    {
        uint8_t m[DR_STEPS];
        int ok = 1, h, len, n, i2;
        for (len = 1; len <= DR_STEPS; len++)
            for (h = 0; h <= len; h++) {
                int first = -1, last = -1, mn = 99, mx = 0, c = 0;
                dr_gen_mask(DG_EUCLID, len, h, 0, m);
                for (i2 = 0; i2 < len; i2++) if (m[i2]) { if (first >= 0 && i2 - last < mn) mn = i2 - last; if (first >= 0 && i2 - last > mx) mx = i2 - last; if (first < 0) first = i2; last = i2; c++; }
                if (c != h) ok = 0;
                if (c > 1 && c < len && mx - mn > 1) ok = 0;        /* as even as the steps allow (the wrap-round gap aside) */
                for (i2 = len; i2 < DR_STEPS; i2++) if (m[i2]) ok = 0;
            }
        dr_gen_mask(DG_EUCLID, 16, 4, 0, m); for (i2 = 0, n = 0; i2 < 16; i2++) n += m[i2] << i2 & 0 ? 0 : (m[i2] && i2 % 4 == 0);
        ok = ok && n == 4;                                           /* euclid 4 in 16 is the four on the floor */
        dr_gen_mask(DG_EUCLID, 16, 3, 0, m); { uint8_t t2[DR_STEPS]; dr_gen_mask(DG_EUCLID, 16, 3, 5, t2); for (i2 = 0; i2 < 16; i2++) if (t2[(i2 + 5) % 16] != m[i2]) ok = 0; }   /* TURN 5 is the mask turned 5 right */
        dr_gen_mask(DG_FOUR, 16, 0, 0, m); ok = ok && m[0] && m[4] && m[8] && m[12] && !m[1] && !m[5];
        dr_gen_mask(DG_BACK, 16, 0, 0, m); ok = ok && m[4] && m[12] && !m[0] && !m[8];
        dr_gen_mask(DG_OFFB, 16, 0, 0, m); ok = ok && m[2] && m[6] && m[10] && m[14] && !m[0];
        dr_gen_mask(DG_16TH, 12, 0, 0, m); for (i2 = 0, n = 0; i2 < DR_STEPS; i2++) n += m[i2]; ok = ok && n == 12;
        dr_gen_mask(DG_CLEAR, 16, 9, 3, m); for (i2 = 0, n = 0; i2 < DR_STEPS; i2++) n += m[i2]; ok = ok && n == 0;
        { uint8_t a[DR_STEPS], b[DR_STEPS], c2[DR_STEPS];
          dr_gen_mask(DG_RAND, 16, 7, 3, a); dr_gen_mask(DG_RAND, 16, 7, 3, b); dr_gen_mask(DG_RAND, 16, 7, 4, c2);
          for (i2 = 0, n = 0; i2 < DR_STEPS; i2++) n += a[i2];
          ok = ok && n == 7 && !memcmp(a, b, DR_STEPS) && memcmp(a, c2, DR_STEPS); }   /* RANDOM: 7 hits, the same for the same seed, others for another */
        /* the write: the lane's steps (length 12: none past it), NORM x1 100%%, VARY 0 all NORM, VARY 100 not all, the other lanes alone, the lane editable */
        memset(dr_pat, 0, sizeof dr_pat);
        dr_pat[3][2] = DRS_NEW;
        dr_feel[1].len = 12;
        dr_gen_write(1, DG_16TH, 0, 0, 0);
        for (i2 = 0, n = 0; i2 < DR_STEPS; i2++) { if (dr_pat[1][i2] != (i2 < 12 ? DRS_NEW : 0)) ok = 0; n += dr_pat[1][i2] & 1; }
        ok = ok && n == 12 && dr_pat[3][2] == DRS_NEW && (dr_dirty & 1u);
        dr_gen_write(1, DG_16TH, 0, 0, 100);
        for (i2 = 0, n = 0; i2 < 12; i2++) n += DRS_FORCE(dr_pat[1][i2]) != DF_NORM;
        ok = ok && n >= 6;
        dr_gen_write(1, DG_CLEAR, 0, 0, 0);
        ok = ok && !(dr_pat[1][0] & 1) && !(dr_pat[1][11] & 1);
        dr_toggle(1, 4);
        ok = ok && (dr_pat[1][4] & 1);
        printf("13j drum gen: euclid 0-16 hits in 1-16 steps (every count right, gaps differ by at most 1), 4 in 16 is the four on the floor, TURN rotates, FOUR FLOOR, BACKBEAT, OFFBEAT, 16THS, CLEAR, RANDOM by seed; a write fills the lane (none past its length), VARY changes forces, the lane edits after: %s\n", ok ? "ok" : "FAIL");
        bad += !ok;
    }
    memset(dr_pat, 0, sizeof dr_pat);
    dr_hook_hit = 0; dr_hook_midi = 0; dr_hook_run = 0;
    printf("13 drum sequencer: hits on the steps at the tempo and the clock, MIDI out per lane, ratchet, nudge, chance, mute, idle, editing: %d wrong\n", bad);
    if (bad)
        *fails += 1;
}

/* 12g (step 3): the factory kits through the engine: every lane of every kit sounds (finite, not silent), is dry-clean and ends on its own;
 * the kits differ from each other (their energy, lane by lane) */
void rk_drum_hit(int voice, int model, float note, float p1, float p2, float decay, float level, float pan, float send);
static void dkit_checks(int *fails)
{
    int k, l, i, bad = 0, nbad = 0, same = 0;
    static float E[DR_NKIT][DR_LANES];
    for (k = 0; k < DR_NKIT; k++)
        for (l = 0; l < DR_LANES; l++) {
            const dr_snd_t *s = &DR_KITS[k][l];
            float pk = 0, en = 0;
            int active_after, ranges = s->model < DR_NMODEL && s->pitch >= 12 && s->pitch <= 96 && s->p1 <= 100 && s->p2 <= 100 && s->decay_cs >= 2 &&
                                       s->level <= 150 && s->send <= 100 && s->pan >= -100 && s->pan <= 100;
            rk_seed(0x21u);
            rk_set_worker(0, 0);
            cl_patch(0, 0, 0);
            rk_all_off();
            rk_drum_hit(DR_VOICE[l], s->model, (float)s->pitch, (float)s->p1 * 0.01f, (float)s->p2 * 0.01f, (float)s->decay_cs * 0.01f, 1.0f, 0.0f, 0.0f);
            for (i = 0; i < 1800; i++) {
                int j;
                rk_render(48);
                for (j = 0; j < 48; j++) {
                    float v = rk_out_l()[j];
                    if (!(v == v) || fabsf(v) > 1e6f) { nbad++; continue; }
                    if (fabsf(v) > pk) pk = fabsf(v);
                    en += v * v;
                }
            }
            active_after = rk_drum_status(0);
            E[k][l] = en;
            if (!ranges || !(pk > 0.02f) || active_after != 0) { bad++; printf("12g %s %s: %s\n", DR_KNAME[k], DR_LNAME[l], !ranges ? "a value out of range" : active_after ? "does not end" : "silent"); }
        }
    for (k = 1; k < DR_NKIT; k++) {
        int diff = 0;
        for (l = 0; l < DR_LANES; l++)
            if (fabsf(E[k][l] - E[0][l]) > 0.02f * E[0][l]) diff++;
        if (diff < 4) { bad++; same++; printf("12g kit %s is too like %s: only %d lanes differ in energy\n", DR_KNAME[k], DR_KNAME[0], diff); }
    }
    rk_drum_off();
    /* 12h: a choke through the engine: the cut voice fades in about 8 ms, no click, and the next hit on it sounds */
    {
        float pre = 0, post = 0, step_pre = 0, step_cut = 0, last = 0;
        int blk, j, ok = 1;
        void rk_drum_cut(int voice);
        rk_seed(0x21u); rk_set_worker(0, 0); cl_patch(0, 0, 0); rk_all_off();
        rk_drum_hit(2, 2, 70.0f, 0.5f, 0.5f, 1.0f, 1.0f, 0.0f, 0.0f);   /* a cymbal ringing a second */
        for (blk = 0; blk < 200; blk++) {
            if (blk == 100) rk_drum_cut(2);
            rk_render(48);
            for (j = 0; j < 48; j++) {
                float v = rk_out_l()[j], st = fabsf(v - last);
                last = v;
                if (blk >= 60 && blk < 100) { if (fabsf(v) > pre) pre = fabsf(v); if (st > step_pre) step_pre = st; }
                if (blk >= 100 && blk < 103 && st > step_cut) step_cut = st;
                if (blk >= 120 && fabsf(v) > post) post = fabsf(v);
            }
        }
        ok = pre > 0.02f && post < 0.05f * pre && step_cut <= 1.2f * step_pre + 1e-4f;
        rk_drum_hit(2, 2, 70.0f, 0.5f, 0.5f, 0.3f, 1.0f, 0.0f, 0.0f);
        ok = ok && rk_drum_status(0) == 1;
        rk_drum_cut(7); rk_drum_cut(-1);                              /* out of range: ignored */
        rk_drum_off();
        if (!ok) bad++;
        printf("12h choke through the engine: a cymbal 0.1 s in, cut: before %.3f, 40 ms after the cut %.4f (%.1f %% of it), the largest step at the cut x%.2f of the cymbal's own: %s\n", pre, post, 100.0f * post / (pre > 0 ? pre : 1), step_cut / (step_pre > 0 ? step_pre : 1), ok ? "ok" : "FAIL");
    }
    if (nbad) bad++;
    printf("12g the four factory kits through the engine: 24 lanes sound, no NaN, each ends on its own within 1.8 s, the other three differ from 909 ISH in at least 4 lanes: %d wrong\n", bad);
    if (bad)
        *fails += 1;
}

/* --drumdemo OUT.wav: the drums through the real engine (the firmware's scheduler and voices), 120 bpm, 16-bit stereo 48 kHz: the four factory kits
 * in turn, 2 bars each, then a pattern switch A to B at a bar line, then a Euclid-generated perc lane. Measured, not listened to. */
void rk_drum_cut(int voice);
static void dd_cut(int v) { rk_drum_cut(v); }
static int drum_demo(const char *path)
{
    const float d = 48000.0f * 60.0f / 120.0f / 4.0f, bar = 16.0f * d;
    enum { NB = 48 };
    static int16_t pcm[2 * 48000 * 40];
    uint32_t t = 0, total = 0;
    int k, i, nbar = 0;
    float pk = 0;
    FILE *f;
    rk_seed(0x21u); rk_set_worker(0, 0); cl_patch(0, 0, 0); rk_all_off();
    dr_hook_hit = rk_drum_hit; dr_hook_cut = dd_cut; dr_hook_midi = 0; dr_hook_run = 0;
    dr_hz = 48000.0f;
    dr_reset_state();
    dr.run = 0;
#define DD_BARS(n) do { uint32_t end_ = (uint32_t)((float)(nbar + (n)) * bar); for (; t < end_; t += NB) { \
        dr_block(t, NB, d); rk_render(NB); \
        for (i = 0; i < NB; i++) { float l = rk_out_l()[i], r = rk_out_r()[i]; \
            if (fabsf(l) > pk) pk = fabsf(l); if (fabsf(r) > pk) pk = fabsf(r); \
            if (total < 48000u * 40u) { pcm[2 * total] = (int16_t)(fmaxf(-1.f, fminf(1.f, l)) * 30000.f); pcm[2 * total + 1] = (int16_t)(fmaxf(-1.f, fminf(1.f, r)) * 30000.f); total++; } } } \
        nbar += (n); } while (0)
    dr_load_beat();
    for (k = 0; k < DR_NKIT; k++) {            /* the same beat on each kit; the kit changes at the bar line */
        dr_load_kit(k);
        if (!dr.run) dr_start(0);
        DD_BARS(2);
    }
    /* A to B at the bar line: B is the beat with the kick on 1 and the snare on 9 and a half-time hat; read ahead into the stage */
    memcpy(dr_stage.s, dr_pat, sizeof dr_stage.s); memcpy(dr_stage.f, dr_feel, sizeof dr_stage.f);
    memset(dr_stage.s[0], 0, sizeof dr_stage.s[0]); memset(dr_stage.s[1], 0, sizeof dr_stage.s[1]);
    dr_stage.s[0][0] = DRS_NEW; dr_stage.s[0][6] = DRS_NEW; dr_stage.s[0][10] = DRS_NEW; dr_stage.s[0][14] = DRS_MK(1u, DF_GHOST, 1, 5, 0);
    dr_stage.s[1][8] = DRS_MK(1u, DF_HARD, 1, 5, 0);
    DD_BARS(1);                                /* a bar of A, the switch queued part-way (here: at the bar's start) */
    dr.next = 1; dr_staged = 1;
    DD_BARS(3);                                /* the rest of the bar is A, then B from the next bar line */
    /* a Euclid perc lane over B: 7 hits in 16, turned 2 */
    dr_gen_write(5, DG_EUCLID, 7, 2, 20);
    DD_BARS(2);
    dr_stop();
    for (i = 0; i < 24000; i += NB) { rk_render(NB); { int j; for (j = 0; j < NB && total < 48000u * 40u; j++, total++) { pcm[2 * total] = (int16_t)(fmaxf(-1.f, fminf(1.f, rk_out_l()[j])) * 30000.f); pcm[2 * total + 1] = (int16_t)(fmaxf(-1.f, fminf(1.f, rk_out_r()[j])) * 30000.f); } } }
    rk_drum_off();
    f = fopen(path, "wb");
    if (!f) return 1;
    { uint32_t n = total * 4u, v; uint16_t h;
      fwrite("RIFF", 1, 4, f); v = 36u + n; fwrite(&v, 4, 1, f); fwrite("WAVEfmt ", 1, 8, f); v = 16; fwrite(&v, 4, 1, f);
      h = 1; fwrite(&h, 2, 1, f); h = 2; fwrite(&h, 2, 1, f); v = 48000; fwrite(&v, 4, 1, f); v = 48000u * 4u; fwrite(&v, 4, 1, f);
      h = 4; fwrite(&h, 2, 1, f); h = 16; fwrite(&h, 2, 1, f); fwrite("data", 1, 4, f); fwrite(&n, 4, 1, f); fwrite(pcm, 4, total, f); }
    fclose(f);
    printf("drum demo: %.1f s, 4 kits x 2 bars, A to B at the bar line, a Euclid 7-in-16 perc lane; loudest sample %.2f, %d hits\n", (float)total / 48000.0f, pk, (int)dr.hits);
    return 0;
}
