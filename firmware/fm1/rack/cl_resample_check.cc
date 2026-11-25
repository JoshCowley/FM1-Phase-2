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
    // update 44 (CLQ_16K): the 48 <-> 16 kHz pair. A tone below 5 kHz comes back with its own gain (the pair is -3 dB at 5 kHz and -8 dB at 6 kHz: both ways) and
    // CL16_DELAY frames late, with no jump at a block edge; a tone above 10 kHz folds to 16 kHz minus the tone, and must come out 30 dB down (60 dB above 11 kHz)
    printf("\n16 kHz pair (CLQ_16K): delay %d frames\n%8s %9s %10s %10s\n", CL16_DELAY, "Hz", "gain dB", "max err", "edge jump");
    static const double F16[] = {200, 1000, 2000, 3000, 4000, 5000, 6000, 8000, 9000, 10000, 11000, 12000, 15000, 18000, 21000};
    for (double f : F16) {
        Cl16Down dn; Cl16Up up; memset(&dn, 0, sizeof dn); memset(&up, 0, sizeof up);
        double e_re = 0, e_im = 0, maxerr = 0, edge = 0, prev = 0, ss = 0; long n = 0;
        for (int b = 0; b < 400; b++) {
            float x[48], y[16], z[48];
            for (int i = 0; i < 48; i++) x[i] = (float)(0.5 * sin(2 * PI * f * (b * 48 + i) / 48000.0));
            dn.run(x, y); up.run(y, z);
            for (int i = 0; i < 48; i++) {
                double t = b * 48 + i, ph = 2 * PI * f * (t - CL16_DELAY) / 48000.0;
                if (b >= 20) {
                    e_re += z[i] * sin(ph); e_im += z[i] * cos(ph); ss += (double)z[i] * z[i]; n++;
                    if (f <= 5000) { double er = fabs(z[i] - 0.5 * sin(ph)); if (er > maxerr) maxerr = er; }
                    double st = fabs(z[i] - prev); if (i == 0 && st > edge) edge = st;
                }
                prev = z[i];
            }
        }
        double amp = 2 * sqrt(e_re * e_re + e_im * e_im) / n, rms = sqrt(ss / n);
        double gain = f <= 5000 ? 20 * log10(amp / 0.5) : 20 * log10(rms * 1.41421356 / 0.5);
        double fa = f < 8000 ? f : f < 16000 ? 16000 - f : f < 24000 ? f - 16000 : f;
        double step_ok = 2 * PI * (f <= 8000 ? f : fa) / 48000.0 * 0.5 * 1.1 + 1e-3;
        printf("%8.0f %9.1f %10.5f %10.5f%s\n", f, gain, f <= 5000 ? maxerr : 0.0, edge, f >= 9000 ? "   (all that comes out: the alias)" : "");
        if (f <= 3000 && fabs(gain) > 1.5) bad++;
        if (f <= 2000 && maxerr > 0.01) bad++;
        if (f <= 8000 && edge > step_ok * 1.5) bad++;
        if (f >= 9000 && f <= 10000 && gain > -24.0) bad++;
        if (f >= 11000 && gain > -40.0) bad++;
    }
    printf("%s\n", bad ? "FAIL" : "resampler check passed");
    return bad ? 1 : 0;
}
