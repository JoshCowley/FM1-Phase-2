// SPDX-License-Identifier: GPL-3.0
// FM1 Phase 2: licensed GPL-3.0, see LICENSE; third-party code and its licences: NOTICE.md.
// FM1 Phase 2's copy of clouds/dsp/fx/pitch_shifter.h (pichenettes/eurorack, MIT), found before the original by the include path (engine/shim first). One addition (update 44,
// CLQ_16K, Clouds' LIGHT): set_half(true) before Init runs the same shifter with windows of at most 1023 samples in a buffer of 2048 (Clouds at 16 kHz: the same seconds in half the memory). Off (the default) it is the original, sample for sample.
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
// Pitch shifter.

#ifndef CLOUDS_DSP_FX_PITCH_SHIFTER_H_
#define CLOUDS_DSP_FX_PITCH_SHIFTER_H_

#include "stmlib/stmlib.h"

#include "clouds/dsp/frame.h"
#include "clouds/dsp/fx/fx_engine.h"

namespace clouds {

class PitchShifter {
 public:
  PitchShifter() { }
  ~PitchShifter() { }
  
  void Init(uint16_t* buffer) {
    if (half_) engine8_.Init(buffer); else engine_.Init(buffer);
    phase_ = 0;
    size_ = half_ ? 1023.0f : 2047.0f;
  }
  inline void set_half(bool half) { half_ = half; }   // before Init (update 44)

  void Clear() {
    if (half_) engine8_.Clear(); else engine_.Clear();
  }

  inline void Process(FloatFrame* input_output, size_t size) {
    while (size--) {
      Process(input_output);
      ++input_output;
    }
  }
  
  void Process(FloatFrame* input_output) {
    if (half_) Run<E8, 1023>(engine8_, input_output); else Run<E, 2047>(engine_, input_output);
  }

 private:
  template<class EN, int MAXSIZE>
  void Run(EN& engine, FloatFrame* input_output) {
    typedef typename EN::template Reserve<MAXSIZE, typename EN::template Reserve<MAXSIZE> > Memory;
    typename EN::template DelayLine<Memory, 0> left;
    typename EN::template DelayLine<Memory, 1> right;
    typename EN::Context c;
    engine.Start(&c);
    
    phase_ += (1.0f - ratio_) / size_;
    if (phase_ >= 1.0f) {
      phase_ -= 1.0f;
    }
    if (phase_ <= 0.0f) {
      phase_ += 1.0f;
    }
    float tri = 2.0f * (phase_ >= 0.5f ? 1.0f - phase_ : phase_);
    float phase = phase_ * size_;
    float half = phase + size_ * 0.5f;
    if (half >= size_) {
      half -= size_;
    }
    
    c.Read(input_output->l, 1.0f);
    c.Write(left, 0.0f);
    c.Interpolate(left, phase, tri);
    c.Interpolate(left, half, 1.0f - tri);
    c.Write(input_output->l, 0.0f);

    c.Read(input_output->r, 1.0f);
    c.Write(right, 0.0f);
    c.Interpolate(right, phase, tri);
    c.Interpolate(right, half, 1.0f - tri);
    c.Write(input_output->r, 0.0f);
  }
  
  public:
  inline void set_ratio(float ratio) {
    ratio_ = ratio;
  }
  
  inline void set_size(float size) {
    float target_size = half_ ? 64.0f + (1023.0f - 64.0f) * size * size * size : 128.0f + (2047.0f - 128.0f) * size * size * size;
    ONE_POLE(size_, target_size, 0.05f)
  }
  
 private:
  typedef FxEngine<4096, FORMAT_16_BIT> E;
  typedef FxEngine<2048, FORMAT_16_BIT> E8;
  E engine_;
  E8 engine8_;
  bool half_ = false;
  float phase_;
  float ratio_;
  float size_;
  
  DISALLOW_COPY_AND_ASSIGN(PitchShifter);
};

}  // namespace clouds

#endif  // CLOUDS_DSP_FX_MINI_CHORUS_H_