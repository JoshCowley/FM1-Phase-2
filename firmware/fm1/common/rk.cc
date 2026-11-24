// SPDX-License-Identifier: GPL-3.0
// FM1 Phase 2: licensed GPL-3.0, see LICENSE; third-party code and its licences: NOTICE.md.
// The engine's C interface (engine/rack.h) under names of FM1 Rack's own: fm1-base's unity build, which the
// firmware's C is part of, has functions called init, render and so on of its own. Built as one of the C++ units.
// (fm1/bench has its own copy, as it was built and tested.)
#include <stdint.h>
#include "../../engine/rack.h"
#include "stmlib/utils/random.h"

extern "C" {
void rk_init(void) { init(); }
void rk_seed(uint32_t s) { stmlib::Random::Seed(s); }   // each case starts from the same random numbers
void rk_set_module(int slot, int id) { set_module(slot, id); }
void rk_set_param(int slot, int param, float v) { set_param(slot, param, v); }
void rk_set_param_glide(int slot, int param, float v) { set_param_glide(slot, param, v); }
void rk_set_meta(int slot, int param, float lo, float hi, float span) { set_meta(slot, param, lo, hi, span); }
void rk_set_route(int i, int src, int slot, int param, float amt, int via) { set_route(i, src, slot, param, amt, via); }
void rk_set_macro(int i, float v) { set_macro(i, v); }
void rk_set_macro_glide(int i, float v) { set_macro_glide(i, v); }
void rk_set_macro_target(int k, int idx, int slot, int param, int depth, int start, int end, int curve) { set_macro_target(k, idx, slot, param, depth, start, end, curve); }
void rk_clear_macro_targets(void) { clear_macro_targets(); }
void rk_set_jam_table(const uint8_t *pairs, int n) { set_jam_table(pairs, n); }
void rk_set_jam_offsets(const float *o) { set_jam_offsets(o); }
void rk_clear_jam(void) { clear_jam(); }
int rk_get_macro_target(int k, int idx, int *out) { return get_macro_target(k, idx, out); }
float rk_macro_curve(float k, int start, int end, int curve) { return macro_curve(k, start, end, curve); }
#ifdef TEST
float rk_eff_param(int slot, int param) { return eff_param(slot, param); }
float rk_eff_param_nojam(int slot, int param) { return eff_param_nojam(slot, param); }
#endif
void rk_set_master(float v) { set_master(v); }
void rk_set_master_fx(float lo, float mid, float hi, float comp) { set_master_fx(lo, mid, hi, comp); }
void rk_set_legato(int on) { set_legato(on); }
void rk_set_vintage(float v) { set_vintage(v); }
void rk_set_fx_bypass(int slot, int off) { set_fx_bypass(slot, off); }
int rk_fx_bypass(int slot) { return fx_bypass(slot); }
void rk_set_ext_slew(int i, float s) { set_ext_slew(i, s); }
void rk_set_ext(int i, float v) { set_ext(i, v); }
void rk_pump_set(float depth, float release_s, float shape, int target) { pump_set(depth, release_s, shape, target); }   /* update 41 step 5: the kick's ducking of the synth */
void rk_set_tune(int note, float semis) { set_tune(note, semis); }
void rk_set_voices(int n) { set_voices(n); }
void rk_clouds_opts(int k32, int skip_rev, int grains) { set_clouds_opts(k32, skip_rev, grains); }
void rk_clouds_mask(unsigned m) { set_clouds_mask(m); }
void rk_note_on(float note, float vel) { note_on(note, vel); }
void rk_note_off(float note) { note_off(note); }
void rk_set_bend(float semis) { set_bend(semis); }
void rk_all_off(void) { all_off(); }
void rk_render(int n) { render(n); }
float *rk_out_l(void) { return out_l(); }
float *rk_out_r(void) { return out_r(); }
int rk_fm1_status(int what) { return fm1_status(what); }
void rk_fm1_fit(void) { fm1_fit(); }
float rk_voice_env(int i) { return voice_env(i); }
int rk_fm1_fit_needed(void) { return fm1_fit_needed(); }
int rk_list_map(int module, int i) { return list_map(module, i); }   // (update 61)
float rk_get_param(int slot, int param) { return get_param(slot, param); }
float rk_get_route(int i, int *out) { return get_route(i, out); }
float *rk_monitor(int slot) { return monitor(slot); }
#ifdef RACK_GR_STATS
void rk_gr_stats(double *out, int reset) { grain_stats(out, reset); }   // the host check's (CLOUDS grain starts)
#endif
// the second core: the platform's worker (plat_cpu2_run, plat_cpu2_wait) renders half the voices (rack.cc)
void rk_set_worker(int (*run)(void (*)(uint32_t), uint32_t), int (*wait)(void)) { set_worker(run, wait); }
uint32_t rk_worker_blocks(void) { return worker_blocks(); }
void rk_set_worker_ex(void (*sync)(void), int (*on)(void)) { set_worker_ex(sync, on); }
void rk_set_ticks(uint32_t (*ticks)(void)) { set_ticks(ticks); }
void rk_fx_core2(int on) { set_fx_core2(on); }
int rk_fx_core2_active(void) { return fx_core2_active(); }
void rk_fx_core2_voices(int on) { set_fx_core2_voices(on); }
int rk_shed_helps(void) { return shed_helps(); }
void rk_core_stats(uint32_t *job, uint32_t *wait, uint32_t *inl) { core_stats(job, wait, inl); }
void rk_core_stats_reset(void) { core_stats_reset(); }
void rk_drum_hit(int voice, int model, float note, float p1, float p2, float decay, float level, float pan, float send) { drum_hit(voice, model, note, p1, p2, decay, level, pan, send); }
void rk_drum_run(int on) { drum_run(on); }
void rk_drum_busy(int mode) { drum_busy(mode); }
void rk_drum_cut(int voice) { drum_cut(voice); }
void rk_drum_off(void) { drum_off(); }
int rk_drum_status(int what) { return drum_status(what); }
#ifdef RACK_HOST_CHECKS
int rk_drum_identity(int model, int blocks, uint32_t seed) { return drum_identity(model, blocks, seed); }
int rk_drum_sizes(int what) { return drum_sizes(what); }
#endif
}
