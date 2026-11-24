// SPDX-License-Identifier: GPL-3.0
// FM1 Phase 2: licensed GPL-3.0, see LICENSE; third-party code and its licences: NOTICE.md.
// The engine's C interface (engine/rack.h) under names of the benchmark's own: fm1-base's unity build, which
// bench.c is part of, has functions called init, render and so on of its own. Built as one of the C++ units.
#include <stdint.h>
#include "../../engine/rack.h"
#include "stmlib/utils/random.h"

extern "C" {
void rk_init(void) { init(); }
void rk_seed(uint32_t s) { stmlib::Random::Seed(s); }   // each case starts from the same random numbers
void rk_set_module(int slot, int id) { set_module(slot, id); }
void rk_set_param(int slot, int param, float v) { set_param(slot, param, v); }
void rk_set_meta(int slot, int param, float lo, float hi, float span) { set_meta(slot, param, lo, hi, span); }
void rk_set_route(int i, int src, int slot, int param, float amt, int via) { set_route(i, src, slot, param, amt, via); }
void rk_set_macro(int i, float v) { set_macro(i, v); }
void rk_set_macro_glide(int i, float v) { set_macro_glide(i, v); }
void rk_set_macro_target(int k, int idx, int slot, int param, int depth, int start, int end, int curve) { set_macro_target(k, idx, slot, param, depth, start, end, curve); }
void rk_modsum_precompute(int on) { set_modsum_precompute(on); }
void rk_rv_mono(int on) { set_rv_mono(on); }
void rk_lim_fast(int on) { set_lim_fast(on); }
void rk_quality(unsigned m) { set_quality(m); }
unsigned rk_quality_default(void) { return quality_default(); }
void rk_set_master(float v) { set_master(v); }
void rk_set_master_fx(float lo, float mid, float hi, float comp) { set_master_fx(lo, mid, hi, comp); }
void rk_set_legato(int on) { set_legato(on); }
void rk_set_vintage(float v) { set_vintage(v); }
void rk_set_ext_slew(int i, float s) { set_ext_slew(i, s); }
void rk_set_tune(int note, float semis) { set_tune(note, semis); }
void rk_set_voices(int n) { set_voices(n); }
void rk_clouds_opts(int k32, int skip_rev, int grains) { set_clouds_opts(k32, skip_rev, grains); }
void rk_note_on(float note, float vel) { note_on(note, vel); }
void rk_all_off(void) { all_off(); }
void rk_render(int n) { render(n); }
float *rk_out_l(void) { return out_l(); }
float *rk_out_r(void) { return out_r(); }
int rk_fm1_status(int what) { return fm1_status(what); }
int rk_list_map(int module, int i) { return list_map(module, i); }   // (update 61)
void rk_fm1_fit(void) { fm1_fit(); }   // as the firmware does after loading a preset (bench.c, the preset cases)
// the second core (rack.cc set_worker): the 2 CORES cases
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
void rk_pump_set(float depth, float release_s, float shape, int target) { pump_set(depth, release_s, shape, target); }
void rk_drum_hit(int voice, int model, float note, float p1, float p2, float decay, float level, float pan, float send) { drum_hit(voice, model, note, p1, p2, decay, level, pan, send); }
void rk_drum_run(int on) { drum_run(on); }
void rk_drum_busy(int mode) { drum_busy(mode); }
void rk_drum_off(void) { drum_off(); }
int rk_drum_status(int what) { return drum_status(what); }
#ifdef RACK_HOST_CHECKS
int rk_drum_identity(int model, int blocks, uint32_t seed) { return drum_identity(model, blocks, seed); }
int rk_drum_sizes(int what) { return drum_sizes(what); }
#endif
}
