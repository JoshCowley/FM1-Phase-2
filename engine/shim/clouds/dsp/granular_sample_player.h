// SPDX-License-Identifier: GPL-3.0
// FM1 Phase 2: licensed GPL-3.0, see LICENSE; third-party code and its licences: NOTICE.md.
// FM1 Rack's copy of clouds/dsp/granular_sample_player.h (pichenettes/eurorack, MIT), found before the original by the include
// path (engine/shim first). One addition (update 27, the GR-1 style grain controls): WINDOW, SPEED and JITTER, which pick where each
// grain starts. With WINDOW at 0 (the neutral setting) every line below runs exactly as the original's, and draws no extra random number.
// The settings arrive in the rack_gr_* globals (engine/rack.cc sets them each block; the second core's job reads them).
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
// Granular playback of audio stored in a buffer.

#ifndef CLOUDS_DSP_GRANULAR_SAMPLE_PLAYER_H_
#define CLOUDS_DSP_GRANULAR_SAMPLE_PLAYER_H_

#include "stmlib/stmlib.h"

#include <algorithm>
#include <math.h>

#include "stmlib/dsp/atan.h"
#include "stmlib/dsp/units.h"
#include "stmlib/utils/random.h"

#include "clouds/dsp/audio_buffer.h"
#include "clouds/dsp/frame.h"
#include "clouds/dsp/grain.h"
#include "clouds/dsp/parameters.h"

#include "clouds/resources.h"

namespace clouds {

// FM1 Rack: the grain controls (engine/rack.cc defines and sets them). window 0..1 (the width of the region grains start in, as a share of the
// buffer: 0 = POSITION alone, the original behaviour), speed in x real time (+ forward, - reverse) for the playhead inside the window,
// jitter 0..1 (each grain's start moved at random over that share of the window). ph, pos, lo, hi: the playhead (state and what the screen reads)
extern float rack_gr_window, rack_gr_speed, rack_gr_jitter, rack_gr_ph, rack_gr_pos, rack_gr_lo, rack_gr_hi;

#ifdef RACK_GR_STATS
void rack_gr_stat(float start);   // the host check's: where each grain was scheduled (engine/rack.cc)
#endif

const int32_t kMaxNumGrains = 64;

using namespace stmlib;

class GranularSamplePlayer {
 public:
  GranularSamplePlayer() { }
  ~GranularSamplePlayer() { }
  
  void Init(int32_t num_channels, int32_t max_num_grains) {
    max_num_grains_ = max_num_grains;
    num_midfi_grains_ = 3 * max_num_grains / 4;
    gain_normalization_ = 1.0f;
    for (int32_t i = 0; i < kMaxNumGrains; ++i) {
      grains_[i].Init();
    }
    num_grains_ = 0.0f;
    num_channels_ = num_channels;
    grain_size_hint_ = 1024.0f;
    rack_dph_ = 0.0f;
  }
  
  template<Resolution resolution>
  void Play(
      const AudioBuffer<resolution>* buffer,
      const Parameters& parameters,
      float* out, size_t size) {
    float overlap = parameters.granular.overlap;
    overlap = overlap * overlap * overlap;
    float target_num_grains = max_num_grains_ * overlap;
    float p = target_num_grains / static_cast<float>(grain_size_hint_);
    float space_between_grains = grain_size_hint_ / target_num_grains;
    if (parameters.granular.use_deterministic_seed) {
      p = -1.0f;
    } else {
      grain_rate_phasor_ = -1000.0f;
    }
    
    // FM1 Rack: the playhead's step a sample (forward = the position moving toward the newest audio), before the grains are scheduled.
    // The window's width is floored so a tiny window cycles fast but finitely.
    rack_dph_ = 0.0f;
    if (rack_gr_window > 0.0f && rack_gr_speed != 0.0f) {
      float w = rack_gr_window < 0.002f ? 0.002f : rack_gr_window;
      rack_dph_ = -rack_gr_speed / (static_cast<float>(buffer->size()) * w);
    }

    // Build a list of available grains.
    int32_t num_available_grains = FillAvailableGrainsList();
    
    // Try to schedule new grains.
    bool seed_trigger = parameters.trigger;
    for (size_t t = 0; t < size; ++t) {
      grain_rate_phasor_ += 1.0f;
      bool seed_probabilistic = Random::GetFloat() < p
          && target_num_grains > num_grains_;
      bool seed_deterministic = grain_rate_phasor_ >= space_between_grains;
      bool seed = seed_probabilistic || seed_deterministic || seed_trigger;
      if (num_available_grains && seed) {
        --num_available_grains;
        int32_t index = available_grains_[num_available_grains];
        GrainQuality quality;
        if (num_available_grains < num_midfi_grains_) {
          quality = GRAIN_QUALITY_MEDIUM;
        } else {
          quality = GRAIN_QUALITY_HIGH;
        }
        
        Grain* g = &grains_[index];
        ScheduleGrain(
            g,
            parameters,
            t,
            buffer->size(),
            buffer->head() - size + t,
            quality);
        grain_rate_phasor_ = 0.0f;
        seed_trigger = false;
      }
    }
    
    if (rack_gr_window > 0.0f) {   // the playhead moves on by the block, and tells the screen where it is
      rack_gr_ph += rack_dph_ * static_cast<float>(size);
      rack_gr_ph -= floorf(rack_gr_ph);
      RackWindow(parameters.position, &rack_gr_lo, &rack_gr_hi);
      rack_gr_pos = rack_gr_lo + (rack_gr_hi - rack_gr_lo) * rack_gr_ph;
    } else {
      rack_gr_lo = rack_gr_hi = rack_gr_pos = parameters.position;
    }

    // Overlap grains.
    std::fill(&out[0], &out[size * 2], 0.0f);
    float* e = envelope_buffer_;
    for (int32_t i = 0; i < max_num_grains_; ++i) {
      Grain* g = &grains_[i];
      if (g->recommended_quality() == GRAIN_QUALITY_HIGH) {
        if (num_channels_ == 1) {
          g->OverlapAdd<1, GRAIN_QUALITY_HIGH>(buffer, out, e, size);
        } else {
          g->OverlapAdd<2, GRAIN_QUALITY_HIGH>(buffer, out, e, size);
        }
      } else if (g->recommended_quality() == GRAIN_QUALITY_MEDIUM) {
        if (num_channels_ == 1) {
          g->OverlapAdd<1, GRAIN_QUALITY_MEDIUM>(buffer, out, e, size);
        } else {
          g->OverlapAdd<2, GRAIN_QUALITY_MEDIUM>(buffer, out, e, size);
        }
      } else {
        if (num_channels_ == 1) {
          g->OverlapAdd<1, GRAIN_QUALITY_LOW>(buffer, out, e, size);
        } else {
          g->OverlapAdd<2, GRAIN_QUALITY_LOW>(buffer, out, e, size);
        }
      }
    }
    
    // Compute normalization factor.
    int32_t active_grains = max_num_grains_ - num_available_grains;
    SLOPE(num_grains_, static_cast<float>(active_grains), 0.9f, 0.2f);

    float gain_normalization = num_grains_ > 2.0f
        ? fast_rsqrt_carmack(num_grains_ - 1.0f)
        : 1.0f;  
    float window_gain = 1.0f + 2.0f * parameters.granular.window_shape;
    CONSTRAIN(window_gain, 1.0f, 2.0f);
    gain_normalization *= Crossfade(
        1.0f, window_gain, parameters.granular.overlap);

    // Apply gain normalization.
    for (size_t t = 0; t < size; ++t) {
      ONE_POLE(gain_normalization_, gain_normalization, 0.01f)
      *out++ *= gain_normalization_;
      *out++ *= gain_normalization_;
    }
  }
  
 private:
  int32_t FillAvailableGrainsList() {
    int32_t num_available_grains = 0;
    for (int32_t i = 0; i < max_num_grains_; ++i) {
      if (!grains_[i].active()) {
        available_grains_[num_available_grains] = i;
        ++num_available_grains;
      }
    }
    return num_available_grains;
  }
  
  // the window around `position`: width WINDOW, moved to stay inside 0..1 when it would run past an end
  static void RackWindow(float position, float* lo, float* hi) {
    float w = rack_gr_window > 1.0f ? 1.0f : rack_gr_window;
    float l = position - 0.5f * w;
    if (l > 1.0f - w) l = 1.0f - w;
    if (l < 0.0f) l = 0.0f;
    *lo = l;
    *hi = l + w;
  }

  void ScheduleGrain(
      Grain* grain,
      const Parameters& parameters,
      int32_t pre_delay,
      int32_t buffer_size,
      int32_t buffer_head,
      GrainQuality quality) {
    float position = parameters.position;
    if (rack_gr_window > 0.0f) {
      // FM1 Rack: the region [lo, hi] of the width WINDOW around POSITION (kept inside the buffer), the playhead's place in it
      // for this grain's moment in the block, JITTER moving it over that share of the window (wrapped: even over the whole of it)
      float lo, hi, u = rack_gr_ph + static_cast<float>(pre_delay) * rack_dph_;
      RackWindow(position, &lo, &hi);
      u -= floorf(u);
      if (rack_gr_jitter > 0.0f) {
        u += rack_gr_jitter * (Random::GetFloat() - 0.5f);
        u -= floorf(u);
      }
      position = lo + (hi - lo) * u;
    }
#ifdef RACK_GR_STATS
    rack_gr_stat(position);
#endif
    float pitch = parameters.pitch;
    float window_shape = parameters.granular.window_shape;
    float grain_size = Interpolate(lut_grain_size, parameters.size, 256.0f);
    float pitch_ratio = SemitonesToRatio(pitch);
    float inv_pitch_ratio = SemitonesToRatio(-pitch);
    float pan = 0.5f + parameters.stereo_spread * (Random::GetFloat() - 0.5f);
    float gain_l, gain_r;
    if (num_channels_ == 1) {
      gain_l = Interpolate(lut_sin, pan, 256.0f);
      gain_r = Interpolate(lut_sin + 256, pan, 256.0f);
    } else {
      if (pan < 0.5f) {
        gain_l = 1.0f;
        gain_r = 2.0f * pan;
      } else {
        gain_r = 1.0f;
        gain_l = 2.0f * (1.0f - pan);
      }
    }
    
    if (pitch_ratio > 1.0f) {
      // The grain's play-head moves faster than the buffer record-head.
      // we must make sure that the grain will not consume too much data.
      // In some situations, it might be necessary to reduce the size of the
      // grain.
      grain_size = std::min(grain_size, buffer_size * 0.25f * inv_pitch_ratio);
    }

    float eaten_by_play_head = grain_size * pitch_ratio;
    float eaten_by_recording_head = grain_size;

    float available = 0.0;
    available += static_cast<float>(buffer_size);
    available -= eaten_by_play_head;
    available -= eaten_by_recording_head;

    int32_t size = static_cast<int32_t>(grain_size) & ~1;
    int32_t start = buffer_head - static_cast<int32_t>(
        position * available + eaten_by_play_head);
    grain->Start(
        pre_delay,
        buffer_size,
        start,
        size,
        static_cast<uint32_t>(pitch_ratio * 65536.0f),
        window_shape,
        gain_l,
        gain_r,
        quality);
    ONE_POLE(grain_size_hint_, grain_size, 0.1f);
  }
  
  int32_t max_num_grains_;
  int32_t num_midfi_grains_;
  int32_t num_channels_;

  float num_grains_;
  float gain_normalization_;
  float grain_size_hint_;
  float rack_dph_;
  float grain_rate_phasor_;
  
  Grain grains_[kMaxNumGrains];
  int32_t available_grains_[kMaxNumGrains];
  float envelope_buffer_[kMaxBlockSize];
  
  DISALLOW_COPY_AND_ASSIGN(GranularSamplePlayer);
};

}  // namespace clouds

#endif  // CLOUDS_DSP_GRANULAR_SAMPLE_PLAYER_H_
