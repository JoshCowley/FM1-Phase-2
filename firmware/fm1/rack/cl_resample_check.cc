// SPDX-License-Identifier: GPL-3.0
// FM1 Phase 2: licensed GPL-3.0, see LICENSE; third-party code and its licences: NOTICE.md.
// The 48 kHz <-> 32 kHz resampler of RACK_CLOUDS_32K (engine/cl_resample.h), alone, on a computer:
//   c++ -O2 -o build/cl_resample_check cl_resample_check.cc -lm && build/cl_resample_check
// Down 48 -> 32 then up 32 -> 48, in the engine's blocks (48 frames in, 32 between). Prints, for sines of each frequency: the gain of
// the round trip (dB), the worst error against the ideal delayed sine, the worst jump across a block edge; for tones above 16 kHz
// the aliased image's level (it lands at 32 kHz minus the tone's frequency); and the latency. Exit 1 when a limit is missed.
#include <cstdio>
#include <cmath>
#include <cstring>
#include "../../../engine/cl_resample.h"
int main()
{
    const double PI = 3.14159265358979, LAT = (21 + 22) / 2.0;   // samples at 48 kHz
    static const double F[] = {200, 1000, 5000, 10000, 12000, 14000, 16000, 17000, 18000, 19000, 20000, 22000};
    int bad = 0;
    printf("%8s %9s %10s %10s\n", "Hz", "gain dB", "max err", "edge jump");
    for (double f : F) {
        Cl32Down dn; Cl32Up up;
        memset(&dn, 0, sizeof dn); memset(&up, 0, sizeof up);
        double e_re = 0, e_im = 0, maxerr = 0, edge = 0, prev = 0, ss = 0, maxstep = 0;
        long n = 0;
        for (int b = 0; b < 400; b++) {
            float x[48], y[32], z[48];
            for (int i = 0; i < 48; i++) x[i] = (float)(0.5 * sin(2 * PI * f * (b * 48 + i) / 48000.0));
            dn.run(x, y); up.run(y, z);
            for (int i = 0; i < 48; i++) {
                double t = b * 48 + i, ideal = 0.5 * sin(2 * PI * f * (t - LAT) / 48000.0);
                if (b >= 20) {
                    e_re += z[i] * sin(2 * PI * f * (t - LAT) / 48000.0); e_im += z[i] * cos(2 * PI * f * (t - LAT) / 48000.0);
                    ss += (double)z[i] * z[i]; n++;
                    if (f <= 14000) { double er = fabs(z[i] - ideal); if (er > maxerr) maxerr = er; }
                    double st = fabs(z[i] - prev); if (i == 0 && st > edge) edge = st; if (st > maxstep) maxstep = st;
                }
                prev = z[i];
            }
        }
        double amp = 2 * sqrt(e_re * e_re + e_im * e_im) / n, rms = sqrt(ss / n);
        double gain = f <= 14000 ? 20 * log10(amp / 0.5) : 20 * log10(rms * 1.41421356 / 0.5);   // above 16 kHz: all that comes out (the alias)
        double step_ok = 2 * PI * (f <= 14000 ? f : 32000 - f) / 48000.0 * 0.5 * 1.05 + 1e-4;
        printf("%8.0f %9.1f %10.5f %10.5f%s\n", f, gain, maxerr, edge, f > 16000 ? "   (alias level, at 32 kHz minus the tone)" : "");
        if (f <= 12000 && fabs(gain) > 1.5) bad++;
        if (f <= 10000 && maxerr > 0.005) bad++;
        if (edge > step_ok) bad++;                          // no click at the block edge: the step across it is the signal's own
        if (f >= 18000 && gain > -30.0) bad++;
        if (f >= 20000 && gain > -60.0) bad++;
    }
    printf("latency: %.1f samples at 48 kHz = %.3f ms (down 21 + up 22 samples at 96 kHz)\n", LAT, LAT / 48.0);
    printf("%s\n", bad ? "FAIL" : "resampler check passed");
    return bad ? 1 : 0;
}
