// SPDX-License-Identifier: GPL-3.0
// FM1 Phase 2: licensed GPL-3.0, see LICENSE; third-party code and its licences: NOTICE.md.
// Update 45 (CLQ_E_RS, Clouds ECO): the 48 kHz <-> 16 kHz resamplers of ECO, a good deal cheaper than update 44's pair (cl_resample.h): one channel (the mono mix),
// down a 15-tap Kaiser sinc (beta 3, cutoff 6 kHz: -1.3 dB at 4 kHz, -6 dB at 6 kHz, -17 dB at 8 kHz, -42 dB from 10.5 kHz; 8 multiplies an output, the taps folded), up a
// cubic (Catmull-Rom) through the 16 kHz frames (phase 0 is a copy; 4 multiplies for each of the other two). Delay: 5 frames down, 6 up, 11 at 48 kHz in all (CL16E_DELAY).
#ifndef CL_ECO_H_
#define CL_ECO_H_
#define CL16E_DELAY 11
static const float CL_GE[8] = {0.249769f, 0.219343f, 0.143776f, 0.059488f, 0.0f, -0.022534f, -0.018376f, -0.006582f};
struct Cl16DownE {   // 48 frames in (the two channels, summed here: g = 0.5 x IN GAIN), 16 out; 14 frames of history
  float h[14 + 48];
  void run(const float* l, const float* r, float g, float* y) {
    for (int n = 0; n < 48; ++n) h[14 + n] = l[n] + r[n];
    for (int m = 0; m < 16; ++m) {
      const float* s = h + 3 * m + 2;   // 15 taps s[0] .. s[14], the centre s[7]
      float v = CL_GE[0] * s[7];
      v += CL_GE[1] * (s[6] + s[8]);
      v += CL_GE[2] * (s[5] + s[9]);
      v += CL_GE[3] * (s[4] + s[10]);
      v += CL_GE[5] * (s[2] + s[12]);
      v += CL_GE[6] * (s[1] + s[13]);
      v += CL_GE[7] * (s[0] + s[14]);
      y[m] = v * g;   // (tap 4, s[3] and s[11], is 0)
    }
    for (int n = 0; n < 14; ++n) h[n] = h[n + 48];
  }
};
struct Cl16UpE {     // 16 frames in, 48 out; 3 frames of history
  float h[3 + 16];
  void run(const float* y, float* x) {
    for (int m = 0; m < 16; ++m) h[3 + m] = y[m];
    for (int g = 0; g < 16; ++g) {
      const float a = h[g], b = h[g + 1], c = h[g + 2], d = h[g + 3];   // a b c d: the newest is d; the outputs run from b to c (Catmull-Rom at 0, 1/3 and 2/3)
      x[3 * g + 0] = b;
      x[3 * g + 1] = (-0.0740740741f) * a + 0.777777778f * b + 0.333333333f * c + (-0.0370370370f) * d;
      x[3 * g + 2] = (-0.0370370370f) * a + 0.333333333f * b + 0.777777778f * c + (-0.0740740741f) * d;
    }
    for (int n = 0; n < 3; ++n) h[n] = h[n + 16];
  }
};
#endif
