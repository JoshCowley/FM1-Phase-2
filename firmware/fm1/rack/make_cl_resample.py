#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0
# FM1 Phase 2: licensed GPL-3.0, see LICENSE; third-party code and its licences: NOTICE.md.
"""Writes engine/cl_resample.h: the 48 kHz <-> 32 kHz resampler tables of RACK_CLOUDS_32K (update 8).
One Kaiser-windowed sinc (43 taps at the 96 kHz common rate, cutoff 14.5 kHz, beta 6: -0.7 dB at 12 kHz, -4 dB at 14 kHz,
-22 dB at 17 kHz, -36 dB at 18 kHz, -75 dB from 19 kHz), cut into polyphase rows: 48 -> 32 (up 2, down 3: two rows of 22 taps)
and 32 -> 48 (up 3, down 2: three rows of 15 taps). Each row is scaled to a gain of exactly 1 at DC.
  python3 make_cl_resample.py            (from this folder; the header is committed)"""
import numpy as np
from pathlib import Path
R, FC, BETA, FS = 21, 14500.0, 6.0, 96000.0
n = np.arange(-R, R + 1)
h = np.sinc(2 * FC / FS * n) * (2 * FC / FS) * np.kaiser(2 * R + 1, BETA)
def row(c0, step, ntap, d0, pc):
    out = []
    for k in range(ntap):
        i = R + pc - step * (k + d0)
        out.append(h[i] if 0 <= i <= 2 * R else 0.0)
    out = np.array(out); return out / out.sum()
HD = [row(0, 2, 22, -10, pc) for pc in (0, 1)]
HU = [row(0, 3, 15, -7, pc) for pc in (0, 1, 2)]
def num(v):
    t = f"{v:.9g}"
    return t + ("f" if any(c in t for c in ".en") else ".0f")
def fmt(rows):
    return ",\n".join("  {" + ", ".join(num(v) for v in r) + "}" for r in rows)
STRUCTS = r'''
struct Cl32Down {    // 48 frames in, 32 out (one channel); 21 frames of history
  float h[21 + 48];
  void run(const float* x, float* y) {
    for (int n = 0; n < 48; ++n) h[21 + n] = x[n];
    for (int m = 0; m < 32; ++m) {
      int c = 3 * m - 21, q = c >> 1;
      const float* t = CL_HD[c & 1]; const float* s = h + 21 + q - 10;
      float v = 0.0f;
      for (int k = 0; k < 22; ++k) v += t[k] * s[k];
      y[m] = v;
    }
    for (int n = 0; n < 21; ++n) h[n] = h[n + 48];
  }
  // update 42 (CLQ_FOLD): the same sums with the symmetric taps added first (row 0: 21 taps about the 11th, row 1: 22 taps, mirrored: 11 multiplies, not 22), and no
  // index sums: the outputs alternate rows, two at a time (outputs 2i and 2i + 1 start 3i and 3i + 2 into the history)
  void runf(const float* x, float* y) {
    for (int n = 0; n < 48; ++n) h[21 + n] = x[n];
    const float* t1 = CL_HD[1]; const float* t0 = CL_HD[0];
    for (int i = 0; i < 16; ++i) {
      const float* s = h + 3 * i;
      float v = t1[0] * (s[0] + s[21]);
      v += t1[1] * (s[1] + s[20]);
      v += t1[2] * (s[2] + s[19]);
      v += t1[3] * (s[3] + s[18]);
      v += t1[4] * (s[4] + s[17]);
      v += t1[5] * (s[5] + s[16]);
      v += t1[6] * (s[6] + s[15]);
      v += t1[7] * (s[7] + s[14]);
      v += t1[8] * (s[8] + s[13]);
      v += t1[9] * (s[9] + s[12]);
      v += t1[10] * (s[10] + s[11]);
      y[2 * i] = v;
      s += 2;
      v = t0[10] * s[10];
      v += t0[0] * (s[0] + s[20]);
      v += t0[1] * (s[1] + s[19]);
      v += t0[2] * (s[2] + s[18]);
      v += t0[3] * (s[3] + s[17]);
      v += t0[4] * (s[4] + s[16]);
      v += t0[5] * (s[5] + s[15]);
      v += t0[6] * (s[6] + s[14]);
      v += t0[7] * (s[7] + s[13]);
      v += t0[8] * (s[8] + s[12]);
      v += t0[9] * (s[9] + s[11]);
      y[2 * i + 1] = v;
    }
    for (int n = 0; n < 21; ++n) h[n] = h[n + 48];
  }
};
struct Cl32Up {      // 32 frames in, 48 out; 16 frames of history
  float h[16 + 32];
  void run(const float* y, float* x) {
    for (int m = 0; m < 32; ++m) h[16 + m] = y[m];
    for (int n = 0; n < 48; ++n) {
      int c = 2 * n - 22, q = (c + 30) / 3 - 10, pc = c - 3 * q;
      const float* t = CL_HU[pc]; const float* s = h + 16 + q - 7;
      float v = 0.0f;
      for (int k = 0; k < 15; ++k) v += t[k] * s[k];
      x[n] = v;
    }
    for (int n = 0; n < 16; ++n) h[n] = h[n + 32];
  }
  // update 42 (CLQ_FOLD): row 0 is symmetric about its 8th tap (8 multiplies, not 15); rows 1 and 2 start with a zero tap (14, not 15); and no index sums: the
  // outputs 3g, 3g + 1, 3g + 2 are rows 2, 1, 0 and start 2g + 1, 2g + 2, 2g + 3 into the history
  void runf(const float* y, float* x) {
    for (int m = 0; m < 32; ++m) h[16 + m] = y[m];
    const float* t0 = CL_HU[0]; const float* t1 = CL_HU[1]; const float* t2 = CL_HU[2];
    for (int g = 0; g < 16; ++g) {
      const float* s = h + 2 * g + 1;
      float v = t2[1] * s[1];
      v += t2[2] * s[2];
      v += t2[3] * s[3];
      v += t2[4] * s[4];
      v += t2[5] * s[5];
      v += t2[6] * s[6];
      v += t2[7] * s[7];
      v += t2[8] * s[8];
      v += t2[9] * s[9];
      v += t2[10] * s[10];
      v += t2[11] * s[11];
      v += t2[12] * s[12];
      v += t2[13] * s[13];
      v += t2[14] * s[14];
      x[3 * g] = v;
      s += 1;
      v = t1[1] * s[1];
      v += t1[2] * s[2];
      v += t1[3] * s[3];
      v += t1[4] * s[4];
      v += t1[5] * s[5];
      v += t1[6] * s[6];
      v += t1[7] * s[7];
      v += t1[8] * s[8];
      v += t1[9] * s[9];
      v += t1[10] * s[10];
      v += t1[11] * s[11];
      v += t1[12] * s[12];
      v += t1[13] * s[13];
      v += t1[14] * s[14];
      x[3 * g + 1] = v;
      s += 1;
      v = t0[7] * s[7];
      v += t0[0] * (s[0] + s[14]);
      v += t0[1] * (s[1] + s[13]);
      v += t0[2] * (s[2] + s[12]);
      v += t0[3] * (s[3] + s[11]);
      v += t0[4] * (s[4] + s[10]);
      v += t0[5] * (s[5] + s[9]);
      v += t0[6] * (s[6] + s[8]);
      x[3 * g + 2] = v;
    }
    for (int n = 0; n < 16; ++n) h[n] = h[n + 32];
  }
};
'''
text = f"""// SPDX-License-Identifier: GPL-3.0
// FM1 Phase 2: licensed GPL-3.0, see LICENSE; third-party code and its licences: NOTICE.md.
// Generated by firmware/fm1/rack/make_cl_resample.py: the 48 kHz <-> 32 kHz tables of RACK_CLOUDS_32K (engine/rack.cc).
// A Kaiser-windowed sinc, 43 taps at 96 kHz, cutoff 14.5 kHz, beta 6, as polyphase rows, each of gain 1 at DC.
// Cl32Down / Cl32Up: the two resamplers (one channel each). CL_RS_R: its half length (the centre tap), in 96 kHz samples.
#define CL_RS_R {R}
static const float CL_HD[2][22] = {{   // 48 -> 32: row = parity of the centre (in 96 kHz samples), taps from 10 samples before it
{fmt(HD)}
}};
static const float CL_HU[3][15] = {{   // 32 -> 48: row = the centre's phase (0, 1, 2 of 3), taps from 7 samples before it
{fmt(HU)}
}};
{STRUCTS}"""
(Path(__file__).resolve().parent.parent.parent.parent / "engine" / "cl_resample.h").write_text(text)
print("engine/cl_resample.h written")
