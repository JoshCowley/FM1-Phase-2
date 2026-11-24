// SPDX-License-Identifier: GPL-3.0
// FM1 Phase 2: licensed GPL-3.0, see LICENSE; third-party code and its licences: NOTICE.md.
// Update 42: the Clouds quality switches (CLQ_*), one bit each. engine/rack.cc sets clouds::rack_cl_q from the Clouds page's RENDER (HIGH, NORMAL, LITE);
// the Clouds code (the shim player, the generated granular_processor unit) reads it. HIGH = 0 is exactly the 2.40 Clouds.
#ifndef RACK_CL_Q_H_
#define RACK_CL_Q_H_
enum {
  CLQ_FOLD = 1,       // the 3:2 resamplers add their symmetric taps first (rounding only)
  CLQ_FASTMIX = 2,    // the BLEND crossfade gains: slid between the block's two ends (the engine's mix), and Clouds' own, with BLEND at 1, a constant (no table lookups a sample)
  CLQ_SKIPDIF = 4,    // Clouds' diffuser is not run while its amount is 0 (GRANULR with TEXTURE under 0.75)
  CLQ_SKIPFB = 8,     // Clouds' feedback path (its high-pass and soft clip) is not run while FEEDBACK is 0
  CLQ_CLRV = 16,      // Clouds' own reverb without the smear of its first allpass and without its fourth input allpass (the engine's reverb trim, update 38)
  CLQ_CAP16 = 32,     // at most 16 grains
  CLQ_CAP12 = 64,     // at most 12 grains
  CLQ_24K = 128       // Clouds runs at 24 kHz (a halfband each way, not the 3:2 resamplers)
};
namespace clouds { extern unsigned rack_cl_q; }
#endif
