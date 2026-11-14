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
void rk_set_master(float v) { set_master(v); }
void rk_set_master_fx(float lo, float mid, float hi, float comp) { set_master_fx(lo, mid, hi, comp); }
void rk_set_legato(int on) { set_legato(on); }
void rk_set_vintage(float v) { set_vintage(v); }
void rk_set_fx_bypass(int slot, int off) { set_fx_bypass(slot, off); }
int rk_fx_bypass(int slot) { return fx_bypass(slot); }
void rk_set_ext_slew(int i, float s) { set_ext_slew(i, s); }
void rk_set_ext(int i, float v) { set_ext(i, v); }
void rk_set_tune(int note, float semis) { set_tune(note, semis); }
void rk_set_voices(int n) { set_voices(n); }
void rk_clouds_opts(int k32, int skip_rev, int grains) { set_clouds_opts(k32, skip_rev, grains); }
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
float rk_get_param(int slot, int param) { return get_param(slot, param); }
float rk_get_route(int i, int *out) { return get_route(i, out); }
float *rk_monitor(int slot) { return monitor(slot); }
// the second core: the platform's worker (plat_cpu2_run, plat_cpu2_wait) renders half the voices (rack.cc)
void rk_set_worker(int (*run)(void (*)(uint32_t), uint32_t), int (*wait)(void)) { set_worker(run, wait); }
uint32_t rk_worker_blocks(void) { return worker_blocks(); }
void rk_set_worker_ex(void (*sync)(void), int (*on)(void)) { set_worker_ex(sync, on); }
void rk_set_ticks(uint32_t (*ticks)(void)) { set_ticks(ticks); }
void rk_fx_core2(int on) { set_fx_core2(on); }
int rk_fx_core2_active(void) { return fx_core2_active(); }
void rk_core_stats(uint32_t *job, uint32_t *wait, uint32_t *inl) { core_stats(job, wait, inl); }
void rk_core_stats_reset(void) { core_stats_reset(); }
}
