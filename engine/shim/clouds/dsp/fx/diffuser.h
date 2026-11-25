// SPDX-License-Identifier: GPL-3.0
// FM1 Phase 2: licensed GPL-3.0, see LICENSE; third-party code and its licences: NOTICE.md.
// FM1 Phase 2's copy of clouds/dsp/fx/diffuser.h (pichenettes/eurorack, MIT), found before the original by the include path (engine/shim first). One addition (update 44,
// CLQ_16K, Clouds' LIGHT): set_half(true) before Init runs the same allpass chain at half the delay lengths in a buffer of 1024 (Clouds at 16 kHz: the same seconds in half the memory). Off (the default) it is the original, sample for sample.
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
// AP diffusion network.

#ifndef CLOUDS_DSP_FX_DIFFUSER_H_
#define CLOUDS_DSP_FX_DIFFUSER_H_

#include "stmlib/stmlib.h"

#include "clouds/dsp/fx/fx_engine.h"

namespace clouds {

class Diffuser {
 public:
  Diffuser() { }
  ~Diffuser() { }
  
  void Init(float* buffer) {
    if (half_) engine8_.Init(buffer); else engine_.Init(buffer);
  }
  inline void set_half(bool half) { half_ = half; }   // before Init (update 44)
  inline void set_mono(bool mono) { mono_ = mono; }   // update 45 (ECO): the two sides are the same (LOOPDLY on a mono buffer): the left is run and copied
  inline void set_lite(bool lite) { lite_ = lite; }   // update 44 (CLQ_DIFLITE): two allpasses a side, not four (the third and fourth are not run)

  void Process(FloatFrame* in_out, size_t size) {
    if (half_) Run<E8, 2>(engine8_, in_out, size); else Run<E, 1>(engine_, in_out, size);
  }

 private:
  template<class EN, int S>
  void Run(EN& engine, FloatFrame* in_out, size_t size) {
    typedef typename EN::template Reserve<126 / S,
      typename EN::template Reserve<180 / S,
      typename EN::template Reserve<269 / S,
      typename EN::template Reserve<444 / S,
      typename EN::template Reserve<151 / S,
      typename EN::template Reserve<205 / S,
      typename EN::template Reserve<245 / S,
      typename EN::template Reserve<405 / S> > > > > > > > Memory;
    typename EN::template DelayLine<Memory, 0> apl1;
    typename EN::template DelayLine<Memory, 1> apl2;
    typename EN::template DelayLine<Memory, 2> apl3;
    typename EN::template DelayLine<Memory, 3> apl4;
    typename EN::template DelayLine<Memory, 4> apr1;
    typename EN::template DelayLine<Memory, 5> apr2;
    typename EN::template DelayLine<Memory, 6> apr3;
    typename EN::template DelayLine<Memory, 7> apr4;
    typename EN::Context c;
    const float kap = 0.625f;
    while (size--) {
      engine.Start(&c);
      
      float wet = 0.0f;
      c.Read(in_out->l);
      c.Read(apl1 TAIL, kap);
      c.WriteAllPass(apl1, -kap);
      c.Read(apl2 TAIL, kap);
      c.WriteAllPass(apl2, -kap);
      if (!lite_) {
        c.Read(apl3 TAIL, kap);
        c.WriteAllPass(apl3, -kap);
        c.Read(apl4 TAIL, kap);
        c.WriteAllPass(apl4, -kap);
      }
      c.Write(wet, 0.0f);
      in_out->l += amount_ * (wet - in_out->l);
      if (mono_) { in_out->r = in_out->l; ++in_out; continue; }
      
      c.Read(in_out->r);
      c.Read(apr1 TAIL, kap);
      c.WriteAllPass(apr1, -kap);
      c.Read(apr2 TAIL, kap);
      c.WriteAllPass(apr2, -kap);
      if (!lite_) {
        c.Read(apr3 TAIL, kap);
        c.WriteAllPass(apr3, -kap);
        c.Read(apr4 TAIL, kap);
        c.WriteAllPass(apr4, -kap);
      }
      c.Write(wet, 0.0f);
      in_out->r += amount_ * (wet - in_out->r);

      ++in_out;
    }
  }
  
  public:
  void set_amount(float amount) {
    amount_ = amount;
  }
  
 private:
  typedef FxEngine<2048, FORMAT_32_BIT> E;
  typedef FxEngine<1024, FORMAT_32_BIT> E8;
  E engine_;
  E8 engine8_;
  bool half_ = false;
  bool lite_ = false;
  bool mono_ = false;
  
  float amount_;
  DISALLOW_COPY_AND_ASSIGN(Diffuser);
};

}  // namespace clouds

#endif  // CLOUDS_DSP_FX_DIFFUSER_H_
