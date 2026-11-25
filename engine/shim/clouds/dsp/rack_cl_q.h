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
  CLQ_24K = 128,      // Clouds runs at 24 kHz (a halfband each way, not the 3:2 resamplers)
  CLQ_16K = 256,      // update 44 (LIGHT): Clouds runs at 16 kHz (a 3:1 pair of 25-tap filters, engine/cl_resample.h), blocks of 16 frames; the grain sizes, the FFT, the reverb, diffuser and shifter lengths and the buffers follow it
  CLQ_RVALL = 512,    // update 44: Clouds' own reverb with all four of its trims (no smear, two input allpasses, no modulated read of the second delay, one tank allpass less each side)
  CLQ_SKIPFLT = 1024, // update 44: LOOPDLY's LP and HP (TEXTURE 0.5 to 0.55: the low-pass at the Nyquist, the high-pass under 30 Hz) are not run; they start again, emptied, when TEXTURE leaves that range
  CLQ_DIFLITE = 2048  // update 44: Clouds' diffuser (LOOPDLY's, and GRANULR's TEXTURE over 0.75) with two allpasses a side, not four
};
namespace clouds { extern unsigned rack_cl_q; }
#endif
