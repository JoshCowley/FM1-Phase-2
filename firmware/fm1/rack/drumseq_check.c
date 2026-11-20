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
static int dq_nm, dq_runs[8], dq_nrun;
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
    dq_n = dq_nm = dq_nrun = 0;
    dr_hook_hit = dq_hit_hook; dr_hook_midi = dq_midi_hook; dr_hook_run = dq_run_hook;
    dr_hz = 44100.0f;
    dr_load_kit(0);
    dr.run = 0; dr.mute = 0; dr.ring = 0; dr.nq = 0; dr.cur = -1; dr.hits = 0;
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
        ok = dq_n == 0 && dq_nm == 0 && dq_nrun == 0 && dr.nq == 0 && dr.hits == 0 && dr.cur == -1;
        dr_start(0); dr_start(0); dr_block(0, 48, d); dr_stop(); dr_stop();
        ok = ok && dq_nrun == 2 && dq_runs[0] == 1 && dq_runs[1] == 0 && dr.cur == -1 && !dr.run;
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
    if (nbad) bad++;
    printf("12g the four factory kits through the engine: 24 lanes sound, no NaN, each ends on its own within 1.8 s, the other three differ from 909 ISH in at least 4 lanes: %d wrong\n", bad);
    if (bad)
        *fails += 1;
}
