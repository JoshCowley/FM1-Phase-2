// SPDX-License-Identifier: GPL-3.0
// FM1 Phase 2: licensed GPL-3.0, see LICENSE; third-party code and its licences: NOTICE.md.
/* The drum sequencer in the firmware (drum track, steps 2-4): drumseq.c's hooks set to the engine's drum voices and the MIDI
 * out, and the two calls from the audio interrupt. The drums follow the sequencer's PLAY (sq.run) and a STYLE pattern playing (STYLE starts and stops
 * them, unless it is switched off on the KIT page) and nothing else: stopped, no hit is queued, the engine's drum guard is off and no block renders (rk_drum_run 0). Included by app.c after seq.c. */
#include "drumseq.c"

void rk_drum_hit(int voice, int model, float note, float p1, float p2, float decay, float level, float pan, float send);
void rk_drum_run(int on);
void rk_pump_set(float depth, float release_s, float shape, int target);
void rk_drum_cut(int voice);
void rk_drum_busy(int mode);
int rk_drum_status(int what);

static void dr_midi_out(int ch0, int note, int vel)   /* GLOBAL > MIDI OUT gates it, as the notes' */
{
    if (GI(G_MOUT))
        midi_ch_note(ch0, note, vel);
}
static void dr_hit_out(int voice, int model, float note, float p1, float p2, float decay, float level, float pan, float send)
{
    rk_drum_hit(voice, model, note, p1, p2, decay, level, pan, send);
}
static void dr_run_out(int on) { rk_drum_run(on); }
static void dr_cut_out(int voice) { rk_drum_cut(voice); }
static void dr_init(void)
{
    dr_hook_hit = dr_hit_out;
    dr_hook_midi = dr_midi_out;
    dr_hook_run = dr_run_out;
    dr_hook_cut = dr_cut_out;
    dr_hz = PL_HZ;
    dr_reset_state();
}
static float dr_step_frames(void)             /* a 16th: the MIDI clock's 6 ticks, or the tempo's */
{
    if (GI(G_CLOCK) == 1 && pa.tick_frames > 0.0f)
        return (ck.lock ? ck.per : pa.tick_frames) * 6.0f;
    return 60.0f / tempo_bpm() / 4.0f * PL_HZ;
}
/* each block in the interrupt, after sq_block (which started or stopped the sequencer: pl_now is still the block's start) */
static void dr_audio(uint32_t n)
{
    if (rc_audio.pause_req)
        return;
    {   /* the pump's settings, every block (the note-synced release follows the tempo); the kick's strike itself is in the engine's drum_hit */
        float rel = dr_pump_rel(dr_step_frames() / PL_HZ);
        rk_pump_set((float)dr_pdepth * 0.01f, rel, (float)dr_pshape * 0.01f, dr_ptgt);
    }
    if (dr_aud) {                             /* a page asked to hear a lane (it is turning the lane's sound): sound only, once */
        int l = dr_aud - 1;
        dr_aud = 0;
        if (!dr.run && l < DR_LANES)
            dr_sound(l, DF_NORM);
    }
    {
        int want = sq.run || (dr_style && st.on && st_style());   /* PLAY, or (STYLE starts the drums, on by default) a STYLE pattern playing */
        if (want && !dr.run)
            dr_start(pl_now);
        else if (!want && dr.run)
            dr_stop();
    }
    if (!dr.run)
        return;                               /* stopped: nothing queued, nothing struck, nothing rendered */
    if (GI(G_CLOCK) != 1)
        dr_block(pl_now, n, dr_step_frames());
    else
        dr_flush(pl_now, n);
}
static void dr_clock_tick(void)               /* from pl_clock_tick, after sq_clock_tick */
{
    if (dr.run && GI(G_CLOCK) == 1)
        dr_tick(pl_tick_at, dr_step_frames());
}
