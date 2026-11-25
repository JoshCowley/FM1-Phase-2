// SPDX-License-Identifier: GPL-3.0
// FM1 Phase 2: licensed GPL-3.0, see LICENSE; third-party code and its licences: NOTICE.md.
// FM1 Phase 2's copy of clouds/dsp/fx/reverb.h (pichenettes/eurorack, MIT), found before the original by the include path (engine/shim first). One change (2.38,
// Josh's request for about 10 % less CPU): set_lite(true) takes out the input diffuser's modulated smear (the first allpass's interpolated read written back) and the
// fourth input allpass, 3 of the tank's 14 reads and 3 of its 9 writes. The default (false) is Mutable's, sample for sample: Clouds' own reverb is not touched.
// Update 44 (CLQ_16K, Clouds' LIGHT): set_half(true) before Init runs the same tank at half the delay lengths (and twice the LFO rate a sample) in a buffer of 8192, for
// Clouds at 16 kHz: the same seconds, in half the memory and half the samples. Off (the default) it is the original, sample for sample.
//
// Copyright 2014 Emilie Gillet.
//
// Author: Emilie Gillet (emilie.o.gillet@gmail.com)
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
// 
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
// 
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.
// 
// See http://creativecommons.org/licenses/MIT/ for more information.
//
// -----------------------------------------------------------------------------
//
// Reverb.

#ifndef CLOUDS_DSP_FX_REVERB_H_
#define CLOUDS_DSP_FX_REVERB_H_

#include "stmlib/stmlib.h"

#include "clouds/dsp/fx/fx_engine.h"
#include "clouds/dsp/rack_cl_q.h"

namespace clouds {

class Reverb {
 public:
  Reverb() { }
  ~Reverb() { }
  
  void Init(uint16_t* buffer) {
    eco_ = (rack_cl_q & CLQ_E_RV) != 0 && half_;   // update 45 (ECO): the small float tank in the same 16 KB
    if (eco_) { InitEco(reinterpret_cast<float*>(buffer)); lp_ = 0.7f; diffusion_ = 0.625f; lite_ = 0; return; }
    if (half_) {
      engine8_.Init(buffer);
      engine8_.SetLFOFrequency(LFO_1, 0.5f / 16000.0f);
      engine8_.SetLFOFrequency(LFO_2, 0.3f / 16000.0f);
    } else {
      engine_.Init(buffer);
      engine_.SetLFOFrequency(LFO_1, 0.5f / 32000.0f);
      engine_.SetLFOFrequency(LFO_2, 0.3f / 32000.0f);
    }
    lp_ = 0.7f;
    diffusion_ = 0.625f;
    lite_ = 0;
  }
  inline void set_half(bool half) { half_ = half; }   // before Init (update 44)
  inline void set_lite(int lite) { lite_ = lite; }   // bit 0: 2.38's trims; bit 1 (update 42): no modulated read of the second delay, three input allpasses become two; bit 2: one allpass less in each loop; bit 3: one input allpass
  
  void Process(FloatFrame* in_out, size_t size) {
    if (eco_) { RunEco(in_out, size); return; }
    if (half_) Run<E8, 2>(engine8_, in_out, size); else Run<E, 1>(engine_, in_out, size);
  }

  // update 45 (CLQ_E_RV, ECO): a small reverb in floats. The input (the two sides summed) goes through two allpasses, then two delays that feed each other through a
  // one-pole low-pass and an allpass each (the same tank as Dattorro's, with its lengths at about half and no modulation), the two taken out as left and right; the
  // gains, TIME, damping and amount as the full one's (set_time, set_lp, set_diffusion, set_amount), the wet at the same level. 3,660 floats of the 4,096.
  static const int kE1 = 61, kE2 = 89, kEA = 307, kEB = 359, kED1 = 1321, kED2 = 1523, kETotal = kE1 + kE2 + kEA + kEB + kED1 + kED2;
  void InitEco(float* b) {
    for (int i = 0; i < 4096; ++i) b[i] = 0.0f;
    e_ap1_ = b; e_ap2_ = e_ap1_ + kE1; e_apa_ = e_ap2_ + kE2; e_apb_ = e_apa_ + kEA; e_d1_ = e_apb_ + kEB; e_d2_ = e_d1_ + kED1;
    e_i1_ = e_i2_ = e_ia_ = e_ib_ = e_id1_ = e_id2_ = 0;
    lp_decay_1_ = lp_decay_2_ = 0.0f;
  }
  static inline float EAp(float* b, int* i, int n, float x, float k) {
    const float z = b[*i], w = x + k * z;
    b[*i] = w; if (++*i == n) *i = 0;
    return z - k * w;
  }
  void RunEco(FloatFrame* io, size_t size) {
    const float kap = diffusion_ * 0.85f, klp = lp_, krt = reverb_time_, amount = amount_, gain = input_gain_;
    float lp1 = lp_decay_1_, lp2 = lp_decay_2_;
    while (size--) {
      float x = (io->l + io->r) * gain;
      x = EAp(e_ap1_, &e_i1_, kE1, x, kap);
      x = EAp(e_ap2_, &e_i2_, kE2, x, kap);
      const float d1 = e_d1_[e_id1_], d2 = e_d2_[e_id2_];
      lp1 += klp * (x + krt * d2 - lp1);
      const float y1 = EAp(e_apa_, &e_ia_, kEA, lp1, -kap);
      lp2 += klp * (x + krt * d1 - lp2);
      const float y2 = EAp(e_apb_, &e_ib_, kEB, lp2, kap);
      e_d1_[e_id1_] = y1; if (++e_id1_ == kED1) e_id1_ = 0;
      e_d2_[e_id2_] = y2; if (++e_id2_ == kED2) e_id2_ = 0;
      io->l += (2.0f * y1 - io->l) * amount;
      io->r += (2.0f * y2 - io->r) * amount;
      ++io;
    }
    lp_decay_1_ = lp1; lp_decay_2_ = lp2;
  }

 private:
  template<class EN, int S>
  void Run(EN& engine, FloatFrame* in_out, size_t size) {
    // This is the Griesinger topology described in the Dattorro paper
    // (4 AP diffusers on the input, then a loop of 2x 2AP+1Delay).
    // Modulation is applied in the loop of the first diffuser AP for additional
    // smearing; and to the two long delays for a slow shimmer/chorus effect.
    typedef typename EN::template Reserve<113 / S,
      typename EN::template Reserve<162 / S,
      typename EN::template Reserve<241 / S,
      typename EN::template Reserve<399 / S,
      typename EN::template Reserve<1653 / S,
      typename EN::template Reserve<2038 / S,
      typename EN::template Reserve<3411 / S,
      typename EN::template Reserve<1913 / S,
      typename EN::template Reserve<1663 / S,
      typename EN::template Reserve<(4782 - (S - 1) * 2) / S> > > > > > > > > > Memory;
    typename EN::template DelayLine<Memory, 0> ap1;
    typename EN::template DelayLine<Memory, 1> ap2;
    typename EN::template DelayLine<Memory, 2> ap3;
    typename EN::template DelayLine<Memory, 3> ap4;
    typename EN::template DelayLine<Memory, 4> dap1a;
    typename EN::template DelayLine<Memory, 5> dap1b;
    typename EN::template DelayLine<Memory, 6> del1;
    typename EN::template DelayLine<Memory, 7> dap2a;
    typename EN::template DelayLine<Memory, 8> dap2b;
    typename EN::template DelayLine<Memory, 9> del2;
    typename EN::Context c;

    const float kap = diffusion_;
    const float klp = lp_;
    const float krt = reverb_time_;
    const float amount = amount_;
    const float gain = input_gain_;

    float lp_1 = lp_decay_1_;
    float lp_2 = lp_decay_2_;
    const bool lite = (lite_ & 1) != 0, lite2 = (lite_ & 2) != 0, lite3 = (lite_ & 4) != 0, lite4 = (lite_ & 8) != 0;

    while (size--) {
      float wet;
      float apout = 0.0f;
      engine.Start(&c);
      
      // Smear AP1 inside the loop.
      if (!lite) {
        c.Interpolate(ap1, 10.0f / S, LFO_1, 60.0f / S, 1.0f);
        c.Write(ap1, 100, 0.0f);
      }
      
      c.Read(in_out->l + in_out->r, gain);

      // Diffuse through 4 allpasses.
      c.Read(ap1 TAIL, kap);
      c.WriteAllPass(ap1, -kap);
      if (!lite4) {
        c.Read(ap2 TAIL, kap);
        c.WriteAllPass(ap2, -kap);
      }
      if (!lite2) {
        c.Read(ap3 TAIL, kap);
        c.WriteAllPass(ap3, -kap);
      }
      if (!lite) {
        c.Read(ap4 TAIL, kap);
        c.WriteAllPass(ap4, -kap);
      }
      c.Write(apout);
      
      // Main reverb loop.
      c.Load(apout);
      if (lite2) c.Read(del2, 4680 / S, krt); else c.Interpolate(del2, 4680.0f / S, LFO_2, 100.0f / S, krt);
      c.Lp(lp_1, klp);
      c.Read(dap1a TAIL, -kap);
      c.WriteAllPass(dap1a, kap);
      if (!lite3) {
        c.Read(dap1b TAIL, kap);
        c.WriteAllPass(dap1b, -kap);
      }
      c.Write(del1, 2.0f);
      c.Write(wet, 0.0f);

      in_out->l += (wet - in_out->l) * amount;

      c.Load(apout);
      // c.Interpolate(del1, 4450.0f, LFO_1, 50.0f, krt);
      c.Read(del1 TAIL, krt);
      c.Lp(lp_2, klp);
      c.Read(dap2a TAIL, kap);
      c.WriteAllPass(dap2a, -kap);
      if (!lite3) {
        c.Read(dap2b TAIL, -kap);
        c.WriteAllPass(dap2b, kap);
      }
      c.Write(del2, 2.0f);
      c.Write(wet, 0.0f);

      in_out->r += (wet - in_out->r) * amount;
      
      ++in_out;
    }
    
    lp_decay_1_ = lp_1;
    lp_decay_2_ = lp_2;
  }

 public:
  inline void set_amount(float amount) {
    amount_ = amount;
  }
  
  inline void set_input_gain(float input_gain) {
    input_gain_ = input_gain;
  }

  inline void set_time(float reverb_time) {
    reverb_time_ = reverb_time;
  }
  
  inline void set_diffusion(float diffusion) {
    diffusion_ = diffusion;
  }
  
  inline void set_lp(float lp) {
    lp_ = lp;
  }
  
 private:
  typedef FxEngine<16384, FORMAT_12_BIT> E;
  typedef FxEngine<8192, FORMAT_12_BIT> E8;
  E engine_;
  E8 engine8_;
  bool half_ = false;
  bool eco_ = false;
  float *e_ap1_, *e_ap2_, *e_apa_, *e_apb_, *e_d1_, *e_d2_;
  int e_i1_, e_i2_, e_ia_, e_ib_, e_id1_, e_id2_;
  
  float amount_;
  float input_gain_;
  float reverb_time_;
  float diffusion_;
  float lp_;
  
  float lp_decay_1_;
  float lp_decay_2_;
  int lite_;
  
  DISALLOW_COPY_AND_ASSIGN(Reverb);
};

}  // namespace clouds

#endif  // CLOUDS_DSP_FX_REVERB_H_
