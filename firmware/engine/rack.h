// SPDX-License-Identifier: GPL-3.0
// FM1 Phase 2: licensed GPL-3.0, see LICENSE; third-party code and its licences: NOTICE.md.
// FM1 Rack engine: the C interface a platform layer calls.
//
// The engine is engine/rack.cc, the same file the browser build compiles. Build it with -DRACK_FM1
// (and optionally -DRACK_ARENA_KB=<n>, default 340: the page's 384K FREE RAM setting less the engine's
// 44 KB of fixed memory) for the firmware-ready version:
//   - the big buffers (Plaits, Braids, Elements, Rings, Warps, Clouds, delay, reverb, chorus) come from one
//     static arena of RACK_ARENA_KB, laid out for the modules actually loaded;
//   - voices are limited to what fits (up to 6); modules that can't fit at all are unloaded, FX 2 first;
//   - the delay line is 1 s and the delay and chorus lines are 16-bit.
// Everything else (envelopes, LFOs, matrix, Peaks, Tides, Marbles, Grids, master) is small and static.
//
// Threading: render() runs in the audio interrupt. Every other call changes engine state and must not run
// while render() is running: queue them from the UI and apply them at the start of the audio callback
// (the browser build does the same through its AudioWorklet message queue).
#pragma once
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif

// slots: 0 SRC, 1 SHP, 2 MOD A, 3 MOD B, 4 FX 1, 5 FX 2, 6 AMP, 7 GLOBAL (param 0 = tempo), 8 LFOs
// module ids: 1 Plaits, 2 Braids, 3 Elements, 10 Rings, 11 Warps, 12 Filter, 20 Peaks, 21 Tides, 22 Marbles,
//             23 Grids, 30 Clouds, 31 Delay, 32 Reverb, 33 Chorus, 0 empty
void init(void);
void set_module(int slot, int id);
void set_param(int slot, int param, float value);
void set_param_glide(int slot, int param, float value);   // the same, reached over a few ms (a knob turned)
void set_meta(int slot, int param, float lo, float hi, float span);   // the parameter's range, for modulation
void set_route(int i, int src, int slot, int param, float amount, int via);   // 12 matrix rows, then 6 LFO targets
void set_macro(int i, float v);
void set_macro_glide(int i, float v);   // the same, reached over a few ms
void set_macro_target(int k, int idx, int slot, int param, int depth, int start, int end, int curve);   // knob k's target idx (0-7): a parameter (slot -1..8) or, slot 9, a row's depth; depth, start, end in percent; curve 0-4
void clear_macro_targets();
void set_jam_table(const uint8_t* pairs, int n);
void set_jam_offsets(const float* o);
void clear_jam();
int get_macro_target(int k, int idx, int* out);   // out[0] slot, [1] param, [2] start, [3] end, [4] curve; returns the depth (0: none)
float macro_curve(float k, int start, int end, int curve);
#ifdef TEST
float eff_param(int slot, int param);
float eff_param_nojam(int slot, int param);   // a parameter as the voices hear it (host checks only)
#endif
void set_master(float v);
void set_master_fx(float low_db, float mid_db, float high_db, float comp);
void set_modsum_precompute(int on);   // the benchmark's A/B of mod_sum's sums ahead (-DRACK_BENCH_AB only)
void set_rv_mono(int on);             // the benchmark's A/B of the reverb tank taking one sum (1) or both sides down-sampled (0: 2.35) (-DRACK_BENCH_AB only)
void set_quality(unsigned mask);       // the 2.38 quality trims as a mask (QB_* in rack.cc: 1 reverb sum, 2 short halfband, 4 lighter reverb diffusion; 0 = the 2.37 sound; -DRACK_BENCH_AB only)
unsigned quality_default(void);        // the build's own mask (-DRACK_BENCH_AB only)
void set_lim_fast(int on);            // the benchmark's A/B of the limiter's block-wise peak (1) or the falling queue (0: 2.35) (-DRACK_BENCH_AB only)
void set_rv_post(int on);              // 2.43: the reverb's wet job after its slot on the second core (1) or the first core's early work (0: 2.42); the benchmark's A/B
void set_split_learn(int on);         // 2.43: the voices' learnt split on (1) or the table's alone (0)
void set_clouds_mask(unsigned m);   // update 42: the Clouds quality bits (CLQ_* in shim/clouds/dsp/rack_cl_q.h) as a mask for the benchmark; ~0u = the page's RENDER
void set_clouds_opts(int k32, int skip_rev, int grains);   // update 8: the Clouds changes' runtime switches (the benchmark's comparison cases only)
void set_voices(int n);          // 1-6; the FM-1 build caps this at what fits
void set_legato(int on);
void set_vintage(float amount);
void set_fx_bypass(int slot, int off);   /* FM-1: an effect slot (4, 5) skipped; on again: its buffers cleared */
int fx_bypass(int slot);
void set_tune(int note, float semitones);
void set_bend(float semitones);
void set_ext(int i, float v);    // modulation lanes
void pump_set(float depth, float release_s, float shape, int target);   // update 41 step 5: the kick ducking the synth (0 depth: off)
void set_ext_slew(int i, float s);
void note_on(float note, float velocity);
void note_off(float note);
void all_off(void);
void reset_clock(void);

// audio: renders n frames (any n) into out_l() / out_r(), floats at 48 kHz
void render(int n);
float* out_l(void);
float* out_r(void);

// screen data
float* monitor(int slot);
#ifdef RACK_GR_STATS
void grain_stats(double* out, int reset);   // the host check's: CLOUDS' grain start positions (count, sum, sum of squares, least, greatest)
#endif
float voice_env(int i);
float get_param(int slot, int param);   // a parameter as set (before modulation)
float get_route(int i, int* out);       // a matrix row: out[0] source, [1] slot, [2] param, [3] via; returns the amount           // voice i's amplitude envelope, 0-1 (the screen's voice lights)        // 200 values: live parameters, envelopes, voices, LFOs, levels
float* scope(void);              // 128 samples for the scope

#ifdef RACK_FM1
// 0 arena bytes used, 1 arena size, 2 voices that fit, 3 voices playing,
// 4 unloaded slots (bit per slot: 0 SRC, 1 SHP, 4 FX 1, 5 FX 2)
int fm1_status(int what);
// after a patch has loaded: size the delay, the reverb's pre-delay and shimmer, and Rings' reverb to what the patch can
// use, and lay the arena out again (more voices; the same sound)
void fm1_fit(void);
int fm1_fit_needed(void);       // 1: the patch now reaches past what fm1_fit sized (fit again, between renders)
// the second core: run(fn, arg) hands fn(arg) to it (1) or declines (0); wait() returns 0 when done, -1 when it
// dropped the job. With a worker set, 2 or more voices are split between the cores. worker_blocks: blocks shared
void set_worker(int (*run)(void (*)(uint32_t), uint32_t), int (*wait)(void));
uint32_t worker_blocks(void);
// update 9 (RACK_FX_CORE2): sync = plat_cpu2_sync, on = plat_cpu2_on (either may be 0); ticks: a clock for the cores' own times (the benchmark);
// set_fx_core2(0): Clouds stays on the first core and the voices split, as update 8 (the benchmark's comparison cases);
// fx_core2_active: 1 when Clouds runs on the second core in this patch; core_stats: ticks the jobs took, ticks the first core waited,
// blocks Clouds' job was done on the first core instead
void set_worker_ex(void (*sync)(void), int (*on)(void));
void set_ticks(uint32_t (*ticks)(void));
void set_fx_core2(int on);
int fx_core2_active(void);
// update 10: set_fx_core2_voices(0): the voices above the first four stay on the first core (the benchmark's comparison cases); shed_helps: 1 when
// one voice fewer relieves the core that is the slower one (the overload guard asks; always 1 unless Clouds runs on the second core and a clock is set)
void set_fx_core2_voices(int on);
int shed_helps(void);
int list_map(int module, int i);   // (update 61) a Braids shape (module 2) or Rings model (10) as it plays: a withdrawn one gives the nearest kept
void core_stats(uint32_t *job, uint32_t *wait, uint32_t *inl);
void core_stats_reset(void);
// FM1 Phase 2 step 1 (drums; built with RACK_DRUMS, else these do nothing): four drum voices of Braids' drum models (rack.cc "DRUMS").
// drum_hit: voice 0-3, model 0-8 (KICK, SNARE, CYMBAL, STRUCK DRUM, STRUCK BELL, FILTERED NOISE, CLOCKED NOISE, PARTICLE NOISE, TWIN PEAKS), the note, the
// model's two parameters 0..1, DECAY (seconds to fall 60 dB), LEVEL, PAN -1..1, SEND 0..1 (the share that goes through the effects).
// drum_run: the firmware's pattern is playing; drum_busy: IF BUSY 0 NONE, 1 CLOUD (Clouds' DENSITY held at 0.9 while the drums run), 2 VOICES.
// drum_status: 0 voices sounding, 1 arena bytes taken, 2 Clouds held back now, 3 blocks rendered in a job, 4 blocks rendered by the first core, 5 has room
void drum_hit(int voice, int model, float note, float p1, float p2, float decay, float level, float pan, float send);
void drum_run(int on);
void drum_busy(int mode);
void drum_off(void);
void drum_cut(int voice);   // a CHOKE: the voice dies away in about 8 ms
int drum_status(int what);
#ifdef RACK_HOST_CHECKS
int drum_identity(int model, int blocks, uint32_t seed);   // the host check: samples that differ between the stock Braids class and the drum copy
int drum_sizes(int what);
#endif
#endif

#ifdef __cplusplus
}
#endif
