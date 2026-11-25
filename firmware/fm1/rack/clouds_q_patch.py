# SPDX-License-Identifier: GPL-3.0
# FM1 Phase 2: licensed GPL-3.0, see LICENSE; third-party code and its licences: NOTICE.md.
"""Update 42 (RENDER): the changes to Clouds' granular_processor.cc that the CLQ_* switches need (engine/shim/clouds/dsp/rack_cl_q.h), shared by
rack/make_units.py and bench/make_units.py. apply(gp, _sub) returns the changed text; call it after the update 8 changes."""

FB_OLD = """  float cutoff = (20.0f + 100.0f * feedback * feedback) / sample_rate();
  fb_filter_[0].set_f_q<FREQUENCY_FAST>(cutoff, 1.0f);
  fb_filter_[1].set(fb_filter_[0]);
  fb_filter_[0].Process<FILTER_MODE_HIGH_PASS>(&fb_[0].l, &fb_[0].l, size, 2);
  fb_filter_[1].Process<FILTER_MODE_HIGH_PASS>(&fb_[0].r, &fb_[0].r, size, 2);
  float fb_gain = feedback * (1.0f - freeze_lp_);
  for (size_t i = 0; i < size; ++i) {
    in_[i].l += fb_gain * (
        SoftLimit(fb_gain * 1.4f * fb_[i].l + in_[i].l) - in_[i].l);
    in_[i].r += fb_gain * (
        SoftLimit(fb_gain * 1.4f * fb_[i].r + in_[i].r) - in_[i].r);
  }
"""
FB_NEW = "  if (!((rack_cl_q & CLQ_SKIPFB) && feedback <= 0.0f)) {   // (FEEDBACK 0: its gain is 0, the sum below adds nothing)\n" + FB_OLD + "  }\n"


def apply(gp, _sub):
    gp = _sub(gp, "void GranularProcessor::Init(", """#include "clouds/dsp/rack_cl_q.h"
unsigned rack_cl_q = 0;               // update 42: the CLQ_* switches (engine/rack.cc sets them from RENDER)
static float* df_buf_;                // the diffuser's memory (CLQ_SKIPDIF clears it when the diffuser comes back)
static int df_idle_ = 0;
static int flt_idle_ = 0;            // update 44: LOOPDLY's filters skipped (CLQ_SKIPFLT)
void GranularProcessor::Init(""")
    gp = _sub(gp, "    float sr = sample_rate();\n", """    // update 44 (CLQ_16K, LIGHT): Clouds at 16 kHz in half the memory: the audio 12,288 samples a channel (stereo: 0.77 s) or 24,576 (mono), the workspace 24,576 B
    // (diffuser 1024 floats, reverb 8192 words, shifter 2048 words: each the same tank at half the delay lengths), the spectral mode's FFT 2048 (the same 128 ms)
    const bool half = (rack_cl_q & CLQ_16K) != 0;
    if (half) {
      if (num_channels_ == 1) {
        if (buffer_size[0] > 49152) buffer_size[0] = 49152;
      } else if (buffer_size[0] > 24576) {
        buffer_size[0] = buffer_size[1] = 24576;
        workspace = static_cast<uint8_t*>(buffer[0]) + buffer_size[0];
      }
      if (workspace_size > 24576) workspace_size = 24576;
    }
    float sr = sample_rate();
""")
    gp = _sub(gp, "    diffuser_.Init(allocator.Allocate<float>(2048));",
              "    diffuser_.set_half(half);\n    df_buf_ = allocator.Allocate<float>(half ? 1024 : 2048);\n    diffuser_.Init(df_buf_);\n    df_idle_ = 0; flt_idle_ = 0;")
    gp = _sub(gp, "    pitch_shifter_.Init((uint16_t*)correlator_data);", "    pitch_shifter_.set_half(half);\n    pitch_shifter_.Init((uint16_t*)correlator_data);")
    gp = _sub(gp, "          lut_sine_window_4096, 4096,", "          lut_sine_window_4096, half ? 2048 : 4096,")
    gp = _sub(gp, "    CONSTRAIN(lp_cutoff, 0.0f, 0.499f);\n", "    if (rack_cl_q & CLQ_16K) { lp_cutoff *= 2.0f; hp_cutoff *= 2.0f; }   // update 44: TEXTURE's cut-offs in Hz as at 32 kHz\n    CONSTRAIN(lp_cutoff, 0.0f, 0.499f);\n")
    gp = _sub(gp, "    float cutoff = parameters_.texture;\n", """    float cutoff = parameters_.texture;
    if ((rack_cl_q & CLQ_SKIPFLT) && cutoff >= 0.5f && cutoff <= 0.55f) {
      flt_idle_ = 1;                                      // (update 44) the low-pass sits at the Nyquist and the high-pass under 30 Hz: the filters are not run
    } else {
    if (flt_idle_) { for (int i = 0; i < 2; ++i) { lp_filter_[i].Init(); hp_filter_[i].Init(); } flt_idle_ = 0; }
""")
    gp = _sub(gp, "        &out_[0].r, &out_[0].r, size, 2);\n  }\n  \n  // This is what is fed back.", "        &out_[0].r, &out_[0].r, size, 2);\n    }\n  }\n  \n  // This is what is fed back.")
    gp = _sub(gp, FB_OLD, FB_NEW)
    gp = _sub(gp, "  float cutoff = (20.0f + 100.0f * feedback * feedback) / sample_rate();", "  float cutoff = (20.0f + 100.0f * feedback * feedback) / ((rack_cl_q & CLQ_16K) ? 16000.0f : sample_rate());   // (update 44: in Hz)")
    gp = _sub(gp, "    diffuser_.set_amount(diffusion);\n    diffuser_.Process(out_, size);", """    diffuser_.set_amount(diffusion);
    diffuser_.set_lite((rack_cl_q & CLQ_DIFLITE) != 0);
    if ((rack_cl_q & CLQ_SKIPDIF) && diffusion <= 0.0f) {
      df_idle_ = 1;                                       // amount 0: out_ += 0 * (wet - out_) leaves out_ as it is
    } else {
      if (df_idle_) { diffuser_.Init(df_buf_); df_idle_ = 0; }   // it was skipped: empty memory
      diffuser_.Process(out_, size);
    }""")
    gp = _sub(gp, "  reverb_.set_amount(reverb_amount * 0.54f);",
              "  reverb_.set_lite((rack_cl_q & CLQ_RVALL) ? 15 : (rack_cl_q & CLQ_CLRV) != 0);   // CLQ_RVALL: all four trims (update 44); CLQ_CLRV: the engine reverb's update 38 trim\n  reverb_.set_amount(reverb_amount * 0.54f);")
    # the BLEND crossfade: with BLEND at 1 and settled (the engine always asks Clouds for all wet) the two gains are constants, not 2 table lookups a sample
    gp = _sub(gp, """  ParameterInterpolator dry_wet_mod(&dry_wet_, parameters_.dry_wet, size);
  for (size_t i = 0; i < size; ++i) {
    float dry_wet = dry_wet_mod.Next();""", """  if ((rack_cl_q & CLQ_FASTMIX) && dry_wet_ == 1.0f && parameters_.dry_wet == 1.0f) {
    const float fade_in = Interpolate(lut_xfade_in, 1.0f, 16.0f);
    const float fade_out = Interpolate(lut_xfade_out, 1.0f, 16.0f);
    for (size_t i = 0; i < size; ++i) {
      float l = static_cast<float>(input[i].l) / 32768.0f * fade_out;
      float r = static_cast<float>(input[i].r) / 32768.0f * fade_out;
      l += out_[i].l * post_gain * fade_in;
      r += out_[i].r * post_gain * fade_in;
      output[i].l = SoftConvert(l);
      output[i].r = SoftConvert(r);
    }
    return;
  }
  ParameterInterpolator dry_wet_mod(&dry_wet_, parameters_.dry_wet, size);
  for (size_t i = 0; i < size; ++i) {
    float dry_wet = dry_wet_mod.Next();""")
    return gp
